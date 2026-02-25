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
#include "request_profiler.h"

namespace MINDIE::MS {

RequestProfiler::RequestProfiler(const RequestConfig& config) : maxSummaryCount_(config.maxSummaryCount)
{
}

void RequestProfiler::ExportEndedReq(std::shared_ptr<DIGSRequest>& req)
{
    std::unique_lock<std::mutex> lock(summaryMutex_);
    inputLengths_.push_back(req->InputLen());
    outputLengths_.push_back(req->OutputLen());

    // 超过最大容量时，丢弃最前面的请求数据
    if (inputLengths_.size() > maxSummaryCount_) {
        inputLengths_.pop_front();
        outputLengths_.pop_front();
    }
}

void RequestProfiler::CalculateSummary()
{
    std::unique_lock<std::mutex> lock(summaryMutex_);
    if (inputLengths_.empty() || outputLengths_.empty()) {
        avgInputLength_ = 0;
        avgOutputLength_ = 0;
        return;
    }
    size_t totalInputLength = std::accumulate(inputLengths_.begin(), inputLengths_.end(), size_t(0));
    avgInputLength_ = totalInputLength / inputLengths_.size();
    size_t totalOutputLength = std::accumulate(outputLengths_.begin(), outputLengths_.end(), size_t(0));
    avgOutputLength_ = totalOutputLength / outputLengths_.size();
    inputLengths_.clear();
    outputLengths_.clear();
}

double RequestProfiler::GetAvgInputLength() const
{
    return avgInputLength_;
}

double RequestProfiler::GetAvgOutputLength() const
{
    return avgOutputLength_;
}
}
