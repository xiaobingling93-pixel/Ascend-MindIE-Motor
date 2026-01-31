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
#include "CCAEStatus.h"
#include <cstdint>
#include <algorithm>

namespace MINDIE::MS {
// 定义合理的最大周期值，避免int64_t回绕
static const int64_t MAX_METRIC_PERIOD = INT64_MAX / 2;
void CCAEStatus::SetForcedUpdate(const std::string modelID, bool isForcedUpdate)
{
    mIsForcedUpdate[modelID] = isForcedUpdate;
}

void CCAEStatus::SetMetricPeriod(const std::string modelID, int64_t metricPeriod)
{
    // 限制metricPeriod的值，避免过大导致回绕
    int64_t safeMetricPeriod = std::min(metricPeriod, MAX_METRIC_PERIOD);
    if (mMetricPeriod.find(modelID) == mMetricPeriod.end() || mMetricPeriod.at(modelID) != safeMetricPeriod) {
        mMetricPeriod[modelID] = safeMetricPeriod;
    }
}

bool CCAEStatus::ISForcedUpdate(const std::string modelID)
{
    if (mIsForcedUpdate.find(modelID) != mIsForcedUpdate.end()) {
        return mIsForcedUpdate.at(modelID);
    }
    return false;
}

int64_t CCAEStatus::GetMetricPeriod(const std::string modelID)
{
    if (mMetricPeriod.find(modelID) != mMetricPeriod.end()) {
        int64_t period = mMetricPeriod.at(modelID);
        // 确保返回的值在安全范围内
        return std::min(period, MAX_METRIC_PERIOD);
    }
    return 0;
}
}