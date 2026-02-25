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
#include <cstdint>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <map>
#include <set>
#include "nlohmann/json.hpp"
#include "ResourceManager.h"
#include "ControllerConfig.h"
#include "Logger.h"
#include "RankTableLoader.h"

namespace MINDIE::MS {
constexpr size_t DEFAULT_SERVER_GROUP_NUM = 3; // 默认非跨机场景下，仅支持三组:coordinator, controller, server
constexpr size_t MAX_MULTI_SERVER_GROUP_NUM = 98; // 跨机场景，支持coordinator、controller、96个server，共98个
constexpr size_t MIN_SERVER_NODES = 2;
constexpr uint64_t MAX_SERVER_NODES = 6 * 16; // 最多有6个组，每个组内16个节点
constexpr uint64_t MAX_GROUP_LIST_NUM = 3; // 一共有3种类型的节点
constexpr uint64_t MIN_GROUP_LIST_NUM = 2; // 一共有2种类型的节点
constexpr int32_t MIN_DEVICE_ID = 0;
constexpr int32_t MAX_DEVICE_ID = 2048; // 2的30次方
constexpr int32_t MIN_SUPER_DEVICE_ID = 0;
constexpr int32_t MAX_SUPER_DEVICE_ID = 1073741824; // 2的30次方
constexpr int32_t NUM_INSTANCES_SINGLE_CONTAINER = 16; // max containers at 16 per pod
constexpr int32_t MAX_COORDINATOR_NUM = 2;  // 最大支持2个coordinator
constexpr size_t PARSE_INSTANCE_LOG_FREQUENCY = 10;

std::mutex RankTableLoader::mutexFile_;
static std::unordered_map<std::string, int32_t> ipToIdMap;
static int32_t g_startIdNumber = 0;
static size_t g_loadGRTCount = 0;

bool AllocateNodeId(std::unique_ptr<NodeInfo> &node, uint64_t &id)
{
    if (!IsValidIp(node->ip) || !IsValidPort(static_cast<int64_t>(std::stoi(node->mgmtPort)))) {
        LOG_E("[%s] [RankTableLoader] node ip %s or mgmtPort %s is invalid.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
              node->ip.c_str(), node->mgmtPort.c_str());
        return false;
    }
    if (ipToIdMap.find(node->ip + node->mgmtPort) != ipToIdMap.end()) {
        id = ipToIdMap[node->ip + node->mgmtPort];
    } else {
        LOG_I("[RankTableLoader] AllocateNodeId: node ip %s, mgmtPort %s, id %lu",
            node->ip.c_str(), node->mgmtPort.c_str(), g_startIdNumber);
        id = g_startIdNumber;
        ipToIdMap[node->ip + node->mgmtPort] = g_startIdNumber++;
    }
    node->instanceInfo.staticInfo.groupId = 0;
    node->instanceInfo.staticInfo.id = id;
    node->instanceInfo.dynamicInfo.id = id;
    node->instanceInfo.scheduleInfo.id = id;
    return true;
}

void RestIptoIdMap()
{
    ipToIdMap.clear();
    g_startIdNumber = 0;
}

bool UpdateIptoIdMap(const NodeInfo &node, uint64_t preId)
{
    bool ret = false;
    uint64_t id = preId;
    if (!IsValidIp(node.ip) || !IsValidPort(static_cast<int64_t>(std::stoi(node.mgmtPort)))) {
        LOG_E("[%s] [RankTableLoader] node ip %s or mgmtPort %s is invalid.",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
              node.ip.c_str(), node.mgmtPort.c_str());
        return false;
    }
    if (ipToIdMap.find(node.ip + node.mgmtPort) != ipToIdMap.end()) {
        id = ipToIdMap[node.ip + node.mgmtPort];
        ret = true;
        // 更新map映射表
        ipToIdMap[node.ip + node.mgmtPort] = preId;
        LOG_I("[RankTableLoader] Change node(ip %s, mgmtPort %s) id:%d  to %d", node.ip.c_str(),
            node.mgmtPort.c_str(), id, preId);
    } else {
        LOG_E("[RankTableLoader] Can not find node ip %s, mgmtPort %s in ipToIdMap",
            node.ip.c_str(), node.mgmtPort.c_str());
    }
    return ret;
}

static void UpdateNodePorts(std::unique_ptr<NodeInfo> &node, nlohmann::json::iterator it)
{
    node->port = it->value("predict_port", ControllerConfig::GetInstance()->GetMindIEServerPort());
    node->mgmtPort = it->value("mgmt_port", ControllerConfig::GetInstance()->GetMindIEServerControlPort());
    node->metricPort = it->value("metric_port", ControllerConfig::GetInstance()->GetMindIEServerMetricPort());
    // Inter communication port of server node is empty when p and d are distributed.
    node->interCommPort = it->value("inter_comm_port", "");
}

static bool UpdateMasterNodeInfo(std::unique_ptr<NodeInfo> &node, nlohmann::json::iterator master)
{
    try {
        node->hostId = master->at("server_id").get<std::string>();
        node->ip = master->at("server_ip").get<std::string>();
    } catch (const std::exception &e) {
        LOG_E("master info not contains server_id or server_ip : create node failed.");
        return false;
    }

    UpdateNodePorts(node, master);
    node->roleState = ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN);

    uint64_t id;
    if (!AllocateNodeId(node, id)) {
        LOG_E("[%s] [RankTableLoader] UpdateMasterNodeInfo: Allocate id for node %s failed.",
              GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
              node->ip.c_str());
        return false;
    }
    return true;
}

