#include "resource_dump_entry.hpp"

#include "dump_utils.hpp"
#include "host_transport_exts.hpp"
#include "op_dump_consts.hpp"
#include "resource_dump_serialize.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace openpower
{
namespace dump
{
namespace resource
{
// TODO #ibm-openbmc/issues/2859
// Revisit host transport impelementation
// This value is used to identify the dump in the transport layer to host,
constexpr auto TRANSPORT_DUMP_TYPE_IDENTIFIER = 9;
using namespace phosphor::logging;

void Entry::initiateOffload(std::string uri)
{
    using NotAllowed =
        sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
    using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;
    log<level::INFO>(
        fmt::format(
            "Resource dump offload request id({}) uri({}) source dumpid({})",
            id, uri, sourceDumpId())
            .c_str());

    if (!phosphor::dump::isHostRunning())
    {
        elog<NotAllowed>(
            Reason("This dump can be offloaded only when the host is up"));
        return;
    }
    phosphor::dump::Entry::initiateOffload(uri);
    phosphor::dump::host::requestOffload(sourceDumpId());
#ifdef LOG_PEL_ON_DUMP_ACTIONS
    auto bus = sdbusplus::bus::new_default();
    auto path = std::filesystem::path(RESOURCE_DUMP_SERIAL_PATH) /
                std::to_string(id);
    // Log PEL for dump offload
    phosphor::dump::createPEL(
        bus, path, "Resource Dump", id,
        "xyz.openbmc_project.Logging.Entry.Level.Informational",
        "xyz.openbmc_project.Dump.Error.Offload");
#endif
}

void Entry::update(uint64_t timeStamp, uint64_t dumpSize, uint32_t sourceId)
{
    sourceDumpId(sourceId);
    elapsed(timeStamp);
    size(dumpSize);
    // TODO: Handled dump failure case with
    // #bm-openbmc/2808
    status(OperationStatus::Completed);
    completedTime(timeStamp);

    // serialize as dump is successfully completed
    serialize(*this);
}

void Entry::delete_()
{
    auto srcDumpID = sourceDumpId();
    auto dumpId = id;
    auto dumpPathOffLoadUri = offloadUri();

    if ((!offloadUri().empty()) && (phosphor::dump::isHostRunning()))
    {
        log<level::ERR>(fmt::format("Dump offload in progress, cannot delete "
                                    "dump, id({}) srcdumpid({})",
                                    dumpId, srcDumpID)
                            .c_str());
        elog<sdbusplus::xyz::openbmc_project::Common::Error::Unavailable>();
    }

    log<level::INFO>(fmt::format("Resource dump delete id({}) srcdumpid({})",
                                 dumpId, srcDumpID)
                         .c_str());
    auto path = std::filesystem::path(RESOURCE_DUMP_SERIAL_PATH) /
                std::to_string(dumpId);

    // Remove resource dump when host is up by using source dump id
    // which is present in resource dump entry dbus object as a property.
    if ((phosphor::dump::isHostRunning()) && (srcDumpID != INVALID_SOURCE_ID))
    {
        try
        {
            phosphor::dump::host::requestDelete(srcDumpID,
                                                TRANSPORT_DUMP_TYPE_IDENTIFIER);
        }
        catch (const std::exception& e)
        {
            log<level::ERR>(fmt::format("Error deleting dump from host id({}) "
                                        "host id({}) error({})",
                                        dumpId, srcDumpID, e.what())
                                .c_str());
            elog<sdbusplus::xyz::openbmc_project::Common::Error::Unavailable>();
        }
    }

    // Remove Dump entry D-bus object
    phosphor::dump::Entry::delete_();
    try
    {
        std::filesystem::remove_all(path);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        // Log Error message and continue
        log<level::ERR>(
            fmt::format("Failed to delete dump file({}), errormsg({})",
                        path.string(), e.what())
                .c_str());
    }

#ifdef LOG_PEL_ON_DUMP_ACTIONS
    // Log PEL for dump
    auto dBus = sdbusplus::bus::new_default();
    phosphor::dump::createPEL(
        dBus, path, "Resource Dump", dumpId,
        "xyz.openbmc_project.Logging.Entry.Level.Informational",
        "xyz.openbmc_project.Dump.Error.Invalidate");
#endif
}
} // namespace resource
} // namespace dump
} // namespace openpower
