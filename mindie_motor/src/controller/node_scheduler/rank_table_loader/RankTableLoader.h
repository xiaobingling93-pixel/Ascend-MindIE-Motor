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
#ifndef MINDIE_MS_RANK_TABLE_LOADER_H
#define MINDIE_MS_RANK_TABLE_LOADER_H
#include <memory>
#include <vector>
#include <shared_mutex>
#include "NodeStatus.h"
#include "CoordinatorStore.h"
namespace MINDIE::MS {
    
struct NPUInfo {
    std::string npuID;
    std::string npuIP;
};

struct PodInfo {
    std::string groupId;
    std::string podIP;
    std::string podName;
    std::string hostIP;
    std::vector<NPUInfo> podAssociatedInfoList {};
};

struct InstanceInfo {
    MINDIE::MS::DIGSInstanceRole role = MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE;
    std::vector<std::string> serverInfoList {};
    size_t hashID {};
};

bool UpdateIptoIdMap(const NodeInfo &node, uint64_t preId);
void RestIptoIdMap();
#ifdef UT_FLAG
bool AllocateNodeId(std::unique_ptr<NodeInfo> &node, uint64_t &id);
#endif
class RankTableLoader {
public:

    // 单例模式
    static std::shared_ptr<RankTableLoader> GetInstance()
    {
        static std::shared_ptr<RankTableLoader> instance = std::make_shared<RankTableLoader>();
        return instance;
    }

    int32_t LoadRankTable(std::vector<std::unique_ptr<NodeInfo>> &serverNodes,
        std::shared_ptr<CoordinatorStore> coordinatorStore) const;
    int32_t WriteRankTable(std::string &rankTable) const;
    std::vector<std::unique_ptr<PodInfo>> GetPDPodInfoList() const;
    std::vector<std::unique_ptr<InstanceInfo>> GetInstanceInfoListByRankTable() const;
    int32_t GetInstanceStartIdNumber() const;
    void SetInstanceStartIdNumber(int32_t startId) const;

    void UpdateIpToIdMapByInstanceStartNumber(std::vector<std::unique_ptr<NodeInfo>> &recoverNodes,
        std::vector<std::unique_ptr<NodeInfo>> &currentAllNodes) const;

    RankTableLoader() = default;
    RankTableLoader(const RankTableLoader &) = delete;
    RankTableLoader &operator=(const RankTableLoader &) = delete;
private:
    static std::mutex mutexFile_;
};
}
#endif // MINDIE_MS_RANK_TABLE_LOADER_H
