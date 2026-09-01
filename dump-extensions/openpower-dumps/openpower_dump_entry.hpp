#pragma once

#include "dump_entry.hpp"
#include "dump_utils.hpp"

#include <com/ibm/Dump/Create/common.hpp>
#include <com/ibm/Dump/Create/server.hpp>
#include <com/ibm/Dump/Entry/Hardware/server.hpp>
#include <com/ibm/Dump/Entry/Hostboot/server.hpp>
#include <com/ibm/Dump/Entry/SBE/server.hpp>
#include <org/open_power/Logging/PEL/PELID/server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Dump/Entry/System/server.hpp>

#include <concepts>
#include <filesystem>

namespace openpower::dump
{

using originatorTypes = sdbusplus::xyz::openbmc_project::Common::server::
    OriginatedBy::OriginatorTypes;

/** @class Entry
 *  @brief Represents a generic dump entry for the OpenPOWER dumps.
 */
class Entry : public virtual phosphor::dump::Entry
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    virtual ~Entry() = default;

    /** @brief Constructor for the generic Dump Entry Object
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] timeStamp - Dump creation timestamp since the epoch.
     *  @param[in] fileSize - Size of the dump file in bytes.
     *  @param[in] file - Path to the dump file.
     *  @param[in] status - Current status of the dump.
     *  @param[in] originatorId - Identifier of the originator of the dump.
     *  @param[in] originatorType - Type of the originator.
     *  @param[in] eid - Error log identifier associated with the dump.
     *  @param[in] failingUnit - Identifier of the failing unit associated with
     * the dump.
     *  @param[in] parent - Reference to the managing dump manager.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          const std::filesystem::path& file,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              file, status, originatorId, originatorType,
                              parent)
    {}

    /** @brief Constructor for creating a dump entry with default values
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] parent - Reference to the managing dump manager.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0, "",
                              phosphor::dump::OperationStatus::InProgress, "",
                              originatorTypes::Internal, parent)
    {}

    /** @brief Delete the dump and D-Bus object
     */
    void delete_() override;

    /** @brief Method to initiate the offload of dump
     *  @param[in] uri - URI to offload dump
     */
    void initiateOffload(std::string) override;

    /** @brief Method to update an existing dump entry, once the dump creation
     *  is completed this function will be used to update the entry which got
     *  created during the dump request.
     *  @param[in] timeStamp - Dump creation timestamp
     *  @param[in] fileSize - Dump file size in bytes.
     *  @param[in] file - Name of dump file.
     */
    void update(uint64_t timeStamp, uint64_t fileSize,
                const std::filesystem::path& filePath)
    {
        elapsed(timeStamp);
        size(fileSize);
        // TODO: Handled dump failed case with #ibm-openbmc/2808
        status(OperationStatus::Completed);
        file = filePath;
        // TODO: serialization of this property will be handled with
        // #ibm-openbmc/2597
        completedTime(timeStamp);

        serialize();
    }
};

namespace system
{

using SystemIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Dump::Entry::server::System,
    sdbusplus::org::open_power::Logging::PEL::server::PELID>;

using SystemImpact =
    sdbusplus::common::xyz::openbmc_project::dump::entry::System::SystemImpact;

/** @class Entry
 *  @brief File-backed System Dump Entry implementation.
 *  @details A concrete implementation of the System dump D-Bus API that uses
 *  the common OpenPOWER entry for local file offload and deletion.
 */
class Entry : public virtual openpower::dump::Entry, public virtual SystemIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    ~Entry() = default;

