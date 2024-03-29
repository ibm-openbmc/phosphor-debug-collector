#include "config.h"

#include "dump_manager_resource.hpp"

#include "dump-extensions/openpower-dumps/openpower_dumps_config.h"

#include "dump_utils.hpp"
#include "op_dump_consts.hpp"
#include "resource_dump_entry.hpp"
#include "resource_dump_serialize.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>

namespace openpower
{
namespace dump
{
namespace resource
{

using namespace phosphor::logging;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

void Manager::notify(uint32_t dumpId, uint64_t size)
{
    // Get the timestamp
    uint64_t timeStamp =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    // If there is an entry with invalid id update that.
    // If there a completed one with same source id ignore it
    // if there is no invalid id, create new entry
    openpower::dump::resource::Entry* upEntry = NULL;
    for (auto& entry : entries)
    {
        openpower::dump::resource::Entry* resEntry =
            dynamic_cast<openpower::dump::resource::Entry*>(entry.second.get());

        // If there is already a completed entry with input source id then
        // ignore this notification.
        if ((resEntry->sourceDumpId() == dumpId) &&
            (resEntry->status() == phosphor::dump::OperationStatus::Completed))
        {
            log<level::INFO>(
                fmt::format("Resource dump entry with source dump id({}) is "
                            "already present with entry id({})",
                            dumpId, resEntry->getDumpId())
                    .c_str());
            return;
        }

        // Save the fist entry with INVALID_SOURCE_ID
        // but continue in the loop to make sure the
        // new entry is not duplicate
        if ((resEntry->status() ==
             phosphor::dump::OperationStatus::InProgress) &&
            (resEntry->sourceDumpId() == INVALID_SOURCE_ID) &&
            (upEntry == NULL))
        {
            upEntry = resEntry;
        }
    }
    if (upEntry != NULL)
    {
        log<level::INFO>(fmt::format("Resouce Dump Notify: Updating dumpId({}) "
                                     "with source Id({}) Size({})",
                                     upEntry->getDumpId(), dumpId, size)
                             .c_str());
        upEntry->update(timeStamp, size, dumpId);
        return;
    }

    // Get the id
    auto id = lastEntryId + 1;
    auto idString = std::to_string(id);
    auto objPath = std::filesystem::path(baseEntryPath) / idString;

    // TODO: Get the generator Id from the persisted file.
    // For now replacing it with null

    try
    {
        log<level::INFO>(fmt::format("Resouce Dump Notify: creating new dump "
                                     "entry dumpId({}) Id({}) Size({})",
                                     id, dumpId, size)
                             .c_str());
        auto entry = std::make_unique<resource::Entry>(
            bus, objPath.c_str(), id, timeStamp, size, dumpId, std::string(),
            std::string(), std::string(),
            phosphor::dump::OperationStatus::Completed, baseEntryPath, *this);
        serialize(*entry.get());
        entries.insert(std::make_pair(id, std::move(entry)));
    }
    catch (const std::invalid_argument& e)
    {
        log<level::ERR>(fmt::format("Error in creating resource dump entry, "
                                    "errormsg({}),OBJECTPATH({}),ID({}),"
                                    "TIMESTAMP({}),SIZE({}),SOURCEID({})",
                                    e.what(), objPath.c_str(), id, timeStamp,
                                    size, dumpId)
                            .c_str());
        report<InternalFailure>();
        return;
    }
    lastEntryId++;
}

sdbusplus::message::object_path
    Manager::createDump(phosphor::dump::DumpCreateParams params)
{
    using NotAllowed =
        sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
    using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;

    // Allow creating resource dump only when the host is up.
    if (!phosphor::dump::isHostRunning())
    {
        elog<NotAllowed>(
            Reason("Resource dump can be initiated only when the host is up"));
        return std::string();
    }

    using InvalidArgument =
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
    using Argument = xyz::openbmc_project::Common::InvalidArgument;
    using CreateParameters =
        sdbusplus::com::ibm::Dump::server::Create::CreateParameters;
    using CreateParametersXYZ =
        sdbusplus::xyz::openbmc_project::Dump::server::Create::CreateParameters;

    auto id = lastEntryId + 1;
    auto idString = std::to_string(id);
    auto objPath = std::filesystem::path(baseEntryPath) / idString;
    std::time_t timeStamp = std::time(nullptr);

    std::string vspString;
    auto iter = params.find(
        sdbusplus::com::ibm::Dump::server::Create::
            convertCreateParametersToString(CreateParameters::VSPString));
    if (iter == params.end())
    {
        // Host will generate a default dump if no resource selector string
        // is provided. The default dump will be a non-disruptive system dump.
        log<level::INFO>(
            "VSP string is not provided, a non-disruptive system dump will be "
            "generated by the host");
    }
    else
    {
        try
        {
            vspString = std::get<std::string>(iter->second);
        }
        catch (const std::bad_variant_access& e)
        {
            // Exception will be raised if the input is not string
            log<level::ERR>("An invalid  vsp string is passed",
                            entry("ERROR_MSG=%s", e.what()));
            elog<InvalidArgument>(Argument::ARGUMENT_NAME("VSP_STRING"),
                                  Argument::ARGUMENT_VALUE("INVALID INPUT"));
        }
    }

    std::string pwd;
    iter = params.find(
        sdbusplus::com::ibm::Dump::server::Create::
            convertCreateParametersToString(CreateParameters::Password));
    if (iter == params.end())
    {
        log<level::INFO>("Password is not provide for resource dump");
    }
    else
    {
        try
        {
            pwd = std::get<std::string>(iter->second);
        }
        catch (const std::bad_variant_access& e)
        {
            // Exception will be raised if the input is not string
            log<level::ERR>(
                fmt::format("An invalid password string is passed errormsg({})",
                            e.what())
                    .c_str());
            elog<InvalidArgument>(Argument::ARGUMENT_NAME("PASSWORD"),
                                  Argument::ARGUMENT_VALUE("INVALID INPUT"));
        }
    }

    std::string generatorId;
    iter = params.find(
        sdbusplus::xyz::openbmc_project::Dump::server::Create::
            convertCreateParametersToString(CreateParametersXYZ::GeneratorId));
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
            elog<InvalidArgument>(Argument::ARGUMENT_NAME("GENERATOR_ID"),
                                  Argument::ARGUMENT_VALUE("INVALID INPUT"));
        }
    }

