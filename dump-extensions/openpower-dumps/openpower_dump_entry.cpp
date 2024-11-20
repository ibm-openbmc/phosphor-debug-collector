#include "openpower_dump_entry.hpp"

#include "dump_manager.hpp"
#include "dump_offload.hpp"
#include "dump_utils.hpp"

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
    auto bus = sdbusplus::bus::new_default();
    // Log PEL for dump delete
    phosphor::dump::createPELOnDumpActions(
        bus, file, "Openpower Dump", std::format("{:08x}", id),
        "xyz.openbmc_project.Logging.Entry.Level.Informational",
        "xyz.openbmc_project.Dump.Error.Invalidate");
    // Remove Dump entry D-bus object
    phosphor::dump::Entry::delete_();
}

void Entry::initiateOffload(std::string uri)
{
    phosphor::dump::offload::requestOffload(file, id, uri);
    offloaded(true);
}

} // namespace openpower::dump