static bool UpdateNodeInfo(std::unique_ptr<NodeInfo> &node, const nlohmann::json &master, uint64_t &initialPort)
{
    node->hostId = master.at("server_id").get<std::string>();
    node->ip = master.at("server_ip").get<std::string>();
    node->port = std::to_string(initialPort);
    node->mgmtPort = std::to_string(++initialPort);
    node->metricPort = std::to_string(++initialPort);
    node->interCommPort = std::to_string(++initialPort);
    node->roleState = ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN);

    uint64_t id;
    if (!AllocateNodeId(node, id)) {
        LOG_E("[%s] [RankTableLoader] UpdateMasterNodeInfo: Allocate id for node %s failed.",
              GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
              node->ip.c_str());
        return false;
    }
    return true;
}

static DeviceInfo GetDeviceInfo(nlohmann::json deviceInfo)
{
    DeviceInfo info;
    info.id = deviceInfo["device_id"].get<std::string>();
    info.ip = deviceInfo["device_ip"].get<std::string>();
    info.logicalId = deviceInfo["device_logical_id"].get<std::string>();
    info.rankId = static_cast<uint32_t>(std::stoi(deviceInfo["rank_id"].get<std::string>()));
    if (deviceInfo.contains("super_device_id")) {
        info.superDeviceId =
            std::make_optional<std::string>(deviceInfo["super_device_id"].get<std::string>());
    }
    return info;
}

static std::unique_ptr<NodeInfo> ParseMultiNodeInfo(nlohmann::json &serverList, DeployMode deployMode,
                                                    bool isHeterogeneous,
                                                    std::map<std::string, std::string> &superPodListMap)
{
    std::unique_ptr<NodeInfo> node;
    try {
        node = std::make_unique<NodeInfo>();
    } catch (const std::exception& e) {
        LOG_E("[%s] [RankTableLoader] ParseMultiNodeInfo: create node failed.",
              GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::RANK_TABLE_LOADER).c_str());
        return nullptr;
    }

    auto masterIt = serverList.begin();
    uint32_t minRankId = UINT32_MAX;
    for (auto it = serverList.begin(); it != serverList.end(); ++it) {
        ServerInfo server;
        server.hostId = it->at("server_id").get<std::string>();
        server.ip = it->at("server_ip").get<std::string>();
        if (superPodListMap.find(server.hostId) != superPodListMap.end()) {
            server.superPodId = std::make_optional<std::string>(superPodListMap[server.hostId]);
        }
        if (deployMode == DeployMode::PD_SEPARATE) {
            for (auto &deviceInfo : it->at("device")) {
                auto info = GetDeviceInfo(deviceInfo);
                if (info.rankId < minRankId) {
                    minRankId = info.rankId;
                    masterIt = it;
                }
                server.deviceInfos.emplace_back(info);
            }
        }
        server.spSize = ControllerConfig::GetInstance()->GetPSpSize();
        server.cpSize = ControllerConfig::GetInstance()->GetPCpSize();
        node->serverInfoList.emplace_back(server);
    }
    auto offset = static_cast<size_t>(masterIt - serverList.begin());
    node->serverInfoList[offset].isMaster = true;
    if (!UpdateMasterNodeInfo(node, masterIt)) {
        LOG_E("[%s] [RankTableLoader] ParseMultiNodeInfo: get master node info failed.",
            GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::RANK_TABLE_LOADER).c_str());
        return nullptr;
    }
    if (deployMode == DeployMode::PD_SEPARATE && isHeterogeneous) {
        node->instanceInfo.staticInfo.nodeRes.hardwareType = masterIt->at("hardware_type").get<std::string>();
    }
    if (g_loadGRTCount % PARSE_INSTANCE_LOG_FREQUENCY == 0) {
        LOG_I("[RankTableLoader]ParseMultiNodeInfo: parse centralized instance success, master ip %s.",
            node->ip.c_str());
    }
    return node;
}

static std::vector<std::unique_ptr<NodeInfo>> ParseDistributeNodeInfo(
    nlohmann::json &serverList,
    std::vector<uint64_t>& innerIds,
    bool isDistribute,
    MINDIE::MS::DIGSInstanceRole role,
    std::map<std::string, std::string> &superPodListMap)
{
    std::vector<std::unique_ptr<NodeInfo>> nodeVector;
    uint32_t tpSize = 1;
    uint32_t spSize = 1;
    uint32_t cpSize = 1;
    if (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
        tpSize = ControllerConfig::GetInstance()->GetPTpSize();
        spSize = ControllerConfig::GetInstance()->GetPSpSize();
        cpSize = ControllerConfig::GetInstance()->GetPCpSize();
    } else if (role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
        tpSize = ControllerConfig::GetInstance()->GetDTpSize();
        spSize = ControllerConfig::GetInstance()->GetDSpSize();
        cpSize = ControllerConfig::GetInstance()->GetDCpSize();
    }
    uint64_t npuNumPerDp = cpSize * tpSize;

    try {
        for (const auto& serverJson : serverList) {
            uint64_t initialPort = ControllerConfig::GetInstance()->GetInitialDpServerPort();
            auto deviceJson = serverJson.at("device");
            for (size_t i = 0; i < deviceJson.size(); i += npuNumPerDp) {
                ServerInfo server;
                server.hostId = serverJson.at("server_id").get<std::string>();
                server.ip = serverJson.at("server_ip").get<std::string>();
                if (superPodListMap.find(server.hostId) != superPodListMap.end()) {
                    server.superPodId = std::make_optional<std::string>(superPodListMap[server.hostId]);
                }
                auto node = std::make_unique<NodeInfo>();
                for (size_t j = 0; j < npuNumPerDp && (i + j) < deviceJson.size(); ++j) {
                    auto info = GetDeviceInfo(deviceJson[i + j]);
                    server.deviceInfos.emplace_back(info);
                }
                server.spSize = spSize;
                server.cpSize = cpSize;
                node->serverInfoList.emplace_back(server);
                if (!UpdateNodeInfo(node, serverJson, initialPort)) {
                    LOG_E("[%s] [RankTableLoader] Update distribute node info failed for server %s",
                          GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
                          server.hostId.c_str());
                    return {};
                }
                initialPort++;
                node->isDistribute = isDistribute;
                node->instanceInfo.staticInfo.role = role;
                innerIds.push_back(node->instanceInfo.staticInfo.id);
                nodeVector.emplace_back(std::move(node));
            }
        }
    } catch (const std::exception& e) {
        LOG_E("[RankTableLoader] JSON parsing error: %s", e.what());
        return {};
    }
    if (g_loadGRTCount % PARSE_INSTANCE_LOG_FREQUENCY == 0) {
        LOG_I("[RankTableLoader] Parse distributed instance success, total %zu nodes", nodeVector.size());
    }
    return nodeVector;
}

