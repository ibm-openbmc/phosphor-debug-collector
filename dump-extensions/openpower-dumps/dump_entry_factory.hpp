#pragma once

#include "dump_entry.hpp"
#include "dump_manager.hpp"
#include "dump_utils.hpp"
#include "op_dump_consts.hpp"
#include "op_dump_util.hpp"
#include "openpower_dump_entry.hpp"

#include <com/ibm/Dump/Create/common.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Dump/Create/server.hpp>

#include <filesystem>
#include <memory>

namespace openpower::dump
{

/**
 * @class DumpEntryFactory
 * @brief Factory class to create dump entries based on dump type.
 *        This class encapsulates the creation of dump entries.
 */
class DumpEntryFactory
{
  public:
    /**
     * @brief Constructs a dump entry factory.
     * @param[in] bus Reference to the D-Bus bus object.
     * @param[in] baseEntryPath Base object path for the dump entries.
     * @param[in] mgr Reference to the dump manager handling these dumps.
     */
    DumpEntryFactory(sdbusplus::bus::bus& bus, const std::string& baseEntryPath,
                     phosphor::dump::Manager& mgr) :
        bus(bus), baseEntryPath(baseEntryPath), mgr(mgr)
    {}

    /**
     * @brief Creates a dump entry based on provided parameters.
     * @param[in] id The unique identifier for the new dump entry.
     * @param[in] params Parameters defining the dump creation specifics.
     * @return A unique pointer to a newly created dump entry, or nullptr if
     * creation fails.
     */
    std::unique_ptr<phosphor::dump::Entry> createEntry(
        uint32_t id, phosphor::dump::DumpCreateParams& params);

    std::unique_ptr<phosphor::dump::Entry> createEntryWithDefaults(
        uint32_t id, const std::filesystem::path& objPath);

  private:
    /**
     * @brief Creates a system dump entry.
     * @param[in] id The unique identifier for the system dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created system dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createSystemDumpEntry(
        uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /**
     * @brief Creates a resource dump entry.
     * @param[in] id The unique identifier for the system dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created resource dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createResourceDumpEntry(
        uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
        bool createSysDump, const DumpParameters& dumpParams);

    /**
     * @brief Creates a hostboot dump entry.
     * @param[in] id The unique identifier for the system dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created hostboot dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createHostbootDumpEntry(
        uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /**
     * @brief Creates a hardware dump entry.
     * @param[in] id The unique identifier for the system dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created hardware dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createHardwareDumpEntry(
        uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /**
     * @brief Creates a SBE dump entry.
     * @param[in] id The unique identifier for the system dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created SBE dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createSBEDumpEntry(
        uint32_t id, std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /**
     * @brief Retrieves the dump ID prefix based on the dump type.
     * @param[in] dumpType Type of the dump (system, resource, etc.).
     * @return The prefix to be used for the dump ID.
     */
    uint32_t getDumpIdPrefix(OpDumpTypes dumpType)
    {
        switch (dumpType)
        {
            case OpDumpTypes::Hardware:
                return HARDWARE_DUMP_ID_PREFIX;
            case OpDumpTypes::Hostboot:
                return HOSTBOOT_DUMP_ID_PREFIX;
            case OpDumpTypes::SBE:
                return SBE_DUMP_ID_PREFIX;
            case OpDumpTypes::MemoryBufferSBE:
                return MSBE_DUMP_ID_PREFIX;
            case OpDumpTypes::System:
                return SYSTEM_DUMP_ID_PREFIX;
            case OpDumpTypes::Resource:
                return RESOURCE_DUMP_ID_PREFIX;
            default:
                lg2::error("unsupported {TYPE}", "TYPE", dumpType);
        }
        return 0xFF;
    }

    /**
     * @brief Determines the dump type based on the ID prefix.
     *
     * @param id The dump ID from which to extract the dump type.
     * @return The dump type as an enumeration value of OpDumpTypes.
     *         If the prefix does not match any known type,
     *         it returns OpDumpTypes::System as a default.
     */
    OpDumpTypes getDumpTypeFromId(uint32_t id)
    {
        using namespace phosphor::logging;
        // Extract the highest byte as the prefix
        uint32_t prefix = id & DUMP_ID_PREFIX_MASK;

        switch (prefix)
        {
            case HARDWARE_DUMP_ID_PREFIX:
                return OpDumpTypes::Hardware;
            case HOSTBOOT_DUMP_ID_PREFIX:
                return OpDumpTypes::Hostboot;
            case SBE_DUMP_ID_PREFIX:
                return OpDumpTypes::SBE;
            case MSBE_DUMP_ID_PREFIX:
                return OpDumpTypes::MemoryBufferSBE;
            case SYSTEM_DUMP_ID_PREFIX:
                return OpDumpTypes::System;
            case RESOURCE_DUMP_ID_PREFIX:
                return OpDumpTypes::Resource;
            default:
                lg2::error("Unknown dump type");
        }

        return OpDumpTypes::System;
    }

    /**
     * @brief Converts a string to uppercase.
     *
     * @param str The string to convert to uppercase.
     * @return A new string where all characters are converted to uppercase.
     */
    inline static std::string toUpper(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    /** @brief sdbusplus DBus bus connection. */
    sdbusplus::bus_t& bus;

    /** @brief Base D-Bus path for dump entries. */
    const std::string& baseEntryPath;

    /** @brief Reference to the managing object for dumps.*/
    phosphor::dump::Manager& mgr;
};

} // namespace openpower::dump
