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
#ifndef MINDIE_PREFIX_CACHE_EXECUTOR_H
#define MINDIE_PREFIX_CACHE_EXECUTOR_H

#include <iostream>
#include <list>
#include <unordered_map>
#include <string>
#include <mutex>
#include <shared_mutex>
#include "BaseAlgorithm.h"
#include "LRUCache.h"
#include "Logger.h"

namespace MINDIE::MS {
class PrefixCacheExecutor : public BaseAlgorithm {
public:
    PrefixCacheExecutor(std::unique_ptr<NodeStore> &nodeStorePtr,
        std::map<std::string, std::string> config) : nodeStore(nodeStorePtr),
        slotsThresh(0.0f), blockThresh(0.0f)
    {
        int cacheCapacity = 0;
        try {
            cacheCapacity = std::stoi(config["cache_size"]);
            slotsThresh = std::stof(config["slots_thresh"]);
            blockThresh = std::stof(config["block_thresh"]);
        } catch (...) {
            LOG_E("[%s] [PrefixCacheExecutor] Invalid parameters, use default value.",
                GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::PREFIXCACHE_EXECUTOR).c_str());
            cacheCapacity = 100;    // 默认100
            slotsThresh = 0.05;     // 默认0.05
            blockThresh = 0.05;     // 默认0.05
        }
        cache = std::make_unique<LRUCache>(cacheCapacity);
    };

    int32_t Execute(DeployMode deployMode, std::string requestBody, std::vector<uint64_t> &pickedNodes,
        int type) override;

private:
    bool PreProcessMessage(const std::string &requestBody, size_t &historyHash, size_t &newHash) const;
    bool CacheAffinity(size_t &historyHash, uint64_t &pickedNode);
    bool RoundRobin(uint64_t &bestNode);
    bool IsNodeResourceAvail(uint64_t nodeId);
    int32_t ProcessFirstRequest(const std::string &requestBody, std::vector<uint64_t> &pickedNodes);
    int32_t DoRoundRobin(std::vector<uint64_t> &pickedNodes);
    std::unique_ptr<NodeStore> &nodeStore; // 节点的信息

    std::unique_ptr<LRUCache> cache;
    double slotsThresh;
    double blockThresh;

    uint64_t curNodeId = 0;   // 选中的节点的id
    uint64_t curNodeIndex = 0; // 当前节点id所在Index
};
}

#endif