static void GetSuperPodListMap(std::map<std::string, std::string> &superPodListMap,
                               nlohmann::json &superPodList)
{
    for (auto superPod: superPodList) {
        auto superPodId = superPod["super_pod_id"];
        for (auto server_list : superPod["server_list"]) {
            superPodListMap[server_list["server_id"]] = superPodId;
        }
    }
}

static std::vector<std::unique_ptr<NodeInfo>> ParseGroupServerNodes(nlohmann::json &serverList, DeployMode deployMode,
    bool isHeterogeneous, std::map<std::string, std::string> &superPodListMap)
{
    std::set<uint64_t> ids;
    std::vector<std::unique_ptr<NodeInfo>> ret;
    uint32_t localInstanceIdx = 0;
    for (auto it = serverList.begin(); it != serverList.end(); ++it) {
        std::unique_ptr<NodeInfo> node;
        try {
            node = std::make_unique<NodeInfo>();
        } catch (const std::exception& e) {
            LOG_E("[%s] [RankTableLoader] Create node failed during group server node parsing.",
                GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::RANK_TABLE_LOADER).c_str());
            return {};
        }
        
        ServerInfo server;
        server.isMaster = true;
        server.hostId = it->at("server_id").get<std::string>();
        if (superPodListMap.find(server.hostId) != superPodListMap.end()) {
            server.superPodId = std::make_optional<std::string>(superPodListMap[server.hostId]);
        }
        server.ip = it->at("server_ip").get<std::string>();
        if (deployMode == DeployMode::PD_SEPARATE) {
            for (auto &deviceInfo : it->at("device")) {
                auto info = GetDeviceInfo(deviceInfo);
                server.deviceInfos.emplace_back(info);
            }
        }
        node->serverInfoList.emplace_back(server);
        node->instanceIdxInPod = localInstanceIdx;
        localInstanceIdx++;
        node->numInstancesPerPod =
            ControllerConfig::GetInstance()->GetDIGSIsSingleContainer() ? NUM_INSTANCES_SINGLE_CONTAINER : 1;

        if (!UpdateMasterNodeInfo(node, it)) {
            LOG_E("[%s] [RankTableLoader] Get master node info failed.",
                GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::RANK_TABLE_LOADER).c_str());
            return {};
        }
        if (ids.find(node->instanceInfo.staticInfo.id) != ids.end()) {
            LOG_E("[%s] [RankTableLoader] Non-unique node ID detected for IP %s",
                GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
                node->ip.c_str());
            return {};
        }
        if (deployMode == DeployMode::PD_SEPARATE && isHeterogeneous) {
            node->instanceInfo.staticInfo.nodeRes.hardwareType = it->at("hardware_type").get<std::string>();
        }
        LOG_I("[RankTableLoader] Parsed node: ID %lu, IP %s, device size %zu.",
            node->instanceInfo.staticInfo.id, node->ip.c_str(), node->serverInfoList[0].deviceInfos.size());
        ids.insert(node->instanceInfo.staticInfo.id);
        ret.push_back(std::move(node));
    }
    return ret;
}

static std::vector<std::unique_ptr<NodeInfo>> ParseServerNodes(nlohmann::json &serverGroupList, DeployMode deployMode,
    bool isHeterogeneous)
{
    std::vector<std::unique_ptr<NodeInfo>> ret;
    for (auto serverGroup : std::as_const(serverGroupList)) {
        auto groupId = serverGroup["group_id"].get<std::string>();
        if (groupId != ControllerConstant::GetInstance()->GetMindIEServerGroup()) {
            continue;
        }
        auto serverList = serverGroup["server_list"];
        std::map<std::string, std::string> superPodListMap;
        if (serverGroup.contains("super_pod_list")) {
            GetSuperPodListMap(superPodListMap, serverGroup["super_pod_list"]);
        }
        return ParseGroupServerNodes(serverList, deployMode, isHeterogeneous, superPodListMap);
    }
    return {};
}

static std::pair<bool, MINDIE::MS::DIGSInstanceRole> GetDeployConfig(const nlohmann::json& serverGroup)
{
    bool isDistribute = false;
    MINDIE::MS::DIGSInstanceRole initRole = MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE;
    if (serverGroup.contains("deploy_server")) {
        const auto deployServer = std::stoi(serverGroup["deploy_server"].get<std::string>());
        isDistribute = static_cast<bool>(deployServer);
        initRole = ControllerConfig::GetInstance()->GetPDInitRole(deployServer);
    }
    return {isDistribute, initRole};
}

