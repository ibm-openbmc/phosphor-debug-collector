#include "config.h"

#include "dump_manager_bmc.hpp"

#include "bmc_dump_entry.hpp"
#include "dump_internal.hpp"
#include "xyz/openbmc_project/Common/error.hpp"
#include "xyz/openbmc_project/Dump/Create/error.hpp"

#include <fmt/core.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdeventplus/exception.hpp>
#include <sdeventplus/source/base.hpp>

#include <cmath>
#include <ctime>

namespace phosphor
{
namespace dump
{
namespace bmc
{

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

namespace internal
{

/** @brief Flag to reject user intiated dump if a dump is in progress*/
// TODO: https://github.com/openbmc/phosphor-debug-collector/issues/19
static bool fUserDumpInProgress = false;

void Manager::create(Type type, std::vector<std::string> fullPaths)
{
    dumpMgr.phosphor::dump::bmc::Manager::captureDump(type, fullPaths);
}

} // namespace internal

sdbusplus::message::object_path
    Manager::createDump(phosphor::dump::DumpCreateParams params)
{
    using NotAllowed =
        sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
    using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;
    if (params.size() > CREATE_DUMP_MAX_PARAMS)
    {
        log<level::WARNING>(
            "BMC dump accepts not more than 2 additional parameter");
    }

    // Get the originator id and type from params
    std::string originatorId;
    originatorTypes originatorType;
    phosphor::dump::extractOriginatorProperties(params, originatorId,
                                                originatorType);

    if (internal::fUserDumpInProgress == true)
    {
        elog<NotAllowed>(Reason("User initiated dump is already in progress"));
    }
    log<level::INFO>("User initiated dump started, setting flag");
    internal::fUserDumpInProgress = true;
    std::filesystem::path objPath;
    try
    {
        // Get the generator id from params
        using InvalidArgument =
            sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
        using Argument = xyz::openbmc_project::Common::InvalidArgument;
        using CreateParameters = sdbusplus::xyz::openbmc_project::Dump::server::
            Create::CreateParameters;

        std::string generatorId;
        auto iter = params.find(
            sdbusplus::xyz::openbmc_project::Dump::server::Create::
                convertCreateParametersToString(CreateParameters::GeneratorId));
        if (iter == params.end())
        {
            log<level::INFO>(
                "GeneratorId is not provided. Replacing the string with null");
        }
        else
        {
            try
            {
                generatorId = std::get<std::string>(iter->second);
            }
            catch (const std::bad_variant_access& e)
            {
                // Exception will be raised if the input is not string
                log<level::ERR>(
                    "An invalid  generatorId passed. It should be a string",
                    entry("ERROR_MSG=%s", e.what()));
                elog<InvalidArgument>(
                    Argument::ARGUMENT_NAME("GENERATOR_ID"),
                    Argument::ARGUMENT_VALUE("INVALID INPUT"));
            }
        }

        std::vector<std::string> paths;
        auto id = captureDump(Type::UserRequested, paths);

        // Entry Object path.
        objPath = std::filesystem::path(baseEntryPath) / std::to_string(id);

        std::time_t timeStamp = std::time(nullptr);

        createEntry(id, objPath, timeStamp, 0, std::string(), generatorId,
                    phosphor::dump::OperationStatus::InProgress, originatorId,
                    originatorType);
    }
    catch (const std::exception& ex)
    {
        log<level::INFO>("User initiated dump exception, resetting flag");
        internal::fUserDumpInProgress = false;
        log<level::ERR>(
            fmt::format("Exception caught errormsg({}), rethrowing", ex.what())
                .c_str());
        throw;
    }
    return objPath.string();
}

void Manager::createEntry(const uint32_t id, const std::string objPath,
                          const uint64_t ms, uint64_t fileSize,
                          const std::filesystem::path& file,
                          const std::string& generatorId,
                          phosphor::dump::OperationStatus status,
                          const std::string& originatorId,
                          originatorTypes originatorType)
{
    try
    {
        entries.insert(std::make_pair(
            id, std::make_unique<phosphor::dump::bmc::Entry>(
                    bus, objPath.c_str(), id, ms, fileSize, file, generatorId,
                    status, originatorId, originatorType, *this)));
    }
    catch (const std::invalid_argument& e)
    {
        log<level::ERR>(fmt::format("Error in creating BMC dump entry, "
                                    "errormsg({}), OBJECTPATH({}), ID({})",
                                    e.what(), objPath.c_str(), id)
                            .c_str());
        elog<InternalFailure>();
    }
}

void Manager::checkAndInitialize()
{
    checkAndCreateCoreDump();
}

void Manager::createEntry(const uint32_t id, const std::string objPath,
                          const uint64_t ms, uint64_t fileSize,
                          const std::filesystem::path& file,
                          phosphor::dump::OperationStatus status)
{
    createEntry(id, objPath, ms, fileSize, file, "", status, "",
                originatorTypes::Internal);
}

void Manager::checkAndCreateCoreDump()
{
    if (std::filesystem::exists(CORE_FILE_DIR) &&
        std::filesystem::is_directory(CORE_FILE_DIR))
    {
        std::vector<std::string> files;
        for (auto const& file :
             std::filesystem::directory_iterator(CORE_FILE_DIR))
        {
            if (std::filesystem::is_regular_file(file) &&
                (file.path().filename().string().starts_with("core.")))
            {
                // Consider only file name start with "core."
                files.push_back(file.path().string());
            }
        }
        if (!files.empty())
        {
            log<level::INFO>(
                fmt::format("Core file found, files size {}", files.size())
                    .c_str());
            try
            {
                captureDump(Type::ApplicationCored, files);
            }
            catch (const std::exception& excp)
            {
                log<level::ERR>(
                    fmt::format("Exception received while capturing dump: "
                                "{}",
                                excp.what())
                        .c_str());
            }
        }
    }
}

uint32_t Manager::captureDump(Type type,
                              const std::vector<std::string>& fullPaths)
{
    // Get Dump size.
    auto size = getAllowedSize();

    pid_t pid = fork();

    if (pid == 0)
    {
        std::filesystem::path dumpPath(dumpDir);
        auto id = std::to_string(lastEntryId + 1);
        dumpPath /= id;

        // get dreport type map entry
        auto tempType = TypeMap.find(type);

        execl("/usr/bin/dreport", "dreport", "-d", dumpPath.c_str(), "-i",
              id.c_str(), "-s", std::to_string(size).c_str(), "-q", "-v", "-p",
              fullPaths.empty() ? "" : fullPaths.front().c_str(), "-t",
              tempType->second.c_str(), nullptr);

        // dreport script execution is failed.
        auto error = errno;
        log<level::ERR>(
            fmt::format(
                "Error occurred during dreport function execution, errno({})",
                error)
                .c_str());
        elog<InternalFailure>();
    }
    else if (pid > 0)
    {
        Child::Callback callback = [this, type, pid](Child&, const siginfo_t*) {
            if (type == Type::UserRequested)
            {
                log<level::INFO>(
                    "User initiated dump completed, resetting flag");
                internal::fUserDumpInProgress = false;
            }
            this->childPtrMap.erase(pid);
        };
        try
        {
            childPtrMap.emplace(pid,
                                std::make_unique<Child>(eventLoop.get(), pid,
                                                        WEXITED | WSTOPPED,
                                                        std::move(callback)));
        }
        catch (const sdeventplus::SdEventError& ex)
        {
            // Failed to add to event loop
            log<level::ERR>(
                fmt::format(
                    "Error occurred during the sdeventplus::source::Child "
                    "creation ex({})",
                    ex.what())
                    .c_str());
            elog<InternalFailure>();
        }
    }
    else
    {
        auto error = errno;
        log<level::ERR>(
            fmt::format("Error occurred during fork, errno({})", error)
                .c_str());
        elog<InternalFailure>();
    }

    return ++lastEntryId;
}

} // namespace bmc
} // namespace dump
} // namespace phosphor
