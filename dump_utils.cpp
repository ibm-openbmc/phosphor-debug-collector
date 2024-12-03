#include "dump_utils.hpp"

#include "dump_types.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor
{
namespace dump
{

std::string getService(sdbusplus::bus_t& bus, const std::string& path,
                       const std::string& interface)
{
    constexpr auto objectMapperName = "xyz.openbmc_project.ObjectMapper";
    constexpr auto objectMapperPath = "/xyz/openbmc_project/object_mapper";

    auto method = bus.new_method_call(objectMapperName, objectMapperPath,
                                      objectMapperName, "GetObject");

    method.append(path);
    method.append(std::vector<std::string>({interface}));

    std::vector<std::pair<std::string, std::vector<std::string>>> response;

    try
    {
        auto reply = bus.call(method);
        reply.read(response);
        if (response.empty())
        {
            lg2::error(
                "Error in mapper response for getting service name, PATH: "
                "{PATH}, INTERFACE: {INTERFACE}",
                "PATH", path, "INTERFACE", interface);
            return std::string{};
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        lg2::error("Error in mapper method call, errormsg: {ERROR}, "
                   "PATH: {PATH}, INTERFACE: {INTERFACE}",
                   "ERROR", e, "PATH", path, "INTERFACE", interface);
        throw;
    }
    return response[0].first;
}

void createPEL(
    sdbusplus::bus::bus& dBus, const std::string& pelSev,
    const std::string& errIntf,
    const std::unordered_map<std::string_view, std::string_view>& userDataMap)
{
    try
    {
        constexpr auto loggerObjectPath = "/xyz/openbmc_project/logging";
        constexpr auto loggerCreateInterface =
            "xyz.openbmc_project.Logging.Create";
        constexpr auto loggerService = "xyz.openbmc_project.Logging";
        // Set up a connection to D-Bus object
        auto busMethod = dBus.new_method_call(loggerService, loggerObjectPath,
                                              loggerCreateInterface, "Create");
        busMethod.append(errIntf, pelSev, userDataMap);
        // Implies this is a call from Manager. Hence we need to make an async
        // call to avoid deadlock with Phosphor-logging.
        auto retVal =
            busMethod.call_async([&](sdbusplus::message::message&& reply) {
            if (reply.is_method_error())
            {
                log<level::ERR>("Error in calling async method to create PEL");
            }
        });
        if (!retVal)
        {
            log<level::ERR>("Return object contains null pointer");
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Error in calling creating PEL. Exception caught",
                        entry("ERROR=%s", e.what()));
    }
}
void createPELOnDumpActions(sdbusplus::bus::bus& dBus,
                            const std::string dumpFilePath,
                            const std::string dumpFileType,
                            const std::string dumpId, const std::string pelSev,
                            const std::string errIntf)
{
    constexpr auto dumpFileString = "File Name";
    constexpr auto dumpFileTypeString = "Dump Type";
    constexpr auto dumpIdString = "Dump ID";
    const std::unordered_map<std::string_view, std::string_view> userDataMap = {
        {dumpIdString, dumpId},
        {dumpFileString, dumpFilePath},
        {dumpFileTypeString, dumpFileType}};
    createPEL(dBus, pelSev, errIntf, userDataMap);
}

} // namespace dump
} // namespace phosphor
