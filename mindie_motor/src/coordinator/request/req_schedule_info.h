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
#ifndef MINDIE_DIGS_REQ_SCHEDULE_INFO_H
#define MINDIE_DIGS_REQ_SCHEDULE_INFO_H

#include "digs_scheduler/meta_resource.h"

namespace MINDIE::MS {

class ReqScheduleInfo {
public:
    explicit ReqScheduleInfo(std::string& reqId, std::unique_ptr<MetaResource> demand)
        : reqId_(reqId), groupId_(common::ILLEGAL_INSTANCE_ID), prefillInst_(common::ILLEGAL_INSTANCE_ID),
        decodeInst_(common::ILLEGAL_INSTANCE_ID), demand_(std::move(demand)) {};

    ~ReqScheduleInfo() = default;

    const std::string& ReqId() const { return reqId_; }

    const std::unique_ptr<MetaResource>& Demand() const { return demand_; }

    uint64_t PrefillInst() const { return prefillInst_; }

    uint64_t DecodeInst() const { return decodeInst_; }

    uint64_t GroupId() const { return groupId_; }

    void SetPrefillInst(uint64_t prefill) { prefillInst_ = prefill; }

    void SetDecodeInst(uint64_t decode) { decodeInst_ = decode; }

    void SetGroupId(uint64_t groupId) { groupId_ = groupId; }

    bool PrefillRelease()
    {
        bool release = false;
        std::call_once(prefillReleased_, [&release]() { release = true; });
        return release;
    }

    bool DecodeRelease()
    {
        bool release = false;
        std::call_once(decodeReleased_, [&release]() { release = true; });
        return release;
    }

private:
    std::string& reqId_;
    uint64_t groupId_;
    uint64_t prefillInst_;
    uint64_t decodeInst_;
    std::unique_ptr<MetaResource> demand_;

    std::once_flag prefillReleased_;
    std::once_flag decodeReleased_;
};

}
#endif
