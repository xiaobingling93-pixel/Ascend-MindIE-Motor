/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <thread>
#include "Logger.h"
#include "AlarmManager.h"
#include "AlarmRequestHandler.h"
#include "ResourceManager.h"

namespace MINDIE::MS {
void ResourceManager::Init(std::vector<std::unique_ptr<InstanceInfo>>& instanceInfoList)
{
    mPInstanceData = InstanceData();
    mDInstanceData = InstanceData();
    mPInstanceNewID = 0;
    mDInstanceNewID = 0;

    for (auto& info : instanceInfoList) {
        if (info == nullptr) {
            continue;
        }
        auto& data = (info->role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ? mPInstanceData : mDInstanceData;
        auto& newID = (info->role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE)
                      ? mPInstanceNewID : mDInstanceNewID;
        data.instanceTable[newID] = info->serverInfoList;
        data.instanceStatus[newID] = InstanceStatus::ACTIVE;
        data.instanceHash[info->hashID] = newID;
        data.instanceLogicIds.push_back(newID);
        newID++;
    }
    LOG_I("[ResourceManager] Successfully initialized the instance table.");
}

void ResourceManager::InitInstanceStatus(std::unordered_map<uint64_t, InstanceStatus>& instanceStatus) const
{
    for (auto& instance : instanceStatus) {
        instance.second = InstanceStatus::UNKNOWN;
    }
}

std::vector<uint64_t> ResourceManager::GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole role) const
{
    return (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ?
        mPInstanceData.instanceLogicIds : mDInstanceData.instanceLogicIds;
}

std::vector<std::string> ResourceManager::GetInstanceAllServerIP(MINDIE::MS::DIGSInstanceRole role,
    uint64_t instanceLogicId) const
{
    std::string instanceID = ((role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ? "P_" : "D_")
        + std::to_string(instanceLogicId);
    auto& instanceTable = (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ?
        mPInstanceData.instanceTable : mDInstanceData.instanceTable;
    auto iter = instanceTable.find(instanceLogicId);
    if (iter == instanceTable.end()) {
        LOG_E("[%s] [ResourceManager] The Id of instance %s is not found in IP table.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::RESOURCE_MANAGER).c_str(),
            instanceID.c_str());
        return {};
    }
    return iter->second;
}

InstanceStatus ResourceManager::GetInstanceStatus(MINDIE::MS::DIGSInstanceRole role, uint64_t instanceLogicId) const
{
    std::string instanceID = ((role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ? "P_" : "D_")
        + std::to_string(instanceLogicId);
    auto& instanceStatus = (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ?
        mPInstanceData.instanceStatus : mDInstanceData.instanceStatus;
    auto iter = instanceStatus.find(instanceLogicId);
    if (iter == instanceStatus.end() || iter->second == InstanceStatus::UNKNOWN) {
        LOG_E("[%s] [ResourceManager] Unable to get the status of instance %s.",
            GetErrorCode(ErrorType::NOT_FOUND, ControllerFeature::RESOURCE_MANAGER).c_str(),
            instanceID.c_str());
        return {};
    }
    return iter->second;
}

void SetInstanceStatusToFault(
    MINDIE::MS::DIGSInstanceRole role,
    std::unordered_map<uint64_t, InstanceStatus>& instanceStatus)
{
    for (auto& [index, status] : instanceStatus) {
        if (status == InstanceStatus::UNKNOWN) {
            status = InstanceStatus::FAULT;
            std::string instanceID = ((role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ? "P_" : "D_")
                + std::to_string(index);
            LOG_W("[ResourceManager] %s status set to FAULT (unmatched).", instanceID.c_str());
        }
    }
}

void SendAlarm(MINDIE::MS::DIGSInstanceRole role, AlarmCategory category, uint64_t index)
{
    std::string instanceID = ((role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ? "P_" : "D_")
        + std::to_string(index);
    std::string alarmMsg = AlarmRequestHandler::GetInstance()->FillInstanceExceptionAlarmInfo(
        category, instanceID, InstanceExceptionReason::INSTANCE_EXCEPTION);
    AlarmManager::GetInstance()->AlarmAdded(alarmMsg); // 原始告警直接入队
    LOG_D("[ResourceManager] Add %s alarm successfully. Instance ID: %s",
        (category == AlarmCategory::ALARM_CATEGORY_ALARM) ? "new" : "cleared", instanceID.c_str());
}

void ResourceManager::UpdateInstanceInfo(MINDIE::MS::DIGSInstanceRole role, InstanceData newData)
{
    LOG_D("[ResourceManager] Update %s instance table start.",
        (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ? "P" : "D");
    auto& targetData = (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ?
        mPInstanceData : mDInstanceData;
    for (const auto& [index, newStatus] : newData.instanceStatus) {
        auto oldIt = targetData.instanceStatus.find(index);
        if (oldIt == targetData.instanceStatus.end()) {
            LOG_W("[ResourceManager] The Id of instance %lu is not found in old table.", index);
            continue;
        }

        const auto& oldStatus = oldIt->second;
        if (oldStatus == InstanceStatus::FAULT && newStatus == InstanceStatus::ACTIVE) {
            SendAlarm(role, AlarmCategory::ALARM_CATEGORY_CLEAR, index);
        } else if (oldStatus == InstanceStatus::ACTIVE && newStatus == InstanceStatus::FAULT) {
            SendAlarm(role, AlarmCategory::ALARM_CATEGORY_ALARM, index);
        }
    }
    targetData = newData;
    LOG_D("[ResourceManager] Update %s instance table end.",
        (role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ? "P" : "D");
}

void MatchInstanceInfo(InstanceData& targetInstanceData, std::unique_ptr<InstanceInfo>& instanceInfo,
    std::vector<std::unique_ptr<InstanceInfo>>& unMatchedInstanceList)
{
    if (instanceInfo == nullptr) {
        return;
    }
    auto it = targetInstanceData.instanceHash.find(instanceInfo->hashID);
    if (it != targetInstanceData.instanceHash.end()) {
        targetInstanceData.instanceStatus[it->second] = InstanceStatus::ACTIVE;
        std::string instanceID = ((instanceInfo->role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ? "P_" : "D_")
            + std::to_string(it->second);
        LOG_D("[ResourceManager] Instance %s status set to ACTIVE.", instanceID.c_str());
    } else {
        unMatchedInstanceList.push_back(std::move(instanceInfo));
    }
}

void ResourceManager::HandleUnmatchedInstanceInfo(std::queue<uint64_t>& targetUnknownLogicIds,
    InstanceData& targetInstanceData, const std::unique_ptr<InstanceInfo>& instanceInfo)
{
    if (!targetUnknownLogicIds.empty()) {
        uint64_t reuseIndex = targetUnknownLogicIds.front();
        targetUnknownLogicIds.pop();
        if (instanceInfo == nullptr) {
            LOG_E("[%s] [ResourceManager] InstanceInfo is null! Aborting reuse.",
                GetErrorCode(ErrorType::CALL_ERROR, ControllerFeature::RESOURCE_MANAGER).c_str());
            return;
        }
        targetInstanceData.instanceTable[reuseIndex] = instanceInfo->serverInfoList;
        targetInstanceData.instanceStatus[reuseIndex] = InstanceStatus::ACTIVE;
        for (auto it = targetInstanceData.instanceHash.begin(); it != targetInstanceData.instanceHash.end(); ++it) {
            if (it->second == reuseIndex) {
                it = targetInstanceData.instanceHash.erase(it);
                targetInstanceData.instanceHash[instanceInfo->hashID] = reuseIndex;
                break;
            }
        }
        LOG_D("[ResourceManager] Reused UNKNOWN instance %lu for new %c-role instance",
            reuseIndex, instanceInfo->role);
    } else {
        auto& newID = (instanceInfo->role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ?
            mPInstanceNewID : mDInstanceNewID;
        targetInstanceData.instanceTable[newID] = instanceInfo->serverInfoList;
        targetInstanceData.instanceStatus[newID] = InstanceStatus::ACTIVE;
        targetInstanceData.instanceHash[instanceInfo->hashID] = newID;
        targetInstanceData.instanceLogicIds.push_back(newID);
        LOG_D("[ResourceManager] Created new %c-role instance %lu",
            instanceInfo->role, newID);
        newID++;
    }
}

void ResourceManager::UpdateInstanceTable(std::vector<std::unique_ptr<InstanceInfo>>& instanceInfoList)
{
    InstanceData tmpPInstanceData = mPInstanceData;
    InstanceData tmpDInstanceData = mDInstanceData;

    std::vector<std::unique_ptr<InstanceInfo>> unMatchedInstanceList {};
    std::queue<uint64_t> unknownPInstanceLogicIds = {};
    std::queue<uint64_t> unknownDInstanceLogicIds = {};

    InitInstanceStatus(tmpPInstanceData.instanceStatus);
    InitInstanceStatus(tmpDInstanceData.instanceStatus);
    
    LOG_D("[ResourceManager] Match instance information start.");
    for (auto& instanceInfo : instanceInfoList) {
        if (instanceInfo == nullptr) {
            continue;
        }
        MatchInstanceInfo(((instanceInfo->role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ?
            tmpPInstanceData : tmpDInstanceData), instanceInfo, unMatchedInstanceList);
    }
    LOG_D("[ResourceManager] Match instance information end.");

    if (unMatchedInstanceList.empty()) {
        SetInstanceStatusToFault(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, tmpPInstanceData.instanceStatus);
        SetInstanceStatusToFault(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, tmpDInstanceData.instanceStatus);
        UpdateInstanceInfo(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, tmpPInstanceData);
        UpdateInstanceInfo(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, tmpDInstanceData);
        LOG_D("[ResourceManager] Complete instance table update: unmatched instance list is empty.");
        return;
    }

    for (const auto& [index, status] : tmpPInstanceData.instanceStatus) {
        if (status == InstanceStatus::UNKNOWN) {
            unknownPInstanceLogicIds.push(index);
        }
    }
    for (const auto& [index, status] : tmpDInstanceData.instanceStatus) {
        if (status == InstanceStatus::UNKNOWN) {
            unknownDInstanceLogicIds.push(index);
        }
    }

    LOG_D("[ResourceManager] Handle unmatched instance information start.");
    for (const auto& instanceInfo : unMatchedInstanceList) {
        auto& targetUnknownLogicIds = (instanceInfo->role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ?
            unknownPInstanceLogicIds : unknownDInstanceLogicIds;
        auto& targetInstanceData = (instanceInfo->role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) ?
            tmpPInstanceData : tmpDInstanceData;

        HandleUnmatchedInstanceInfo(targetUnknownLogicIds, targetInstanceData, instanceInfo);
    }
    LOG_D("[ResourceManager] Handle unmatched instance information end.");

    SetInstanceStatusToFault(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, tmpPInstanceData.instanceStatus);
    SetInstanceStatusToFault(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, tmpDInstanceData.instanceStatus);
    UpdateInstanceInfo(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, tmpPInstanceData);
    UpdateInstanceInfo(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, tmpDInstanceData);
    LOG_I("[ResourceManager] Complete instance table update.");
    return;
}
}