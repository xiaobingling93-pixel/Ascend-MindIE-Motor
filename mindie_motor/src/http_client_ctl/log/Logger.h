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
#ifndef MINDIE_MS_LOG_H
#define MINDIE_MS_LOG_H

#include <string>
#include <sstream>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <cstdint>
#include "ConfigParams.h"
#include "LoggerDef.h"
namespace MINDIE {
namespace MS {
std::string FilterLogStr(std::string logStr);
std::string GetErrorCode(ErrorType type, ControllerFeature controller);
std::string GetErrorCode(ErrorType type, CoordinatorFeature coordinator);
std::string GetErrorCode(ErrorType type, DeployerFeature deployer);
std::string GetErrorCode(ErrorType type, CommonFeature common);
std::string GetWarnCode(ErrorType type, ControllerFeature controller);
std::string GetWarnCode(ErrorType type, CoordinatorFeature coordinator);
std::string GetWarnCode(ErrorType type, DeployerFeature deployer);
std::string GetWarnCode(ErrorType type, CommonFeature common);
std::string GetCriticalCode(ErrorType type, ControllerFeature controller);
std::string GetCriticalCode(ErrorType type, CoordinatorFeature coordinator);
std::string GetCriticalCode(ErrorType type, DeployerFeature deployer);
std::string GetCriticalCode(ErrorType type, CommonFeature common);

constexpr uint32_t LOG_MSG_STR_SIZE_MIN = 128;
constexpr uint32_t LOG_MSG_STR_SIZE_MAX = 65535;
constexpr uint32_t MAX_SPLIT_NUM = 10;

struct OutputOption {
    bool toFile = true;
    bool toStdout = false;
    uint32_t maxLogFileSize = DEFAULT_MAX_LOG_FILE_SIZE;
    uint32_t maxLogFileNum = DEFAULT_MAX_LOG_FILE_NUM;
    SubModule subModule = SubModule::MS_NONAME;
};

struct DefaultLogConfig {
    std::string logLevel = "INFO";
    OutputOption option;
    std::string runLogPath = "";
    std::string operationLogPath = "";
    uint32_t maxLogStrSize = 8192;
};

class Logger {
public:
    Logger() = default;
    int32_t Init(DefaultLogConfig defaultLogConfig, const nlohmann::json &logFileConfig,
        std::string configFilePath = "");
    int32_t Log(LogType type, LogLevel level, const char *message, ...);
    bool GetMIsLogVerbose();
    uint32_t GetMaxLogStrSize();
    void SetLogLevel(LogLevel level);
    static Logger *Singleton()
    {
        static Logger singleton;
        return &singleton;
    }
    static LogLevel GetLogLevel(const std::string level);

private:
    int32_t InitLogLevel(const std::string &logLevel, const nlohmann::json &logFileConfig,
        const std::string& configFilePath);
    int32_t InitLogFile(LogType type, const std::string &logPath);
    void InitLogOption(OutputOption option, uint32_t maxLogStrSize, const nlohmann::json &logFileConfig,
        const std::string& configFilePath);
    int32_t RotateWrite(LogType type);
    int32_t WriteToFile(LogType type, const std::string &logStr);
    std::string GetLogPrefix(LogType type, LogLevel level);
    LogLevel mLogLevel {LogLevel::MINDIE_LOG_INFO};
    bool mToFile = true;
    bool mToStdout = false;
    bool mIsLogFileReady = false;
    bool mIsLogVerbose = true;
    bool mIsLogEnable = true;
    std::mutex mLogMtx;
    uint32_t mMaxLogStrSize = 512;
    std::map<LogType, std::string> mLogPath;
    std::map<LogType, std::ofstream> mOutStream;
    std::string mOperateLogPath;
    uint32_t mMaxLogFileSize = DEFAULT_MAX_LOG_FILE_SIZE * 1024 * 1024;  // 20M
    uint32_t mMaxLogFileNum = DEFAULT_MAX_LOG_FILE_NUM;
    std::map<LogType, std::time_t> mLogFileStart;
    std::map<LogType, uint32_t> mCurrentSize;
};

#define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG(type, level, msg, ...)                                                              \
    do {                                                                                        \
        std::ostringstream oss;                                                                 \
        if (type != LogType::OPERATION &&                                                       \
            (Logger::Singleton()->GetMIsLogVerbose())) { \
            oss << "[" << FILENAME << ":" << __LINE__ << "] ";                                  \
        }                                                                                       \
        oss << ": " << msg;                                                                     \
        Logger::Singleton()->Log(type, level, oss.str().c_str(), ##__VA_ARGS__);                \
    } while (0)

#define LOG_C(msg, ...) LOG(LogType::RUN, LogLevel::MINDIE_LOG_CRITICAL, msg, ##__VA_ARGS__)
#define LOG_E(msg, ...) LOG(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, msg, ##__VA_ARGS__)
#define LOG_W(msg, ...) LOG(LogType::RUN, LogLevel::MINDIE_LOG_WARN, msg, ##__VA_ARGS__)
#define LOG_I(msg, ...) LOG(LogType::RUN, LogLevel::MINDIE_LOG_INFO, msg, ##__VA_ARGS__)
#define LOG_D(msg, ...) LOG(LogType::RUN, LogLevel::MINDIE_LOG_DEBUG, msg, ##__VA_ARGS__)
#define LOG_P(msg, ...) LOG(LogType::RUN, LogLevel::MINDIE_LOG_PERF, msg, ##__VA_ARGS__)
#define LOG_M(msg, ...) LOG(LogType::OPERATION, LogLevel::MINDIE_LOG_INFO, msg, ##__VA_ARGS__)
}
}
#endif