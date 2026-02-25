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
#ifndef MINDIS_MS_DEFAULTGROUPGENERATOR_H
#define MINDIS_MS_DEFAULTGROUPGENERATOR_H

#include "GroupGeneratorFactory.h"
namespace MINDIE::MS {
class DefaultGroupGenerator : public GroupGenerator {
public:
    int32_t GenerateGroups(std::vector<MINDIE::MS::DIGSRoleDecision> &instances,
        std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups,
        std::vector<std::vector<uint64_t>> &flexGroups) override;
};
}
#endif // MINDIS_MS_DEFAULTGROUPGENERATOR_H

