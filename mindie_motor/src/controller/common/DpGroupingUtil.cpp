/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <numeric>
#include <algorithm>
#include "ControllerConfig.h"
#include "Logger.h"
#include "ControllerConstant.h"
#include "DpGroupingUtil.h"

namespace MINDIE::MS {

constexpr uint64_t DP_GROUP_NUM = 10000;

// Helper function to calculate device configuration
static bool CalculateDeviceConfiguration(DIGSInstanceRole role,
    size_t& deviceCount, int& curTpSize, int& curCpSize)
{
    if (role == DIGSInstanceRole::PREFILL_INSTANCE) {
        curTpSize = ControllerConfig::GetInstance()->GetPTpSize();
        curCpSize = ControllerConfig::GetInstance()->GetPCpSize();
        deviceCount = static_cast<size_t>(curTpSize * curCpSize * ControllerConfig::GetInstance()->GetPDpSize());
    } else if (role == DIGSInstanceRole::DECODE_INSTANCE) {
        curTpSize = ControllerConfig::GetInstance()->GetDTpSize();
        curCpSize = ControllerConfig::GetInstance()->GetDCpSize();
        deviceCount = static_cast<size_t>(curTpSize * curCpSize * ControllerConfig::GetInstance()->GetDDpSize());
    } else {
        return false;
    }
    return true;
}

// Helper function to validate device count
static bool ValidateDeviceCount(const NodeInfo& nodeInfo, size_t expectedDeviceCount, uint64_t nodeId)
{
    auto totalCount = std::accumulate(nodeInfo.serverInfoList.begin(), nodeInfo.serverInfoList.end(),
        0, [](size_t sum, const ServerInfo& server) { return sum + server.deviceInfos.size(); });
    if (!nodeInfo.isDistribute && static_cast<size_t>(totalCount) != expectedDeviceCount) {
        LOG_E("[%s] [DpGroupingUtil] AllocateDpGroup: instances id %lu's device count %lu is not equal to the"
            " product of tp size, dp size and cp size %lu.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::NODE_SCHEDULER).c_str(),
            nodeId, totalCount, expectedDeviceCount);
        return false;
    }
    return true;
}

// Determine whether cross-server grouping is needed
static bool ShouldUseCrossServerGrouping(const NodeInfo& nodeInfo, int curTpSize, int curCpSize)
{
    if (nodeInfo.serverInfoList.empty()) {
        return false;
    }
    
    size_t singleServerDeviceCount = nodeInfo.serverInfoList[0].deviceInfos.size();
    return singleServerDeviceCount < static_cast<size_t>(curTpSize * curCpSize);
}

// Single server grouping
static void ProcessSingleServerGrouping(NodeInfo& nodeInfo, uint64_t nodeId,
    int curTpSize, int curCpSize, size_t& dpIdx)
{
    for (size_t j = 0; j < nodeInfo.serverInfoList.size(); ++j) {
        auto devices = nodeInfo.serverInfoList[j].deviceInfos;
        for (size_t k = 0; k < devices.size() / (curTpSize * curCpSize); ++k) {
            auto dpId = nodeId * DP_GROUP_NUM + dpIdx;
            nodeInfo.serverInfoList[j].dpGroupInfos[dpId] =
                std::vector<DeviceInfo>(devices.begin() + k * curTpSize * curCpSize,
                devices.begin() + k * curTpSize * curCpSize + curTpSize * curCpSize);
            dpIdx += 1;
        }
    }
}

// Validate cross-server grouping parameters
static bool ValidateCrossServerGroupingParams(size_t requiredDevicesPerGroup, size_t devicesPerServer)
{
    if (requiredDevicesPerGroup == 0 || devicesPerServer == 0) {
        LOG_E("[%s] [DpGroupingUtil] AllocateDpGroup: required devices per DP group %zu or "
              "devices per server %zu is 0.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::NODE_SCHEDULER).c_str(),
              requiredDevicesPerGroup, devicesPerServer);
        return false;
    }
    if (requiredDevicesPerGroup % devicesPerServer != 0) {
        LOG_E("[%s] [DpGroupingUtil] AllocateDpGroup: required devices per DP group %zu is not "
              "divisible by devices per server %zu.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::NODE_SCHEDULER).c_str(),
              requiredDevicesPerGroup, devicesPerServer);
        return false;
    }
    return true;
}

// Cross-server grouping
static bool ProcessCrossServerGrouping(NodeInfo& nodeInfo, uint64_t nodeId,
    int curTpSize, int curCpSize, size_t& dpIdx)
{
    size_t requiredDevicesPerGroup = static_cast<size_t>(curTpSize * curCpSize);
    size_t devicesPerServer = nodeInfo.serverInfoList[0].deviceInfos.size();
    if (!ValidateCrossServerGroupingParams(requiredDevicesPerGroup, devicesPerServer)) {
        return false;
    }
    if (requiredDevicesPerGroup == 0 || devicesPerServer == 0 || nodeInfo.serverInfoList.size() == 0) {
        LOG_E("[%s] [DpGroupingUtil] AllocateDpGroup: required devices per DP group %zu or "
              "devices per server %zu or server info list size %zu is 0.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::NODE_SCHEDULER).c_str(),
              requiredDevicesPerGroup, devicesPerServer, nodeInfo.serverInfoList.size());
        return false;
    }
    size_t serversPerGroup = requiredDevicesPerGroup / devicesPerServer;
    size_t possibleGroups = nodeInfo.serverInfoList.size() / serversPerGroup;
    
    for (size_t groupIdx = 0; groupIdx < possibleGroups; ++groupIdx) {
        auto dpId = nodeId * DP_GROUP_NUM + dpIdx;
        
        for (size_t serverIdx = 0; serverIdx < serversPerGroup; ++serverIdx) {
            size_t targetServerIndex = groupIdx * serversPerGroup + serverIdx;
            auto& devices = nodeInfo.serverInfoList[targetServerIndex].deviceInfos;
            nodeInfo.serverInfoList[targetServerIndex].dpGroupInfos[dpId] = devices;
        }
        
        dpIdx += 1;
    }
    
    LOG_I("[DpGroupingUtil] Node %lu (role: %c) cross-server grouping: created %zu DP groups, "
          "each group uses %zu servers, total servers: %zu",
          nodeId, nodeInfo.instanceInfo.staticInfo.role, possibleGroups,
          serversPerGroup, nodeInfo.serverInfoList.size());
    
    return true;
}

int32_t DpGroupingUtil::ProcessSingleNodeDpGrouping(std::unique_ptr<NodeInfo>& serverNode)
{
    auto nodeId = serverNode->instanceInfo.staticInfo.id;
    size_t dpIdx = 0;
    size_t deviceCount = 8;
    int curTpSize = 1;
    int curCpSize = 1;
    
    if (!CalculateDeviceConfiguration(serverNode->instanceInfo.staticInfo.role,
        deviceCount, curTpSize, curCpSize)) {
        LOG_E("[%s] [DpGroupingUtil] AllocateDpGroup: instances id %lu's role is not decided yet.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::NODE_SCHEDULER).c_str(), nodeId);
        return -1;
    }
    
    if (!ValidateDeviceCount(*serverNode, deviceCount, nodeId)) {
        return -1;
    }
    
    if (ShouldUseCrossServerGrouping(*serverNode, curTpSize, curCpSize)) {
        // Cross-server grouping
        if (!ProcessCrossServerGrouping(*serverNode, nodeId, curTpSize, curCpSize, dpIdx)) {
            return -1;
        }
    } else {
        // Single server grouping
        ProcessSingleServerGrouping(*serverNode, nodeId, curTpSize, curCpSize, dpIdx);
    }
    
    return 0;
}

} // namespace MINDIE::MS
