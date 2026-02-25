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
#ifndef MINDIE_DIGS_INST_SELECT_POLICY_H
#define MINDIE_DIGS_INST_SELECT_POLICY_H

#include "resource/resource_view_manager.h"
#include "request/digs_request.h"

namespace MINDIE::MS {

enum class InstSelectPolicyType {
    STATIC_ALLOC = 1,
    LOAD_BALANCE,
};

class InstSelectPolicy {
public:
    virtual void LoadResourceView(const std::unique_ptr<ResourceViewManager>& view, DIGSReqStage stage) = 0;

    virtual int32_t SelectPrefillInst(const std::shared_ptr<DIGSRequest>& req,
                                      uint64_t &prefill, uint64_t &groupId) = 0;

    virtual int32_t SelectDecodeInst(const std::shared_ptr<DIGSRequest>& req,
                                     uint64_t prefill, uint64_t groupId, uint64_t &decode) = 0;

    virtual void OffloadResourceView() = 0;

    virtual ~InstSelectPolicy() = default;

protected:
    size_t maxResNum_ = 5000;
};

}
#endif
