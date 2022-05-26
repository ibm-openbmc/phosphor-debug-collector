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
    if (params.size() > 1)
    {
        log<level::WARNING>(
            "BMC dump accepts not more than 1 additional parameter");
    }
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
                    phosphor::dump::OperationStatus::InProgress);
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
                          phosphor::dump::OperationStatus status)
{
    try
    {
        entries.insert(
            std::make_pair(id, std::make_unique<phosphor::dump::bmc::Entry>(
                                   bus, objPath.c_str(), id, ms, fileSize, file,
                                   generatorId, status, *this)));
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
    createEntry(id, objPath, ms, fileSize, file, "", status);
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
            captureDump(Type::ApplicationCored, files);
        }
    }
}

/** @brief sd_event_add_child callback
 *
 *  @param[in] s - event source
 *  @param[in] si - signal info
 *  @param[in] userdata - pointer to Watch object
 *
 *  @returns 0 on success, -1 on fail
 */
static int dreportCallback(sd_event_source*, const siginfo_t*, void* type)
{
    Type* ptr = reinterpret_cast<Type*>(type);
    if (*ptr == Type::UserRequested)
    {
        log<level::INFO>("User initiated dump completed, resetting flag");
        internal::fUserDumpInProgress = false;
    }
    delete ptr;
    return 0;
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
        Type* typePtr = new Type();
        *typePtr = type;
        auto rc = sd_event_add_child(eventLoop.get(), nullptr, pid,
                                     WEXITED | WSTOPPED, dreportCallback,
                                     (void*)(typePtr));
        if (0 > rc)
        {
            // Failed to add to event loop
            log<level::ERR>(
                fmt::format(
                    "Error occurred during the sd_event_add_child call, rc({})",
                    rc)
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
