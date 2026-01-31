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
#include <utility>
#include "resource_manager.h"

namespace MINDIE::MS {

ResourceManager::ResourceManager(ResourceConfig& config)
    : resViewUpdateTimeout_(config.resViewUpdateTimeout), maxResNum_(config.maxResNum),
      resLimitRate_(config.resLimitRate)
{
    LOG_I("[DIGS] Initialize resource manager, max number of resources is %zu", maxResNum_);
    resViewMgr_ = std::make_unique<ResourceViewManager>(maxResNum_);
    MetaResource::InitAttrs(config.metaResourceNames, config.metaResourceValues, config.metaResourceWeights);
}

int32_t ResourceManager::Create(std::shared_ptr<ResourceManager>& resMgr, ResourceConfig& config)
{
    resMgr = std::make_shared<ResourceManager>(config);
    ResourceInfo::SetDynamicMaxResEnable(config.dynamicMaxResEnable);
    ResScheduleInfo::SetDynamicResRateConfig(config.maxDynamicResRateCount, config.dynamicResRateUnit);
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ResourceManager::RegisterInstance(const std::vector<DIGSInstanceStaticInfo>& instances)
{
    int32_t failedCount = 0;
    for (auto& ins : instances) {
        LOG_I("[DIGS] Register instance, ID is %lu", ins.id);
        // copy instance static info to resource info
        auto resInfo = std::make_shared<ResourceInfo>(ins, resLimitRate_);
        // create empty resource load here
        auto resLoad = std::make_shared<ResourceLoad>();
        bool success;
        {
            // write lock
            std::unique_lock<std::shared_timed_mutex> lock(resMapMutex_);
            auto result = resourceMap_.insert(
                std::make_pair(ins.id, std::make_pair(std::move(resInfo), std::move(resLoad))));
            success = result.second;
        }
        if (!success) {
            LOG_E("[%s] [DIGS] Register instance failed, ID is %lu",
                  GetErrorCode(ErrorType::CALL_ERROR, CommonFeature::DIGS).c_str(),
                  ins.id);
            failedCount++;
        }
    }
    return failedCount;
}

int32_t ResourceManager::UpdateInstance(const std::vector<DIGSInstanceDynamicInfo>& instances)
{
    int32_t failedCount = 0;
    for (auto ins : instances) {
        LOG_D("[DIGS] Update instance, ID is %lu", ins.id);
        // copy instance dynamic info to resource load
        auto resLoad = std::make_shared<ResourceLoad>(ins);
        auto isResAvailable = resLoad->IsResAvailable();
        bool success = true;
        {
            // write lock
            std::unique_lock<std::shared_timed_mutex> lock(resMapMutex_);
            auto iter = resourceMap_.find(ins.id);
            if (iter != resourceMap_.end()) {
                iter->second.second = std::move(resLoad);
                // 更新实际负载到调度信息中
                iter->second.first->UpdateStaticInfo(ins);
                iter->second.first->UpdateScheduleLoad(isResAvailable);
            } else {
                success = false;
            }
        }
        if (!success) {
            LOG_W("[%s] [DIGS] Update instance failed, ID is %lu",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  ins.id);
            failedCount++;
        }
    }
    return failedCount;
}

int32_t ResourceManager::RemoveInstance(const std::vector<uint64_t>& instances)
{
    int32_t failedCount = 0;
    for (auto id : instances) {
        LOG_I("[DIGS] Remove instance, ID is %lu", id);
        bool success = false;
        {
            // write lock
            std::unique_lock<std::shared_timed_mutex> lock(resMapMutex_);
            auto it = resourceMap_.find(id);
            if (it != resourceMap_.end()) {
                success = true;
                resourceMap_.erase(id);
            }
        }
        if (!success) {
            LOG_E("[%s] [DIGS] Remove instance failed, ID is %lu",
                  GetErrorCode(ErrorType::NOT_FOUND, CommonFeature::DIGS).c_str(),
                  id);
            failedCount++;
        }
    }
    return failedCount;
}

int32_t ResourceManager::QueryInstanceScheduleInfo(std::vector<DIGSInstanceScheduleInfo>& info)
{
    // read lock
    std::shared_lock<std::shared_timed_mutex> lock(resMapMutex_);
    for (auto& resource : std::as_const(resourceMap_)) {
        auto& prefillDemand = resource.second.first->ScheduleInfo()->PrefillDemands();
        auto& decodeDemand = resource.second.first->ScheduleInfo()->DecodeDemands();
        
        uint64_t prefillSlots = prefillDemand->Slots();
        uint64_t decodeSlots = decodeDemand->Slots();
        uint64_t prefillBlocks = prefillDemand->Blocks();
        uint64_t decodeBlocks = decodeDemand->Blocks();

        // Check Slots overflow
        uint64_t allocatedSlots;
        if (prefillSlots > std::numeric_limits<uint64_t>::max() - decodeSlots) {
            LOG_W("[ResourceManager] Slots overflow detected for instance %lu", resource.first);
            allocatedSlots = std::numeric_limits<uint64_t>::max();
        } else {
            allocatedSlots = prefillSlots + decodeSlots;
        }
        
        // Check Blocks overflow
        uint64_t allocatedBlocks;
        if (prefillBlocks > std::numeric_limits<uint64_t>::max() - decodeBlocks) {
            LOG_W("[ResourceManager] Blocks overflow detected for instance %lu", resource.first);
            allocatedBlocks = std::numeric_limits<uint64_t>::max();
        } else {
            allocatedBlocks = prefillBlocks + decodeBlocks;
        }
        
        auto scheduleInfo = DIGSInstanceScheduleInfo{.id = resource.first,
            .allocatedSlots = allocatedSlots,
            .allocatedBlocks = allocatedBlocks};
        info.push_back(scheduleInfo);
    }
    return static_cast<int32_t>(common::Status::OK);
}

int32_t ResourceManager::CloseInstance(const std::vector<uint64_t>& instances)
{
    int32_t failedCount = 0;
    for (auto& id : instances) {
        LOG_I("[DIGS] Close instance, ID is %lu", id);
        bool success = false;
        {
            // read lock
            std::shared_lock<std::shared_timed_mutex> lock(resMapMutex_);
            auto iter = resourceMap_.find(id);
            if (iter != resourceMap_.end()) {
                iter->second.first->ScheduleInfo()->CloseInstance();
                success = true;
            }
        }
        if (!success) {
            LOG_W("[%s] [DIGS] Close instance failed, ID is %lu",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  id);
            failedCount++;
        }
    }
    return failedCount;
}

int32_t ResourceManager::ActivateInstance(const std::vector<uint64_t>& instances)
{
    int32_t failedCount = 0;
    for (auto& id : instances) {
        LOG_I("[DIGS] Activate instance, ID is %lu", id);
        bool success = false;
        {
            // read lock
            std::shared_lock<std::shared_timed_mutex> lock(resMapMutex_);
            auto iter = resourceMap_.find(id);
            if (iter != resourceMap_.end()) {
                iter->second.first->ScheduleInfo()->ActivateInstance();
                success = true;
            }
        }
        if (!success) {
            LOG_W("[%s] [DIGS] Activate instance failed, ID is %lu",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  id);
            failedCount++;
        }
    }
    return failedCount;
}

int32_t ResourceManager::UpdateResourceView()
{
    // read lock
    std::shared_lock<std::shared_timed_mutex> lock(resMapMutex_, resViewUpdateTimeout_);
    if (lock.owns_lock()) {
        for (auto& resPair : std::as_const(resourceMap_)) {
            if (resPair.second.first->ScheduleInfo()->IsInstanceClosed()) {
                continue;
            }
            resViewMgr_->AddResInfo(resPair.second);
        }
        return static_cast<int32_t>(common::Status::OK);
    }
    return static_cast<int32_t>(common::Status::TIMEOUT);
}

}
