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
#ifndef MINDIE_MS_ROLE_SWITCHER_H
#define MINDIE_MS_ROLE_SWITCHER_H

#include <memory>
#include "NodeStatus.h"
#include "CoordinatorStore.h"
#include "HttpClient.h"
#include "role_manager/InstanceRoleManager.h"
namespace MINDIE::MS {
struct RoleSwitchingInfo {
    std::vector<uint64_t> oldP {};
    std::vector<uint64_t> oldD {};
};

/// The class that performs server role switching.
///
/// This class sends HTTP requests and performs server role switching. The process includes stopping
/// the server's inference service, querying the coordinator for the number of inference tasks, specifying
/// the server's role, etc.
class RoleSwitcher {
public:
    RoleSwitcher(const RoleSwitcher &obj) = delete;
    RoleSwitcher &operator=(const RoleSwitcher &obj) = delete;
    RoleSwitcher(RoleSwitcher &&obj) = delete;
    RoleSwitcher &operator=(RoleSwitcher &&obj) = delete;

    /// Constructor.
    ///
    /// \param nodeStatusInit The shared pointer of nodeStatus which is globally managed in the controller.
    /// \param coordinatorStoreInit The shared pointer of coordinatorStore which is globally managed in the controller.
    RoleSwitcher(std::shared_ptr<NodeStatus> nodeStatusInit, std::shared_ptr<CoordinatorStore> coordinatorStoreInit);

    /// Destructor.
    ~RoleSwitcher();

    /// Initialize the RoleSwitcher object.
    ///
    /// \return The result of the initialization. 0 indicates success. -1 indicates failure.
    int32_t Init();

    /// Process role switching of the server.
    ///
    /// \param decision The decision of role switching.
    void ProcessSingleRoleSwitching(MINDIE::MS::DIGSRoleDecision &decision);
    void UpdateAbnormalRoleWhenRecoverCluster(NodeInfo &node,
        std::pair<std::vector<uint64_t>, std::vector<uint64_t>> &group,
        std::vector<std::unique_ptr<NodeInfo>> &nodesInGroup) const;
private:
    void CheckStatusThenSynchronizeCluster(const std::vector<uint64_t> &success);
    void UpdatePToDWhenRecoverCluster(std::pair<std::vector<uint64_t>, std::vector<uint64_t>> &group,
        NodeInfo &newD, std::vector<std::unique_ptr<NodeInfo>> &nodesInGroup) const;
    void UpdateDToPWhenRecoverCluster(std::pair<std::vector<uint64_t>, std::vector<uint64_t>> &group,
        NodeInfo &newP, std::vector<std::unique_ptr<NodeInfo>> &nodesInGroup) const;
    void SendSwitchedPDRoles(const std::vector<uint64_t> &ids) const;
    void ProcessPSwitchToD(uint64_t groupId, const RoleSwitchingInfo &roleSwitchingInfo, uint64_t newD);
    void ProcessDSwitchToP(uint64_t groupId, const RoleSwitchingInfo &roleSwitchingInfo, uint64_t newP);
    void DNodeUpdatePeers(uint64_t id, std::vector<std::unique_ptr<NodeInfo>> &nodesInGroup,
        const std::vector<uint64_t> &pNodes) const;
    void PNodeUpdatePeers(uint64_t id, std::vector<std::unique_ptr<NodeInfo>> &nodesInGroup,
        const std::vector<uint64_t> &dNodes) const;
    int32_t OldInstanceOffline(uint64_t id);
    bool GetCoordinatorNodes(std::vector<std::unique_ptr<Coordinator>> &coordinatorNodes);
    void UpdateFlexNodeToServer(MINDIE::MS::DIGSRoleDecision &decision);
    NodeInfo BuildNodeInfo(uint64_t groupId, MINDIE::MS::DIGSInstanceRole role, const std::vector<uint64_t> &peers);

    /// Request the coordinator to stop the server's inference service.
    ///
    /// \param id The ID of the server.
    /// \return The result of requesting. 0 indicates success. -1 indicates failure.
    int32_t PostInstanceOffline(uint64_t id);

    /// Request the coordinator to recover the server's inference service.
    ///
    /// \param id The ID of the server.
    /// \return The result of requesting. 0 indicates success. -1 indicates failure.
    void PostInstanceOnline(uint64_t id);

    /// Request the coordinator to query the number of inference tasks.
    ///
    /// \param id The ID of the server.
    /// \param waitSeconds 等待节点任务清零的总时间
    /// \return The query result. 0 indicates no tasks. -1 indicates that tasks are executing.
    int32_t QueryInstanceTasks(uint64_t id, uint64_t waitSeconds);

    /// Request the coordinator to query the number of inference tasks between prefilling and decoding servers.
    ///
    /// \param pId The prefilling server ID.
    /// \param dId The decoding server ID.
    /// \return The query result. 0 indicates no tasks. -1 indicates that tasks are executing.
    int32_t QueryInstancePeerTasks(uint64_t pId, uint64_t dId, MINDIE::MS::DIGSRoleChangeType type);
    void QueryInstancePeersTasksP2D(uint64_t pId, std::vector<uint64_t> dIds, std::vector<uint64_t> &success);
    void QueryInstancePeersTasksD2P(uint64_t dId, std::vector<uint64_t> pIds, std::vector<uint64_t> &success);
    void UpdatePToDWhenAbnormal(uint64_t groupId, uint64_t id, const RoleSwitchingInfo &roleSwitchingInfo);
    void UpdateDToPWhenAbnormal(uint64_t groupId, uint64_t id, const RoleSwitchingInfo &roleSwitchingInfo);
    void ProcessPSwitchToDAbnormal(uint64_t groupId, uint64_t id, const RoleSwitchingInfo &roleSwitchingInfo);
    void ProcessFlexAbnormal(uint64_t id);
    void QueryInstancePeersTasksFlex(uint64_t flexId, std::vector<uint64_t> peers,
        std::vector<uint64_t> &success);

    /// The shared pointer of nodeStatus which is globally managed in the controller.
    std::shared_ptr<NodeStatus> mNodeStatus = nullptr;

    /// The shared pointer of coordinatorStore which is globally managed in the controller.
    std::shared_ptr<CoordinatorStore> mCoordinatorStore = nullptr;

    /// The HTTP client to request server.
    std::shared_ptr<HttpClient> mServerClient = nullptr;

    /// The HTTP client to request coordinator.
    std::shared_ptr<HttpClient> mCoordinatorClient = nullptr;
};
}
#endif // MINDIE_MS_ROLE_SWITCHER_H
