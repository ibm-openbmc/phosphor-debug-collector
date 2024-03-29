#pragma once

#include "dump_entry.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>

namespace phosphor
{
namespace dump
{

using DumpCreateParams =
    std::map<std::string, std::variant<std::string, uint64_t>>;
using Iface = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll>;

/** @class Manager
 *  @brief Dump  manager base class.
 *  @details A concrete implementation for the
 *  xyz::openbmc_project::Collection::server::DeleteAll.
 */
class Manager : public Iface
{
    friend class Entry;

  public:
    Manager() = delete;
    Manager(const Manager&) = default;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;
    virtual ~Manager() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] event - Dump manager sd_event loop.
     *  @param[in] path - Path to attach at.
     *  @param[in] baseEntryPath - Base path of the dump entry.
     */
    Manager(sdbusplus::bus::bus& bus, const char* path,
            const std::string& baseEntryPath) :
        Iface(bus, path, Iface::action::defer_emit),
        bus(bus), lastEntryId(0), baseEntryPath(baseEntryPath)
    {}

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     *  @param[in] baseEntryPath - Base path of the dump entry.
     *  @param[in] startingId - Starting id of the dump.
     */
    Manager(sdbusplus::bus::bus& bus, const char* path,
            const std::string& baseEntryPath, uint32_t startingId) :
        Iface(bus, path),
        bus(bus), lastEntryId(startingId), baseEntryPath(baseEntryPath)
    {}

    /** @brief Construct dump d-bus objects from their persisted
     *        representations.
     */
    virtual void restore() = 0;

    /** @brief Perform any post restore operations after claiming
     *  the bus name. Any new D-Bus dump objects created will be
     *  notified to the subscribers.
     */
    virtual void checkAndInitialize() = 0;

  protected:
    /** @brief Erase specified entry d-bus object
     *
     * @param[in] entryId - unique identifier of the entry
     */
    void erase(uint32_t entryId);

    /** @brief  Erase all BMC dump entries and  Delete all Dump files
     * from Permanent location
     *
     */
    void deleteAll() override;

    /** @brief sdbusplus DBus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief Dump Entry dbus objects map based on entry id */
    std::map<uint32_t, std::unique_ptr<Entry>> entries;

    /** @brief Id of the last Dump entry */
    uint32_t lastEntryId;

    /** @bried base object path for the entry object */
    std::string baseEntryPath;
};

} // namespace dump
} // namespace phosphor
