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
#ifndef MINDIE_DIGS_MPRF_POLICY_H
#define MINDIE_DIGS_MPRF_POLICY_H

#include "req_reordering_policy.h"

namespace MINDIE::MS {

// 最长请求prefix复用优先策略
class MprfPolicy : public ReqReorderingPolicy {
public:
    explicit MprfPolicy() = default;

    int32_t Reordering(std::deque<std::shared_ptr<DIGSRequest>>& reqs) override;

    ~MprfPolicy() override = default;
};

}

#endif // MINDIE_DIGS_MPRF_POLICY_H
