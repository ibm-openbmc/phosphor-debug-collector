#include "bmc_dump_entry.hpp"
#include "dump_manager.hpp"
#include "dump_offload.hpp"
#include "dump_utils.hpp"

#include <fmt/core.h>

#include <phosphor-logging/log.hpp>

namespace phosphor
{
namespace dump
{
namespace bmc_stored
{
using namespace phosphor::logging;

void Entry::delete_()
{
    // Log PEL for dump delete/offload
    {
        auto dBus = sdbusplus::bus::new_default();
        phosphor::dump::createPEL(
            dBus, path(), "BMC Dump", id,
            "xyz.openbmc_project.Logging.Entry.Level.Informational",
            "xyz.openbmc_project.Dump.Error.Invalidate");
    }

    // Delete Dump file from Permanent location
    try
    {
        std::filesystem::remove_all(
            std::filesystem::path(path()).parent_path());
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        // Log Error message and continue
        log<level::ERR>(
            fmt::format("Failed to delete dump file({}), errormsg({})", path(),
                        e.what())
                .c_str());
    }

    // Remove Dump entry D-bus object
    phosphor::dump::Entry::delete_();
}

void Entry::initiateOffload(std::string uri)
{
    phosphor::dump::offload::requestOffload(path(), id, uri);
    offloaded(true);
}

} // namespace bmc_stored
} // namespace dump
} // namespace phosphor
