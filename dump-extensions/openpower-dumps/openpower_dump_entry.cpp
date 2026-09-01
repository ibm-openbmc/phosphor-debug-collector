#include "config.h"

#include "openpower_dump_entry.hpp"

#include "dump_manager.hpp"
#include "dump_offload.hpp"
#include "op_dump_consts.hpp"

#include <phosphor-logging/lg2.hpp>

namespace openpower::dump
{

void Entry::delete_()
{
    // Delete Dump file from Permanent location
    try
    {
        std::filesystem::remove_all(file.parent_path());
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        // Log Error message and continue
        lg2::error("Failed to delete dump file, errormsg: {ERROR}", "ERROR", e);
    }
#ifdef LOG_PEL_ON_DUMP_ACTIONS
    auto bus = sdbusplus::bus::new_default();
    // Log PEL for dump delete
    phosphor::dump::createPELOnDumpActions(
        bus, file, "Openpower Dump", std::format("{:08x}", id),
        "xyz.openbmc_project.Logging.Entry.Level.Informational",
        "xyz.openbmc_project.Dump.Error.Invalidate");
#endif
    // Remove Dump entry D-bus object
    phosphor::dump::Entry::delete_();
}

void Entry::initiateOffload(std::string uri)
{
    phosphor::dump::offload::requestOffload(file, id, uri);
    offloaded(true);
#ifdef LOG_PEL_ON_DUMP_ACTIONS
    auto bus = sdbusplus::bus::new_default();
    // Log PEL for dump offload
    phosphor::dump::createPELOnDumpActions(
        bus, file, "Openpower Dump", std::format("{:08x}", id),
        "xyz.openbmc_project.Logging.Entry.Level.Informational",
        "xyz.openbmc_project.Dump.Error.Offload");
#endif
}

namespace system
{

Entry::Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
             uint64_t timeStamp, uint64_t dumpSize,
             phosphor::dump::OperationStatus status, std::string originatorId,
             phosphor::dump::originatorTypes originatorType,
             phosphor::dump::Manager& parent, uint64_t eid) :
    Entry(bus, objPath, dumpId, timeStamp, dumpSize, status, originatorId,
          originatorType, SystemImpact::Disruptive, std::string(), parent, eid)
{}

Entry::Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
             uint64_t timeStamp, uint64_t dumpSize,
             phosphor::dump::OperationStatus status, std::string originatorId,
             phosphor::dump::originatorTypes originatorType,
             SystemImpact sysImpact, std::string usrChallenge,
             phosphor::dump::Manager& parent, uint64_t eid) :
    phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, dumpSize,
                          std::filesystem::path(), status, originatorId,
                          originatorType, parent),
    openpower::dump::Entry(bus, objPath, dumpId, timeStamp, dumpSize,
                           std::filesystem::path(), status, originatorId,
                           originatorType, parent),
    SystemIntf(bus, objPath.c_str(), SystemIntf::action::defer_emit)
{
    sourceDumpId(INVALID_SOURCE_ID);
    pelid(static_cast<uint32_t>(eid));
    userChallenge(usrChallenge);
    systemImpact(sysImpact);
    this->SystemIntf::emit_object_added();
}

Entry::Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
             phosphor::dump::Manager& parent, uint64_t eid) :
    phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0,
                          std::filesystem::path(),
                          phosphor::dump::OperationStatus::InProgress, "",
                          phosphor::dump::originatorTypes::Internal, parent),
    openpower::dump::Entry(bus, objPath, dumpId, parent),
    SystemIntf(bus, objPath.c_str(), SystemIntf::action::defer_emit)
{
    sourceDumpId(INVALID_SOURCE_ID);
    pelid(static_cast<uint32_t>(eid));
}

} // namespace system

} // namespace openpower::dump