    /** @brief Constructor for a disruptive System dump entry.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] timeStamp - Dump creation timestamp since the epoch.
     *  @param[in] dumpSize - Dump size in bytes.
     *  @param[in] status - Current status of the dump.
     *  @param[in] originatorId - Identifier of the dump originator.
     *  @param[in] originatorType - Type of the dump originator.
     *  @param[in] parent - Reference to the managing dump manager.
     *  @param[in] eid - Error log identifier associated with the dump.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t dumpSize,
          phosphor::dump::OperationStatus status, std::string originatorId,
          phosphor::dump::originatorTypes originatorType,
          phosphor::dump::Manager& parent, uint64_t eid = 0);

    /** @brief Constructor for a disruptive or non-disruptive System dump.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] timeStamp - Dump creation timestamp since the epoch.
     *  @param[in] dumpSize - Dump size in bytes.
     *  @param[in] status - Current status of the dump.
     *  @param[in] originatorId - Identifier of the dump originator.
     *  @param[in] originatorType - Type of the dump originator.
     *  @param[in] sysImpact - Whether the dump is disruptive.
     *  @param[in] usrChallenge - User challenge for authentication.
     *  @param[in] parent - Reference to the managing dump manager.
     *  @param[in] eid - Error log identifier associated with the dump.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t dumpSize,
          phosphor::dump::OperationStatus status, std::string originatorId,
          phosphor::dump::originatorTypes originatorType,
          SystemImpact sysImpact, std::string usrChallenge,
          phosphor::dump::Manager& parent, uint64_t eid = 0);

    /** @brief Constructor for restoring a System dump entry.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] parent - Reference to the managing dump manager.
     *  @param[in] eid - Error log identifier associated with the dump.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent, uint64_t eid = 0);
};

} // namespace system

namespace hostboot
{

using HostbootIntf = sdbusplus::server::object_t<
    sdbusplus::com::ibm::Dump::Entry::server::Hostboot,
    sdbusplus::org::open_power::Logging::PEL::server::PELID>;

/** @class Entry
 *  @brief Host Dump Entry implementation.
 *  @details A concrete implementation for the
 *  host dump type DBus API
 */
class Entry : public virtual openpower::dump::Entry, public virtual HostbootIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    virtual ~Entry() = default;

    /** @brief Constructor for the Hostboot Dump Entry Object
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] timeStamp - Dump creation timestamp since the epoch.
     *  @param[in] fileSize - Size of the dump file in bytes.
     *  @param[in] file - Path to the dump file.
     *  @param[in] status - Current status of the dump.
     *  @param[in] originatorId - Identifier of the originator of the dump.
     *  @param[in] originatorType - Type of the originator.
     *  @param[in] eid - Error log identifier associated with the dump.
     *                   Also used as the PEL Entry ID (truncated to uint32).
     *  @param[in] parent - Reference to the managing dump manager.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          const std::filesystem::path& file,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, uint64_t eid,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              file, status, originatorId, originatorType,
                              parent),
        openpower::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp,
                               fileSize, file, status, originatorId,
                               originatorType, parent),
        HostbootIntf(bus, objPath.c_str(), HostbootIntf::action::defer_emit)
    {
        errorLogId(eid);
        pelid(static_cast<uint32_t>(eid));
        this->openpower::dump::hostboot::HostbootIntf::emit_object_added();
    }

    /** @brief Constructor for creating a Hostboot dump entry with default
     * values
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] parent - Reference to the managing dump manager.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0, "",
                              phosphor::dump::OperationStatus::InProgress, "",
                              originatorTypes::Internal, parent),
        openpower::dump::Entry(bus, objPath.c_str(), dumpId, parent),
        HostbootIntf(bus, objPath.c_str(), HostbootIntf::action::defer_emit)
    {}
};

} // namespace hostboot

namespace hardware
{

using HardwareIntf = sdbusplus::server::object_t<
    sdbusplus::com::ibm::Dump::Entry::server::Hardware,
    sdbusplus::org::open_power::Logging::PEL::server::PELID>;

/** @class Entry
 *  @brief Hardware Dump Entry implementation.
 *  @details A concrete implementation for the hardware dump type DBus API.
 *           This class extends the general dump entry with hardware-specific
 *           attributes, such as error log ID and failing unit ID.
 */
class Entry : public virtual openpower::dump::Entry, public virtual HardwareIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    virtual ~Entry() = default;

