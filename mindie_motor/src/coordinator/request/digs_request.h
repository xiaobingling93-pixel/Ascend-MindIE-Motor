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
#ifndef MINDIE_DIGS_REQUEST_H
#define MINDIE_DIGS_REQUEST_H

#include "common.h"
#include "req_schedule_info.h"
#include "Logger.h"


namespace MINDIE::MS {

enum class DIGSReqState : int32_t {
    INVALID,
    WAITING,  // Waiting to schedule
    SCHEDULING,  // Scheduling
    ALLOCATED,  // Instance allocated
    PREFILL_END,  // Prefill completed
    DECODE_END,  // Decode completed
};

enum class DIGSReqOperation : int32_t {
    REQUEST_UPDATE,
    REQUEST_REMOVE,
};

class DIGSRequest {
public:
    using ControlCallback = std::function<int32_t(DIGSReqOperation, uint64_t, uint64_t, size_t)>;

    using ReleaseCallback = std::function<void(bool)>;

    explicit DIGSRequest(std::string& reqId, size_t inputLen);

    explicit DIGSRequest();

    ~DIGSRequest();

    DIGSRequest(const DIGSRequest &obj) = delete;
    DIGSRequest &operator=(const DIGSRequest &obj) = delete;
    DIGSRequest(const DIGSRequest &&obj) = delete;
    DIGSRequest &operator=(const DIGSRequest &&obj) = delete;

    void InitScheduleInfo(std::unique_ptr<MetaResource>&& demand);

    int32_t UpdateState2PrefillEnd(uint64_t prefillEndTime);

    int32_t UpdateState2Scheduling();

    int32_t UpdateState2Allocated();

    int32_t UpdateState2DecodeEnd(uint64_t prefillEndTime, uint64_t decodeEndTime, size_t outputLength);

    int32_t UpdateState2Invalid();

    void GetRequestControlCallback(ControlCallback& callback);

    const std::string& ReqId() { return reqId_; }

    size_t InputLen() const { return inputLen_; }

    size_t OutputLen() const { return outputLen_; }

    uint64_t PrefillCostTime() const { return prefillCostTime_; }

    const std::unique_ptr<ReqScheduleInfo>& ScheduleInfo() { return scheduleInfo_; }

    DIGSReqState State() const { return state_.load(std::memory_order_relaxed); }

    void SetReleaseCallback(ReleaseCallback callback) { releaseCallback_ = std::move(callback); }

    uint64_t MaxPrefix() const { return maxPrefix_; }

private:
    std::string reqId_;
    size_t inputLen_;
    size_t outputLen_;
    std::unique_ptr<ReqScheduleInfo> scheduleInfo_ = nullptr;

    std::atomic<DIGSReqState> state_;
    uint64_t createTime_;
    uint64_t scheduleTime_;
    uint64_t startTime_;
    uint64_t prefillEndTime_;
    uint64_t decodeEndTime_;
    uint64_t prefillCostTime_;

    ReleaseCallback releaseCallback_ = nullptr;

    uint64_t maxPrefix_ = 0;

    friend std::ostream& operator<<(std::ostream& os, const DIGSRequest& req)
    {
        os << "{\"reqId\":" << req.reqId_ << ",\"createTime\":" << req.createTime_ << ",\"scheduleTime\":"
            << req.scheduleTime_ << ",\"startTime\":" << req.startTime_ << ",\"prefillEndTime\":" << req.prefillEndTime_
            << ",\"decodeEndTime\":" << req.decodeEndTime_ << "}";
        return os;
    }
};

} // namespace MINDIE::MS
#endif
