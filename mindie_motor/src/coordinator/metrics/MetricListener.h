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
#ifndef MINDIE_MS_COORDINATOR_METRICS_LISTENER_H
#define MINDIE_MS_COORDINATOR_METRICS_LISTENER_H

#include <chrono>
#include <string>
#include <mutex>
#include "ClusterNodes.h"

namespace MINDIE::MS {

class MetricListener {
public:
    MetricListener(std::unique_ptr<ClusterNodes>& instancesRec, size_t reuse): instancesRecord(instancesRec),
        reuseTime(reuse) {}

    /// Request /metrics handler.
    ///
    /// If coordinator receive the request /metrics, this function will be called.
    /// Return prometheus metrics of all mindie-server clusters.
    ///
    /// \param connection Request tcp connection.
    void PrometheusMetricsHandler(std::shared_ptr<ServerConnection> connection);

private:
    size_t GetTimeInSec() const
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::unique_ptr<ClusterNodes>& instancesRecord;
    const size_t reuseTime;
    std::mutex mtx {};
    size_t lastCallTime = 0;
    std::string lastMetrics {};
};

}
#endif