static void HandleDistributedNode(std::vector<std::unique_ptr<NodeInfo>> &ret,
    nlohmann::json &serverList, bool isDistribute, MINDIE::MS::DIGSInstanceRole initRole,
    std::map<std::string, std::string> &superPodListMap)
{
    std::vector<uint64_t> innerIds;
    auto nodeInfoVec = ParseDistributeNodeInfo(serverList, innerIds, isDistribute, initRole, superPodListMap);
    if (nodeInfoVec.empty()) {
        return;
    }
    auto virtualId = nodeInfoVec[0]->instanceInfo.staticInfo.id;
    bool isSingleNode = (serverList.size() == 1);
    for (auto& node : nodeInfoVec) {
        node->dpGroupPeers = innerIds;
        node->virtualId = virtualId;
        node->instanceInfo.staticInfo.virtualId = virtualId;
        node->isSingleNode = isSingleNode;
        ret.emplace_back(std::move(node));
    }
}

static std::vector<std::unique_ptr<NodeInfo>> ParseMultiNodeServer(nlohmann::json &serverGroupList,
    DeployMode deployMode, bool isHeterogeneous)
{
    std::vector<std::unique_ptr<NodeInfo>> ret {};
    std::set<uint64_t> ids;
    for (auto serverGroup : std::as_const(serverGroupList)) {
        auto groupId = serverGroup["group_id"].get<std::string>();
        if (std::stoi(groupId) < std::stoi(ControllerConstant::GetInstance()->GetMindIEServerGroup())) {
            continue;
        }
        std::map<std::string, std::string> superPodListMap;
        if (serverGroup.contains("super_pod_list")) {
            GetSuperPodListMap(superPodListMap, serverGroup["super_pod_list"]);
        }
        auto serverList = serverGroup["server_list"];
        const auto [isDistribute, initRole] = GetDeployConfig(serverGroup);
        if (isDistribute) {
            HandleDistributedNode(ret, serverList, isDistribute, initRole, superPodListMap);
        } else {
            auto nodeInfo = ParseMultiNodeInfo(serverList, deployMode, isHeterogeneous, superPodListMap);
            if (!nodeInfo) {
                LOG_E("[%s] [RankTableLoader] ParseMultiNodeServer: Parse node with group_id %s failed.",
                      GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
                      groupId.c_str());
                return {};
            }
            nodeInfo->instanceInfo.staticInfo.role = initRole;
            auto nodeId = nodeInfo->instanceInfo.staticInfo.id;
            if (ids.find(nodeId) != ids.end()) {
                LOG_E("[%s] [RankTableLoader] ParseMultiNodeServer: Id of node %s is not unique.",
                      GetErrorCode(ErrorType::OPERATION_REPEAT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
                      nodeInfo->ip.c_str());
                return {};
            }
            nodeInfo->virtualId = nodeId;
            nodeInfo->instanceInfo.staticInfo.virtualId = nodeId;
            nodeInfo->dpGroupPeers.push_back(nodeId);
            nodeInfo->isSingleNode = (serverList.size() == 1);
            ids.insert(nodeId);
            ret.emplace_back(std::move(nodeInfo));
        }
    }
    return ret;
}

static std::vector<std::unique_ptr<NodeInfo>> GetServerNodes(nlohmann::json &rankTable, DeployMode deployMode,
    bool isHeterogeneous)
{
    auto serverGroupList = rankTable.at("server_group_list");
    // 非跨机情况下，组的个数不会超过3
    if (!ControllerConfig::GetInstance()->IsMultiNodeMode()) {
        return ParseServerNodes(serverGroupList, deployMode, isHeterogeneous);
    } else {
        // 跨机时，目前仅支持PD分离模式，组的个数至少为4，即包含controller、coordinator和至少1P、1D两个实例
        return ParseMultiNodeServer(serverGroupList, deployMode, isHeterogeneous);
    }
}

static std::vector<std::unique_ptr<Coordinator>> ParseGroupCoordinatorNodes(nlohmann::json &serverList)
{
    std::vector<std::unique_ptr<Coordinator>> ret;
    for (auto it = serverList.begin(); it != serverList.end(); ++it) {
        try {
            auto node = std::make_unique<Coordinator>();
            node->ip = it->at("server_ip").get<std::string>();
            ret.push_back(std::move(node));
        } catch (const std::exception& e) {
            LOG_E("[%s] [RankTableLoader] Failed to create a coordinator instance during coordinator node parsing.",
                GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::RANK_TABLE_LOADER).c_str());
            return {};
        }
    }
    return ret;
}
static bool IsValidDeviceInfo(const nlohmann::json &deviceInfo)
{
    if (!IsJsonStringValid(deviceInfo, "device_ip") || !IsValidIp(deviceInfo["device_ip"]) ||
        !IsJsonStringValid(deviceInfo, "device_id") || !IsJsonStringValid(deviceInfo, "device_logical_id") ||
        !IsJsonStringValid(deviceInfo, "rank_id")) {
        return false;
    }
    try {
        int32_t id = std::stoi(deviceInfo["device_id"].get<std::string>());
        if (id < MIN_DEVICE_ID || id > MAX_DEVICE_ID) {
            LOG_E("[%s] [RankTableLoader] Device ID %d is out of range [%d, %d].",
                GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(), id,
                MIN_DEVICE_ID, MAX_DEVICE_ID);
            return false;
        }
        id = std::stoi(deviceInfo["device_logical_id"].get<std::string>());
        if (id < MIN_DEVICE_ID || id > MAX_DEVICE_ID) {
            LOG_E("[%s] [RankTableLoader] Device logical ID %d is out of range [%d, %d]",
                  GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(), id,
                MIN_DEVICE_ID, MAX_DEVICE_ID);
            return false;
        }
        id = std::stoi(deviceInfo["rank_id"].get<std::string>());
        if (id < MIN_DEVICE_ID || id > MAX_DEVICE_ID) {
            LOG_E("[%s] [RankTableLoader] IsValidDeviceInfo: rank id be in range of [%d, %d], but got %d.",
                  GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
                  MIN_DEVICE_ID, MAX_DEVICE_ID, id);
            return false;
        }
        if (deviceInfo.contains("super_device_id")) {
            id = std::stoi(deviceInfo["super_device_id"].get<std::string>());
            if (id < MIN_SUPER_DEVICE_ID || id > MAX_SUPER_DEVICE_ID) {
                LOG_E("[%s] [RankTableLoader] super_device_id: super_device_id be in range of [%d, %d]",
                      GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
                      MIN_SUPER_DEVICE_ID, MAX_DEVICE_ID);
                return false;
            }
        }
        return true;
    } catch (const std::exception &e) {
        LOG_E("[%s] [RankTableLoader] Parse device info failed.",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::RANK_TABLE_LOADER).c_str());
        return false;
    }
}

