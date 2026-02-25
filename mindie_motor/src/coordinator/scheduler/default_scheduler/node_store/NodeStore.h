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
#ifndef MINDIE_NODE_STORE_H
#define MINDIE_NODE_STORE_H

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <set>
#include "digs_instance.h"

namespace MINDIE::MS {

class NodeStore {
public:
     // 增加某些节点可支持调度
    int32_t AddNodes(const std::vector<MINDIE::MS::DIGSInstanceStaticInfo> &instances);

    // 更新节点动态状态
    int32_t DynamicUpdate(const std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> &instances);
 
    // 删除某些节点
    int32_t RemoveNodes(const std::vector<uint64_t> &instances);

    // 提供一系列查询接口
    std::vector<uint64_t> GetNodeList();

    std::vector<uint64_t> GetPNodeList();

    std::map<uint64_t,  std::vector<uint64_t>>  GetP2DNodeMap();

    bool GetNodeById(uint64_t id,
                     MINDIE::MS::DIGSInstanceStaticInfo &staticInfo,
                     MINDIE::MS::DIGSInstanceDynamicInfo &dynamicInfo);

    bool GetNodeByIndex(uint64_t nodeIndex, uint64_t &nodeId);

    bool IsNodeAvailable(uint64_t nodeID);

    NodeStore() = default;
    NodeStore(const NodeStore& other) = delete;
    NodeStore& operator=(const NodeStore& other) = delete;
    NodeStore(NodeStore&& other) = delete;
    NodeStore& operator=(NodeStore&& other) = delete;

    ~NodeStore()
    {
        nodeIndexList.clear();
        pNodeList.clear();

        clusterStaticNodeMap.clear();
        clusterDynamicMap.clear();
        d2PminiGroupNodes.clear();
        p2DminiGroupNodes.clear();
    }
private:
    std::vector<uint64_t> nodeIndexList; // 各个节点的Index；
    std::vector<uint64_t> pNodeList;  // P节点的list

    std::map<uint64_t, MINDIE::MS::DIGSInstanceStaticInfo> clusterStaticNodeMap; // 集群调度节点, 静态信息
    std::map<uint64_t, MINDIE::MS::DIGSInstanceDynamicInfo> clusterDynamicMap; // 集群调度节点, 动态信息
    std::map<uint64_t, std::vector<uint64_t>> d2PminiGroupNodes; // group到节点信息 groupID --> d -->p
    std::map<uint64_t, std::vector<uint64_t>> p2DminiGroupNodes; // group到节点信息 groupID --> p -->d

    std::mutex mtx;
    void UpdateD2PRelations(const MINDIE::MS::DIGSInstanceDynamicInfo &ins);
    void UpdateP2DRelations();
};
}
#endif