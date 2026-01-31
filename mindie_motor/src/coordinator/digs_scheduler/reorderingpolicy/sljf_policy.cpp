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
#include "sljf_policy.h"

namespace MINDIE::MS {

SljfPolicy::SljfPolicy(bool reverse) : reverse_(reverse)
{
}

int32_t SljfPolicy::Reordering(std::deque<std::shared_ptr<DIGSRequest>>& reqs)
{
    auto comp = [this](const std::shared_ptr<DIGSRequest>& req1, const std::shared_ptr<DIGSRequest>& req2) {
        if (this->reverse_) {
            return req1->InputLen() > req2->InputLen();
        }
        return req1->InputLen() < req2->InputLen();
    };

    std::stable_sort(reqs.begin(), reqs.end(), comp);
    return static_cast<int32_t>(common::Status::OK);
}

}