    /** @brief Constructor for the Hardware Dump Entry Object
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] timeStamp - Dump creation timestamp since the epoch.
     *  @param[in] fileSize - Size of the dump file in bytes.
     *  @param[in] file - Path to the dump file.
     *  @param[in] status - Current status of the dump.
     *  @param[in] originatorId - Identifier of the originator of the dump.
     *  @param[in] originatorType - Type of the originator.
     *  @param[in] eid - Error log identifier associated with the dump.
     *                   Also used as the PEL Entry ID (truncated to uint32).
     *  @param[in] failingUnit - Identifier of the failing unit associated with
     * the dump.
     *  @param[in] parent - Reference to the managing dump manager.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          const std::filesystem::path& file,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, uint64_t eid, uint64_t failingUnit,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              file, status, originatorId, originatorType,
                              parent),
        openpower::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp,
                               fileSize, file, status, originatorId,
                               originatorType, parent),
        HardwareIntf(bus, objPath.c_str(), HardwareIntf::action::defer_emit)
    {
        errorLogId(eid);
        failingUnitId(failingUnit);
        pelid(static_cast<uint32_t>(eid));
        this->openpower::dump::hardware::HardwareIntf::emit_object_added();
    }

    /** @brief Constructor for creating a Hardware dump entry with default
     * values
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] parent - Reference to the managing dump manager.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0, "",
                              phosphor::dump::OperationStatus::InProgress, "",
                              originatorTypes::Internal, parent),
        openpower::dump::Entry(bus, objPath.c_str(), dumpId, parent),
        HardwareIntf(bus, objPath.c_str(), HardwareIntf::action::defer_emit)
    {}
};
} // namespace hardware

namespace sbe
{

using SBEIntf = sdbusplus::server::object_t<
    sdbusplus::com::ibm::Dump::Entry::server::SBE,
    sdbusplus::org::open_power::Logging::PEL::server::PELID>;

/** @class Entry
 *  @brief SBE Dump Entry implementation.
 *  @details A concrete implementation for the SBE dump type DBus API.
 *           This class extends the general dump entry with SBE-specific
 *           attributes, such as error log ID and failing unit ID.
 */
class Entry : public virtual openpower::dump::Entry, public virtual SBEIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    ~Entry() = default;

    /** @brief Constructor for the SBE Dump Entry Object
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] timeStamp - Dump creation timestamp since the epoch.
     *  @param[in] fileSize - Size of the dump file in bytes.
     *  @param[in] file - Path to the dump file.
     *  @param[in] status - Current status of the dump.
     *  @param[in] originatorId - Identifier of the originator of the dump.
     *  @param[in] originatorType - Type of the originator.
     *  @param[in] eid - Error log identifier associated with the dump.
     *                   Also used as the PEL Entry ID (truncated to uint32).
     *  @param[in] failingUnit - Identifier of the failing unit associated with
     *  the dump.
     *  @param[in] parent - Reference to the managing dump manager.
     *  @param[in] dumpFilesPath - Optional path to pre-collected dump files.
     *  @param[in] sbeDumpTriggerType - Optional SBE dump trigger type.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          const std::filesystem::path& file,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, uint64_t eid, uint64_t failingUnit,
          phosphor::dump::Manager& parent,
          const std::optional<std::string>& dumpFilesPath = std::nullopt,
          const std::optional<std::string>& sbeDumpTriggerType = std::nullopt) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              file, status, originatorId, originatorType,
                              parent),
        openpower::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp,
                               fileSize, file, status, originatorId,
                               originatorType, parent),
        SBEIntf(bus, objPath.c_str(), SBEIntf::action::defer_emit)
    {
        errorLogId(eid);
        failingUnitId(failingUnit);
        pelid(static_cast<uint32_t>(eid));

        // Set new SBE dump properties if provided
        if (dumpFilesPath.has_value())
        {
            this->dumpFilesPath(dumpFilesPath.value());
        }
        if (sbeDumpTriggerType.has_value())
        {
            // Convert string to enum and set
            auto triggerType = sdbusplus::com::ibm::Dump::server::Create::
                convertSBEDumpTriggerTypeFromString(sbeDumpTriggerType.value());
            this->sbeDumpTriggerType(triggerType);
        }

        this->openpower::dump::sbe::SBEIntf::emit_object_added();
    }

    /** @brief Constructor for creating an SBE dump entry with default values
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] parent - Reference to the managing dump manager.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0, "",
                              phosphor::dump::OperationStatus::InProgress, "",
                              originatorTypes::Internal, parent),
        openpower::dump::Entry(bus, objPath.c_str(), dumpId, parent),
        SBEIntf(bus, objPath.c_str(), SBEIntf::action::defer_emit)
    {}
};
} // namespace sbe

} // namespace openpower::dump
