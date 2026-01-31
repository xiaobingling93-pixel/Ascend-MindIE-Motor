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
#include "DefaultGroupGenerator.h"
#include "GroupGeneratorFactory.h"
#include "GroupGenerator.h"
#include "Logger.h"
#include "ControllerConfig.h"
namespace MINDIE::MS {
constexpr uint32_t LIMIT_MAX = 16;
constexpr uint32_t DISTRIBUTE_LIMIT_MAX = 768;

static void GetPNodesAndDNodes(std::vector<MINDIE::MS::DIGSRoleDecision> &instances,
                               std::vector<uint64_t> &pIndex, std::vector<uint64_t> &dIndex,
                               std::vector<uint64_t> &flexIndex
                            )
{
    uint32_t instanceSize = instances.size();
    for (uint32_t i = 0; i < instanceSize; i++) {
        if (instances[i].role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
            pIndex.push_back(i);
        } else if (instances[i].role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
            dIndex.push_back(i);
        } else if (instances[i].role == MINDIE::MS::DIGSInstanceRole::FLEX_INSTANCE) {
            flexIndex.push_back(i);
        } else {
            LOG_E("[%s] [DefaultGroupGenerator] Ignoring instance with ID %lu due to unrecognized role %c.",
                  GetErrorCode(ErrorType::INVALID_PARAMETER, ControllerFeature::GROUP_GENERATOR).c_str(),
                  instances[i].id, instances[i].role);
        }
    }
    LOG_I("[DefaultGroupGenerator] Get %zu prefill instances and %zu decode instances and %zu flex instances.",
        pIndex.size(), dIndex.size(), flexIndex.size());
}

static void ProcessPNodes(std::vector<std::vector<uint64_t>> &pNodes,
    std::vector<uint64_t> &pIndex, std::vector<MINDIE::MS::DIGSRoleDecision> &instances, uint32_t groupNum)
{
    for (uint32_t i = 0; i < pIndex.size() ; i++) {
        uint32_t groupId = i % groupNum;
        instances[pIndex[i]].groupId = groupId;
        pNodes[groupId].push_back(instances[pIndex[i]].id);
    }
}

static void ProcessDNodes(uint32_t pNum, std::vector<std::vector<uint64_t>> &dNodes,
    std::vector<uint64_t> &dIndex, std::vector<MINDIE::MS::DIGSRoleDecision> &instances, uint32_t groupNum)
{
    for (uint32_t i = 0; i < dIndex.size() ; i++) {
        uint32_t groupId = (i + pNum) % groupNum;
        instances[dIndex[i]].groupId = groupId;
        dNodes[groupId].push_back(instances[dIndex[i]].id);
    }
}

int32_t DefaultGroupGenerator::GenerateGroups(std::vector<MINDIE::MS::DIGSRoleDecision> &instances,
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups,
    std::vector<std::vector<uint64_t>> &flexGroups)
{
    auto maxValue = LIMIT_MAX;
    if (ControllerConfig::GetInstance()->IsMultiNodeMode()) {
        maxValue = DISTRIBUTE_LIMIT_MAX;
    }
    std::vector<uint64_t> pIndex;
    std::vector<uint64_t> dIndex;
    std::vector<uint64_t> flexIndex;
    GetPNodesAndDNodes(instances, pIndex, dIndex, flexIndex);
    auto pSize = pIndex.size();
    auto dSize = dIndex.size();
    auto flexSize = flexIndex.size();
    if (((pSize == 0 || dSize == 0) && flexSize == 0) || (pSize + dSize + flexSize) > maxValue) {
        LOG_E("[%s] [DefaultGroupGenerator] Generate groups failed because prefill rate %zu or decode rate %zu is "
            "invalid.", GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::GROUP_GENERATOR).c_str(),
            pSize, dSize);
        return -1;
    }
    uint32_t groupNum = (pSize + dSize + flexSize) / maxValue;
    if (((pSize + dSize + flexSize) % maxValue) > 0) {
        groupNum++;
    }
    if (groupNum == 0) {
        LOG_E("[%s] [DefaultGroupGenerator] Generate groups failed, number of groups %u is invalid.",
            GetErrorCode(ErrorType::INVALID_PARAMETER, ControllerFeature::GROUP_GENERATOR).c_str(),
            groupNum);
        return -1;
    }
    std::vector<std::vector<uint64_t>> pNodes;
    std::vector<std::vector<uint64_t>> dNodes;
    pNodes.resize(groupNum);
    dNodes.resize(groupNum);
    flexGroups.resize(groupNum);
    ProcessPNodes(pNodes, pIndex, instances, groupNum);
    uint32_t pNum = static_cast<uint32_t>(pNodes.size());
    ProcessDNodes(pNum, dNodes, dIndex, instances, groupNum);
    for (uint32_t i = 0; i < flexIndex.size() ; i++) {
        uint32_t groupId = i % groupNum;
        instances[flexIndex[i]].groupId = groupId;
        flexGroups[groupId].push_back(instances[flexIndex[i]].id);
    }
    for (uint32_t i = 0; i < groupNum ; i++) {
        LOG_D("[DefaultGroupGenerator] Generating groups, group ID %u, size of prefill nodes is %zu,"
            " size of decode nodes is %zu", i, pNodes[i].size(), dNodes[i].size());
        groups.emplace_back(pNodes[i], dNodes[i]);
    }
    LOG_I("[DefaultGroupGenerator] Generating groups end, total group number is %u.", groupNum);
    return 0;
}

REGISTER_GROUP_GENERATOR_CREATOR(DefaultGroupGenerator, Default);
}