static bool IsValidServer(const nlohmann::json &server, DeployMode deployMode, bool isHeterogeneous)
{
    std::string errorCode = GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER);
    if (!IsJsonStringValid(server, "server_ip") || !IsValidIp(server.at("server_ip"))) {
        return false;
    }
    auto ip = server["server_ip"].get<std::string>();
    if (!IsJsonStringValid(server, "server_id") || !IsValidIp(server.at("server_id"))) {
        return false;
    }
    std::vector<std::string> portKey = {"predict_port", "mgmt_port", "metric_port", "inter_comm_port"};
    for (auto &key : portKey) {
        if (server.contains(key)) {
            std::string portStr = server[key].get<std::string>();
            auto portVal = static_cast<int64_t>(std::stoi(portStr));
            if (!IsValidPort(portVal)) {
                LOG_E("[%s] [RankTableLoader] Invalid port value for key '%s' on server IP %s, %s is out of range "
                    "[1024, 65535]", errorCode.c_str(), key.c_str(), ip.c_str(), portStr.c_str());
                return false;
            }
        }
    }
    if (deployMode != DeployMode::PD_SEPARATE) {
        return true;
    }
    if (server.contains("super_pod_id")) {
        LOG_D("[RankTableLoader] Json contains super_pod_id.");
        std::string superPodIdStr = server["super_pod_id"].get<std::string>();
        std::ignore = static_cast<int64_t>(std::stoi(superPodIdStr));
    }
    if (!IsJsonArrayValid(server, "device", 1, 128)) { // 支持输入卡的数量为1~128
        LOG_E("[%s] [RankTableLoader] Server IP %s has invalid device number.", errorCode.c_str(),
            ip.c_str());
        return false;
    }
    for (auto &deviceInfo : server.at("device")) {
        if (!deviceInfo.is_object() || !IsValidDeviceInfo(deviceInfo)) {
            return false;
        }
    }
    if (!isHeterogeneous) {
        return true;
    }
    if (!IsJsonStringValid(server, "hardware_type") ||
        ((server.at("hardware_type") != "800I A2(64G)") && (server.at("hardware_type") != "800I A2(32G)"))) {
        LOG_E("[%s] [RankTableLoader] Server IP %s has invalid hardware type.", errorCode.c_str(),
            ip.c_str());
        return false;
    }
    return true;
}

static bool IsValidServerList(nlohmann::json &serverList, DeployMode deployMode, bool isHeterogeneous)
{
    for (auto server : std::as_const(serverList)) {
        if (!server.is_object() || !IsValidServer(server, deployMode, isHeterogeneous)) {
            LOG_E("[%s] [RankTableLoader] Server is invalid.",
                  GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str());
            return false;
        }
    }
    return true;
}
static bool IsValidServerGroup(nlohmann::json &rankTable, DeployMode deployMode, bool isHeterogeneous)
{
    try {
        auto serverGroupList = rankTable.at("server_group_list");
        for (auto serverGroup : std::as_const(serverGroupList)) {
            if (!serverGroup.is_object() || !IsJsonStringValid(serverGroup, "group_id")) {
                return false;
            }
            auto groupId = std::stoi(serverGroup["group_id"].get<std::string>());
            if (groupId < std::stoi(ControllerConstant::GetInstance()->GetMindIEServerGroup())) {
                continue;
            }
            if (!IsJsonArrayValid(serverGroup, "server_list", 0, MAX_SERVER_NODES)) { // 支持输入server数量为0~96
                LOG_E("[%s] [RankTableLoader] Server list is invalid.",
                    GetErrorCode(ErrorType::RESOURCE_LIMIT, ControllerFeature::RANK_TABLE_LOADER).c_str());
                return false;
            }
            auto serverList = serverGroup["server_list"];
            return IsValidServerList(serverList, deployMode, isHeterogeneous);
        }
        return false;
    } catch (const std::exception& e) {
        LOG_E("[%s] [RankTableLoader] Exception encountered while validating server group: %s.",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::RANK_TABLE_LOADER).c_str(),
            e.what());
        return false;
    }
}

static bool IsValidCoordinatorList(nlohmann::json &serverList)
{
    for (auto server : std::as_const(serverList)) {
        if (!server.is_object() || !IsJsonStringValid(server, "server_ip") || !IsValidIp(server["server_ip"])) {
            LOG_E("[%s] [RankTableLoader] Invalid coordinator server object or IP in coordinator list. ",
                  GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str());
            return false;
        }
    }
    return true;
}

