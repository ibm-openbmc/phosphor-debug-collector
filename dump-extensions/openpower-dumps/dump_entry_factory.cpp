#include "dump_entry_factory.hpp"

#include "dump_utils.hpp"
#include "op_dump_consts.hpp"
#include "op_dump_util.hpp"

#include <com/ibm/Dump/Create/common.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Dump/Create/server.hpp>

namespace openpower::dump
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

std::unique_ptr<phosphor::dump::Entry> DumpEntryFactory::createSystemDumpEntry(
    uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
    const DumpParameters& dumpParams)
{
#if 0 // NOLINT(readability-avoid-unconditional-preprocessor-if)
    // Disabled during testing: allow a new system dump to be created even
    // when one is already in progress.  Re-enable for production.

    using Unavailable =
        sdbusplus::xyz::openbmc_project::Common::Error::Unavailable;

    if (openpower::dump::util::isSystemDumpInProgress(bus))
    {
        lg2::error("Another dump in progress or available to offload");
        elog<Unavailable>();
    }

    using NotAllowed =
        sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
    using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;

    auto isHostRunning = false;
    phosphor::dump::HostState hostState;
    try
    {
        isHostRunning = phosphor::dump::isHostRunning();
        hostState = phosphor::dump::getHostState();
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "System state cannot be determined, system dump is not allowed: "
            "{ERROR}",
            "ERROR", e);
        elog<NotAllowed>(Reason("System dump not allowed currently."));
    }
    bool isHostQuiesced = hostState == phosphor::dump::HostState::Quiesced;
    bool isHostTransitioningToOff =
        hostState == phosphor::dump::HostState::TransitioningToOff;
    // Allow creating system dump only when the host is up or quiesced
    // starting to power off
    if (!isHostRunning && !isHostQuiesced && !isHostTransitioningToOff)
    {
        lg2::error("System dump can be initiated only when the host is up "
                   "or quiesced or starting to poweroff");
        elog<NotAllowed>(Reason(
            "System dump can be initiated only when the host is up "
            "or quiesced or starting to poweroff"));
    }
#endif
    return std::make_unique<system::Entry>(
        bus, objPath.c_str(), id, timeStamp, 0,
        phosphor::dump::OperationStatus::InProgress, dumpParams.originatorId,
        dumpParams.originatorType, mgr, dumpParams.eid.value_or(0));
}

std::unique_ptr<phosphor::dump::Entry>
    DumpEntryFactory::createResourceDumpEntry(
        uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
        bool createSysDump, const DumpParameters& dumpParams)
{
    using NotAllowed =
        sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
    using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;
    if (!phosphor::dump::isHostRunning())
    {
        elog<NotAllowed>(
            Reason("Resource dump can be initiated only when the host is up"));
    }

    std::string vspString =
        (!dumpParams.vspString.has_value()) ? "" : *dumpParams.vspString;

    std::string userChallengeString = (!dumpParams.userChallenge.has_value())
                                          ? ""
                                          : *dumpParams.userChallenge;

    std::string acfPath =
        (!dumpParams.acfPath.has_value()) ? "" : *dumpParams.acfPath;

    if (createSysDump)
    {
        return std::make_unique<system::Entry>(
            bus, objPath.c_str(), id, timeStamp, 0,
            phosphor::dump::OperationStatus::InProgress,
            dumpParams.originatorId, dumpParams.originatorType,
            system::SystemImpact::NonDisruptive, userChallengeString, mgr);
    }

    return std::make_unique<resource::Entry>(
        bus, objPath.c_str(), id, timeStamp, 0, vspString, userChallengeString,
        acfPath, phosphor::dump::OperationStatus::InProgress,
        dumpParams.originatorId, dumpParams.originatorType, mgr);
}

std::unique_ptr<phosphor::dump::Entry>
    DumpEntryFactory::createHostbootDumpEntry(
        uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams)
{
    if (!dumpParams.eid.has_value())
    {
        lg2::error("Required parameter error log id is missing");
        util::throwInvalidArgument("ERROR_LOG_ID", "ARGUMENT_MISSING");
    }

    const uint64_t dump_eid = dumpParams.eid.value_or(0);

    return std::make_unique<hostboot::Entry>(
        bus, objPath.c_str(), id, timeStamp, 0, std::string(),
        phosphor::dump::OperationStatus::InProgress, dumpParams.originatorId,
        dumpParams.originatorType, dump_eid, mgr);
}

