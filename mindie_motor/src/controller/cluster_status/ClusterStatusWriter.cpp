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
#include "ClusterStatusWriter.h"
#include <fstream>
#include <map>
#include "Logger.h"
#include "Util.h"
#include "ConfigParams.h"
#include "ControllerConfig.h"
namespace MINDIE::MS {

int32_t ClusterStatusWriter::SaveStatusToFile(const std::shared_ptr<NodeStatus> &nodeStatus,
    const std::shared_ptr<CoordinatorStore> &coordinatorStore) const
{
    try {
        return DumpStringToFile(ControllerConfig::GetInstance()->GetClusterStatusConfig().filePath,
            GenerateClusterInfo(nodeStatus, coordinatorStore).dump(4)); // 4 json格式行缩进为4
    }  catch (const std::exception& e) {
        LOG_E("[%s] [ClusterStatusWriter] Save Status to file failed, error is %s.",
              GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::CLUSTER_STATUS_WRITER).c_str(),
              e.what());
        return -1;
    }
}

void ClusterStatusWriter::GenerateSingleServerInfo(const std::shared_ptr<NodeStatus> &nodeStatus, nlohmann::json &node,
    const NodeInfo &nodeInfo, bool isFaulty) const
{
    node["delete_time"] = std::chrono::duration_cast<std::chrono::seconds>(nodeInfo.deleteTime).count();
    node["ip"] = nodeInfo.ip;
    node["is_healthy"] = nodeInfo.isHealthy;
    node["model_name"] = nodeInfo.modelName;
    nlohmann::json staticInfo;
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
    for (auto &peer : nodeInfo.peers) {
        auto ip = nodeStatus->GetIpForAllTypeNodes(peer);
        if (ip.empty()) {
            LOG_E("[%s] [ClusterStatusWriter] Generating single server information, ID %lu failed to get IP.",
                GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::CLUSTER_STATUS_WRITER).c_str(),
                nodeInfo.instanceInfo.staticInfo.id);
            continue;
        }
        node["peers"].emplace_back(std::move(ip));
    }
    nlohmann::json dynamicInfo;
    dynamicInfo["avail_slots_num"] = nodeInfo.instanceInfo.dynamicInfo.availSlotsNum;
    dynamicInfo["avail_block_num"] = nodeInfo.instanceInfo.dynamicInfo.availBlockNum;
    dynamicInfo["waiting_request_num"] = nodeInfo.instanceInfo.dynamicInfo.waitingRequestNum;
    dynamicInfo["running_request_num"] = nodeInfo.instanceInfo.dynamicInfo.runningRequestNum;
    dynamicInfo["swapped_request_num"] = nodeInfo.instanceInfo.dynamicInfo.swappedRequestNum;
    dynamicInfo["free_npu_block_nums"] = nodeInfo.instanceInfo.dynamicInfo.freeNpuBlockNums;
    dynamicInfo["free_cpu_block_nums"] = nodeInfo.instanceInfo.dynamicInfo.freeCpuBlockNums;
    dynamicInfo["total_npu_block_nums"] = nodeInfo.instanceInfo.dynamicInfo.totalNpuBlockNums;
    dynamicInfo["total_cpu_block_nums"] = nodeInfo.instanceInfo.dynamicInfo.totalCpuBlockNums;
    node["dynamic_info"] = dynamicInfo;
    node["is_faulty"] = isFaulty;
    LOG_D("[ClusterStatusWriter] Generating single server information: ID %lu, role %c.",
        nodeInfo.instanceInfo.staticInfo.id, nodeInfo.instanceInfo.staticInfo.role);
}

void ClusterStatusWriter::GenerateServerInfo(const std::shared_ptr<NodeStatus> &nodeStatus,
    nlohmann::json &file) const
{
    file["server"] = nlohmann::json::array();
    auto allNodes = nodeStatus->GetAllNodes();
    LOG_D("[ClusterStatusWriter] Generating server information, %zu usable server nodes.", allNodes.size());
    for (auto &it : std::as_const(allNodes)) {
        nlohmann::json node;
        GenerateSingleServerInfo(nodeStatus, node, *(it.second), false);
        file["server"].emplace_back(node);
    }
    auto allFaultyNodes = nodeStatus->GetAllFaultyNodes();
    LOG_D("[ClusterStatusWriter] Generating server information, %zu faulty server nodes.", allFaultyNodes.size());
    for (auto &it : std::as_const(allFaultyNodes)) {
        nlohmann::json node;
        GenerateSingleServerInfo(nodeStatus, node, *(it.second), true);
        file["server"].emplace_back(node);
    }
}

void ClusterStatusWriter::GenerateCoordinatorInfo(const std::shared_ptr<CoordinatorStore> &coordinatorStore,
    nlohmann::json &file) const
{
    file["coordinator"] = nlohmann::json::array();
    auto allNodes = coordinatorStore->GetCoordinators();
    LOG_D("[ClusterStatusWriter] Generating coordinator information, %zu coordinator nodes", allNodes.size());
    for (auto &coordinator : std::as_const(allNodes)) {
        nlohmann::json node;
        node["ip"] = coordinator->ip;
        node["is_healthy"] = coordinator->isHealthy;
        file["coordinator"].emplace_back(node);
        LOG_D("[ClusterStatusWriter] Generating coordinator information, IP %s, healthy %d.", coordinator->ip.c_str(),
              coordinator->isHealthy);
    }
}

nlohmann::json ClusterStatusWriter::GenerateClusterInfo(const std::shared_ptr<NodeStatus> &nodeStatus,
    const std::shared_ptr<CoordinatorStore> &coordinatorStore) const
{
    nlohmann::json file;
    GenerateServerInfo(nodeStatus, file);
    GenerateCoordinatorInfo(coordinatorStore, file);
    return file;
}

void ClusterStatusWriter::Wait()
{
    auto waitSeconds = ControllerConfig::GetInstance()->GetClusterSynchronizationSeconds();
    mWaitSeconds.store(waitSeconds);
    while (mWaitSeconds.load() > 0 && mRun.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 1s检查一次
        mWaitSeconds--;
    }
}

int32_t ClusterStatusWriter::Init(const std::shared_ptr<NodeStatus> &nodeStatus,
    const std::shared_ptr<CoordinatorStore> &coordinatorStore)
{
    if (!ControllerConfig::GetInstance()->GetClusterStatusConfig().toFile) {
        LOG_D("[ClusterStatusWriter] To file is false, do not write cluster status.");
        return 0;
    }
    LOG_D("[ClusterStatusWriter] To file is true, write cluster status.");
    try {
        mMainThread = std::make_unique<std::thread>([this, nodeStatus, coordinatorStore]() {
            while (mRun.load()) {
                LOG_D("[ClusterStatusWriter] Running start.");
                if (GetInstance()->SaveStatusToFile(nodeStatus, coordinatorStore) != 0) {
                    LOG_E("[%s] [ClusterStatusWriter] Run: save status to file failed",
                          GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::CLUSTER_STATUS_WRITER).c_str());
                };
                LOG_D("[ClusterStatusWriter] Running end.");
                Wait();
            }
        });
    } catch (const std::exception& e) {
        LOG_E("[%s] [ClusterStatusWriter] Initialize failed, failed to create main thread.",
              GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, ControllerFeature::CLUSTER_STATUS_WRITER).c_str());
        return -1;
    }
    return 0;
}

void ClusterStatusWriter::Stop()
{
    mRun.store(false);
    if (mMainThread != nullptr && mMainThread->joinable()) {
        mMainThread->join();
    }
    LOG_D("[ClusterStatusWriter]Stop: stop successfully");
}
}