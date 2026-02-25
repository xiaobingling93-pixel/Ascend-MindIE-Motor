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
#ifndef MINDIE_DIGS_LOAD_BALANCE_POLICY_H
#define MINDIE_DIGS_LOAD_BALANCE_POLICY_H

#include "inst_select_policy.h"
#include "Logger.h"


namespace MINDIE::MS {

class LoadBalancePolicy : public InstSelectPolicy {
public:
    explicit LoadBalancePolicy(size_t maxResNum);

    ~LoadBalancePolicy() override = default;

    void LoadResourceView(const std::unique_ptr<ResourceViewManager>& view, DIGSReqStage stage) override;

    int32_t SelectPrefillInst(const std::shared_ptr<DIGSRequest>& req,
                              uint64_t &prefill, uint64_t &groupId) override;

    int32_t SelectDecodeInst(const std::shared_ptr<DIGSRequest>& req,
                             uint64_t prefill, uint64_t groupId, uint64_t &decode) override;

    void OffloadResourceView() override;

    static void SortPrefill(ResourceViewManager::ResView& pool);

    static void SortDecode(ResourceViewManager::ResView& group, const std::unique_ptr<MetaResource>& demand);

    static bool CheckDemandLimit(const ResourceViewManager::ResInfo& resInfo, DIGSReqStage stage);

protected:
    ResourceViewManager::ResView prefillPool_;

    std::map<uint64_t, ResourceViewManager::ResView> decodePool_;
};
}
#endif