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
#ifndef MINDIE_MS_CCAE_STATUS_H
#define MINDIE_MS_CCAE_STATUS_H
#include <atomic>
#include <unordered_map>
#include <string>

namespace MINDIE::MS {
class CCAEStatus {
public:
    void SetForcedUpdate(const std::string modelID, bool isForcedUpdate);
    void SetMetricPeriod(const std::string modelID, int64_t metricPeriod);
    bool ISForcedUpdate(const std::string modelID);
    int64_t GetMetricPeriod(const std::string modelID);

private:
    std::unordered_map<std::string, bool> mIsForcedUpdate;
    std::unordered_map<std::string, int64_t> mMetricPeriod;
};
}
#endif // MINDIE_MS_CCAE_STATUS_H