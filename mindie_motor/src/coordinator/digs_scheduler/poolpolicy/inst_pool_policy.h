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
#ifndef MINDIE_DIGS_INST_POOL_POLICY_H
#define MINDIE_DIGS_INST_POOL_POLICY_H

#include "resource/resource_view_manager.h"

namespace MINDIE::MS {

enum class InstPoolPolicyType {
    STATIC = 1,
    DYNAMIC
};

class InstPoolPolicy {
public:
    virtual int32_t ScheduleInst(const std::unique_ptr<ResourceViewManager>& resView) = 0;

    virtual ~InstPoolPolicy() = default;
};

}
#endif
