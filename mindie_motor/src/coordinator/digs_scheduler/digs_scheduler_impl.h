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
#ifndef MINDIE_DIGS_SCHEDULER_IMPL_H
#define MINDIE_DIGS_SCHEDULER_IMPL_H

#include "scheduler/BaseScheduler.h"
#include "request/request_manager.h"
#include "resource/resource_manager.h"
#include "global_scheduler.h"
#include "Logger.h"


namespace MINDIE::MS {
class DIGSSchedulerImpl : public DIGSScheduler {
public:
    explicit DIGSSchedulerImpl(DIGSScheduler::Config config);

    ~DIGSSchedulerImpl() override;

    DIGSSchedulerImpl(const DIGSSchedulerImpl &obj) = delete;
    DIGSSchedulerImpl &operator=(const DIGSSchedulerImpl &obj) = delete;
    DIGSSchedulerImpl(const DIGSSchedulerImpl &&obj) = delete;
    DIGSSchedulerImpl &operator=(const DIGSSchedulerImpl &&obj) = delete;

    int32_t RegisterInstance(const std::vector<DIGSInstanceStaticInfo>& instances) override;

    int32_t UpdateInstance(const std::vector<DIGSInstanceDynamicInfo>& instances) override;

    int32_t QueryInstanceScheduleInfo(std::vector<DIGSInstanceScheduleInfo>& info) override;

    int32_t CloseInstance(const std::vector<uint64_t>& instances) override;

    int32_t ActivateInstance(const std::vector<uint64_t>& instances) override;

    int32_t RemoveInstance(const std::vector<uint64_t>& instances) override;

    int32_t ProcReq(std::string reqId, size_t promptLen, ReqType type) override;

    int32_t ProcReq(std::string reqId, const std::string& prompt, ReqType type) override;

    int32_t ProcReq(std::string reqId, const std::vector<uint32_t> &tokenList) override;

    int32_t UpdateReq(std::string reqId, DIGSReqStage stage, uint64_t prefillEndTime, uint64_t decodeEndTime,
                      size_t outputLength) override;

    int32_t QueryRequestSummary(DIGSRequestSummary &summary) override;

    int32_t RegisterPDNotifyAllocation(DIGSScheduler::NotifyPDAllocation callback) override;

    void SetBlockSize(size_t blockSize) override;

private:
    template<typename T>
    void GenerateConfig(const std::string& configName,
                        const DIGSScheduler::Config& configSource,
                        T& targetField) const;
    
    void GenerateScheduleConfig(SchedulerConfig& schedConfig);

    void GenerateRequestConfig(RequestConfig& requestConfig);

    void GenerateResourceConfig(ResourceConfig& resourceConfig);

private:
    DIGSScheduler::Config config_;

    std::unique_ptr<GlobalScheduler> globalScheduler_;

    std::shared_ptr<RequestManager> requestManager_;

    std::shared_ptr<ResourceManager> resourceManager_;

    DIGSScheduler::NotifyPDAllocation callback_;
};
}
#endif
