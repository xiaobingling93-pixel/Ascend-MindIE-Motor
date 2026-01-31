/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "Logger.h"
#include "RoundRobinExecutor.h"

namespace MINDIE::MS {
bool RoundRobinExecutor::IsNodeResourceAvail(uint64_t nodeId)
{
    MINDIE::MS::DIGSInstanceStaticInfo staticInfo;
    MINDIE::MS::DIGSInstanceDynamicInfo dynamicInfo;
    auto ret = nodeStore->GetNodeById(nodeId, staticInfo, dynamicInfo);
    if (!ret) {
        LOG_E("[%s] [RoundRobinExecutor] Node with ID %lu not exsit.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::ROUNDROBIN_EXECUTOR).c_str(),
            nodeId);
        return false;
    }

    if (staticInfo.totalSlotsNum < 1 || staticInfo.totalBlockNum < 1) {
        return false;
    }

    if (dynamicInfo.availSlotsNum == 0) {
        return false;
    }

    if (dynamicInfo.availBlockNum == 0) {
        return false;
    }

    return true;
}

int32_t RoundRobinExecutor::Execute(DeployMode deployMode, std::string requestBody, std::vector<uint64_t> &pickedNodes,
    __attribute__((unused)) int type)
{
    if (deployMode == DeployMode::SINGLE_NODE) {
        pickedNodes.resize(1);
        return SingleNodeExecute(requestBody, pickedNodes[0]);
    } else if (deployMode == DeployMode::PD_SEPARATE) {
        pickedNodes.resize(2);  // pd分离至少2个节点
        return PDNodeExecute(requestBody, pickedNodes);
    }
    return -1;
}

int32_t RoundRobinExecutor::SingleNodeExecute(std::string requestBody, uint64_t &pickedNode)
{
    if (requestBody.size() == 0) {
        return 0;
    }
    auto nodeIndexList = nodeStore->GetNodeList();
    if (nodeIndexList.empty()) {
        LOG_E("[%s] [RoundRobinExceutor] Invalid node list.",
            GetErrorCode(ErrorType::UNAVAILABLE, CoordinatorFeature::ROUNDROBIN_EXECUTOR).c_str());
        return -1;
    }

    singleNodeIndex++;
    if (singleNodeIndex >= nodeIndexList.size()) {
        singleNodeIndex = 0;
    }
    uint64_t oldNodeIndex = singleNodeIndex; // 轮询的起点
    do {
        pickedNode = nodeIndexList[singleNodeIndex];
        if (IsNodeResourceAvail(pickedNode)) {
            LOG_D("[RoundRobinExecutor] Picked node: %d for single node execution.", pickedNode);
            return 0; // 资源充分则直接选中
        }
        singleNodeIndex++;
        if (singleNodeIndex >= nodeIndexList.size()) {
            singleNodeIndex = 0;
        }
    } while (singleNodeIndex != oldNodeIndex);
    return -1; // 所有节点资源都不达标
}

bool RoundRobinExecutor::PickDNodeByP(uint64_t pNode, uint64_t &dNode)
{
    auto p2DMap = nodeStore->GetP2DNodeMap();
    if (p2DMap.find(pNode) == p2DMap.end()) {
        LOG_E("[%s] [RoundRobinExecutor] pNode with ID %lu not exist.",
              GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::ROUNDROBIN_EXECUTOR).c_str(),
              pNode);
        return false; // pNode不存在
    }
    auto dList = p2DMap[pNode];

    LOG_D("Prefill-Decode map size is %zu.", p2DMap.size());
    for (auto &iter : std::as_const(p2DMap)) {
        LOG_D("Prefill-Decode map 's first iterator is %lu, size of second iterator is %zu",
            iter.first, iter.second.size());
    }
    // 进一步选中剩余的D
    if (dList.empty()) {
        LOG_E("[%s] [RoundRobinExecutor] Empty decode list.",
            GetErrorCode(ErrorType::UNAVAILABLE, CoordinatorFeature::ROUNDROBIN_EXECUTOR).c_str());
        return false; // 无可用的D
    }

    // 待挑选的D节点Index
    uint64_t dIndex = 0;
    auto iter = p2DIndexMap.find(pNode);
    if (iter == p2DIndexMap.end()) {
        // 该P是首次被调用, 选择首节点
        dIndex = 0;
    } else {
        dIndex = iter->second; // 上次选中的节点
        dIndex++;
        if (dIndex >= dList.size()) {
            dIndex = 0;
        }
    }

    // 选好了D节点
    dNode = dList[dIndex];
    if (!IsNodeResourceAvail(dNode)) {
        LOG_E("[%s] [RoundRobinExecutor] Node with ID %lu has insufficient resource.",
              GetErrorCode(ErrorType::RESOURCE_LIMIT, CoordinatorFeature::ROUNDROBIN_EXECUTOR).c_str(),
              dNode);
        return false;
    }
    p2DIndexMap[pNode] = dIndex; // 缓存记录
    LOG_D("[RoundRobinExecutor] PickDNodeByP: picked p: %d and d: %d", pNode, dNode);
    return true;
}

int32_t RoundRobinExecutor::PDNodeExecute(std::string requestBody, std::vector<uint64_t> &pickedNodes)
{
    if (requestBody.size() == 0) {
        return 0;
    }
    LOG_D("[RoundRobinExecutor] Execute PD node.");
    auto pNodeList = nodeStore->GetPNodeList();
    if (pNodeList.empty()) {
        LOG_E("[%s] [RoundRobinExecutor] Empty prefill node list.",
            GetErrorCode(ErrorType::UNAVAILABLE, CoordinatorFeature::ROUNDROBIN_EXECUTOR).c_str());
        return -1;
    }
    pNodeIndex++;
    if (pNodeIndex >= pNodeList.size()) {
        pNodeIndex = 0;
    }

    uint64_t oldPIndex = pNodeIndex;

    uint64_t pickedPNode;
    uint64_t pickedDNode;
    do {
        pickedPNode = pNodeList[pNodeIndex]; // 当前选择哪一个 pIndex
        if (IsNodeResourceAvail(pickedPNode)) {
            LOG_D("[RoundRobinExecutor] Picked prefill node: %lu for PD node execution.", pickedPNode);
            auto ret = PickDNodeByP(pickedPNode, pickedDNode);
            if (ret) {
                // 传出选中的P节点和D节点
                pickedNodes[0] = pickedPNode;
                pickedNodes[1] = pickedDNode;
                return 0;
            }
        }
        // 换个P继续轮询
        pNodeIndex++;
        if (pNodeIndex >= pNodeList.size()) {
            pNodeIndex = 0;
        }
    } while (oldPIndex != pNodeIndex);

    // 无可用节点
    LOG_E("[%s] [RoundRobinExecutor] No avliable nodes.",
        GetErrorCode(ErrorType::UNAVAILABLE, CoordinatorFeature::ROUNDROBIN_EXECUTOR).c_str());
    return -1;
}
}