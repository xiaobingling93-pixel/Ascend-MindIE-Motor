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
#ifndef MINDIE_DIGS_SCHEDULER_H
#define MINDIE_DIGS_SCHEDULER_H

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace MINDIE::MS {
constexpr size_t FLEX_INSTANCE_P_PERCENTAGE_MAX = 100; // FLEX实例中p_percentage字段最大值

enum class DIGSInstanceLabel {
    PREFILL_PREFER, // 倾向于Prefill实例
    DECODE_PREFER,  // 倾向于Decode实例
    PREFILL_STATIC, // 只允许作为Prefill实例
    DECODE_STATIC,  // 只允许作为Decode实例
    FLEX_STATIC,      // 只允许做为Flex实例
};

enum class DIGSReqStage {
    PREFILL,
    DECODE,
};

enum class DIGSInstanceRole : char {
    UN_DEF_INSTANCE = 'U',
    PREFILL_INSTANCE = 'P',
    DECODE_INSTANCE = 'D',
    FLEX_INSTANCE = 'F',
};

enum class DIGSRatioType {
    DIGS_ROLE_PD_RATIO = 0,
    DIGS_ROLE_PDFLEX_RATIO,
    DIGS_ROLE_UNKNOWN,
};

enum class DIGSRoleChangeType {
    DIGS_ROLE_CHANGE_P2D,
    DIGS_ROLE_CHANGE_D2P,
    DIGS_ROLE_CHANGE_UNKNOWN,
};

struct DIGSRoleDecision {
    uint64_t id;
    uint64_t groupId;
    uint64_t flexPRatio;
    DIGSInstanceRole role = DIGSInstanceRole::UN_DEF_INSTANCE;
};

struct DIGSNodeStaticResource {
    std::string hardwareType;
    uint64_t npuMem = 0;
    uint64_t npuBW = 0;
    uint64_t cpuMem = 0;
};

struct DIGSInstanceStaticInfo {
    uint64_t id;
    uint64_t groupId;
    uint64_t flexPRatio;
    DIGSInstanceRole role = DIGSInstanceRole::UN_DEF_INSTANCE;
    // 服务信息，身份决策时不需要设置
    size_t maxSeqLen = 2048;
    size_t maxOutputLen = 512;
    size_t totalSlotsNum = 200; // 一般为实例的maxBatchSize
    size_t totalBlockNum = 1024;
    size_t blockSize = 128;
    DIGSInstanceLabel label = DIGSInstanceLabel::PREFILL_PREFER;
    // 节点信息
    DIGSNodeStaticResource nodeRes; // 节点平均值
    uint64_t maxConnectionNum = 2000;
    uint64_t virtualId = 1;
};

struct DIGSInstanceDynamicInfo {
    uint64_t id;
    size_t availSlotsNum = 200;
    size_t availBlockNum = 1024;
    size_t totalSlotsNum = 200;
    size_t totalBlockNum = 1024;
    size_t maxAvailBlockNum = 1024;
    std::vector<uint64_t> prefixHash {};
    std::vector<uint64_t> peers {}; // 当实例为D实例时，Group内与其连接的P实例id
    size_t waitingRequestNum = 0;
    size_t runningRequestNum = 0;
    size_t swappedRequestNum = 0;
    size_t freeNpuBlockNums = 0;
    size_t freeCpuBlockNums = 0;
    size_t totalNpuBlockNums = 0;
    size_t totalCpuBlockNums = 0;
};

struct DIGSInstanceScheduleInfo {
    uint64_t id;
    size_t allocatedSlots = 0;
    size_t allocatedBlocks = 0;
};

struct DIGSInstanceInfo {
    DIGSInstanceStaticInfo staticInfo;
    DIGSInstanceDynamicInfo dynamicInfo;
    DIGSInstanceScheduleInfo scheduleInfo;
};

struct DIGSRequestSummary {
    size_t inputLength = 0;
    size_t outputLength = 0;
};

struct DIGSGroupPDRatio {
    uint64_t groupId = 0;
    double pdRatio = 0.0;
    double pAbility = 0.0;
    double dAbility = 0.0;
    double tAbility = 0.0;
    size_t pNum = 0;
    size_t dNum = 0;
    size_t flexNum = 0;
    double flexPRatio = 0.0;
};

} // namespace MINDIE::MS
#endif
