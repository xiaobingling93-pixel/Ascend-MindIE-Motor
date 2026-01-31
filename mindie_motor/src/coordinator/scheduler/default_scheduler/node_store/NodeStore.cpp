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
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "Logger.h"
#include "NodeStore.h"

namespace MINDIE::MS {

// 增加某些节点可支持调度
int32_t NodeStore::AddNodes(const std::vector<MINDIE::MS::DIGSInstanceStaticInfo> &instances)
{
    std::lock_guard<std::mutex> lck(mtx);
    for (auto ins : instances) {
        // 检查节点ID是否已存在
        if (clusterStaticNodeMap.find(ins.id) != clusterStaticNodeMap.end()) {
            // 记录警告日志，提示重复添加
            LOG_W("[DefaultScheduler] Node id: %d already exists, skip adding.", ins.id);
            continue; // 跳过重复节点
        }
        clusterStaticNodeMap[ins.id] = ins;
        LOG_I("[DefaultScheduler] Add one node id: %d, role: %c.", ins.id, ins.role);
        nodeIndexList.push_back(ins.id);
        if (ins.role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
            pNodeList.push_back(ins.id);
        }
    }
    return 0;
}

// 删除某些节点
int32_t NodeStore::RemoveNodes(const std::vector<uint64_t> &instances)
{
    std::lock_guard<std::mutex> lck(mtx);
    for (auto insID : instances) {
        clusterStaticNodeMap.erase(insID);
        LOG_I("[DefaultScheduler] Remove one node id %d.", insID);
        // 从nodeList中移除
        for (auto iter = nodeIndexList.begin(); iter != nodeIndexList.end();) {
            if (*iter == insID) {
                iter = nodeIndexList.erase(iter);
            } else {
                iter++;
            }
        }

        for (auto iter = pNodeList.begin(); iter != pNodeList.end();) {
            if (*iter == insID) {
                iter = pNodeList.erase(iter);
            } else {
                iter++;
            }
        }
    }
    return 0;
}

// 内部调用，与DynamicUpdate共锁
void NodeStore::UpdateD2PRelations(const MINDIE::MS::DIGSInstanceDynamicInfo &ins)
{
    if (clusterStaticNodeMap[ins.id].role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
        if (d2PminiGroupNodes.find(ins.id) != d2PminiGroupNodes.end()) {
            d2PminiGroupNodes[ins.id].clear();
        }
        for (auto p : ins.peers) {
            d2PminiGroupNodes[ins.id].push_back(p);
        }
    }
}

// 内部调用，与DynamicUpdate共锁
void NodeStore::UpdateP2DRelations()
{
    p2DminiGroupNodes.clear();
    // 根据全量的d2PminiGroupNodes，每次重新生成全新的p2DminiGroupNodes
    for (auto &iter : std::as_const(d2PminiGroupNodes)) {
        auto dNodeID = iter.first;
        auto peers = iter.second;
        for (auto p : peers) {
            p2DminiGroupNodes[p].push_back(dNodeID);
        }
    }
}

// 更新节点动态状态
int32_t NodeStore::DynamicUpdate(const std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> &instances)
{
    std::lock_guard<std::mutex> lck(mtx);
    for (auto ins : instances) {
        clusterDynamicMap[ins.id] = ins;
        UpdateD2PRelations(ins);
    }
    UpdateP2DRelations();
    return 0;
}

bool NodeStore::GetNodeById(uint64_t id, MINDIE::MS::DIGSInstanceStaticInfo &staticInfo,
    MINDIE::MS::DIGSInstanceDynamicInfo &dynamicInfo)
{
    std::lock_guard<std::mutex> lck(mtx);
    auto iter = clusterStaticNodeMap.find(id);
    if (iter == clusterStaticNodeMap.end()) {
        // 查询失败
        return false;
    }
    staticInfo = clusterStaticNodeMap[id];
    dynamicInfo = clusterDynamicMap[id];
    return true;
}

std::vector<uint64_t> NodeStore::GetNodeList()
{
    std::lock_guard<std::mutex> lck(mtx);
    return nodeIndexList;
}

std::vector<uint64_t> NodeStore::GetPNodeList()
{
    std::lock_guard<std::mutex> lck(mtx);
    return pNodeList;
}

std::map<uint64_t, std::vector<uint64_t>> NodeStore::GetP2DNodeMap()
{
    std::lock_guard<std::mutex> lck(mtx);
    return p2DminiGroupNodes;
}


bool NodeStore::GetNodeByIndex(uint64_t nodeIndex, uint64_t &nodeId)
{
    std::lock_guard<std::mutex> lck(mtx);
    if (nodeIndex >= nodeIndexList.size()) {
        return false;
    }

    nodeId = nodeIndexList[nodeIndex];
    return true;
}

bool NodeStore::IsNodeAvailable(uint64_t nodeID)
{
    std::lock_guard<std::mutex> lck(mtx);
    auto iter = clusterStaticNodeMap.find(nodeID);
    if (iter == clusterStaticNodeMap.end()) {
        // 查询失败
        return false;
    }

    return true;
}

}