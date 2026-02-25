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
#include "InstanceRoleManager.h"
#include <securec.h>

namespace MINDIE::MS::roleManager {
constexpr size_t CRUISES_SPACE = 15;
constexpr size_t HOUR_TO_SECOND = 3600;
const std::vector<std::string> PREFILL_HARDWARE_TYPES = {"800i a2(32g)"};
const std::vector<std::string> DECODE_HARDWARE_TYPES = {"800i a2(64g)"};
constexpr double T_ABILITY_INIT_VAL = 1e9;

#define GET_FLEXINST_NUM ((ratioType_ == DIGSRatioType::DIGS_ROLE_PDFLEX_RATIO) ? (DIGS_ROLE_FLEX_NUM) : (0))

int32_t InstanceRoleManager::Create(InstanceRoleManager::Config config, NotifyRoleDecision callout,
                                    InstancesCollector collector, std::unique_ptr<InstanceRoleManager> &manager)
{
    std::unique_ptr<ProportionCalculator> proportionCalculator;
    if (MINDIE::MS::roleManager::ProportionCalculator::Create(config, proportionCalculator) !=
        static_cast<int32_t>(common::Status::OK)) {
        LOG_E("[%s] [RoleManager] Create proportion calculator Failed.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    try {
        auto roleManager = std::make_unique<InstanceRoleManager>(std::move(config), proportionCalculator);
        if (roleManager->RegisterInstancesCollector(std::move(collector)) != static_cast<int32_t>(common::Status::OK)) {
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }
        if (roleManager->RegisterNotifyRoleDecision(std::move(callout)) != static_cast<int32_t>(common::Status::OK)) {
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }

        manager = std::move(roleManager);
    } catch (const std::exception &e) {
        LOG_E("[%s] [RoleManager] Failed to create instance role manager. Error: %s.",
              GetErrorCode(ErrorType::RESOURCE_EXHAUSTED, CommonFeature::DIGS).c_str(), e.what());
        return static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE);
    }
    return static_cast<int32_t>(common::Status::OK);
}

InstanceRoleManager::InstanceRoleManager(InstanceRoleManager::Config config,
                                         std::unique_ptr<ProportionCalculator> &proportionCalculator)
    : config_(std::move(config)), timeout_(HOUR_TO_SECOND), scheduleTerminate_(true)
{
    // 获取配置的周期时间, 单位秒
    size_t ulItem = 0;
    if (MINDIE::MS::common::GetConfig("time_period", ulItem, config_)) {
        timeout_ = ulItem;
    }
    
    std::string isSkipDecisionForCrossNodeMode;
    if (MINDIE::MS::common::GetConfig("is_skip_decision_for_cross_node_mode",
                                      isSkipDecisionForCrossNodeMode, config_)) {
        MINDIE::MS::common::Str2Bool(isSkipDecisionForCrossNodeMode, isSkipDecisionForCrossNodeMode_);
    }

    std::string isHeterogeneous;
    if (MINDIE::MS::common::GetConfig("is_heterogeneous", isHeterogeneous, config_)) {
        MINDIE::MS::common::Str2Bool(isHeterogeneous, isHeterogeneous_);
    }
    std::string isAutoSwitching;
    if (MINDIE::MS::common::GetConfig("is_auto_pd_role_switching", isAutoSwitching, config_)) {
        MINDIE::MS::common::Str2Bool(isAutoSwitching, isAutoSwitching_);
    }
    std::string hasFlex;
    if (MINDIE::MS::common::GetConfig("has_flex", hasFlex, config_)) {
        if (hasFlex == "true") {
            ratioType_ = DIGSRatioType::DIGS_ROLE_PDFLEX_RATIO;
        } else if (hasFlex == "false") {
            ratioType_ = DIGSRatioType::DIGS_ROLE_PD_RATIO;
        }
    }
    proportionCalculator_ = std::move(proportionCalculator);
}

InstanceRoleManager::~InstanceRoleManager()
{
    LOG_I("[RoleManager] Role schedule stopping...");
    // set thread signal
    scheduleTerminate_.store(true);

    // notify thread
    scheduleCv_.notify_one();

    // wait for thread exit
    if (scheduleThread_.joinable()) {
        scheduleThread_.join();
    }
    LOG_I("[RoleManager] Role schedule stopped!");
}

int32_t InstanceRoleManager::InitExpectPDRate(std::vector<DIGSInstanceInfo>& infos, DIGSRequestSummary& summary,
    size_t& pRate, size_t& dRate)
{
    // 外部配置pRate和dRate
    if (pRate != 0 && dRate != 0) {
        return static_cast<int32_t>(common::Status::OK);
    }
    ProportionInput input;
    input.instanceNum = infos.size();
    input.flexInstNum = GET_FLEXINST_NUM;
    input.summary = summary;
    input.type = ratioType_;
    return proportionCalculator_->ClusterExpectRatio(input, globalPDRatios_, pRate, dRate);
}

bool InstanceRoleManager::Init(std::vector<DIGSInstanceInfo>& infos, DIGSRequestSummary& summary, size_t& pRate,
                               size_t& dRate)
{
    // 异构场景 不支持
    if (isHeterogeneous_) {
        AssignRoleByHardwareType(infos);
        return true;
    }
    // 跨机且PD节点数量不同场景，根据PD配置决策身份
    if (isSkipDecisionForCrossNodeMode_) {
        AssignRoleForCrossNodeSize(infos, dRate, pRate);
        return true;
    }
    GroupInstanceInfo groupInstanceInfos;
    GroupDIGSInstanceInfos(groupInstanceInfos, infos);

    if (pRate != 0 && dRate != 0) {
        AssignRoleByDefiniteRatio(groupInstanceInfos, dRate, pRate);
    } else {
        std::vector<DIGSRoleDecision> digRoleDecisions;
        // 直接计算最佳配比，进行角色分配，为pRate和dRate赋值最佳配比
        if (AllocateInstanceRole(groupInstanceInfos, summary, digRoleDecisions, true) !=
            static_cast<int32_t>(common::Status::OK)) {
            return false;
        }
        if (notifier_(digRoleDecisions) != static_cast<int32_t>(common::Status::OK)) {
            LOG_W("RoleManager: notify role decision failed!");
        }
        InitExpectPDRate(infos, summary, pRate, dRate);
    }

    // 启动周期性决策线程
    if (!isAutoSwitching_) {
        LOG_I("[RoleManager] Role schedule is off.");
        return true;
    }
    bool start = true;
    if (scheduleTerminate_.compare_exchange_strong(start, false)) {
        scheduleThread_ = std::thread([this]() {
            this->ScheduleThread();
        });
    }
    LOG_I("[RoleManager] Role schedule start(%s)...", start ? "true" : "false");
    return true;
}

void InstanceRoleManager::AssignRoleByHardwareType(std::vector<DIGSInstanceInfo> &infos)
{
    std::vector<DIGSRoleDecision> digRoleDecisions;
    for (auto &info : infos) {
        DIGSRoleDecision digsRoleDecision;
        digsRoleDecision.id = info.staticInfo.id;
        digsRoleDecision.groupId = info.staticInfo.groupId;
        std::string type = info.staticInfo.nodeRes.hardwareType;
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        if (std::find(PREFILL_HARDWARE_TYPES.begin(), PREFILL_HARDWARE_TYPES.end(), type) !=
            PREFILL_HARDWARE_TYPES.end()) {
            info.staticInfo.role = DIGSInstanceRole::PREFILL_INSTANCE;
            digsRoleDecision.role = DIGSInstanceRole::PREFILL_INSTANCE;
            digsRoleDecision.flexPRatio = 0;
        } else if (std::find(DECODE_HARDWARE_TYPES.begin(), DECODE_HARDWARE_TYPES.end(), type) !=
                   DECODE_HARDWARE_TYPES.end()) {
            info.staticInfo.role = DIGSInstanceRole::DECODE_INSTANCE;
            digsRoleDecision.role = DIGSInstanceRole::DECODE_INSTANCE;
            digsRoleDecision.flexPRatio = 0;
        } else {
            LOG_W("[%s] [RoleManager] No role decision for ID %lu",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(), info.staticInfo.id);
            continue;
        }
        digRoleDecisions.emplace_back(digsRoleDecision);
    }
    if (notifier_(digRoleDecisions) != static_cast<int32_t>(common::Status::OK)) {
        LOG_W("[%s] [RoleManager] Notify role decision failed!",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
    }
}

void InstanceRoleManager::AssignRoleForCrossNodeSize(std::vector<DIGSInstanceInfo> &infos,
    size_t &pAbility, size_t &dAbility)
{
    std::vector<DIGSRoleDecision> digRoleDecisions;
    for (auto &info : infos) {
        DIGSRoleDecision digsRoleDecision;
        digsRoleDecision.id = info.staticInfo.id;
        digsRoleDecision.groupId = info.staticInfo.groupId;
        digsRoleDecision.role = info.staticInfo.role;
        if (info.staticInfo.role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
            pAbility++;
        } else if (info.staticInfo.role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
            dAbility++;
        }
        digRoleDecisions.emplace_back(digsRoleDecision);
    }
    if (notifier_(digRoleDecisions) != static_cast<int32_t>(common::Status::OK)) {
        LOG_W("[%s] [RoleManager] Notify multi node role decision failed!",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
    }
}

void InstanceRoleManager::AssignRoleByDefiniteRatio(GroupInstanceInfo& groupInstanceInfo, const size_t &pAbility,
                                                    const size_t &dAbility)
{
    // 基于外部输入配比分配角色
    std::vector<DIGSRoleDecision> digRoleDecisions;
    DIGSGroupPDRatio ratio = {};
    (void)memset_s(&ratio, sizeof(DIGSGroupPDRatio), 0, sizeof(DIGSGroupPDRatio));
    ratio.pAbility = static_cast<double>(pAbility);
    ratio.dAbility = static_cast<double>(dAbility);
    ratio.tAbility = T_ABILITY_INIT_VAL; // 初始化时tAbility为0无法计算
    for (auto &pair: groupInstanceInfo) {
        size_t instNum = pair.second.size();
        ProportionInput input;
        input.instanceNum = instNum;
        input.flexInstNum =  GET_FLEXINST_NUM;
        input.summary = DIGSRequestSummary();
        input.type = ratioType_;
        proportionCalculator_->InitBestRatioWithExternInput(input, ratio);
        // 记录下全局实例信息
        globalPDRatios_ = ratio;
        uint64_t pNum = 0, dNum = 0, flexNum = 0;
        for (auto &info : pair.second) {
            DIGSRoleDecision digsRoleDecision;
            digsRoleDecision.id = info.staticInfo.id;
            digsRoleDecision.groupId = info.staticInfo.groupId;
            if (pNum < ratio.pNum) {
                info.staticInfo.role = DIGSInstanceRole::PREFILL_INSTANCE;
                digsRoleDecision.role = DIGSInstanceRole::PREFILL_INSTANCE;
                digsRoleDecision.flexPRatio = 0;
                digRoleDecisions.emplace_back(digsRoleDecision);
                pNum++;
            } else if (dNum < ratio.dNum) {
                info.staticInfo.role = DIGSInstanceRole::DECODE_INSTANCE;
                digsRoleDecision.role = DIGSInstanceRole::DECODE_INSTANCE;
                digsRoleDecision.flexPRatio = 0;
                digRoleDecisions.emplace_back(digsRoleDecision);
                dNum++;
            } else if (flexNum < ratio.flexNum) {
                info.staticInfo.role = DIGSInstanceRole::FLEX_INSTANCE;
                digsRoleDecision.role = DIGSInstanceRole::FLEX_INSTANCE;
                digsRoleDecision.flexPRatio = static_cast<uint64_t>(ratio.flexPRatio *
                                                                   MINDIE::MS::FLEX_INSTANCE_P_PERCENTAGE_MAX);
                digRoleDecisions.emplace_back(digsRoleDecision);
                flexNum++;
            }
        }
        LOG_I("pNum: " << ratio.pNum << ", dNum: " << ratio.dNum << ", flexNum: " << ratio.flexNum <<
            ", flexPRatio: " << ratio.flexPRatio);
    }
    if (notifier_(digRoleDecisions) != static_cast<int32_t>(common::Status::OK)) {
        LOG_W("[%s] [RoleManager] Notify role decision failed!",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
    }
}

int32_t InstanceRoleManager::RegisterNotifyRoleDecision(InstanceRoleManager::NotifyRoleDecision callout)
{
    if (callout == nullptr) {
        LOG_E("[%s] [RoleManager] Notify function is nullptr.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    notifier_ = std::move(callout);
    return static_cast<int32_t>(common::Status::OK);
}

int32_t InstanceRoleManager::RegisterInstancesCollector(InstanceRoleManager::InstancesCollector collector)
{
    if (collector == nullptr) {
        LOG_E("[%s] [RoleManager] Collect function is nullptr.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    collector_ = std::move(collector);
    return static_cast<int32_t>(common::Status::OK);
}

void InstanceRoleManager::ScheduleThread()
{
    LOG_I("[RoleManager] Schedule thread start!");
    while (!scheduleTerminate_.load()) {
        std::unique_lock<std::mutex> lock(scheduleMutex_);
        std::chrono::seconds timeoutSeconds(timeout_);
        scheduleCv_.wait_for(lock, timeoutSeconds);
        if (scheduleTerminate_.load()) {
            break;
        }
        do {
            if (collector_ == nullptr || notifier_ == nullptr) {
                LOG_W("[%s] [RoleManager] Skip role allocate, collector or notifier unset!",
                      GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
                break;
            }

            // 获取实例清单和请求统计信息
            std::vector<DIGSInstanceInfo> instanceInfo;
            DIGSRequestSummary requestSummary = {};
            if (collector_(instanceInfo, requestSummary) != static_cast<int32_t>(common::Status::OK)) {
                LOG_W("[%s] [RoleManager] Skip role allocate, collect failed!",
                      GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
                break;
            }
            LOG_I("[RoleManager] Collect instances count %zu, summary: %zu %zu",
                  instanceInfo.size(), requestSummary.inputLength, requestSummary.outputLength);
            // 计算最佳配比，分配角色
            GroupInstanceInfo groupInstanceInfos;
            GroupDIGSInstanceInfos(groupInstanceInfos, instanceInfo);
            std::vector<DIGSRoleDecision> digRoleDecisions;
            AllocateInstanceRole(groupInstanceInfos, requestSummary, digRoleDecisions, false);
        } while (false);
    }
    LOG_I("[RoleManager] Schedule thread stop!");
}

int32_t InstanceRoleManager::AllocateInstanceRole(GroupInstanceInfo& groupInstanceInfo,
                                                  MINDIE::MS::DIGSRequestSummary& summary,
                                                  std::vector<DIGSRoleDecision>& digRoleDecisions, bool first)
{
    if (summary.inputLength == 0 || summary.outputLength == 0) {
        LOG_W("[%s] [RoleManager] Allocate Instance decision failed, Input length or output length is 0.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
    }
    for (auto& pair: groupInstanceInfo) {
        size_t groupSize = pair.second.size();
        // 调用算法， 传入输入长度、输出长度、实例个数，获得组内最佳P和D的个数
        DIGSGroupPDRatio ratio = {};
        (void)memset_s(&ratio, sizeof(DIGSGroupPDRatio), 0, sizeof(DIGSGroupPDRatio));
        ProportionInput input;
        input.instanceNum = groupSize;
        input.flexInstNum = GET_FLEXINST_NUM;
        input.summary = summary;
        input.type = ratioType_;
        input.isFirst = first;
        if (proportionCalculator_->CalBestRatio(input, ratio) != static_cast<int32_t>(common::Status::OK)) {
            LOG_W("RoleManager: skip role allocate, calculate ratio failed!");
            return static_cast<int32_t>(common::Status::ILLEGAL_PARAMETER);
        }
        if (first) {
            globalPDRatios_ = ratio;
        }
        LOG_I("[InstanceRoleManager] allocate instance role: pNum: " << ratio.pNum <<
        ", dNum: " <<ratio.dNum << ", flexNum: " << ratio.flexNum <<
        ", flexPRatio: " << ratio.flexPRatio << " input.type " << static_cast<uint32_t>(input.type));
        AssignRole(digRoleDecisions, pair, ratio);
    }
    return static_cast<int32_t>(common::Status::OK);
}

void InstanceRoleManager::AssignRole(std::vector<DIGSRoleDecision> &digRoleDecisions,
                                     std::pair<const uint64_t, std::vector<DIGSInstanceInfo>> &pair,
                                     DIGSGroupPDRatio &ratio)
{
    ratio.groupId = pair.first;
    dIGSGroupPDRatios_[pair.first] = ratio;
    size_t pNumDest = ratio.pNum;
    size_t dNumDest = ratio.dNum;
    size_t flexNumDest = ratio.flexNum;
    // 当前分组中的PDFlex角色数量
    auto pActualSize = static_cast<size_t>(std::count_if(pair.second.begin(), pair.second.end(),
        [](const DIGSInstanceInfo &instanceInfo) {
            return instanceInfo.staticInfo.role ==
                DIGSInstanceRole::PREFILL_INSTANCE;
        }));
    auto dActualSize = static_cast<size_t>(std::count_if(pair.second.begin(), pair.second.end(),
        [](const DIGSInstanceInfo &instanceInfo) {
            return instanceInfo.staticInfo.role ==
                DIGSInstanceRole::DECODE_INSTANCE;
        }));
    std::vector<DIGSRoleDecision> roleDecisions;
        // 先设置flex实例，固定group中最后n个实例为mix实例，每个PDMix切换周期固定下发FLEX实例，因为x会变化
    GetFlexRoleDecision(roleDecisions, pair.second, flexNumDest, ratio.flexPRatio);
    if (pNumDest > pActualSize) {
        GetRoleDecision(roleDecisions, pair.second, pNumDest - pActualSize,
                        DIGSInstanceRole::PREFILL_INSTANCE);
    }
    if (dNumDest > dActualSize) {
        GetRoleDecision(roleDecisions, pair.second, dNumDest - dActualSize,
                        DIGSInstanceRole::DECODE_INSTANCE);
    }
    digRoleDecisions.insert(digRoleDecisions.end(), std::make_move_iterator(roleDecisions.begin()),
                            std::make_move_iterator(roleDecisions.end()));
    if (notifier_(digRoleDecisions) != static_cast<int32_t>(common::Status::OK)) {
        LOG_W("[%s] [RoleManager] Notify role decision failed!",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
    }
}

void InstanceRoleManager::GetFlexRoleDecision(std::vector<DIGSRoleDecision>& roleDecisions,
    std::vector<DIGSInstanceInfo>& allInstance, size_t flexNumDest, double pRatioX) const
{
    auto flexActualSize = static_cast<size_t>(std::count_if(allInstance.begin(), allInstance.end(),
        [](const DIGSInstanceInfo &instanceInfo) {
            return instanceInfo.staticInfo.role ==
                DIGSInstanceRole::FLEX_INSTANCE;
        }));
    // 如果不需要任何flex实例
    if (flexNumDest == 0) {
        return;
    }
    uint64_t pratio = static_cast<uint64_t>(pRatioX * MINDIE::MS::FLEX_INSTANCE_P_PERCENTAGE_MAX);
    if (flexActualSize < flexNumDest) {
        auto needToAdd = flexNumDest - flexActualSize;
        auto containerSize = allInstance.size();
        auto endPos = std::min(static_cast<size_t>(containerSize), needToAdd);
        for (auto rit = allInstance.rbegin(); rit != allInstance.rbegin() + endPos; ++rit) {
            rit->staticInfo.role = DIGSInstanceRole::FLEX_INSTANCE;
            DIGSRoleDecision decision;
            decision.groupId = rit->staticInfo.groupId;
            decision.role = DIGSInstanceRole::FLEX_INSTANCE;
            decision.id = rit->staticInfo.id;
            decision.flexPRatio = pratio;
            roleDecisions.emplace_back(std::move(decision));
            LOG_I("groupid: " << decision.groupId << " id" << decision.id <<
                ", " << static_cast<uint32_t>(decision.role) << ", flexPRatio: " << decision.flexPRatio);
        }
    } else {
        size_t count = 0;
        for (const auto& instance : allInstance) {
            if (instance.staticInfo.role == DIGSInstanceRole::FLEX_INSTANCE && count < flexNumDest) {
                if (pratio == instance.staticInfo.flexPRatio) {
                    ++count;
                    continue;
                }
                DIGSRoleDecision decision;
                decision.groupId = instance.staticInfo.groupId;
                decision.role = DIGSInstanceRole::FLEX_INSTANCE;
                decision.id = instance.staticInfo.id;
                decision.flexPRatio = pratio;
                roleDecisions.emplace_back(std::move(decision));
                LOG_I("groupid: " << decision.groupId << " id" << decision.id <<
                    ", " << static_cast<uint32_t>(decision.role) << ", flexPRatio: " <<
                    decision.flexPRatio);
                ++count;
            }
        }
    }
}

void InstanceRoleManager::GetRoleDecision(std::vector<DIGSRoleDecision> &roleDecisions,
                                          std::vector<DIGSInstanceInfo> &allInstance, size_t size,
                                          DIGSInstanceRole role) const
{
    std::vector<DIGSInstanceInfo> need2SwitchInstance;
    std::copy_if(allInstance.begin(), allInstance.end(), std::back_inserter(need2SwitchInstance),
                 [&role](const DIGSInstanceInfo &instanceInfo) {
                     return ((instanceInfo.staticInfo.role != role) &&
                             (instanceInfo.staticInfo.role != DIGSInstanceRole::FLEX_INSTANCE));
    });
    SelectSwitchPDInstance(roleDecisions, need2SwitchInstance, size, role);
    RefreshInstanceRole(allInstance, roleDecisions);
}

void InstanceRoleManager::RefreshInstanceRole(std::vector<DIGSInstanceInfo>& instances,
                                              const std::vector<DIGSRoleDecision>& roleDecisions) const
{
    for (auto roleDecision : roleDecisions) {
        for (auto& instance : instances) {
            if (roleDecision.id == instance.staticInfo.id) {
                instance.staticInfo.role = roleDecision.role;
                break;
            }
        }
    }
}

void InstanceRoleManager::GroupDIGSInstanceInfos(std::map<uint64_t,
                                                 std::vector<DIGSInstanceInfo>>& groupInstanceInfo,
                                                 std::vector<DIGSInstanceInfo>& dIGSInstanceInfos) const
{
    for (auto& dIGSInstanceInfo : dIGSInstanceInfos) {
        uint64_t groupId = dIGSInstanceInfo.staticInfo.groupId;
        if (groupInstanceInfo.find(groupId) == groupInstanceInfo.end()) {
            std::vector<DIGSInstanceInfo> instanceInfo;
            groupInstanceInfo[groupId] = instanceInfo;
        }
        groupInstanceInfo[groupId].emplace_back(std::move(dIGSInstanceInfo));
    }
}

void InstanceRoleManager::SortInstance(std::vector<DIGSInstanceInfo>& instances, DIGSInstanceLabel label) const
{
    // 先优先选择没有角色的实例
    // 再优先选择Prefer标签
    // 再根据负载情况排序
    auto comp = [&label](const DIGSInstanceInfo& a, const DIGSInstanceInfo& b) {
        if (a.staticInfo.role != b.staticInfo.role) {
            if (a.staticInfo.role == DIGSInstanceRole::UN_DEF_INSTANCE) {
                return true;
            }
            if (b.staticInfo.role == DIGSInstanceRole::UN_DEF_INSTANCE) {
                return false;
            }
        } else if (a.staticInfo.label != b.staticInfo.label) {
            if (a.staticInfo.label == label) {
                return true;
            }
            if (b.staticInfo.label == label) {
                return false;
            }
        } else {
            // 再根据负载排序
            // 先比较已经分配的slot
            if (a.scheduleInfo.allocatedSlots != b.scheduleInfo.allocatedSlots) {
                return a.scheduleInfo.allocatedSlots < b.scheduleInfo.allocatedSlots;
            }
            // 再比较已经分配的block
            if (a.scheduleInfo.allocatedBlocks != b.scheduleInfo.allocatedBlocks) {
                return a.scheduleInfo.allocatedBlocks < b.scheduleInfo.allocatedBlocks;
            }
        }
        return false;
    };
    std::sort(instances.begin(), instances.end(), comp);
}

void InstanceRoleManager::SelectSwitchPDInstance(std::vector<DIGSRoleDecision>& roleDecision,
                                                 std::vector<DIGSInstanceInfo>& instances, uint64_t num,
                                                 DIGSInstanceRole switchRole) const
{
    DIGSInstanceLabel label;
    switch (switchRole) {
        case DIGSInstanceRole::PREFILL_INSTANCE:
            label = DIGSInstanceLabel::DECODE_PREFER;
            break;
        case DIGSInstanceRole::DECODE_INSTANCE:
            label = DIGSInstanceLabel::PREFILL_PREFER;
            break;
        default:
            LOG_W("[%s] [DIGS] Unsupported role %d",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
                  static_cast<int32_t>(switchRole));
            return;
    }
    SortInstance(instances, label);
    for (uint64_t i = 0; i < num; i++) {
        DIGSInstanceInfo& instanceInfo = instances[i];
        DIGSRoleDecision decision;
        decision.groupId = instanceInfo.staticInfo.groupId;
        decision.role = switchRole;
        decision.id = instanceInfo.staticInfo.id;
        decision.flexPRatio = 0;
        instanceInfo.staticInfo.role = switchRole;
        roleDecision.emplace_back(decision);
        LOG_I("groupid: " << decision.groupId << " id" << decision.id <<
            ", " << static_cast<int32_t>(decision.role) << ", flexPRatio: " << decision.flexPRatio);
    }
}

void InstanceRoleManager::ComputePDNum(DIGSGroupPDRatio& digsGroupPdRatio, size_t size, size_t& alloc2PInstance) const
{
    size_t pNum = digsGroupPdRatio.pNum;
    size_t dNum = digsGroupPdRatio.dNum;
    double pAbility = digsGroupPdRatio.pAbility;
    double dAbility = digsGroupPdRatio.dAbility;
    double pAbilityAll =  pAbility * static_cast<double>(pNum);
    double dAbilityAll = dAbility * static_cast<double>(dNum);
    size_t alloc2DInstance = 0;
    while (alloc2PInstance + alloc2DInstance < size) {
        if (pAbilityAll <= dAbilityAll) {
            alloc2PInstance++;
            pAbilityAll = pAbilityAll + pAbility * 1;
        } else {
            alloc2DInstance++;
            dAbilityAll = dAbilityAll + dAbility * 1;
        }
    }
    digsGroupPdRatio.pNum = pNum + alloc2PInstance;
    digsGroupPdRatio.dNum = dNum + alloc2DInstance;
}

}