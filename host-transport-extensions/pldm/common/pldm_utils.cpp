// SPDX-License-Identifier: Apache-2.0

#include "pldm_utils.hpp"

#include "dump_utils.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

namespace phosphor
{
namespace dump
{
namespace pldm
{

using namespace phosphor::logging;
using NotAllowed = sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;

int openPLDM()
{
    auto fd = pldm_open();
    if (fd < 0)
    {
        auto e = errno;
        log<level::ERR>(
            fmt::format("pldm_open failed, errno({}), FD({})", e, fd).c_str());
        elog<NotAllowed>(Reason("Required host dump action via pldm is not "
                                "allowed due to pldm_open failed"));
    }
    return fd;
}

} // namespace pldm
} // namespace dump
} // namespace phosphor
