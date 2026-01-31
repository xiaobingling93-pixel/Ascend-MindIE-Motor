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
#ifndef MINDIE_MS_COORDINATOR_PD_CLUSTER_H
#define MINDIE_MS_COORDINATOR_PD_CLUSTER_H

#include <chrono>
#include <memory>
#include <list>
#include <map>
#include <vector>
#include <tuple>
#include <shared_mutex>
#include <atomic>
#include <unordered_set>
#include "nlohmann/json.hpp"
#include "ServerConnection.h"
#include "BaseScheduler.h"

namespace MINDIE::MS {
using namespace std::chrono;

struct InstanceInfo {
    InstanceInfo() : role(MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE), retry(0), ip(""), port(""), modelName("")
    {}
    InstanceInfo(const std::string &ipInit, const std::string &portInit, MINDIE::MS::DIGSInstanceRole roleInit,
        const std::string &modelNameInit)
        : role(roleInit), retry(0), ip(ipInit), port(portInit), modelName(modelNameInit) {}
    MINDIE::MS::DIGSInstanceRole role;
    size_t retry;
    std::string ip;
    std::string port;
    std::string modelName;
    std::unordered_set<std::string> tasks;
    std::string metricPort {};
    std::string interCommPort {};
    size_t totalBlockNum = 0;
    size_t totalSlotsNum = 0;
    uint64_t virtualId = 1;
};

constexpr size_t DECODE_INS_ID_TRANSFER_BY_FLEX = UINT64_MAX - 1; // 由FLEX转化或衍生出的D实例的ID

struct ClusterFlexInstanceInfo {
    bool clusterHasFlex = false;
    uint64_t originFlexInsId = 0;
    uint64_t pPercentage = 0;
    uint64_t splitDInsId = DECODE_INS_ID_TRANSFER_BY_FLEX;
};

class ClusterNodes {
public:
    using RollType = std::tuple<std::vector<uint64_t>, std::vector<uint64_t>, std::vector<uint64_t>>;
    ClusterNodes() = default;
    ~ClusterNodes() = default;
    bool AddInstance(uint64_t id, const std::string &ip, const std::string &port, MINDIE::MS::DIGSInstanceRole role,
        const std::string &modelName);
    void UpdateExtraInfo(uint64_t id, std::pair<const std::string&, const std::string &> httpParam,
        size_t totalBlockNum, size_t totalSlotsNum, uint64_t virtualId);
    
    void RemoveInstance(uint64_t id);
    int64_t GetTask(uint64_t id);
    std::string GetIp(uint64_t id);
    std::string GetPort(uint64_t id);
    std::string GetInterCommPort(uint64_t id);
    MINDIE::MS::DIGSInstanceRole GetRole(uint64_t id);
    // 返回值：需要新增的实例，需要更新的实例，需要删除的实例
    RollType Roll(const std::vector<uint64_t> &newIds);
    const std::map<uint64_t, std::unique_ptr<InstanceInfo>> &GetInstanceInfos();
    bool IsAvailable();
    uint64_t GetId(const std::string &ip, const std::string &port);
    void AddTask(uint64_t id, const std::string &reqId);
    void DecreaseTask(uint64_t id, const std::string &reqId);
    void AddRetry(uint64_t id);
    size_t GetRetry(uint64_t id);
    std::string GetModelName(uint64_t id);
    uint64_t GetTokenizerIns();
    bool HasModelName(const std::string &modelName);
    std::unordered_set<std::string> GetTasksById(uint64_t id);
    size_t GetTotalBlockNum(uint64_t id);
    size_t GetTotalSlotsNum(uint64_t id);