static bool IsValidCoordinatorGroup(nlohmann::json &rankTable)
{
    try {
        auto serverGroupList = rankTable.at("server_group_list");
        for (auto groupIt : std::as_const(serverGroupList)) {
            if (!groupIt.is_object() || !IsJsonStringValid(groupIt, "group_id")) {
                return false;
            }
            auto groupId = groupIt["group_id"].get<std::string>();
            if (groupId != ControllerConstant::GetInstance()->GetMindIECoordinatorGroup()) {
                continue;
            }
            if (!IsJsonArrayValid(groupIt, "server_list", 0, MAX_COORDINATOR_NUM)) { // 支持输入coordinator数量为0~2
                LOG_E("[%s] [RankTableLoader] Coordinator group is invalid.",
                      GetErrorCode(ErrorType::RESOURCE_LIMIT, ControllerFeature::RANK_TABLE_LOADER).c_str());
                return false;
            }
            auto serverList = groupIt["server_list"];
            return IsValidCoordinatorList(serverList);
        }
        return false;
    } catch (const std::exception& e) {
        LOG_E("[%s] [RankTableLoader] Exception encountered while validating coordinator group, error is %s.",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::RANK_TABLE_LOADER).c_str(),
            e.what());
        return false;
    }
}

static std::vector<std::unique_ptr<Coordinator>> GetCoordinatorNode(nlohmann::json &rankTable)
{
    auto serverGroupList = rankTable.at("server_group_list");
    std::vector<std::unique_ptr<Coordinator>> ret;
    for (auto groupIt : std::as_const(serverGroupList)) {
        auto groupId = groupIt["group_id"].get<std::string>();
        if (groupId != ControllerConstant::GetInstance()->GetMindIECoordinatorGroup()) {
            continue;
        }
        auto serverList = groupIt["server_list"];
        return ParseGroupCoordinatorNodes(serverList);
    }
    return {};
}

static void CheckNonHeterogeneousServers(const std::vector<std::unique_ptr<NodeInfo>> &serverNodes)
{
    if (serverNodes.size() < MIN_SERVER_NODES) {
        LOG_W("[%s] [RankTableLoader] Server nodes count %zu is less than the minimum required %zu.",
            GetWarnCode(ErrorType::WARNING, ControllerFeature::RANK_TABLE_LOADER).c_str(),
            serverNodes.size(), MIN_SERVER_NODES);
    }
}

static void CheckHeterogeneousServers(const std::vector<std::unique_ptr<NodeInfo>> &serverNodes)
{
    uint32_t nodeType64 = 0;
    uint32_t nodeType32 = 0;
    for (auto &node : std::as_const(serverNodes)) {
        if (node == nullptr) {
            continue;
        }
        if (node->instanceInfo.staticInfo.nodeRes.hardwareType == "800I A2(64G)") {
            nodeType64++;
        } else {
            nodeType32++;
        }
    }
    if (nodeType64 == 0) { // 0表示没有64G的节点
        LOG_W("[%s] [RankTableLoader] No nodes with hardware type '800I A2(64G)' found.",
            GetWarnCode(ErrorType::WARNING, ControllerFeature::RANK_TABLE_LOADER).c_str());
    }
    if (nodeType32 == 0) { // 0表示没有32G的节点
        LOG_W("[%s] [RankTableLoader] No nodes with hardware type '800I A2(32G)' found.",
            GetWarnCode(ErrorType::WARNING, ControllerFeature::RANK_TABLE_LOADER).c_str());
    }
}

static void CheckServers(const std::vector<std::unique_ptr<NodeInfo>> &serverNodes)
{
    if (serverNodes.size() < MIN_SERVER_NODES) {
        LOG_W("[%s] [RankTableLoader] Server nodes count %zu is less than the required minimum %zu",
            GetWarnCode(ErrorType::WARNING, ControllerFeature::RANK_TABLE_LOADER).c_str(),
            serverNodes.size(), MIN_SERVER_NODES);
    }
}

int32_t RankTableLoader::WriteRankTable(std::string &rankTable) const
{
    auto path = ControllerConfig::GetInstance()->GetGlobalRankTablePath();
    std::lock_guard<std::mutex> lock(mutexFile_);
    try {
        std::ofstream outFile(path, std::ios::out);
        LOG_I("[RankTableLoader] Writing global rank table to file %s.", path.c_str());
        outFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        outFile << rankTable;

        uint32_t mode = 0640;
        if (chmod(path.c_str(), mode) != 0) {
            LOG_E("[%s] [RankTableLoader] Failed to set permissions for global rank table file: %s.",
                GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::RANK_TABLE_LOADER).c_str(),
                path.c_str());
            return -1;
        }

        return 0;
    }
    catch (const std::ofstream::failure& e) {
        std::error_code ec = std::error_code(errno, std::system_category());
        LOG_E("[%s] [RankTableLoader] Failed to write global rank table to file %s. Error: %s, System error: %s",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
            path.c_str(),
            e.what(),
            ec.message().c_str());
        return -1;
    }
    catch (const std::exception& e) {
        LOG_E("[%s] [RankTableLoader] Unexpected error while writing global rank table file %s. Error: %s",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
            path.c_str(),
            e.what());
        return -1;
    }
}

