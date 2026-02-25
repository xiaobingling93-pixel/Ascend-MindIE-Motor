/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MINDIE_DYNAMIC_LOG_HANDLER_H
#define MINDIE_DYNAMIC_LOG_HANDLER_H

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include "Logger.h"

namespace MINDIE {
namespace MS {
class LogLevelDynamicHandler {
public:
    static LogLevelDynamicHandler &GetInstance();
    static void Init(size_t intervalMs = 5000, std::string component = "Controller", bool needWriteToFile = false);
    static void Stop();
    LogLevelDynamicHandler(const LogLevelDynamicHandler &) = delete;
    LogLevelDynamicHandler &operator=(const LogLevelDynamicHandler &) = delete;
    LogLevelDynamicHandler(LogLevelDynamicHandler &&) = delete;
    LogLevelDynamicHandler &operator=(LogLevelDynamicHandler &&) = delete;

private:
    LogLevelDynamicHandler();
    ~LogLevelDynamicHandler();
    std::string GetConfigPath(std::string component) const;
    bool GetHomePath(std::string &homePath) const;
    void GetAndSetLogConfig();
    void InsertLogConfigToFile();
    void SetConfigFile(const std::string key, const std::string value, bool isNumber);
    void ClearLogConfigParam();
    bool CheckDynamicLogLevelChanged(const nlohmann::json &dynamicLogLevel);
    bool CheckValidHoursChanged(const nlohmann::json &dynamicLogLevelValidHours);
    void UpdateDynamicLogParam(const bool sameDynamicLogLevel, const bool sameValidHours);
    void UpdateDynamicLogParamToFile();
    bool IsCurrentTimeWithinValidRange(const std::string &validTimeStr, int validHours) const;
    bool CheckAndFlushInvalidParam(const nlohmann::json &logConfigJson);
    bool CheckLogConfig(const std::string &jsonPath, nlohmann::json &inputJsonData) const;
    bool ReadLogConfig(const std::string &jsonPath, nlohmann::json &inputJsonData) const;
    static bool IsGreaterThanNow(const std::string& other);

private:
    static std::atomic<LogLevelDynamicHandler *> instance_;
    static std::mutex mtx_;
    std::atomic<bool> isRunning_{false};
    size_t intervalMs_ = 0;
    bool needWriteToFile_ = false;
    std::string jsonPath_;
    bool hasSetDynamicLog_ = false;
    std::string lastLogLevel_;
    int lastValidHours_ = 2;
    std::string lastValidTime_;
    std::string currentLevel_;
    int currentValidHours_ = 2;
    std::string currentValidTime_;
    std::string defaultLogLevel_; // default value is from the environment variable
};

}
}

#endif // MINDIE_DYNAMIC_LOG_HANDLER_H
