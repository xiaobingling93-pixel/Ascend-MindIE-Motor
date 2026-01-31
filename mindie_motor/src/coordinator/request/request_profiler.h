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
#ifndef MINDIE_DIGS_REQUEST_PROFILER_H
#define MINDIE_DIGS_REQUEST_PROFILER_H

#include "digs_request.h"
#include "digs_config.h"

namespace MINDIE::MS {

class RequestProfiler {
public:
    explicit RequestProfiler(const RequestConfig& config);

    ~RequestProfiler() = default;

    void ExportEndedReq(std::shared_ptr<DIGSRequest>& req);

    void CalculateSummary();

    double GetAvgInputLength() const;

    double GetAvgOutputLength() const;

private:
    std::function<int32_t(std::shared_ptr<DIGSRequest>& req)> exportFunc_;

    std::deque<size_t> inputLengths_;

    std::deque<size_t> outputLengths_;

    size_t maxSummaryCount_ = common::DEFAULT_MAX_SUMMARY_COUNT;

    size_t avgInputLength_ = 0;

    size_t avgOutputLength_ = 0;

    std::mutex summaryMutex_;
};
}
#endif
