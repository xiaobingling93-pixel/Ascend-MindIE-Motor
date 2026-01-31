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
#include "static_pool_policy.h"

namespace MINDIE::MS {

int32_t StaticPoolPolicy::ScheduleInst(const std::unique_ptr<ResourceViewManager>& resView)
{
    auto& globalPool = resView->GlobalPool();
    if (globalPool.empty()) {
        return static_cast<int32_t>(common::Status::OK);
    }
    for (auto iter = globalPool.begin(); iter != globalPool.end();) {
        ptrdiff_t nextOffset = 0;
        auto groupId = iter->first->StaticInfo().groupId;
        ResourceViewManager::ResInfo resInfo;
        switch (iter->first->StaticInfo().label) {
            case DIGSInstanceLabel::PREFILL_STATIC:
            case DIGSInstanceLabel::PREFILL_PREFER:
                nextOffset = resView->GlobalPoolRemoveAndNext(iter - globalPool.begin(), resInfo);
                resInfo.first->ScheduleInfo()->UpdateDuty(InstanceDuty::PREFILLING);
                resView->Add2PrefillPool(std::move(resInfo));
                iter = globalPool.begin() + nextOffset;
                break;
            case DIGSInstanceLabel::DECODE_STATIC:
            case DIGSInstanceLabel::DECODE_PREFER:
                nextOffset = resView->GlobalPoolRemoveAndNext(iter - globalPool.begin(), resInfo);
                resInfo.first->ScheduleInfo()->UpdateDuty(InstanceDuty::DECODING);
                resView->Add2DecodePool(std::move(resInfo), groupId);
                iter = globalPool.begin() + nextOffset;
                break;
            default:
                LOG_W("[%s] [DIGS] Unsupported instance label when schedule instance, ID is %d label is %d",
                      GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                      iter->first->StaticInfo().id,
                      static_cast<int32_t>(iter->first->StaticInfo().label));
                ++iter;
                break;
        }
    }
    return static_cast<int32_t>(common::Status::OK);
}
}