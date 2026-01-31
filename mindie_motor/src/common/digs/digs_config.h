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
#ifndef MINDIE_DIGS_CONFIG_H
#define MINDIE_DIGS_CONFIG_H

#include "common.h"

namespace MINDIE::MS {

struct SchedulerConfig {
    size_t maxScheduleCount = 1000;
    int32_t reorderingType = 1;
    int32_t selectType = 1;
    int32_t poolType = 1;
    size_t blockSize = 128;
};

struct RequestConfig {
    uint64_t pullRequestTimeout = 500;
    size_t maxSummaryCount = common::DEFAULT_MAX_SUMMARY_COUNT;
};

struct ResourceConfig {
    size_t maxResNum = 5000;
    uint64_t resViewUpdateTimeout = 500;
    double resLimitRate = 1.0;
    std::string metaResourceNames = ""; // 逗号分隔
    std::string metaResourceValues = "1,1";
    std::string metaResourceWeights = "0,0.22,1024,24,6,0,1,0,1";
    bool dynamicMaxResEnable = false;
    size_t maxDynamicResRateCount = 3;
    double dynamicResRateUnit = 0.1;
};
}
#endif
