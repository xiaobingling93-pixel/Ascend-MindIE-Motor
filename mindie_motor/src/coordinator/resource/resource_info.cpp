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
#include "resource_info.h"

namespace MINDIE::MS {

bool ResourceInfo::isDynamicMaxResEnabled_ = false;

ResourceInfo::ResourceInfo(DIGSInstanceStaticInfo insInfo, double maxResRate)
    : instanceStaticInfo_(std::move(insInfo)), maxResRate_(maxResRate), dynamicResRate_(maxResRate)
{
    scheduleInfo_ = std::make_shared<ResScheduleInfo>(instanceStaticInfo_.id, instanceStaticInfo_.label);

    std::vector<uint64_t> resAttr;
    for (auto& name : MetaResource::AttrName()) {
        if (name == "slots") {
            resAttr.push_back(instanceStaticInfo_.totalSlotsNum);
        } else if (name == "blocks") {
            resAttr.push_back(instanceStaticInfo_.totalBlockNum);
        } else if (name == "cpuMem") {
            resAttr.push_back(instanceStaticInfo_.nodeRes.cpuMem);
        } else if (name == "npuMem") {
            resAttr.push_back(instanceStaticInfo_.nodeRes.npuMem);
        } else if (name == "npuBW") {
            resAttr.push_back(instanceStaticInfo_.nodeRes.npuBW);
        } else {
            LOG_W("[%s] [DIGS] Unsupported resource (%s), total resource set to 0",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  name.c_str());
            resAttr.push_back(0);
        }
    }
    totalResource_ = std::make_unique<MetaResource>(std::move(resAttr));
    std::unique_ptr<MetaResource> maxPrefillRes;
    std::unique_ptr<MetaResource> maxDecodeRes;
    MetaResource::ResMul(maxPrefillRes, totalResource_, maxResRate_);
    MetaResource::ResMul(maxDecodeRes, totalResource_, maxResRate_);
    LOG_I("[DIGS] Initialize instance max resource limit, max prefill is %s max decode is %s",
          MINDIE::MS::common::ToStr(*maxPrefillRes).c_str(),
          MINDIE::MS::common::ToStr(*maxDecodeRes).c_str());
    scheduleInfo_->SetMaxRes(std::move(maxPrefillRes), std::move(maxDecodeRes));
    scheduleInfo_->UpdateRole(instanceStaticInfo_.role);
}

void ResourceInfo::AddDemand(const std::unique_ptr<MetaResource>& demand, MINDIE::MS::DIGSReqStage stage)
{
    scheduleInfo_->AddDemand(demand, stage);
}

bool ResourceInfo::UpdateScheduleLoad(bool isResAvailable)
{
    if (!isDynamicMaxResEnabled_) {
        return false;
    }

    scheduleInfo_->CountAllocateUnmatch(isResAvailable);
    return true;
}

bool ResourceInfo::ReviseMaxResource()
{
    if (!isDynamicMaxResEnabled_) {
        return false;
    }

    if (scheduleInfo_->GenerateDynamicResRate(dynamicResRate_)) {
        std::unique_ptr<MetaResource> maxPrefillRes;
        std::unique_ptr<MetaResource> maxDecodeRes;
        MetaResource::ResMul(maxPrefillRes, totalResource_, dynamicResRate_);
        MetaResource::ResMul(maxDecodeRes, totalResource_, dynamicResRate_);
        LOG_I("DIGS: revise instance max resource limit, resId: %d max prefill: %s max decode: %s",
              instanceStaticInfo_.id,
              MINDIE::MS::common::ToStr(*maxPrefillRes).c_str(),
              MINDIE::MS::common::ToStr(*maxDecodeRes).c_str());
        scheduleInfo_->SetMaxRes(std::move(maxPrefillRes), std::move(maxDecodeRes));
        return true;
    }
    return false;
}

void ResourceInfo::UpdateStaticInfo(const DIGSInstanceDynamicInfo& dynaInfo)
{
    if (instanceStaticInfo_.totalSlotsNum != dynaInfo.totalSlotsNum ||
        instanceStaticInfo_.totalBlockNum != dynaInfo.totalBlockNum) {
        LOG_I("[DIGS] Updated instance staticinfo, totalSlotsNum: %zu → %zu, totalBlockNum: %zu → %zu",
            instanceStaticInfo_.totalSlotsNum, dynaInfo.totalSlotsNum,
            instanceStaticInfo_.totalBlockNum, dynaInfo.totalBlockNum);
    }
    instanceStaticInfo_.totalSlotsNum = dynaInfo.totalSlotsNum;
    instanceStaticInfo_.totalBlockNum = dynaInfo.totalBlockNum;
}

}
