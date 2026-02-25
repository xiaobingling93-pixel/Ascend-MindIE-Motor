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
#ifndef MINDIE_MS_COORDINATOR_CONFIGURE_H
#define MINDIE_MS_COORDINATOR_CONFIGURE_H

#include <string>
#include <mutex>
#include "nlohmann/json.hpp"
#include "ConfigParams.h"

namespace MINDIE::MS {

enum class DeployMode {
    SINGLE_NODE,  // 单机
    PD_SEPARATE // PD分离
};

enum class AlgorithmMode {
    LOAD_BALANCE,    // 负载均衡
    CACHE_AFFINITY,  // Cache亲和
    ROUND_ROBIN // 轮询算法
};

struct HttpConfig {
    size_t connectionPoolMaxConn = 10000; // 10000默认值
    size_t serverThreadNum = 1;
    size_t clientThreadNum = 1;
    size_t httpTimeoutS = 10; // 10秒默认值
    size_t keepAliveS = 180; // 180秒默认值
    std::string controllerIP;
    std::string predIp;
    std::string predPort;
    std::string managementIp;
    std::string managementPort;
    std::string statusPort;
    std::string externalPort;
    std::string alarmPort;
    std::string serverName;
    std::string userAgent;
    bool allAllZeroIpListening;
};

struct MetricsConfig {
    bool metrics = false;
    size_t triggerSize = 100; // 100默认值
};

struct PodEtcdBackUpConfig {
    bool funSw = false;
    std::string serverDns {};
    uint32_t serverPort = 0;
};

struct PrometheusMetricsConfig {
    size_t reuseTime = 3; // 3默认值
};

struct ExceptionConfig {
    size_t maxRetry = 5; // 5默认值
    size_t scheduleTimeout = 60; // 60秒默认值
    size_t firstTokenTimeout = 60; // 60秒默认值
    size_t inferTimeout = 300; // 300秒默认值
    size_t tokenizerTimeout = 300; // 300秒默认值
};

struct RequestLimit {
    size_t connMaxReqs = 10000; // 10000默认值
    size_t singleNodeMaxReqs = 1000; // 1000默认值
    size_t maxReqs = 10000; // 10000默认值
    size_t bodyLimit = 10485760; // 10485760表示10MB默认值
    double reqCongestionAlarmThreshold = 0.85;
    double reqCongestionClearThreshold = 0.75;
};

struct Configure {
    float strTokenRate = 4.2; // 4.2默认值
    HttpConfig httpConfig;
    MetricsConfig metricsConfig;
    PrometheusMetricsConfig promMetricsConfig;
    ExceptionConfig exceptionConfig;
    RequestLimit reqLimit;
    std::map<std::string, std::string> schedulerConfig;
    TlsItems controllerServerTlsItems;
    TlsItems statusTlsItems;
    TlsItems externalTlsItems;
    
    TlsItems requestServerTlsItems;
    TlsItems mindieClientTlsItems;
    TlsItems mindieMgmtTlsItems;
    TlsItems alarmClientTlsItems;
    TlsItems coordinatorEtcdTlsItems;
    PodEtcdBackUpConfig coordinatorBackUpConfig;
    int32_t SetControllerIP(std::string controllerIP);
    bool checkMountedFiles = true;
    bool IsMaster();
    void SetMaster(bool master);
    bool IsAbnormal();
    void SetAbnormal(bool abnormal);
    bool CheckBackup();
    static Configure *Singleton()
    {
        static Configure singleton;
        return &singleton;
    }
    int32_t Init();
    void SetScheduleTimeout(size_t scheduleTimeout);
    void SetFirstTokenTimeout(size_t firstTokenTimeout);
    void SetInferTimeout(size_t inferTimeout);
private:
    int32_t ReadHttpConfig(const nlohmann::json &config);
    int32_t ReadMetricsConfig(const nlohmann::json &config);
    int32_t ReadExceptionConfig(const nlohmann::json &config);
    int32_t ReadLogInfo(const nlohmann::json &config, const std::string& configFile) const;
    int32_t ReadSchedulerConfig(const nlohmann::json &config);
    int32_t ReadTlsConfig(const nlohmann::json &config);
    int32_t ReadRequestLimit(const nlohmann::json &config);
    int32_t ReadPrometheusMetricsConfig(const nlohmann::json &config);
    int32_t ReadStrTokenRateConfig(const nlohmann::json &config);
    void ValidateReqLimitAlarmConfig();
    int mHeartbeatProducerIntervalMs = 5000;
    int mHeartbeatMonitorIntervalMs = 3000;
    int mHeartbeatTimeoutMs = 15000;
    int32_t ReadBackupConfig(const nlohmann::json &config);
    int32_t CheckBackupValid(nlohmann::json confJson);
    bool isMaster = false;
    bool isAbnormal = false;
    bool backupEnable = false;
    std::mutex mtx;
};

}
#endif