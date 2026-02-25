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
#ifndef MINDIE_MS_NODE_STATUS_H
#define MINDIE_MS_NODE_STATUS_H
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <shared_mutex>
#include <memory>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include <optional>
#include <atomic>
#include "digs_instance.h"
#include "SecurityUtils.h"

namespace MINDIE::MS {
struct DeviceInfo {
    std::string id {};
    std::string ip {};
    std::string logicalId {};
    uint32_t rankId {}; // 用于区分master节点和slave节点
    std::optional<std::string> superDeviceId; // 仅部分设备涉及该字段
};

enum class InferenceType : int32_t {
    AVAILABLE,
    INITIALIZING_STATIC_TOTAL_INFO, // 正在初始化slot总量和block总量
    PREFILL_UPDATING_PEERS, // P节点身份不变仅更新peers
    UNAVAILABLE,  // 新增：不可用状态
};

enum class NodeChangePrintLogType : int32_t {
    REMOVED = 0,
    EXPIRED = 1,
    NEW = 2,
    REAPPEARED = 3,
    DEVICE_CHANGED = 4,
};

struct ServerInfo {
    std::string hostId {}; // server_id in ranktable, indicate the host ip of current node
    std::string ip {}; // server_ip
    std::vector<DeviceInfo> deviceInfos {};
    std::map<size_t, std::vector<DeviceInfo>> dpGroupInfos;
    bool isMaster = false;
    std::optional<std::string> superPodId; // 仅部分设备涉及该字段
    uint32_t spSize = 1;
    uint32_t cpSize = 1;
};

struct NodeInfo {
    std::string hostId {}; // server_id in ranktable, indicate the host ip of current node
    std::string ip {}; // server_ip
    std::string port {}; // server_port
    std::string mgmtPort {}; // mgmt_port in ranktable, indicate the management port of server node
    std::string metricPort {}; // metric_port in ranktable
    std::string interCommPort {}; // inter_comm_port in ranktable, port use for communication between p and d
    bool isHealthy = false; // 获取状态的接口是否能通，返回非200，代表不健康
    bool isInitialized = false; // 初次下发身份或者动态变化，下发身份接口返回200则认为可用
    InferenceType inferenceType = InferenceType::AVAILABLE;
    std::string roleState {};
    MINDIE::MS::DIGSInstanceRole currentRole = MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE;  // status接口查询到的身份
    std::string modelName {};
    MINDIE::MS::DIGSInstanceInfo instanceInfo {};
    std::vector<uint64_t> peers {};
    std::vector<uint64_t> activePeers {};
    std::chrono::seconds deleteTime = std::chrono::seconds(0);
    uint64_t initRetryTimes = 0;
    bool isInherited = false;
    uint64_t inheritedId = 0;
    bool isRoleChangeNode = false;
    bool isDistribute = false;
    std::vector<uint64_t> dpGroupPeers {};
    std::vector<ServerInfo> serverInfoList {};
    uint32_t instanceIdxInPod;
    uint32_t numInstancesPerPod;
    uint64_t virtualId = 1;
    bool isSingleNode = false;
    bool isStaticInfoinit = false;
};

void to_json(nlohmann::json& deviceJson, const DeviceInfo& deviceInfo);

void to_json(nlohmann::json& serverInfoJson, const ServerInfo& serverInfo);

struct NodeChanges {
    std::vector<uint64_t> newIDs {};
    std::vector<uint64_t> removedIDs {};
    std::vector<uint64_t> reappearIDs {};
};

class NodeStatus {
public:
    void AddNodes(std::vector<std::unique_ptr<NodeInfo>> &nodeVec);
    void AddNode(std::unique_ptr<NodeInfo> node);
    void AddFaultyNodes(std::vector<std::unique_ptr<NodeInfo>> &nodeVec);
    void AddFaultyNode(std::unique_ptr<NodeInfo> node);
    void AddExpiredNode(uint64_t id);
    void AddInitRetryTimes(uint64_t id);
    void UpdateNodeDynamicStatus(uint64_t id, MINDIE::MS::DIGSInstanceRole role, const std::string &roleState,
        const MINDIE::MS::DIGSInstanceDynamicInfo &info, const std::vector<uint64_t> &peers);
    void UpdateInferenceType(uint64_t id, InferenceType type);
    void UpdateNodeStaticInfo(uint64_t id, const MINDIE::MS::DIGSInstanceDynamicInfo &info);
    void UpdateRoleState(uint64_t id, const std::string &roleState, bool isHealthy = true, bool isInitialized = true);
    void UpdateRoleStateAndPeers(uint64_t groupId, uint64_t id, const std::string &roleState,
         const std::vector<uint64_t> &peers);
    void UpdateNode(uint64_t id, const NodeInfo &nodeInfo);
    void UpdateNodeScheduleInfo(uint64_t id, const MINDIE::MS::DIGSInstanceScheduleInfo &info);
    void UpdateNodeDeleteTime(uint64_t id);
    void UpdateInheritInfo(uint64_t id, uint64_t inheritId);
    void RemoveNode(uint64_t id);
    void RemoveExpiredNode(uint64_t id);
    std::string GetRoleState(uint64_t id);
    std::string GetIpForAllTypeNodes(uint64_t id);
    std::unique_ptr<NodeInfo> GetNode(uint64_t id);
    std::unique_ptr<NodeInfo> GetNode(std::string ip, std::string port);
    std::map<uint64_t, std::unique_ptr<NodeInfo>> GetAllNodes();
    std::map<uint64_t, std::unique_ptr<NodeInfo>> GetAllFaultyNodes();
    std::vector<uint64_t> GetAllGroupIds();
    std::vector<uint64_t> GetAllNodeIds();
    std::vector<uint64_t> GetDeletedNodeIds();
    std::set<uint64_t> GetExpiredNodeIds();
    void AddGroup(uint64_t groupId, std::pair<std::vector<uint64_t>, std::vector<uint64_t>> &group);
    void AddFlexGroup(uint64_t groupId, std::vector<uint64_t> &flexGroup);
    void AddFaultyGroup(uint64_t groupId, std::pair<std::vector<uint64_t>, std::vector<uint64_t>> &group);
    void AddFaultyFlexGroup(uint64_t groupId, std::vector<uint64_t> &flexGroup);
    void UpdateGroup(uint64_t groupId, std::pair<std::vector<uint64_t>, std::vector<uint64_t>> group);
    void UpdateFlexGroup(uint64_t groupId, std::vector<uint64_t> flexGroup);
    uint64_t GetNodesInGroup(uint64_t groupId);
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> GetGroup(uint64_t groupId);
    std::vector<uint64_t> GetFlexGroup(uint64_t groupId);
    NodeChanges DetectNodeChanges(const std::vector<std::unique_ptr<NodeInfo>> &serverNodes);
    bool IsPostRoleNeeded(uint64_t id);
    bool IsIgnoredInPDSeparate(uint64_t id);
    bool IsIgnoredInSingleNode(uint64_t id);
    bool IsNodeLinkedByPeer(uint64_t peer, uint64_t id);
    void UpdateRanktableChangeTime();
    int64_t GetRanktableChangeTime() const;
    static std::string ConvertNodeIdVector2Str(const std::vector<uint64_t>& nodeIdVec);
private:
    std::map<uint64_t, std::unique_ptr<NodeInfo>> mNodes;
    std::map<uint64_t, std::unique_ptr<NodeInfo>> mFaultyNodes;
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> mGroups;
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> mFaultyGroups;
    std::map<uint64_t, std::vector<uint64_t>> mFlexGroups{};
    std::map<uint64_t, std::vector<uint64_t>> mFaultyFlexGroups{};
    std::set<uint64_t> mExpiredNodeIds = {};
    std::shared_mutex mMtx;
    std::atomic<int64_t> ranktableChangeTime = -1;
    std::atomic<uint64_t> mCheckNodeLinkRoundCounter = {0};
};
}
#endif // MINDIE_MS_NODE_STATUS_H