    try
    {
        auto entry = std::make_unique<resource::Entry>(
            bus, objPath.c_str(), id, timeStamp, 0, INVALID_SOURCE_ID,
            vspString, pwd, generatorId,
            phosphor::dump::OperationStatus::InProgress, baseEntryPath, *this);
        serialize(*entry.get());
        entries.insert(std::make_pair(id, std::move(entry)));
    }
    catch (const std::invalid_argument& e)
    {
        log<level::ERR>(
            fmt::format(
                "Error in creating resource dump "
                "entry,errormsg({}),OBJECTPATH({}), VSPSTRING({}), ID({})",
                e.what(), objPath.c_str(), vspString, id)
                .c_str());
        elog<InternalFailure>();
        return std::string();
    }
    lastEntryId++;
    return objPath.string();
}

void Manager::restore()
{
    std::filesystem::path dir(RESOURCE_DUMP_SERIAL_PATH);
    if (!std::filesystem::exists(dir) || std::filesystem::is_empty(dir))
    {
        log<level::INFO>(
            fmt::format(
                "Resource dump does not exist in the path ({}) to restore",
                dir.c_str())
                .c_str());
        return;
    }

    std::vector<uint32_t> dumpIds;
    for (auto& file : std::filesystem::directory_iterator(dir))
    {
        try
        {
            auto idNum = std::stol(file.path().filename().c_str());
            auto idString = std::to_string(idNum);
            auto objPath = std::filesystem::path(baseEntryPath) / idString;
            auto entry = std::make_unique<Entry>(
                bus, objPath, 0, 0, 0, 0, " ", " ", " ",
                phosphor::dump::OperationStatus::InProgress, baseEntryPath,
                *this, false);
            if (deserialize(file.path(), *entry))
            {
                entries.insert(std::make_pair(idNum, std::move(entry)));
                dumpIds.push_back(idNum);
            }
        }
        // Continue to retrieve the next dump entry
        catch (const std::invalid_argument& e)
        {
            log<level::ERR>(fmt::format("Exception caught during restore with "
                                        "errormsg({}) and OBJECTPATH({})",
                                        e.what(),
                                        file.path().filename().c_str())
                                .c_str());
        }
        catch (const std::exception& e)
        {
            log<level::ERR>(fmt::format("Exception caught during restore with "
                                        "errormsg({}) and OBJECTPATH({})",
                                        e.what(),
                                        file.path().filename().c_str())
                                .c_str());
        }
    }
    if (!dumpIds.empty())
    {
        lastEntryId = *(std::max_element(dumpIds.begin(), dumpIds.end()));
    }
}

} // namespace resource
} // namespace dump
} // namespace openpower
