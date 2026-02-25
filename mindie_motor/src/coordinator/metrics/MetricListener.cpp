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
#include "MetricListener.h"
#include "HttpClient.h"
#include "Metrics.h"
#include "Logger.h"

namespace MINDIE::MS {

void MetricListener::PrometheusMetricsHandler(std::shared_ptr<ServerConnection> connection)
{
    if (!connection) {
        LOG_E("[MetricListener] Invalid null ServerConnection in PrometheusMetricsHandler.");
        return;
    }
    LOG_D("[MetricListener] Handling Prometheus metrics request.");
    ServerRes reply;
    // 复用上一轮结果
    if (GetTimeInSec() - lastCallTime < reuseTime) {
        LOG_I("[MetricListener] Rapid recall detected. Reusing last metric results.");
        reply.body = lastMetrics;
        connection->SendRes(reply);
        return;
    }

    Metrics m;
    m.InitMetricsPattern();
    auto ret = m.GetAndAggregateMetrics(instancesRecord->GetInstanceInfos());
    if (ret.empty()) {
        LOG_W("[%s] [MetricListener] Failed to get and aggregate cluster metrics.",
            GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::METRICS).c_str());
        reply.state = boost::beast::http::status::service_unavailable;
        connection->SendRes(reply);
        return;
    }
    
    {
        std::unique_lock<std::mutex> lock(mtx);
        lastMetrics = ret;
        lastCallTime = GetTimeInSec();
    }
    reply.body = std::move(ret);
    connection->SendRes(reply);
    LOG_D("[MetricListener] Successfully processed Prometheus metrics request. Last call time: %u", lastCallTime);
}
}
