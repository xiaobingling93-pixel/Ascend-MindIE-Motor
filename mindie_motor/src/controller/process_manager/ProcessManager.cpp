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
#include "ProcessManager.h"
#include <fstream>
#include <map>
#include "Logger.h"
#include "Util.h"
#include "file_lock_guard.h"
#include "ConfigParams.h"
#include "ControllerConfig.h"
#include "ControllerLeaderAgent.h"
#include "RankTableLoader.h"

namespace MINDIE::MS {
constexpr uint32_t MAX_GROUP_NUMBER = 6; // 最多有6个组
constexpr int64_t MAX_SERVER_NODES_PER_GROUP = 768; // 每个组内最多768个节点
constexpr uint32_t MAX_RANK_TB_ROLE_TYPE = 3; // 3: RANKTABLE表三种角色：controller、server、coordinator
// 集群信息表全部变更的场景下，可能出现全部替换
constexpr int64_t MAX_SERVER_NUMBER = MAX_GROUP_NUMBER * MAX_SERVER_NODES_PER_GROUP * MAX_RANK_TB_ROLE_TYPE;
constexpr int64_t MIN_INT_VALUE = 0;
constexpr int64_t MAX_INT_VALUE = 4294967295;

void AddPeersInfo(const std::vector<uint64_t> &peers, nlohmann::json &peersArray)
{
    for (auto &peer : peers) {
        peersArray.emplace_back(peer);
    }
}

void AddDeviceInfo(const std::vector<DeviceInfo> &deviceInfos, nlohmann::json &deviceArray)
{
    for (auto &deviceInfo : deviceInfos) {
        nlohmann::json info;
        info["id"] = deviceInfo.id;
        info["ip"] = deviceInfo.ip;
        info["logical_id"] = deviceInfo.logicalId;
        info["rank_id"] = deviceInfo.rankId;
        if (deviceInfo.superDeviceId.has_value()) {
            info["super_device_id"] = deviceInfo.superDeviceId.value();
        }
        deviceArray.emplace_back(info);
    }
}

void AddServerInfo(const std::vector<ServerInfo> &serverInfoList, nlohmann::json &serverArray)
{
    for (auto &serverInfo : serverInfoList) {
        auto info = nlohmann::json::object();
        info["hostId"] = serverInfo.hostId;
        info["ip"] = serverInfo.ip;
        info["is_master"] = serverInfo.isMaster;
        info["sp_size"] = serverInfo.spSize;
        info["cp_size"] = serverInfo.cpSize;
        if (serverInfo.superPodId.has_value()) {
            info["super_pod_id"] = serverInfo.superPodId.value();
        }
        info["device_infos"] = nlohmann::json::array();
        AddDeviceInfo(serverInfo.deviceInfos, info["device_infos"]);
        info["dp_group_infos"] = nlohmann::json::array();
        for (auto &dpGroup : serverInfo.dpGroupInfos) {
            nlohmann::json dpGroupInfo;
            dpGroupInfo["id"] = dpGroup.first;
            dpGroupInfo["device_infos"] = nlohmann::json::array();
            AddDeviceInfo(dpGroup.second, dpGroupInfo["device_infos"]);
            info["dp_group_infos"].emplace_back(dpGroupInfo);
        }
        serverArray.emplace_back(info);
    }
}

static bool IsValidRole(const nlohmann::json &staticInfo)
{
    if (!staticInfo.contains("role") || !staticInfo["role"].is_number_integer()) {
        LOG_E("[%s] [ProcessManager] Server role not exists or is not integer.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str());
        return false;
    }
    auto role = staticInfo["role"];
    if (role != MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE &&
        role != MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE &&
        role != MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE &&
        role != MINDIE::MS::DIGSInstanceRole::FLEX_INSTANCE) {
        LOG_E("[%s] [ProcessManager] Server label has invalid role.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str());
        return false;
    }
    return true;
}

static bool IsValidLabel(const nlohmann::json &staticInfo)
{
    if (!staticInfo.contains("label") || !staticInfo["label"].is_number_integer()) {
        LOG_E("[%s] [ProcessManager] Server label not exists or is not integer.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str());
        return false;
    }
    auto label = staticInfo["label"];
    if (label != MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER &&
        label != MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER &&
        label != MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC &&
        label != MINDIE::MS::DIGSInstanceLabel::DECODE_STATIC &&
        label != MINDIE::MS::DIGSInstanceLabel::FLEX_STATIC) {
        LOG_E("[%s] [ProcessManager] Server label has invalid value.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str());
        return false;
    }
    return true;
}

static bool IsValidLowerBoundedInt(const nlohmann::json &serverInfo, const std::string &key, int64_t min)
{
    if (!serverInfo.contains(key) || !serverInfo[key].is_number_integer()) {
        LOG_E("[%s] [ProcessManager] Key %s not exists or is not integer.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str(),
              key.c_str());
        return false;
    }
    int64_t val = serverInfo[key];

    if (val < MIN_INT_VALUE) {
        LOG_E("[%s] [ProcessManager] Key %s must be larger than %ld.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str(),
            key.c_str(), min);
        return false;
    }
    return true;
}

static bool IsValidServerInfo(const nlohmann::json &serverInfo)
{
    if (!IsValidLowerBoundedInt(serverInfo, "delete_time", MIN_INT_VALUE) ||
        !IsJsonStringValid(serverInfo, "ip") || !IsValidIp(serverInfo["ip"]) ||
        !IsJsonBoolValid(serverInfo, "is_initialized") ||
        !IsJsonBoolValid(serverInfo, "is_faulty") ||
        !IsJsonArrayValid(serverInfo, "peers", 0, MAX_SERVER_NODES_PER_GROUP) ||
        !IsJsonObjValid(serverInfo, "static_info") ||
        !IsValidLowerBoundedInt(serverInfo["static_info"], "group_id", MIN_INT_VALUE) ||
        !IsJsonIntValid(serverInfo["static_info"], "total_slots_num", 0, 5000) || // 取值范围0~5000
        !IsJsonIntValid(serverInfo["static_info"], "total_block_num", 0, MAX_INT_VALUE) || // 取值范围0~4294967295
        !IsValidLabel(serverInfo["static_info"]) || !IsValidRole(serverInfo["static_info"])) {
        LOG_E("[%s] [RankTableLoader] Server is invalid.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str());
        return false;
    }
    return true;
}

static bool IsValidProcessFile(const nlohmann::json &processFile)
{
    if (processFile.empty()) {
        LOG_E("[%s] [ProcessManager] Process file is empty.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str());
        return false;
    }
    if (!IsJsonArrayValid(processFile, "server", 0, MAX_SERVER_NUMBER)) {
        LOG_E("[%s] [RankTableLoader] Process file has invalid server list.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str());
        return false;
    }
    for (auto &serverInfo : processFile.at("server")) {
        if (!serverInfo.is_object() || !IsValidServerInfo(serverInfo)) {
            return false;
        }
    }
    return true;
}

nlohmann::json ProcessManager::LoadProcessFile() const
{
    LOG_I("[ProcessManager] Start to load process file.");
    nlohmann::json processFile;
    if (ControllerConfig::GetInstance()->GetCtrlBackUpConfig().funSw &&
        ControllerConfig::GetInstance()->IsLeader()) {
        if (!ControllerLeaderAgent::GetInstance()->ReadNodes(processFile)) {
            LOG_E("[ProcessManager] load file from etcd failed");
            return {};
        }
        LOG_I("[ProcessManager] load file from etcd successfully.");
    } else {
        if (!ControllerConfig::GetInstance()->GetProcessManagerConfig().toFile) {
            return {};
        }
        bool checkProcessFile = ControllerConfig::GetInstance()->GetCheckMountedFiles();
        uint32_t mode = checkProcessFile ? 0640 : 0777;
        processFile = FileToJsonObj(ControllerConfig::GetInstance()->GetProcessManagerConfig().filePath,
            mode);
        LOG_I("[ProcessManager] load file from local file successfully.");
    }
    try {
        // 提取节点状态部分，移除故障信息
        nlohmann::json nodeStatusData;
        // 复制所有字段，除了故障信息
        for (auto& [key, value] : processFile.items()) {
            if (key != "processed_switch_faults") {
                nodeStatusData[key] = value;
            }
        }
        if (!IsValidProcessFile(nodeStatusData)) {
            LOG_E("[%s] [ProcessManager] Process file is invalid.",
                GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str());
            return {};
        }
        return nodeStatusData;
    } catch (const std::exception &e) {
        LOG_E("[%s] [ProcessManager] Load process file error : %s.",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::PROCESS_MANAGER).c_str(),
            e.what());
        return {};
    }
    return {};
}

void ProcessManager::GenerateSingleNodesInfo(nlohmann::json &nodesJson, const NodeInfo &nodeInfo, bool isFaulty) const
{
    nlohmann::json node;
    node["delete_time"] = std::chrono::duration_cast<std::chrono::seconds>(nodeInfo.deleteTime).count();
    node["id"] = nodeInfo.instanceInfo.staticInfo.id;
    node["host_id"] = nodeInfo.hostId;
    node["ip"] = nodeInfo.ip;
    node["port"] = nodeInfo.port;
    node["mgmt_port"] = nodeInfo.mgmtPort;
    node["metric_port"] = nodeInfo.metricPort;
    node["inter_comm_port"] = nodeInfo.interCommPort;
    node["is_healthy"] = nodeInfo.isHealthy;
    node["is_initialized"] = nodeInfo.isInitialized;
    node["inference_type"] = static_cast<int32_t>(nodeInfo.inferenceType);
    node["role_state"] = nodeInfo.roleState;
    node["current_role"] = static_cast<char>(nodeInfo.currentRole);
    node["model_name"] = nodeInfo.modelName;
    nlohmann::json staticInfo;
    staticInfo["id"] = nodeInfo.instanceInfo.staticInfo.id;
    staticInfo["group_id"] = nodeInfo.instanceInfo.staticInfo.groupId;
    staticInfo["max_seq_len"] = nodeInfo.instanceInfo.staticInfo.maxSeqLen;
    staticInfo["max_output_len"] = nodeInfo.instanceInfo.staticInfo.maxOutputLen;
    staticInfo["total_slots_num"] = nodeInfo.instanceInfo.staticInfo.totalSlotsNum;
    staticInfo["total_block_num"] = nodeInfo.instanceInfo.staticInfo.totalBlockNum;
    staticInfo["block_size"] = nodeInfo.instanceInfo.staticInfo.blockSize;
    staticInfo["label"] = nodeInfo.instanceInfo.staticInfo.label;
    staticInfo["role"] = nodeInfo.instanceInfo.staticInfo.role;
    staticInfo["p_percentage"] = nodeInfo.instanceInfo.staticInfo.flexPRatio;
    staticInfo["virtual_id"] = nodeInfo.instanceInfo.staticInfo.virtualId;
    node["static_info"] = staticInfo;
    node["peers"] = nlohmann::json::array();

    AddPeersInfo(nodeInfo.peers, node["peers"]);
    node["active_peers"] = nlohmann::json::array();
    AddPeersInfo(nodeInfo.activePeers, node["active_peers"]);
    node["is_distribute"] = nodeInfo.isDistribute;
    node["dp_group_peers"] = nlohmann::json::array();
    AddPeersInfo(nodeInfo.dpGroupPeers, node["dp_group_peers"]);

    node["server_info_list"] = nlohmann::json::array();
    AddServerInfo(nodeInfo.serverInfoList, node["server_info_list"]);

    nlohmann::json dynamicInfo;
    dynamicInfo["avail_slots_num"] = nodeInfo.instanceInfo.dynamicInfo.availSlotsNum;
    dynamicInfo["avail_block_num"] = nodeInfo.instanceInfo.dynamicInfo.availBlockNum;
    node["dynamic_info"] = dynamicInfo;
    node["is_faulty"] = isFaulty;
    nodesJson["server"].emplace_back(node);
    LOG_D("[ClusterStatusWriter] Generating single server information: ID %lu, role %c.",
        nodeInfo.instanceInfo.staticInfo.id, nodeInfo.instanceInfo.staticInfo.role);
}

nlohmann::json ProcessManager::GenerateClusterInfoByList(const std::vector<std::unique_ptr<NodeInfo>> &servers) const
{
    nlohmann::json nodes;
    nodes["server"] = nlohmann::json::array();
    LOG_D("[ProcessManager] Generating cluster info by list, number of nodes is %zu.", servers.size());
    for (auto &server : std::as_const(servers)) {
        if (server == nullptr) {
            continue;
        }
        GenerateSingleNodesInfo(nodes, *server, false);
    }
    return nodes;
}

nlohmann::json ProcessManager::GenerateClusterInfo(const std::shared_ptr<NodeStatus> &nodeStatus) const
{
    nlohmann::json nodes;
    nodes["server"] = nlohmann::json::array();
    auto allNodes = nodeStatus->GetAllNodes();   // 保存group_id\role的信息
    LOG_D("[ProcessManager] Generating cluster infomation, number of nodes %zu.", allNodes.size());
    for (auto &it : std::as_const(allNodes)) {
        GenerateSingleNodesInfo(nodes, *it.second, false);
    }
    auto allFaultyNodes = nodeStatus->GetAllFaultyNodes();
    LOG_D("[ProcessManager] Generating cluster infomation, %zu faulty server nodes.", allFaultyNodes.size());
    for (auto &it : std::as_const(allFaultyNodes)) {
        GenerateSingleNodesInfo(nodes, *it.second, true);
    }
    // 序列化g_startIdNumber
    nodes["instance_start_id_number"] = RankTableLoader::GetInstance()->GetInstanceStartIdNumber();
    return nodes;
}

static void FillNodeInfo(NodeInfo &server, const nlohmann::json &node)
{
    server.instanceInfo.staticInfo.totalSlotsNum = node["static_info"]["total_slots_num"].get<size_t>();
    server.instanceInfo.staticInfo.totalBlockNum = node["static_info"]["total_block_num"].get<size_t>();
    server.instanceInfo.staticInfo.label = node["static_info"]["label"];
    server.instanceInfo.staticInfo.role = node["static_info"]["role"];
    if (node["static_info"].contains("p_percentage")) {
        server.instanceInfo.staticInfo.flexPRatio = node["static_info"]["p_percentage"].get<uint64_t>();
    }
    server.isInitialized = node["is_initialized"].get<bool>();
}

int32_t ProcessManager::ResizeGroups(const nlohmann::json &nodes,
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups,
    std::map<uint64_t, std::vector<uint64_t>> &flexGroup,
    const std::map<std::string, uint32_t> &ipToIndex, std::vector<std::unique_ptr<NodeInfo>> &servers) const
{
    for (auto &node : std::as_const(nodes)) {
        auto &ip = node["ip"];
        auto iter = ipToIndex.find(ip);
        if ((iter == ipToIndex.end()) || (servers[iter->second] == nullptr)) {
            continue;
        }
        auto groupId = node["static_info"]["group_id"].get<uint64_t>();
        auto &role = node["static_info"]["role"];
        if (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
            groups[groupId].first.push_back(servers[iter->second]->instanceInfo.staticInfo.id);
        } else if (role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
            groups[groupId].second.push_back(servers[iter->second]->instanceInfo.staticInfo.id);
        } else if (role == MINDIE::MS::DIGSInstanceRole::FLEX_INSTANCE) {
            flexGroup[groupId].push_back(servers[iter->second]->instanceInfo.staticInfo.id);
        } else {
            LOG_E("[%s] [ProcessManager] Node %lu has invalid role.",
                GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str(),
                servers[iter->second]->instanceInfo.staticInfo.id);
            continue;
        }
    }
    LOG_D("[ProcessManager] Resizing groups, there is %zu groups.", groups.size());
    if (groups.size() > MAX_GROUP_NUMBER) {
        LOG_E("[%s] [ProcessManager] Resizing groups, number of groups %zu should not be larger than %u.",
            GetErrorCode(ErrorType::RESOURCE_LIMIT, ControllerFeature::PROCESS_MANAGER).c_str(),
            groups.size(),
            MAX_GROUP_NUMBER);
        return -1;
    }
    return 0;
}

void ProcessManager::RemoveInvalidGroups(
    std::map<uint64_t, std::vector<uint64_t>> &flexGroup,
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &nodesInGroup) const
{
    for (auto iter = nodesInGroup.begin(); iter != nodesInGroup.end();) {
        uint64_t groupId = iter->first;
        uint64_t flexNum = 0;
        if (flexGroup.find(groupId) != flexGroup.end()) {
            flexNum = flexGroup[groupId].size();
        }
        if (iter->second.first.empty() || iter->second.second.empty()) {
            LOG_E("[%s] [ProcessManager] Recording empty groups, group ID %lu has %zu prefill nodes and "
                "%zu decode nodes. Prefill nodes or decode nodes is empty.",
                GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::PROCESS_MANAGER).c_str(),
                iter->first, iter->second.first.size(), iter->second.second.size());
            iter++;
        } else if ((iter->second.first.size() + iter->second.second.size() + flexNum) > MAX_SERVER_NODES_PER_GROUP) {
            LOG_E("[%s] [ProcessManager] Group ID %lu has too many servers.",
                GetErrorCode(ErrorType::RESOURCE_LIMIT, ControllerFeature::PROCESS_MANAGER).c_str(),
                iter->first);
            iter = nodesInGroup.erase(iter);
        } else {
            LOG_D("[ProcessManager] Group ID %lu has %zu prefill nodes and %zu ddecode nodes.", iter->first,
                  iter->second.first.size(), iter->second.second.size());
            iter++;
        }
    }
}

static void RefreshPeersByGroup(std::vector<std::unique_ptr<NodeInfo>> &retServers,
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups,
    std::map<uint64_t, std::vector<uint64_t>> &flexGroup)
{
    for (const auto &server : retServers) {
        if (server == nullptr) {
            continue;
        }
        uint64_t groupId = server->instanceInfo.staticInfo.groupId;
        std::vector<uint64_t> flexNodes;
        if (flexGroup.find(groupId) != flexGroup.end()) {
            flexNodes = flexGroup[groupId];
        }
        if (groups.find(groupId) != groups.end()) {
            if (server->instanceInfo.staticInfo.role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
                server->peers = groups[server->instanceInfo.staticInfo.groupId].second;
                server->peers.insert(server->peers.end(), flexNodes.begin(), flexNodes.end());
            } else if (server->instanceInfo.staticInfo.role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
                server->peers = groups[server->instanceInfo.staticInfo.groupId].first;
                server->peers.insert(server->peers.end(), flexNodes.begin(), flexNodes.end());
            } else if (server->instanceInfo.staticInfo.role == MINDIE::MS::DIGSInstanceRole::FLEX_INSTANCE) {
                std::vector<uint64_t> dNodes = groups[server->instanceInfo.staticInfo.groupId].second;
                server->peers = groups[server->instanceInfo.staticInfo.groupId].first;
                server->peers.insert(server->peers.end(), dNodes.begin(), dNodes.end());
            }
        }
    }
}
static void ClearServersVector(std::vector<std::unique_ptr<NodeInfo>> &availableServers,
    std::vector<std::unique_ptr<NodeInfo>> &faultyServers)
{
    availableServers.clear();
    faultyServers.clear();
}

int32_t ProcessManager::GetTotalAvailableResource(const nlohmann::json &statusJson,
    std::vector<std::unique_ptr<NodeInfo>> &servers,
    std::vector<std::unique_ptr<NodeInfo>> &availableServers) const
{
    availableServers.clear();
    std::map<std::string, uint32_t> ipToIndex;
    for (uint32_t i = 0; i < servers.size(); i++) {
        if (servers[i] == nullptr) {
            continue;
        }
        ipToIndex[servers[i]->ip] = i;
    }
    try {
        auto &nodes = statusJson["server"];
        LOG_I("[ProcessManager] %zu nodes in process file, %zu servers to recover.",
              nodes.size(), servers.size());
        for (auto &node : std::as_const(nodes)) {
            auto &ip = node["ip"];
            auto iter = ipToIndex.find(ip);
            if (iter == ipToIndex.end()) {
                continue;
            }
            auto &serverPtr = servers[iter->second];
            if (serverPtr == nullptr) {
                continue;
            }
            if (node["is_faulty"].get<bool>() ||
                node["static_info"]["role"] != MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE) {
                LOG_W("[%s] [ProcessManager] Retrieve available resource, ignore node with IP %s.",
                    GetWarnCode(ErrorType::WARNING, ControllerFeature::PROCESS_MANAGER).c_str(),
                    servers[iter->second]->ip.c_str());
                continue;
            }
            auto newPtr = std::unique_ptr<NodeInfo>(servers[iter->second].release());
            FillNodeInfo(*(newPtr), node);
            availableServers.emplace_back(std::move(newPtr));
        }
        return 0;
    } catch (const std::exception &e) {
        LOG_E("[%s] [ProcessManager] Get available resource error %s",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::PROCESS_MANAGER).c_str(),
            e.what());
        availableServers.clear();
        return -1;
    }
}

int32_t ProcessManager::GetStatusFromPath(const nlohmann::json &statusJson,
                                          std::vector<std::unique_ptr<NodeInfo>> &servers,
                                          std::vector<std::unique_ptr<NodeInfo>> &availableServers,
                                          std::vector<std::unique_ptr<NodeInfo>> &faultyServers,
                                          GroupInfo &groupInfo) const
{
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups = groupInfo.groups;
    std::map<uint64_t, std::vector<uint64_t>> &flexGroup = groupInfo.flexGroups;
    ClearServersVector(availableServers, faultyServers);
    std::map<std::string, uint32_t> ipToIndex;
    for (uint32_t i = 0; i < servers.size(); i++) {
        if (servers[i] == nullptr) {
            continue;
        }
        ipToIndex[servers[i]->ip] = i;
    }
    try {
        auto &nodes = statusJson["server"];
        LOG_I("[ProcessManager] %zu nodes in process file, %zu servers to recover.", nodes.size(), servers.size());
        if (ResizeGroups(nodes, groups, flexGroup, ipToIndex, servers) != 0) {
            ClearServersVector(availableServers, faultyServers);
            return -1;
        }
        RemoveInvalidGroups(flexGroup, groups);
        for (auto &node : std::as_const(nodes)) {
            auto &ip = node["ip"];
            auto iter = ipToIndex.find(ip);
            if (iter == ipToIndex.end()) {
                continue;
            }
            auto &serverPtr = servers[iter->second];
            if (serverPtr == nullptr) {
                continue;
            }
            servers[iter->second]->instanceInfo.staticInfo.groupId = node["static_info"]["group_id"].get<uint64_t>();
            auto groupId = servers[iter->second]->instanceInfo.staticInfo.groupId;
            auto newPtr = std::unique_ptr<NodeInfo>(servers[iter->second].release());
            FillNodeInfo(*(newPtr), node);
            if (groups.find(groupId) == groups.end()) {
                LOG_W("[%s] [ProcessManager] Group ID %lu is invalid, ignore node %lu.",
                    GetWarnCode(ErrorType::WARNING, ControllerFeature::PROCESS_MANAGER).c_str(),
                    groupId, newPtr->instanceInfo.staticInfo.id);
                faultyServers.emplace_back(std::move(newPtr));
                continue;
            }
            availableServers.emplace_back(std::move(newPtr));
        }
        RefreshPeersByGroup(availableServers, groups, flexGroup);
        return 0;
    } catch (const std::exception &e) {
        LOG_E("[%s] [ProcessManager] Get status from path failed, error is %s.",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::PROCESS_MANAGER).c_str(),
            e.what());
        ClearServersVector(availableServers, faultyServers);
        return -1;
    }
}

void GetPeersInfo(std::vector<uint64_t> &peers, const nlohmann::json &peersArray)
{
    for (auto &peer : peersArray) {
        peers.emplace_back(peer.get<uint64_t>());
    }
}

void GetDeviceInfo(std::vector<DeviceInfo> &deviceInfos, nlohmann::json &deviceArray)
{
    for (auto &device : deviceArray) {
        DeviceInfo deviceInfo;
        deviceInfo.id = device["id"].get<std::string>();
        deviceInfo.ip = device["ip"].get<std::string>();
        deviceInfo.logicalId = device["logical_id"].get<std::string>();
        deviceInfo.rankId = device["rank_id"].get<uint32_t>();
        if (device.contains("super_device_id")) {
            deviceInfo.superDeviceId =
                std::make_optional<std::string>(device["super_device_id"].get<std::string>());
        }
        deviceInfos.emplace_back(deviceInfo);
    }
}

void GetServerInfo(std::vector<ServerInfo> &serverInfoList, const nlohmann::json &serverArray)
{
    for (auto &server : serverArray) {
        ServerInfo info;
        info.hostId = server["hostId"].get<std::string>();
        info.ip = server["ip"].get<std::string>();
        info.isMaster = server["is_master"].get<bool>();
        info.spSize = server["sp_size"].get<int32_t>();
        info.cpSize = server["cp_size"].get<int32_t>();
        auto deviceInfos = server["device_infos"];
        GetDeviceInfo(info.deviceInfos, deviceInfos);
        if (server.contains("super_pod_id")) {
            info.superPodId =
                std::make_optional<std::string>(server["super_pod_id"].get<std::string>());
        }
        auto dpGroupInfos = server["dp_group_infos"];
        for (auto &dpGroup : dpGroupInfos) {
            // 获取dpGroup的id
            size_t id = dpGroup["id"].get<size_t>();
            // 获取dpGroup的设备信息
            std::vector<DeviceInfo> dpgroupDevices;
            GetDeviceInfo(dpgroupDevices, dpGroup["device_infos"]);

            info.dpGroupInfos[id] = dpgroupDevices;
        }
        serverInfoList.emplace_back(info);
    }
}

void GetStaticInfo(MINDIE::MS::DIGSInstanceStaticInfo &staticInfo, const nlohmann::json &staticInfoJson)
{
    staticInfo.id = staticInfoJson["id"].get<uint64_t>();
    staticInfo.groupId = staticInfoJson["group_id"].get<uint64_t>();
    staticInfo.maxSeqLen = staticInfoJson["max_seq_len"].get<size_t>();
    staticInfo.maxOutputLen = staticInfoJson["max_output_len"].get<size_t>();
    staticInfo.totalSlotsNum = staticInfoJson["total_slots_num"].get<size_t>();
    staticInfo.totalBlockNum = staticInfoJson["total_block_num"].get<size_t>();
    staticInfo.blockSize = staticInfoJson["block_size"].get<size_t>();
    staticInfo.label = static_cast<MINDIE::MS::DIGSInstanceLabel>(staticInfoJson["label"]);
    staticInfo.role = static_cast<MINDIE::MS::DIGSInstanceRole>(staticInfoJson["role"]);
    staticInfo.virtualId = staticInfoJson["virtual_id"].get<uint64_t>();
}

void GetBasicNodeInfo(NodeInfo &nodeInfo, const nlohmann::json &node)
{
    nodeInfo.hostId = node["host_id"].get<std::string>();
    nodeInfo.ip = node["ip"].get<std::string>();
    nodeInfo.deleteTime = std::chrono::seconds(node["delete_time"].get<int64_t>());
    nodeInfo.port = node["port"].get<std::string>();
    nodeInfo.mgmtPort = node["mgmt_port"].get<std::string>();
    nodeInfo.metricPort = node["metric_port"].get<std::string>();
    nodeInfo.interCommPort = node["inter_comm_port"].get<std::string>();
    nodeInfo.isHealthy = node["is_healthy"].get<bool>();
    nodeInfo.isInitialized = node["is_initialized"].get<bool>();
    nodeInfo.inferenceType = static_cast<InferenceType>(node["inference_type"].get<int32_t>());
    nodeInfo.roleState = node["role_state"].get<std::string>();
    nodeInfo.currentRole = static_cast<MINDIE::MS::DIGSInstanceRole>(node["current_role"].get<char>());
    nodeInfo.modelName = node["model_name"].get<std::string>();
    nodeInfo.isDistribute = node["is_distribute"].get<bool>();
}

int32_t ProcessManager::RecoverGroups(const nlohmann::json &nodes,
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups,
    std::map<uint64_t, std::vector<uint64_t>> &flexGroup,
    const std::map<std::pair<std::string, std::string>, uint32_t> &ipToIndex) const
{
    for (auto &node : std::as_const(nodes)) {
        auto key = std::make_pair(node["ip"].get<std::string>(), node["mgmt_port"].get<std::string>());
        if (ipToIndex.count(key) == 0) {
            LOG_I("Node(ip->%s, mgmtPort->%s) not available, can not recover to group",
                key.first.c_str(), key.second.c_str());
            continue;
        }
        const auto& static_info = node.at("static_info");
        auto groupId = static_info.at("group_id").get<uint64_t>();
        auto &role = static_info.at("role");
        auto lastId = static_info.at("id").get<uint64_t>();
        if (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
            groups[groupId].first.push_back(lastId);
        } else if (role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
            groups[groupId].second.push_back(lastId);
        } else if (role == MINDIE::MS::DIGSInstanceRole::FLEX_INSTANCE) {
            flexGroup[groupId].push_back(lastId);
        } else {
            LOG_E("[%s] [ProcessManager] Node %lu has invalid role.",
                GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::PROCESS_MANAGER).c_str(),
                lastId);
            continue;
        }
    }
    LOG_I("[ProcessManager] RecoverGroups, there is %zu groups.", groups.size());
    if (groups.size() > MAX_GROUP_NUMBER) {
        LOG_E("[%s] [ProcessManager] RecoverGroups, number of groups %zu should not be larger than %u.",
            GetErrorCode(ErrorType::RESOURCE_LIMIT, ControllerFeature::PROCESS_MANAGER).c_str(),
            groups.size(), MAX_GROUP_NUMBER);
        return -1;
    }
    return 0;
}

// 反序列化恢复g_startIdNumber信息
void ProcessManager::RecoverInstanceStartIdNumber(const nlohmann::json &statusJson) const
{
    int32_t startId = statusJson["instance_start_id_number"].get<int32_t>();
    RankTableLoader::GetInstance()->SetInstanceStartIdNumber(startId);
    LOG_D("[ProcessManager] recover g_startIdNumber value is %d", startId);
    return ;
}

int32_t ProcessManager::GetNodeInfoFromPath(const nlohmann::json &statusJson,
    std::vector<std::unique_ptr<NodeInfo>> &servers,
    std::vector<std::unique_ptr<NodeInfo>> &availableServers,
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups,
    std::map<uint64_t, std::vector<uint64_t>> &flexGroups) const
{
    availableServers.clear();
    std::map<std::pair<std::string, std::string>, uint32_t> ipToIndex;
    for (uint32_t i = 0; i < servers.size(); i++) {
        if (servers[i] == nullptr) {
            continue;
        }
        ipToIndex[{servers[i]->ip, servers[i]->mgmtPort}] = i;
    }
    try {
        auto &nodes = statusJson["server"];
        LOG_I("[ProcessManager] %zu nodes in process file,  %zu nodes are available", nodes.size(), servers.size());

        if (RecoverGroups(nodes, groups, flexGroups, ipToIndex) != 0) {
            groups.clear();
            return -1;
        }
        for (auto &node : std::as_const(nodes)) {
            auto key = std::make_pair(node["ip"].get<std::string>(), node["mgmt_port"].get<std::string>());
            if (ipToIndex.count(key) == 0) {
                LOG_I("Node(ip->%s, mgmtPort->%s) not available, can not recover", key.first.c_str(),
                      key.second.c_str());
                continue;
            }
            NodeInfo nodeInfo;
            GetBasicNodeInfo(nodeInfo, node);
            GetStaticInfo(nodeInfo.instanceInfo.staticInfo, node["static_info"]);
            GetPeersInfo(nodeInfo.peers, node["peers"]);
            GetPeersInfo(nodeInfo.activePeers, servers[ipToIndex[key]]->activePeers);
            GetPeersInfo(nodeInfo.dpGroupPeers, node["dp_group_peers"]);
            GetServerInfo(nodeInfo.serverInfoList, node["server_info_list"]);

            auto dynamicInfo = node["dynamic_info"];
            nodeInfo.instanceInfo.dynamicInfo.availSlotsNum = dynamicInfo["avail_slots_num"].get<size_t>();
            nodeInfo.instanceInfo.dynamicInfo.availBlockNum = dynamicInfo["avail_block_num"].get<size_t>();
            nodeInfo.instanceInfo.dynamicInfo.id = nodeInfo.instanceInfo.staticInfo.id;
            if (!UpdateIptoIdMap(nodeInfo, nodeInfo.instanceInfo.staticInfo.id)) {
                LOG_E("[ProcessManager] Update ip->%s, mgmtPort->%s to IptoIdMap failed",
                    key.first.c_str(), key.second.c_str());
                continue;
            }
            availableServers.emplace_back(std::make_unique<NodeInfo>(nodeInfo));
        }
        RecoverInstanceStartIdNumber(statusJson);
        LOG_I("[ProcessManager] Get %zu available servers from process file.", availableServers.size());
        return 0;
    }
    catch (const std::exception &e) {
        LOG_E("[%s] [ProcessManager] Get node info from path failed, error is %s.",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::PROCESS_MANAGER).c_str(), e.what());
        availableServers.clear();
        return -1;
    }
}

int32_t ProcessManager::SaveStatusToFile(const std::shared_ptr<NodeStatus> &nodeStatus) const
{
    try {
        if (ControllerConfig::GetInstance()->GetCtrlBackUpConfig().funSw &&
            ControllerConfig::GetInstance()->IsLeader()) {
            // json格式行缩进为4
            if (!ControllerLeaderAgent::GetInstance()->WriteNodes(GenerateClusterInfo(nodeStatus).dump(4))) {
                LOG_E("[ProcessManager] load file from etcd failed");
                return -1;
            }
            return 0;
        }
        auto path = ControllerConfig::GetInstance()->GetProcessManagerConfig().filePath;
        if (!PathCheckForCreate(path)) {
            LOG_E("[ProcessManager] Invalid path of node status file: %s", path.c_str());
            return -1;
        }
        // 使用 RAII 文件锁，自动管理锁的获取和释放
        std::string lockPath = path + ".lock";
        FileLockGuard lockGuard(lockPath);
        if (!lockGuard.IsLocked()) {
            LOG_E("[ProcessManager] Failed to acquire file lock for %s", path.c_str());
            return -1;
        }
        std::string tmpPath = path + ".tmp";
        // 需要读取现有数据并保留故障信息
        nlohmann::json clusterInfo = GenerateClusterInfo(nodeStatus);
        bool checkProcessFile = ControllerConfig::GetInstance()->GetCheckMountedFiles();
        uint32_t mode = checkProcessFile ? 0640 : 0777; // 校验权限是0640, 不校验是0777
        nlohmann::json existingData = FileToJsonObj(path, mode);
        if (existingData.contains("processed_switch_faults")) {
            clusterInfo["processed_switch_faults"] = existingData["processed_switch_faults"];
        }
        // 写入临时文件并原子替换
        if (DumpStringToFile(tmpPath, clusterInfo.dump(4)) != 0) { // 4 json格式行缩进为4
            LOG_E("[ProcessManager] Failed to write temporary file");
            return -1;
        }
        if (rename(tmpPath.c_str(), path.c_str()) != 0) {
            LOG_E("[%s] [ProcessManager] Rename node status tmp file failed",
                GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::PROCESS_MANAGER).c_str());
            return -1;
        }
        LOG_D("[ProcessManager] Save node status file successfully");
        return 0;
    }  catch (const std::exception& e) {
        LOG_E("[%s] [ProcessManager] Save status to file failed, error is %s.",
              GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::PROCESS_MANAGER).c_str(), e.what());
        return -1;
    }
}