    void UpdateClusterFlexInstanceInfo(uint64_t oriFlexId, uint64_t pPercentage);
    void ClearClusterFlexInstanceInfo();
    bool IsClusterHasFlex();
    uint64_t GetOriFlexInsId();
    uint64_t GetSplitedDInsId();
    bool IsFlexSplitedIntoTwoInstance();
    bool IsVecContainsFlex(const std::vector<uint64_t>& vec);
    bool IsBothPAndDFromFlex(uint64_t pId, uint64_t dId);
    bool IsInstanceFromFlex(uint64_t id);
    size_t GetInsNumMax(void);
    int32_t ProcessFlexInstance(std::vector<uint64_t>& nodeIds, nlohmann::json& instances);
    int32_t ConvertMInstanceToD(std::vector<uint64_t>& mNodeIds, nlohmann::json& instance, nlohmann::json& instances);
    int32_t ConvertMInstanceToP(nlohmann::json& instance, nlohmann::json& instances);
    int32_t SplitMInstanceToPAndD(std::vector<uint64_t>& mNodeIds, nlohmann::json& instance, nlohmann::json& instances);
    int32_t RemoveRedundantInsInFlexPeers(nlohmann::json& flexInstance, const std::vector<uint64_t> dumpInfo);
    int32_t FillInstancesInfoSplitedByFlex(nlohmann::json& instance, nlohmann::json& splitDIns, uint64_t flexId,
                                          std::vector<uint64_t> pInsIdVec, std::vector<uint64_t> dInsIdVec);
    int32_t InstancePreProcInConvertMToD(nlohmann::json::iterator& iter, uint64_t flexId, uint64_t flexGroupId,
                                         std::vector<uint64_t>& dInsIdVec);
    void ProcSchedulerInfoUnderFlexSituation(std::vector<MINDIE::MS::DIGSInstanceScheduleInfo>& schedulerInfo);
    void ProcInstanceIdsUnderFlexSituation(std::vector<uint64_t>& nodeIds);
    int64_t GetInstanceTaskNumUnderFlexSituation(uint64_t id);
    void ProcTaskQuaryDInstanceIdUnderFlexSituation(uint64_t& dId);
    void HandlePInsPeersInSameGroupWithFlex(nlohmann::json::iterator& iter, uint64_t flexId,
                                           std::vector<uint64_t>& pInsIdVec);
    /**
     * @brief 检查指定节点ID是否为故障节点
     * @param id 要检查的节点ID
     * @return 如果节点是故障节点返回true，否则返回false
     */
    bool IsFaultyNode(uint64_t id);

    /**
     * @brief 检查指定实例ID是否存在
     * @param id 要检查的实例ID
     * @return 如果实例存在返回true，否则返回false
     */
    bool HasInstance(uint64_t id);

    /**
     * @brief 获取指定虚拟ID对应的所有节点ID集合
     * @param id 虚拟ID
     * @return 返回包含所有相关节点ID的集合
     * @note 虚拟ID可能对应多个实际节点ID，特别是在分布式部署场景下
     */
    std::unordered_set<uint64_t> GetVirtualIdToIds(uint64_t id);

    /**
     * @brief 将指定节点添加到故障节点列表中
     * @param id 要添加的节点ID
     * @note 添加故障节点时会同时记录添加时间，用于后续故障恢复判断
     */
    void AddFaultNode(uint64_t id);

    /**
     * @brief 从故障节点列表中移除指定节点
     * @param id 要移除的节点ID
     * @note 移除后该节点将被视为正常节点，可以重新参与集群工作
     */
    void RemoveFaultNode(uint64_t id);

    /**
     * @brief 获取指定故障节点的删除时间
     * @param id 故障节点ID
     * @return 返回节点被标记为故障的时间点
     * @note 该时间点用于判断故障节点是否可以恢复
     */
    system_clock::time_point GetDeleteTime(uint64_t id);

private:
    std::list<uint64_t> ids;
    std::map<uint64_t, std::unique_ptr<InstanceInfo>> instanceInfos;
    std::map<uint64_t, std::unordered_set<uint64_t>> virtualToIdsMap;
    std::shared_mutex mtx;
    ClusterFlexInstanceInfo clusterFlexInsInfo;
    std::unordered_set<uint64_t> faultVirtualIds;
    std::unordered_set<uint64_t> faultIds;
    std::map<uint64_t, system_clock::time_point> virtualIdToDelTimeMap;
    std::map<uint64_t, system_clock::time_point> idToDelTimeMap;
};

}
#endif