std::unique_ptr<phosphor::dump::Entry>
    DumpEntryFactory::createHardwareDumpEntry(
        uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams)
{
    if (!dumpParams.eid.has_value())
    {
        lg2::error("Required parameter error log id is missing");
        util::throwInvalidArgument("ERROR_LOG_ID", "ARGUMENT_MISSING");
    }
    if (!dumpParams.fid.has_value())
    {
        lg2::error("Required parameter id of failing unit is missing");
        util::throwInvalidArgument("FAILING_UNIT_ID", "ARGUMENT_MISSING");
    }

    const uint64_t dump_eid = dumpParams.eid.value_or(0);
    const uint64_t dump_fid = dumpParams.fid.value_or(0);

    return std::make_unique<hardware::Entry>(
        bus, objPath.c_str(), id, timeStamp, 0, std::string(),
        phosphor::dump::OperationStatus::InProgress, dumpParams.originatorId,
        dumpParams.originatorType, dump_eid, dump_fid, mgr);
}

std::unique_ptr<phosphor::dump::Entry> DumpEntryFactory::createSBEDumpEntry(
    uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
    const DumpParameters& dumpParams)
{
    if (!dumpParams.eid.has_value())
    {
        lg2::error("Required parameter error log id is missing");
        util::throwInvalidArgument("ERROR_LOG_ID", "ARGUMENT_MISSING");
    }
    if (!dumpParams.fid.has_value())
    {
        lg2::error("Required parameter id of failing unit is missing");
        util::throwInvalidArgument("FAILING_UNIT_ID", "ARGUMENT_MISSING");
    }
    const uint64_t dump_eid = dumpParams.eid.value_or(0);
    const uint64_t dump_fid = dumpParams.fid.value_or(0);

    return std::make_unique<sbe::Entry>(
        bus, objPath.c_str(), id, timeStamp, 0, std::string(),
        phosphor::dump::OperationStatus::InProgress, dumpParams.originatorId,
        dumpParams.originatorType, dump_eid, dump_fid, mgr,
        dumpParams.dumpFilesPath, dumpParams.sbeDumpTriggerType);
}

std::unique_ptr<phosphor::dump::Entry> DumpEntryFactory::createEntry(
    uint32_t id, phosphor::dump::DumpCreateParams& params)
{
    DumpParameters dumpParams = util::extractDumpParameters(params);

    // If requested dump type is system and the vsp string is empty or "system"
    // the host will be creating a non-disruptive system dump and responding
    // with system dump type to avoid a never finished resource dump entry,
    // creating system dump entry in such cases

    bool createSystemDump =
        (dumpParams.type == OpDumpTypes::Resource &&
         (!dumpParams.vspString.has_value() || dumpParams.vspString->empty() ||
          toUpper(*dumpParams.vspString) == "SYSTEM"));

    uint32_t dumpIdPrefix = getDumpIdPrefix(
        createSystemDump ? OpDumpTypes::System : dumpParams.type);

    id |= dumpIdPrefix;
    std::string idStr = std::format("{:08X}", id);

    auto objPath = std::filesystem::path(baseEntryPath) / idStr;

    uint64_t timeStamp =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    switch (dumpParams.type)
    {
        case OpDumpTypes::System:
            return createSystemDumpEntry(id, objPath, timeStamp, dumpParams);
        case OpDumpTypes::Resource:
            return createResourceDumpEntry(id, objPath, timeStamp,
                                           createSystemDump, dumpParams);
        case OpDumpTypes::Hostboot:
            return createHostbootDumpEntry(id, objPath, timeStamp, dumpParams);
        case OpDumpTypes::Hardware:
            return createHardwareDumpEntry(id, objPath, timeStamp, dumpParams);
        case OpDumpTypes::MemoryBufferSBE:
        case OpDumpTypes::SBE:
            return createSBEDumpEntry(id, objPath, timeStamp, dumpParams);
        default:
            util::throwInvalidArgument("DUMP_TYPE_NOT_VALID", "INVALID_INPUT");
    }
    return nullptr;
}

std::unique_ptr<phosphor::dump::Entry>
    DumpEntryFactory::createEntryWithDefaults(
        uint32_t id, const std::filesystem::path& objPath)
{
    auto type = getDumpTypeFromId(id);

    switch (type)
    {
        case OpDumpTypes::System:
            return std::make_unique<system::Entry>(bus, objPath.string(), id,
                                                   mgr);
        case OpDumpTypes::Resource:
            return std::make_unique<resource::Entry>(
                bus, objPath.string(), id, 0, 0, "", "", "",
                phosphor::dump::OperationStatus::InProgress, "",
                originatorTypes::Internal, mgr);
        case OpDumpTypes::Hostboot:
            return std::make_unique<hostboot::Entry>(bus, objPath.string(), id,
                                                     mgr);
        case OpDumpTypes::Hardware:
            return std::make_unique<hardware::Entry>(bus, objPath.string(), id,
                                                     mgr);
        case OpDumpTypes::MemoryBufferSBE:
        case OpDumpTypes::SBE:
            return std::make_unique<sbe::Entry>(bus, objPath.string(), id, mgr);

        default:
            throw std::invalid_argument("Unsupported dump type");
    }
}

} // namespace openpower::dump
