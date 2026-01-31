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
#ifndef MINDIE_ROUND_ROBIN_EXECUTOR_H
#define MINDIE_ROUND_ROBIN_EXECUTOR_H

#include <iostream>
#include <list>
#include <unordered_map>
#include <string>
#include <mutex>
#include "BaseAlgorithm.h"
namespace MINDIE::MS {
class RoundRobinExecutor : public BaseAlgorithm {
public:
    explicit RoundRobinExecutor(std::unique_ptr<NodeStore> &nodeStorePtr) : nodeStore(nodeStorePtr) {};
    int32_t Execute(DeployMode deployMode, std::string requestBody, std::vector<uint64_t> &pickedNodes,
        int type) override;

private:
    int32_t SingleNodeExecute(std::string requestBody,  uint64_t &pickedNode);
    int32_t PDNodeExecute(std::string requestBody, std::vector<uint64_t> &pickedNodes);
    bool IsNodeResourceAvail(uint64_t nodeId);
    bool PickDNodeByP(uint64_t pNode, uint64_t &dNode);

    std::unique_ptr<NodeStore> &nodeStore; // 节点的信息
 
    // 单机场景的iter
    uint64_t singleNodeIndex = 0; // 单节点的iter

    // pd 分离的iter
    uint64_t pNodeIndex = 0; // 当前选择哪一个 pIndex
    std::map<uint64_t, uint64_t> p2DIndexMap; // 当前的P选择哪一个 dIndex
};
}

#endif