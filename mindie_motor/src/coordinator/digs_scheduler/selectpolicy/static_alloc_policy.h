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
#ifndef MINDIE_DIGS_STATIC_ALLOC_POLICY_H
#define MINDIE_DIGS_STATIC_ALLOC_POLICY_H

#include "inst_select_policy.h"
#include "Logger.h"


namespace MINDIE::MS {

class StaticAllocPolicy : public InstSelectPolicy {
public:
    explicit StaticAllocPolicy(size_t maxResNum);

    ~StaticAllocPolicy() override = default;

    void LoadResourceView(const std::unique_ptr<ResourceViewManager>& view, DIGSReqStage stage) override;

    int32_t SelectPrefillInst(const std::shared_ptr<DIGSRequest>& req,
                              uint64_t &prefill, uint64_t &groupId) override;

    int32_t SelectDecodeInst(const std::shared_ptr<DIGSRequest>& req,
                             uint64_t prefill, uint64_t groupId, uint64_t &decode) override;

    void OffloadResourceView() override;

private:
    ResourceViewManager::ResView staticPrefillPool_;

    std::map<uint64_t, ResourceViewManager::ResView> staticDecodePool_;
};
}
#endif