int32_t ProcessManager::SaveServerListToFile(const std::vector<std::unique_ptr<NodeInfo>> &servers) const
{
    return DumpStringToFile(ControllerConfig::GetInstance()->GetProcessManagerConfig().filePath,
        GenerateClusterInfoByList(servers).dump(4)); // 4 json格式行缩进为4
}

void ProcessManager::Wait()
{
    auto waitSeconds = ControllerConfig::GetInstance()->GetClusterSynchronizationSeconds();
    mWaitSeconds.store(waitSeconds);
    while (mWaitSeconds.load() > 0 && mRun.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 1s检查一次
        mWaitSeconds--;
    }
}

int32_t ProcessManager::Init(const std::shared_ptr<NodeStatus> &nodeStatus)
{
    if (!ControllerConfig::GetInstance()->GetProcessManagerConfig().toFile) {
        LOG_I("[ProcessManager] To file is false, do not write process status.");
        return 0;
    }
    LOG_I("[ProcessManager] To file is true, write process status.");
    try {
        mMainThread = std::make_unique<std::thread>([this, nodeStatus]() {
            while (mRun.load()) {
                if (!ControllerConfig::GetInstance()->IsLeader()) {
                    Wait();
                    continue;
                }
                LOG_D("[ProcessManager] Running start.");
                if (GetInstance()->SaveStatusToFile(nodeStatus) != 0) {
                    LOG_E("[%s] [ProcessManager] SaveStatusToFile: failed",
                          GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::PROCESS_MANAGER).c_str());
                };
                LOG_D("[ProcessManager] Running end.");
                Wait();
            }
        });
    } catch (const std::exception& e) {
        LOG_E("[%s] [ProcessManager] Failed to create thread.",
            GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::PROCESS_MANAGER).c_str());
        return -1;
    }
    return 0;
}
void ProcessManager::Stop()
{
    mRun.store(false);
    if (mMainThread != nullptr && mMainThread->joinable()) {
        mMainThread->join();
    }
    LOG_I("[ProcessManager] Stop successfully.");
}
}