int32_t RankTableLoader::LoadRankTable(std::vector<std::unique_ptr<NodeInfo>> &serverNodes,
    std::shared_ptr<CoordinatorStore> coordinatorStore) const
{
    serverNodes.clear();
    auto path = ControllerConfig::GetInstance()->GetGlobalRankTablePath();
    uint32_t mode = (ControllerConfig::GetInstance()->GetCheckMountedFiles()) ? 0640 : 0777; // 权限要求是0640, 不校验是0777

    std::lock_guard<std::mutex> lock(mutexFile_);
    g_loadGRTCount++;
    auto rankTable = FileToJsonObj(path, mode, (ControllerConfig::GetInstance()->GetCheckMountedFiles()));
    if (rankTable.empty()) {
        LOG_E("[%s] [RankTableLoader] Read global rank table file %s failed.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
            path.c_str());
        return -1;
    }
    bool isHeterogeneous = ControllerConfig::GetInstance()->GetDIGSIsHeterogeneous();
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    bool isMultiNode = ControllerConfig::GetInstance()->IsMultiNodeMode();
    uint64_t maxGroupNum = isMultiNode ? MAX_MULTI_SERVER_GROUP_NUM : MAX_GROUP_LIST_NUM;
    if (!IsJsonArrayValid(rankTable, "server_group_list", MIN_GROUP_LIST_NUM, maxGroupNum) ||
        !IsValidCoordinatorGroup(rankTable) || !IsValidServerGroup(rankTable, deployMode, isHeterogeneous)) {
        LOG_E("[%s] [RankTableLoader] Load rank table failed, server group list is invalid.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str());
        return -1;
    }

    try {
        serverNodes = GetServerNodes(rankTable, deployMode, isHeterogeneous);
        if (deployMode == DeployMode::PD_SEPARATE && isHeterogeneous) {
            CheckHeterogeneousServers(serverNodes);
        }
        if (deployMode == DeployMode::PD_SEPARATE && !isHeterogeneous) {
            CheckNonHeterogeneousServers(serverNodes);
        }
        if (deployMode == DeployMode::SINGLE_NODE) {
            CheckServers(serverNodes);
        }
        auto coordinatorNodes = GetCoordinatorNode(rankTable);
        if (coordinatorNodes.empty()) {
            LOG_W("[%s] [RankTableLoader] Coordinator nodes list is empty.",
                GetWarnCode(ErrorType::WARNING, ControllerFeature::RANK_TABLE_LOADER).c_str());
        }
        coordinatorStore->UpdateCoordinators(coordinatorNodes);
        return 0;
    }  catch (const std::exception& e) {
        LOG_E("[%s] [RankTableLoader] Failed to load rank table, error is %s.",
            GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::RANK_TABLE_LOADER).c_str(), e.what());
        return -1;
    }
};

bool CheckFields(const nlohmann::json& data, const std::vector<std::string>& requiredFields)
{
    for (const auto& field : requiredFields) {
        if (!data.contains(field) || data[field].is_null()) {
            return false;
        }
    }
    return true;
}

bool isServicedServerGroup(const nlohmann::json& serverGroupJson)
{
    if (CheckFields(serverGroupJson, {"server_list"})) {
        if (serverGroupJson["server_list"].is_array() && !serverGroupJson["server_list"].empty()) {
            return !CheckFields(serverGroupJson["server_list"][0], {"device"});
        }
    }
    return false;
}

std::vector<std::unique_ptr<InstanceInfo>> RankTableLoader::GetInstanceInfoListByRankTable() const
{
    std::vector<std::unique_ptr<InstanceInfo>> instanceInfoList;
    auto path = ControllerConfig::GetInstance()->GetGlobalRankTablePath();
    uint32_t mode = (ControllerConfig::GetInstance()->GetCheckMountedFiles()) ? 0640 : 0777;

    std::lock_guard<std::mutex> lock(mutexFile_);
    auto rankTable = FileToJsonObj(path, mode, (ControllerConfig::GetInstance()->GetCheckMountedFiles()));
    auto serverGroupList = rankTable.at("server_group_list");
    for (auto serverGroup : std::as_const(serverGroupList)) {
        if (!serverGroup.is_object()) {
            LOG_W("[RankTableLoader] Invalid serverGroup structure.");
            continue;
        }
        if (isServicedServerGroup(serverGroup)) {
            continue;
        }
        const std::vector<std::string> serverGroupRequired = {"deploy_server", "server_list"};
        if (!CheckFields(serverGroup, serverGroupRequired)) {
            LOG_W("[RankTableLoader] Missing required fields in serverGroup: deploy_server or server_list.");
            continue;
        }
        InstanceInfo instanceInfo {};
        auto deployServer = std::stoi(serverGroup["deploy_server"].get<std::string>());
        instanceInfo.role = ControllerConfig::GetInstance()->GetPDInitRole(deployServer);
        auto serverList = serverGroup["server_list"];

        size_t serverHashID = 0;
        for (auto it = serverList.begin(); it != serverList.end(); ++it) {
            const std::vector<std::string> serverRequired = {"server_id"};
            if (!CheckFields(*it, serverRequired)) {
                LOG_W("[RankTableLoader] Missing server_id in server entry.");
                continue;
            }
            instanceInfo.serverInfoList.push_back(it->at("server_id").get<std::string>());
            serverHashID ^= std::hash<std::string>{}(it->at("server_id").get<std::string>());
        }
        instanceInfo.hashID = serverHashID;
        instanceInfoList.push_back(std::make_unique<InstanceInfo>(instanceInfo));
    }
    LOG_D("[RankTableLoader] Get instance information list by ranktable successfully, total %zu pods.",
        instanceInfoList.size());
    return instanceInfoList;
}

