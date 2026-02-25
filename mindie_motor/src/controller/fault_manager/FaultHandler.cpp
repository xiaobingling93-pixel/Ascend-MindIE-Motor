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
#include <algorithm>
#include <cstdint>
#include "FaultManager.h"
#include "NPURecoveryManager.h"
#include "Logger.h"
#include "FaultHandler.h"
namespace MINDIE::MS {
static constexpr uint64_t INVALID_ID = UINT64_MAX;
int32_t SubHealthyHardwareFaultHandler(std::shared_ptr<NodeStatus> nodeStatus, uint64_t id)
{
    if (nodeStatus->GetNode(id) == nullptr) {
        LOG_E("[FaultHandler] Subhealthy state handler: node %lu is not found in current cluster.", id);
        return -1;
    }
    return 0;
}

int32_t UnhealthyHardwareFaultHandler(std::shared_ptr<NodeStatus> nodeStatus, uint64_t id)
{
    LOG_I("[FaultHandler] Unhealthy state handler: starting a scale-in strategy for node %lu.", id);
    // 获取节点所属实例ID并退出灵衢恢复流程
    auto node = nodeStatus->GetNode(id);
    if (node != nullptr) {
        uint64_t instanceId = INVALID_ID;
        
        // 确定节点所属实例
        if (!node->dpGroupPeers.empty()) {
            instanceId = *std::min_element(node->dpGroupPeers.begin(), node->dpGroupPeers.end());
        } else {
            instanceId = id;
        }
        
        // 退出该实例的灵衢恢复流程
        if (instanceId != INVALID_ID) {
            NPURecoveryManager::GetInstance()->AbortInstanceNPURecovery(instanceId);
        }
    }
    auto strategy = FaultManager::GetInstance()->GenerateStrategy();
    FaultManager::GetInstance()->StartTimerWithStrategy(strategy);
    if (nodeStatus->GetNode(id) == nullptr) {
        LOG_W("[FaultHandler] Unhealthy state handler: node %lu is not found in current cluster.", id);
    }
    return 0;
}

} // namespace MINDIE::MS