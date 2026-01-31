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
#ifndef MINDIE_DIGS_INSTANCEROLEMANAGER_H
#define MINDIE_DIGS_INSTANCEROLEMANAGER_H

#include "common.h"
#include "Logger.h"
#include "ProportionCalculator.h"


namespace MINDIE::MS::roleManager {

class InstanceRoleManager {
public:
    using Config = std::map<std::string, std::string>;

    using NotifyRoleDecision = std::function<int32_t(std::vector<DIGSRoleDecision>)>;

    using InstancesCollector = std::function<int32_t(std::vector<DIGSInstanceInfo>&, DIGSRequestSummary&)>;
    using GroupInstanceInfo = std::map<uint64_t, std::vector<DIGSInstanceInfo>>;

    static int32_t Create(Config config, NotifyRoleDecision callout, InstancesCollector collector,
                          std::unique_ptr<InstanceRoleManager>& manager);

    explicit InstanceRoleManager(Config config, std::unique_ptr<ProportionCalculator> &proportionCalculator);

    bool Init(std::vector<DIGSInstanceInfo>& infos, DIGSRequestSummary& summary,
              size_t& pRate, size_t& dRate);

    int32_t RegisterNotifyRoleDecision(NotifyRoleDecision callout);

    int32_t RegisterInstancesCollector(InstancesCollector collector);

    ~InstanceRoleManager();

    InstanceRoleManager(const InstanceRoleManager&) = delete;

    InstanceRoleManager& operator=(const InstanceRoleManager&) = delete;

    int32_t InitExpectPDRate(std::vector<DIGSInstanceInfo>& infos, DIGSRequestSummary& summary,
                             size_t& pRate, size_t& dRate);
private:
    void ScheduleThread();

    int32_t AllocateInstanceRole(GroupInstanceInfo& groupInstanceInfo, DIGSRequestSummary& summary,
                                 std::vector<DIGSRoleDecision>& digRoleDecisions, bool first = false);

    void RefreshInstanceRole(std::vector<DIGSInstanceInfo>& instances,
        const std::vector<DIGSRoleDecision>& roleDecisions) const;

    void GroupDIGSInstanceInfos(std::map<uint64_t, std::vector<DIGSInstanceInfo>>& groupInstanceInfo,
        std::vector<DIGSInstanceInfo>& dIGSInstanceInfos) const;

    void SelectSwitchPDInstance(std::vector<DIGSRoleDecision>& roleDecision,
                                   std::vector<DIGSInstanceInfo>& instances, uint64_t num,
                                   DIGSInstanceRole switchRole) const;

    void SortInstance(std::vector<DIGSInstanceInfo>& instances, DIGSInstanceLabel label) const;

    void ComputePDNum(DIGSGroupPDRatio& digsGroupPdRatio, size_t size, size_t& alloc2PInstance) const;

    void AssignRoleByDefiniteRatio(GroupInstanceInfo& groupInstanceInfo,
        const size_t &pAbility, const size_t &dAbility);

    void AssignRoleByHardwareType(std::vector<DIGSInstanceInfo> &infos);

    void AssignRoleForCrossNodeSize(std::vector<DIGSInstanceInfo> &infos, size_t &pAbility, size_t &dAbility);

    void GetRoleDecision(std::vector<DIGSRoleDecision>& roleDecisions, std::vector<DIGSInstanceInfo>& allInstance,
        size_t size, DIGSInstanceRole role) const;

    void GetFlexRoleDecision(std::vector<DIGSRoleDecision>& roleDecisions,
         std::vector<DIGSInstanceInfo>& allInstance, size_t flexNumDest, double pRatioX) const;
    void AssignRole(std::vector<DIGSRoleDecision> &digRoleDecisions,
                    std::pair<const uint64_t, std::vector<DIGSInstanceInfo>> &pair, DIGSGroupPDRatio &ratio);
private:
    InstanceRoleManager::Config config_;
    size_t timeout_;
    bool isSkipDecisionForCrossNodeMode_ = false;
    bool isHeterogeneous_ = false;
    bool isAutoSwitching_ = true;
    DIGSRatioType ratioType_ = DIGSRatioType::DIGS_ROLE_PD_RATIO;

    std::atomic_bool scheduleTerminate_;
    std::thread scheduleThread_;
    std::mutex scheduleMutex_;
    std::condition_variable scheduleCv_;
    std::unique_ptr<ProportionCalculator> proportionCalculator_;

    std::map<uint64_t, DIGSGroupPDRatio> dIGSGroupPDRatios_;
    DIGSGroupPDRatio globalPDRatios_;

    InstancesCollector collector_ = nullptr;
    NotifyRoleDecision notifier_ = nullptr;
};

} // digs
#endif