std::vector<std::unique_ptr<PodInfo>> GetPDPodInfoListByRankTable(nlohmann::json &serverGroupList)
{
    std::vector<std::unique_ptr<PodInfo>> podInfoList;
    for (auto serverGroup : std::as_const(serverGroupList)) {
        if (!serverGroup.is_object()) {
            LOG_I("[RankTableLoader] Invalid serverGroup structure.");
            continue;
        }

        const std::vector<std::string> serverGroupRequired = {"group_id", "server_list"};
        if (!CheckFields(serverGroup, serverGroupRequired)) {
            LOG_I("[RankTableLoader] Missing required fields in serverGroup: group_id or server_list.");
            continue;
        }
        auto groupId = serverGroup["group_id"].get<std::string>();
        auto serverList = serverGroup["server_list"];

        for (auto it = serverList.begin(); it != serverList.end(); ++it) {
            const std::vector<std::string> serverRequired = {"server_ip", "server_id"};
            if (!CheckFields(*it, serverRequired)) {
                LOG_I("[RankTableLoader] Missing server_ip or server_id in server entry.");
                continue;
            }

            PodInfo podInfo {};
            podInfo.podIP = it->at("server_ip").get<std::string>();
            podInfo.hostIP = it->at("server_id").get<std::string>();
            podInfo.groupId = groupId;

            if (it->contains("device")) {
                for (auto &deviceInfo : it->at("device")) {
                    const std::vector<std::string> deviceRequired = {"device_id", "device_ip"};
                    if (!CheckFields(deviceInfo, deviceRequired)) {
                        LOG_I("[RankTableLoader] Missing device_id or device_ip in device info.");
                        continue;
                    }

                    NPUInfo npuInfo {};
                    npuInfo.npuID = deviceInfo["device_id"].get<std::string>();
                    npuInfo.npuIP = deviceInfo["device_ip"].get<std::string>();
                    podInfo.podAssociatedInfoList.emplace_back(npuInfo);
                }
            }
            podInfoList.push_back(std::make_unique<PodInfo>(podInfo));
        }
    }
    LOG_I("[RankTableLoader] GetPDPodInfoListByRankTable: parse ranktable success, total %zu pods.",
        podInfoList.size());
    return podInfoList;
}

// 通过读ranktable文件，获取pod信息
std::vector<std::unique_ptr<PodInfo>> RankTableLoader::GetPDPodInfoList() const
{
    auto path = ControllerConfig::GetInstance()->GetGlobalRankTablePath();
    uint32_t mode = (ControllerConfig::GetInstance()->GetCheckMountedFiles()) ? 0640 : 0777; // 权限要求是0640, 不校验是0777
    std::lock_guard<std::mutex> lock(mutexFile_);
    auto rankTable = FileToJsonObj(path, mode, (ControllerConfig::GetInstance()->GetCheckMountedFiles()));
    if (rankTable.empty()) {
        LOG_E("[%s] [RankTableLoader] Read global rank table file %s failed.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str(),
            path.c_str());
        return {};
    }
    // 文件格式检查
    bool isHeterogeneous = ControllerConfig::GetInstance()->GetDIGSIsHeterogeneous();
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    bool isMultiNode = ControllerConfig::GetInstance()->IsMultiNodeMode();
    uint64_t maxGroupNum = isMultiNode ? MAX_MULTI_SERVER_GROUP_NUM : MAX_GROUP_LIST_NUM;
    if (!IsJsonArrayValid(rankTable, "server_group_list", MIN_GROUP_LIST_NUM, maxGroupNum) ||
        !IsValidCoordinatorGroup(rankTable) || !IsValidServerGroup(rankTable, deployMode, isHeterogeneous)) {
        LOG_E("[%s] [RankTableLoader] Load rank table failed, server group list is invalid.",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::RANK_TABLE_LOADER).c_str());
        return {};
    }
    // 跨机与非跨机的ranktable都使用同一套遍历方式获取pod信息，是兼容的
    auto serverGroupList = rankTable.at("server_group_list");
    return GetPDPodInfoListByRankTable(serverGroupList);
}

// g_startIdNumber作为全局变量，在controller重调度之后会被重置，需序列化保存，
// 在controller重启后反序列化恢复
int32_t RankTableLoader::GetInstanceStartIdNumber() const
{
    std::lock_guard<std::mutex> lock(mutexFile_);
    return g_startIdNumber;
}

void RankTableLoader::SetInstanceStartIdNumber(int32_t startId) const
{
    std::lock_guard<std::mutex> lock(mutexFile_);
    g_startIdNumber = startId;
}

/**
 * 作用：恢复全局变量ipToIdMap的信息，避免id重复分配
 * 在一些极端情况下，此函数会生效，例如：controller重调度或主备切换与缩P保D同时发生：此时recover回来的节点
 * 并不完整，可能只包含部分节点信息，而当前集群（GRT）中还存在其他节点，这些节点可能是扩容回来的节点，并没有被保存，
 * 需要根据当前集群中的节点信息，恢复ipToIdMap，避免id重复分配，跳过所有recover回来的节点
 **/
void RankTableLoader::UpdateIpToIdMapByInstanceStartNumber(std::vector<std::unique_ptr<NodeInfo>> &recoverNodes,
    std::vector<std::unique_ptr<NodeInfo>> &currentAllNodes) const
{
    // 遍历currentAllNodes，排除recoverNodes中的节点
    for (const auto& currentNode : currentAllNodes) {
        bool found = false;
        for (const auto& recoverNode : recoverNodes) {
            if (currentNode->ip == recoverNode->ip) {
                found = true;
                break;
            }
        }
        if (!found) {
            ipToIdMap[currentNode->ip + currentNode->mgmtPort] = g_startIdNumber++;
            LOG_D("[RankTableLoader] UpdateIpToIdMapByInstanceStartNumber: update node ip %s id to %d.",
                currentNode->ip.c_str(), g_startIdNumber - 1);
        }
    }
}

}