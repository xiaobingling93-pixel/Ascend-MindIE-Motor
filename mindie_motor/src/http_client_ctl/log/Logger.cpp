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
#include "Logger.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <regex>
#include <thread>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <securec.h>
#include <pwd.h>
#include "Util.h"
namespace MINDIE {
namespace MS {
constexpr const char* ENV_LEVEL = "MINDIE_LOG_LEVEL";
constexpr const char* ENV_FILE = "MINDIE_LOG_TO_FILE";
constexpr const char* ENV_STDOUT = "MINDIE_LOG_TO_STDOUT";
constexpr const char* ENV_VERBOSE = "MINDIE_LOG_VERBOSE";
constexpr const char* ENV_LEVEL_OLD = "MINDIEMS_LOG_LEVEL";
constexpr const char* ENV_PATH = "MINDIE_LOG_PATH";
constexpr const char* ENV_ROTATE = "MINDIE_LOG_ROTATE";
constexpr uint32_t LOG_ROTATE_MAX_FILE_NUM = 64;
constexpr uint32_t LOG_ROTATE_MAX_FILE_SIZE = 500;  // 单位MB
constexpr uint32_t LOG_ROTATE_MAX_DAY = 180;
constexpr int32_t MAX_ENV_LENGTH = 256;

bool JsonStringValid(const nlohmann::json &jsonObj, const std::string &key, uint32_t minLen, uint32_t maxLen)
{
    if (!jsonObj.contains(key)) {
        std::cout << "Json object does not contain value " << key << std::endl;
        return false;
    }
    if (!jsonObj[key].is_string()) {
        std::cout << "Json type of value for key " << key << " is not string" << std::endl;
        return false;
    }
    std::string val = jsonObj[key];
    if (val.size() < minLen || val.size() > maxLen) {
        std::cout << "Json string length for key " << key << " must in range of [" << minLen <<
            ", " << maxLen << "]" << std::endl;
        return false;
    }
    return true;
}


bool JsonIntValid(const nlohmann::json &jsonObj, const std::string &key, int64_t min, int64_t max)
{
    if (!jsonObj.contains(key)) {
        std::cout << "Json object does not contain value " << key << std::endl;
        return false;
    }
    if (!jsonObj[key].is_number_integer()) {
        std::cout << "Json type of value for key " << key << " is not int number." << std::endl;
        return false;
    }
    int64_t val = jsonObj[key];

    if (val < min || val > max) {
        std::cout << "Json int value for key " << key << " must in range of [" << min <<
            ", " << max << "]" << std::endl;
        return false;
    }
    return true;
}


bool JsonObjValid(const nlohmann::json &jsonObj, const std::string &key)
{
    if (!jsonObj.contains(key)) {
        std::cout << "Json object does not contain value " << key << std::endl;
        return false;
    }
    if (!jsonObj[key].is_object()) {
        std::cout << "Json type of value for key " << key << " is not int object." << std::endl;
        return false;
    }
    return true;
}

bool JsonBoolValid(const nlohmann::json &jsonObj, const std::string &key)
{
    if (!jsonObj.contains(key)) {
        std::cout << "Json object does not contain value " << key << std::endl;
        return false;
    }
    if (!jsonObj[key].is_boolean()) {
        std::cout << "Json type of value for key " << key << " is not boolean." << std::endl;
        return false;
    }
    return true;
}

static std::string GetLevelStr(const LogLevel level)
{
    switch (level) {
        case LogLevel::MINDIE_LOG_CRITICAL:
            return "[CRITICAL] ";
        case LogLevel::MINDIE_LOG_ERROR:
            return "[ERROR] ";
        case LogLevel::MINDIE_LOG_WARN:
            return "[WARN] ";
        case LogLevel::MINDIE_LOG_INFO:
            return "[INFO] ";
        case LogLevel::MINDIE_LOG_DEBUG:
            return "[DEBUG] ";
        case LogLevel::MINDIE_LOG_PERF:
            return "[PERF] ";
        default:
            return "[] ";
    }
}

static std::string GetMotorEnv(const char* envName)
{
    const char* env = std::getenv(envName);
    if (env == nullptr || std::strlen(env) > MAX_ENV_LENGTH) {
        LOG_W("[%s] [Logger] Env variable %s content is nullptr or length exceeds %d",
              GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::CONTROLLER).c_str(),
              envName, MAX_ENV_LENGTH);
        return "";
    }
    std::vector<std::string> eData;

    std::stringstream envStream(env);
    std::string temp;
    while (std::getline(envStream, temp, ';')) {
        if (!temp.empty()) {
            eData.push_back(temp);
        }
    }

    size_t globalIndex = 0;
    size_t localIndex = 0;
    bool globalFlag = false;
    bool localFlag = false;
    size_t posKey = 0;
    size_t posValue = 0;
    for (size_t i = 0; i < eData.size(); ++i) {
        std::string target = eData[i];
        std::transform(target.begin(), target.end(), target.begin(), ::tolower);
        if (target.find(":") == std::string::npos) {
            globalIndex = i;
            globalFlag = true;
        } else if ((posKey = target.find("ms")) != std::string::npos) {
            if (posKey != target.find_first_not_of(" ")) {
                continue;
            }
            size_t posTmp = target.find_first_not_of(" ", posKey + strlen("ms"));
            if (posTmp == std::string::npos) {
                continue;
            }
            bool findValue = (target[posTmp] == ':');
            posValue = findValue ? posTmp : posValue;
            localIndex = findValue ? i : localIndex;
            localFlag = findValue ? true : localFlag;
        }
    }
    if (localFlag) {
        return eData[localIndex].substr(posValue + 1);
    } else if (globalFlag) {
        return eData[globalIndex];
    } else {
        return "";
    }
}

static bool GetLogConfigFromFile(const nlohmann::json &logFileConfig, const std::string& targetLogKey,
    nlohmann::json& result, const std::string& configFilePath)
{
    if (!JsonObjValid(logFileConfig, "log_info")) {
        std::cout << "[Logger] Configuration file " << configFilePath << " does not contain the 'log_info' field," <<
            " will use environment variables or default log settings." << std::endl;
        return false;
    }
    std::unordered_set<std::string> configStringKeys = {"log_level", "run_log_path", "operation_log_path"};
    std::unordered_set<std::string> configBoolKeys = {"to_file", "to_stdout"};
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> configIntRange = {
        {"max_log_str_size", {LOG_MSG_STR_SIZE_MIN, LOG_MSG_STR_SIZE_MAX}},
        {"max_log_file_size", {1, LOG_ROTATE_MAX_FILE_SIZE}},
        {"max_log_file_num", {1, LOG_ROTATE_MAX_FILE_NUM}}
    };
    if (configStringKeys.find(targetLogKey) != configStringKeys.end()) {
        if (!JsonStringValid(logFileConfig["log_info"], targetLogKey, 0, LOG_MSG_STR_SIZE_MAX)) {
            std::cout << "[Logger] Configuration file " << configFilePath << " does not contain the "<< targetLogKey <<
                " field or is invalid, will use environment variables or default log settings." << std::endl;
            return false;
        }
    } else if (configBoolKeys.find(targetLogKey) != configBoolKeys.end()) {
        if (!JsonBoolValid(logFileConfig["log_info"], targetLogKey)) {
            std::cout << "[Logger] Configuration file " << configFilePath << " does not contain the "<< targetLogKey <<
                " field or is invalid, will use environment variables or default log settings." << std::endl;
            return false;
        }
    } else if (configIntRange.find(targetLogKey) != configIntRange.end()) {
        if (!JsonIntValid(logFileConfig["log_info"], targetLogKey, configIntRange[targetLogKey].first,
            configIntRange[targetLogKey].second)) {
            std::cout << "[Logger] Configuration file " << configFilePath << " does not contain the "<< targetLogKey <<
                " field or is invalid, will use environment variables or default log settings." << std::endl;
            return false;
        }
    }
    result = logFileConfig["log_info"][targetLogKey];
    std::cout << "[Logger] Log configuration " << targetLogKey << " retrieved from config file " << configFilePath <<
        ". This configuration will be deprecated in future versions." << std::endl;
    return true;
}

static bool GetBoolEnv(const char *envName, bool option, const std::string& target, const nlohmann::json &logFileConfig,
    const std::string& configFilePath)
{
    std::string envStr = GetMotorEnv(envName);
    if (envStr.empty() && !target.empty()) {
        nlohmann::json configResult;
        if (GetLogConfigFromFile(logFileConfig, target, configResult, configFilePath)) {
            return configResult.get<bool>();
        } else {
            return option;
        }
    } else if (envStr.empty() && target.empty()) {
        return option;
    }
    envStr.erase(0, envStr.find_first_not_of(" "));
    envStr.erase(envStr.find_last_not_of(" ") + 1);
    std::transform(envStr.begin(), envStr.end(), envStr.begin(), ::tolower);
    if (envStr != "true" && envStr != "1" && envStr != "false" && envStr != "0") {
        std::cout << "The value of the environment parameter " << envName << " is invalid, will use default settings. "
            << "Please choose from '1', 'true', '0', or 'false'." << std::endl;
        return option;
    }
    return (envStr == "true" || envStr == "1");
}

LogLevel Logger::GetLogLevel(const std::string level)
{
    if (level == "CRITICAL") {
        return LogLevel::MINDIE_LOG_CRITICAL;
    } else if (level == "ERROR") {
        return LogLevel::MINDIE_LOG_ERROR;
    } else if (level == "WARN" || level == "WARNING") { // 向前兼容
        return LogLevel::MINDIE_LOG_WARN;
    } else if (level == "INFO") {
        return LogLevel::MINDIE_LOG_INFO;
    } else if (level == "DEBUG") {
        return LogLevel::MINDIE_LOG_DEBUG;
    } else if (level == "NULL") {
        return LogLevel::MINDIE_LOG_DISABLE;
    } else {
        std::cout << "The log level " << level << " is invalid, please chose from 'CRITICAL', 'ERROR', 'WARN' " <<
            ", 'INFO', 'DEBUG', will use default level instead." << std::endl;
        return LogLevel::MINDIE_LOG_UNKNOWN;
    }
}

static std::string GetSubModuleName(SubModule module)
{
    switch (module) {
        case SubModule::MS_CONTROLLER:
            return "controller_";
        case SubModule::MS_COORDINATOR:
            return "coordinator_";
        case SubModule::MS_DEPLOYER:
            return "deployer_";
        default:
            return "";
    }
}

void AddLogTypeToPathStream(LogType type, std::stringstream& retStream, SubModule module)
{
    switch (type) {
        case LogType::RUN:
            retStream << "/debug";
            break;
        case LogType::OPERATION:
            retStream << "/security";
            break;
        default:
            break;
    }

    retStream << "/mindie-ms_" << GetSubModuleName(module) << getpid() << "_";
    int64_t nowMs = GetTimeStampNowInMillisec();
    std::time_t now = static_cast<std::time_t>(nowMs / 1000); // 获取秒级时间
    int milliseconds = static_cast<int>(nowMs % 1000); // 获取毫妙时间
    std::tm nowTm;
    localtime_r(&now, &nowTm);
    retStream << std::put_time(&nowTm, "%Y%m%d%H%M%S");
    retStream << std::setfill('0') << std::setw(3) << milliseconds << ".log"; // 保留3位毫秒时间
}

static std::string GetLogPath(const std::string &logPath, LogType type, SubModule module,
    const nlohmann::json &logFileConfig, std::string configFilePath)
{
    std::unordered_map<LogType, std::string> typeToStr = {{LogType::RUN, "run_log_path"},
        {LogType::OPERATION, "operation_log_path"}};
    std::string envStr = GetMotorEnv(ENV_PATH);
    if (!envStr.empty()) {
        envStr.erase(0, envStr.find_first_not_of(" "));
        envStr.erase(envStr.find_last_not_of(" ") + 1);
    }
    if (envStr.empty()) {
        nlohmann::json configStr;
        if (GetLogConfigFromFile(logFileConfig, typeToStr[type], configStr, configFilePath)) {
            envStr = configStr.get<std::string>();
        } else {
            envStr = logPath;
        }
        envStr.erase(0, envStr.find_first_not_of(" "));
        envStr.erase(envStr.find_last_not_of(" ") + 1);
        if (!envStr.empty()) {
            return envStr;  // 保留原有配置
        }
    }
    if (envStr.find_first_of(";:,") != std::string::npos) {
        envStr = "";
        std::cout << "[WARNING] Log path contains invalid characters, will use default path." << std::endl;
    }
    if (envStr.empty() || envStr[0] != '/') {
        const char *homeDir = std::getenv("HOME");
        if (homeDir == nullptr) {
            LOG_E("[%s] [Logger] HOME environment variable not find.",
                GetErrorCode(ErrorType::NOT_FOUND, CommonFeature::LOGGER).c_str());
            return "";
        }
        std::string homePath = homeDir;
        if (envStr.size() >= 2 && envStr.substr(0, 2) == "~/") {  // 前2位开头为~/
            envStr = homePath + envStr.substr(1) + "/log";
        } else if (envStr.empty()) {
            envStr = homePath + "/mindie/log";
        } else {
            envStr = homePath + "/mindie/log/" + envStr + "/log";  // 相对路径，相对的是默认路径
        }
    } else {
        envStr += "/log";
    }
    std::stringstream retStream;
    retStream << envStr;

    if (envStr.substr(envStr.rfind('/')).find(".") == std::string::npos) {     // 末尾不含'.',当作路径处理
        AddLogTypeToPathStream(type, retStream, module);
    }

    return retStream.str();
}

static std::string GenerateCode(ErrorType type, LogLevel level, SubModule module, int32_t feature)
{
    std::stringstream retStream;
    retStream << "MIE03";   // MS组件编码为03
    switch (level) {
        case LogLevel::MINDIE_LOG_WARN:
            retStream << "W";
            break;
        case LogLevel::MINDIE_LOG_ERROR:
            retStream << "E";
            break;
        case LogLevel::MINDIE_LOG_CRITICAL:
            retStream << "C";
            break;
        default:
            return "";
    }
    switch (module) {
        case SubModule::MS_CONTROLLER:
            if (feature < 0 || feature >= static_cast<int32_t>(ControllerFeature::INVALID_ENUM)) {
                return "";
            }
            break;
        case SubModule::MS_COORDINATOR:
            if (feature < 0 || feature >= static_cast<int32_t>(CoordinatorFeature::INVALID_ENUM)) {
                return "";
            }
            break;
        case SubModule::MS_DEPLOYER:
            if (feature < 0 || feature >= static_cast<int32_t>(DeployerFeature::INVALID_ENUM)) {
                return "";
            }
            break;
        case SubModule::MS_COMMON:
            if (feature < 0 || feature >= static_cast<int32_t>(CommonFeature::INVALID_ENUM)) {
                return "";
            }
            break;
        default:
            return "";
    }
    retStream << std::left << std::hex << std::uppercase << std::setfill('0') << std::setw(2)   // 宽度2
              << static_cast<uint32_t>(module);
    retStream << std::right << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << feature; // 宽度为2
    if (type < ErrorType(0) || type >= ErrorType::INVALID_ENUM) {
        return "";
    }
    retStream << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<uint32_t>(type); // 宽度为2
    return retStream.str();
}

std::string GetErrorCode(ErrorType type, ControllerFeature controller)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_ERROR, SubModule::MS_CONTROLLER, static_cast<int32_t>(controller));
}
std::string GetErrorCode(ErrorType type, CoordinatorFeature coordinator)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_ERROR, SubModule::MS_COORDINATOR, static_cast<int32_t>(coordinator));
}
std::string GetErrorCode(ErrorType type, DeployerFeature deployer)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_ERROR, SubModule::MS_DEPLOYER, static_cast<int32_t>(deployer));
}
std::string GetErrorCode(ErrorType type, CommonFeature common)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_ERROR, SubModule::MS_COMMON, static_cast<int32_t>(common));
}
std::string GetWarnCode(ErrorType type, ControllerFeature controller)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_WARN, SubModule::MS_CONTROLLER, static_cast<int32_t>(controller));
}
std::string GetWarnCode(ErrorType type, CoordinatorFeature coordinator)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_WARN, SubModule::MS_COORDINATOR, static_cast<int32_t>(coordinator));
}
std::string GetWarnCode(ErrorType type, DeployerFeature deployer)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_WARN, SubModule::MS_DEPLOYER, static_cast<int32_t>(deployer));
}
std::string GetWarnCode(ErrorType type, CommonFeature common)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_WARN, SubModule::MS_COMMON, static_cast<int32_t>(common));
}
std::string GetCriticalCode(ErrorType type, ControllerFeature controller)
{
    return GenerateCode(
        type, LogLevel::MINDIE_LOG_CRITICAL, SubModule::MS_CONTROLLER, static_cast<int32_t>(controller));
}
std::string GetCriticalCode(ErrorType type, CoordinatorFeature coordinator)
{
    return GenerateCode(
        type, LogLevel::MINDIE_LOG_CRITICAL, SubModule::MS_COORDINATOR, static_cast<int32_t>(coordinator));
}
std::string GetCriticalCode(ErrorType type, DeployerFeature deployer)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_CRITICAL, SubModule::MS_DEPLOYER, static_cast<int32_t>(deployer));
}
std::string GetCriticalCode(ErrorType type, CommonFeature common)
{
    return GenerateCode(type, LogLevel::MINDIE_LOG_CRITICAL, SubModule::MS_COMMON, static_cast<int32_t>(common));
}

int32_t Logger::InitLogLevel(const std::string &logLevel, const nlohmann::json &logFileConfig,
    const std::string& configFilePath)
{
    std::string envIe = GetMotorEnv(ENV_LEVEL);
    LogLevel level = LogLevel::MINDIE_LOG_UNKNOWN;
    if (!envIe.empty()) {
        envIe.erase(0, envIe.find_first_not_of(" "));
        envIe.erase(envIe.find_last_not_of(" ") + 1);
        std::transform(envIe.begin(), envIe.end(), envIe.begin(), ::toupper);
        level = GetLogLevel(envIe);
    }
    if (level == LogLevel::MINDIE_LOG_UNKNOWN) {
        const char* envMs = std::getenv(ENV_LEVEL_OLD);
        if (envMs != nullptr) {
            level = GetLogLevel(envMs);
            std::cout << "[Logger] Log level configuration retrieved from environment variable 'MINDIEMS_LOG_LEVEL'."
                " This configuration will be deprecated in future versions." << std::endl;
        }
    }

    if (level == LogLevel::MINDIE_LOG_UNKNOWN) {
        nlohmann::json finalLevel = "";
        if (GetLogConfigFromFile(logFileConfig, "log_level", finalLevel, configFilePath)) {
            level = GetLogLevel(finalLevel.get<std::string>());
        } else {
            level = GetLogLevel(logLevel);
        }
    }
    if (level == LogLevel::MINDIE_LOG_DISABLE) {
        mIsLogEnable = false;
        return 0;
    }
    if (level == LogLevel::MINDIE_LOG_UNKNOWN) {
        level = GetLogLevel(logLevel);
    }
    mLogLevel = level;
    mIsLogEnable = true;
    return 0;
}

static bool RotateParamStrToValue(const std::string& str, uint32_t& value, uint32_t limit)
{
    if (!std::all_of(str.begin(), str.end(), ::isdigit)) {
        return false;
    }
    try {
        value = stoi(str);
        return value > 0 && value <= limit;
    } catch (const std::exception &e) {
        std::cout << "Error: Invalid env value: MINDIE_LOG_ROTATE,"
                  << "the invalid paramStr is: " << str << std::endl;
        return false;
    }
}

static void GetLogRotateParam(uint32_t &fileSize, uint32_t &fileNum,
    const nlohmann::json &logFileConfig, const std::string& configFilePath)
{
    std::string envStr = GetMotorEnv(ENV_ROTATE);
    if (!envStr.empty()) {
        envStr.erase(0, envStr.find_first_not_of(" "));
        envStr.erase(envStr.find_last_not_of(" ") + 1);
        std::transform(envStr.begin(), envStr.end(), envStr.begin(), ::tolower);
    }
    if (envStr.empty()) {
        nlohmann::json sizeResult;
        if (GetLogConfigFromFile(logFileConfig, "max_log_file_size", sizeResult, configFilePath)) {
            fileSize = sizeResult.get<uint32_t>() * 1024 * 1024; // 1024表示转化为MB
        }
        nlohmann::json numResult;
        if (GetLogConfigFromFile(logFileConfig, "max_log_file_num", numResult, configFilePath)) {
            fileNum = numResult.get<uint32_t>();
        }
    }
    std::vector<std::string> eData;
    std::stringstream envStream(envStr);
    std::string temp;
    while (std::getline(envStream, temp, ' ')) {
        if (!temp.empty()) {
            eData.push_back(temp);
        }
    }
    uint32_t tmp = 0;
    for (size_t i = 0; i + 1 < eData.size(); ++i) {
        if (eData[i] == "-fs") {
            nlohmann::json result;
            if (RotateParamStrToValue(eData[i + 1], tmp, LOG_ROTATE_MAX_FILE_SIZE)) {
                fileSize =  tmp * 1024 * 1024;  // 1024表示转化为MB
            } else if (GetLogConfigFromFile(logFileConfig, "log_file_size", result, configFilePath)) {
                fileSize = result.get<uint32_t>();
            }
        } else if (eData[i] == "-r") {
            nlohmann::json result;
            if (RotateParamStrToValue(eData[i + 1], tmp, LOG_ROTATE_MAX_FILE_NUM)) {
                fileNum = tmp;
            } else if (GetLogConfigFromFile(logFileConfig, "log_file_num", result, configFilePath)) {
                fileNum = result.get<uint32_t>();
            }
        }
    }
    return ;
}

void Logger::InitLogOption(OutputOption option, uint32_t maxLogStrSize, const nlohmann::json &logFileConfig,
    const std::string& configFilePath)
{
    mIsLogVerbose = GetBoolEnv(ENV_VERBOSE, mIsLogVerbose, "", logFileConfig, configFilePath);
    nlohmann::json result;
    if (GetLogConfigFromFile(logFileConfig, "max_log_str_size", result, configFilePath)) {
        mMaxLogStrSize = result.get<uint32_t>();
    } else {
        mMaxLogStrSize = maxLogStrSize;
    }
    mToFile = GetBoolEnv(ENV_FILE, option.toFile, "to_file", logFileConfig, configFilePath);
    mMaxLogFileSize = option.maxLogFileSize * 1024 * 1024; // 1024表示转化为MB
    mMaxLogFileNum = option.maxLogFileNum;
    mToStdout = GetBoolEnv(ENV_STDOUT, option.toStdout, "to_stdout", logFileConfig, configFilePath);
    GetLogRotateParam(mMaxLogFileSize, mMaxLogFileNum, logFileConfig, configFilePath);
    std::time_t timeStampNow = GetTimeStampNow();
    mLogFileStart[LogType::RUN] = timeStampNow;
    mLogFileStart[LogType::OPERATION] = timeStampNow;
    return;
}

int32_t Logger::InitLogFile(LogType type, const std::string &logPath)
{
    std::string canonicalPath = logPath;
    bool isFileExist = false;
    if (!PathCheck(canonicalPath, isFileExist, 0640, true, true)) { // 日志文件的权限要求是0640
        return -1;
    }
    mLogPath[type] = canonicalPath;
    std::string logCodeInput = GetErrorCode(ErrorType::INVALID_INPUT, CommonFeature::LOGGER);
    std::string logCodeCall = GetErrorCode(ErrorType::CALL_ERROR, CommonFeature::LOGGER);
    if (!isFileExist) {
        FILE* fp = fopen(canonicalPath.c_str(), "w");
        if (fp == nullptr) {
            LOG_E("[%s] [Logger] Cannot not create the file %s", logCodeInput.c_str(), canonicalPath.c_str());
            return -1;
        }
        if (fclose(fp) != 0) {
            LOG_E("[%s] [Logger] Failed to close file: %s", logCodeCall.c_str(), canonicalPath.c_str());
            fp = nullptr;
            return -1;
        }
        fp = nullptr;
    }
    FILE* fp = fopen(canonicalPath.c_str(), "a");
    if (fp == nullptr) {
        LOG_E("[%s] [Logger] Cannot not open and append the file %s", logCodeInput.c_str(), canonicalPath.c_str());
        return -1;
    }
    mCurrentSize[type] = static_cast<uint32_t>(ftell(fp));
    if (fclose(fp) != 0) {
        LOG_E("[%s] [Logger] Failed to close file: %s", logCodeCall.c_str(), canonicalPath.c_str());
        fp = nullptr;
        return -1;
    }
    fp = nullptr;
    if (chmod(canonicalPath.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) != 0) {
        LOG_E("[%s] [Logger] Failed to chmod log file to 640: %s", logCodeCall.c_str(), canonicalPath.c_str());
        return -1;
    }
    mOutStream[type] = std::ofstream(canonicalPath, std::ios::app | std::ios::binary);
    if (!mOutStream[type].is_open()) {
        LOG_E("[%s] [Logger] Failed to open log file %s", logCodeCall.c_str(), canonicalPath.c_str());
        return -1;
    }
    return 0;
}

int32_t Logger::Init(DefaultLogConfig defaultLogConfig, const nlohmann::json &logFileConfig, std::string configFilePath)
{
    std::string logLevel = defaultLogConfig.logLevel;
    OutputOption option = defaultLogConfig.option;
    std::string runLogPath = defaultLogConfig.runLogPath;
    std::string operationLogPath = defaultLogConfig.operationLogPath;
    uint32_t maxLogStrSize = defaultLogConfig.maxLogStrSize;

    InitLogLevel(logLevel, logFileConfig, configFilePath);
    InitLogOption(option, maxLogStrSize, logFileConfig, configFilePath);
    if (!mToFile) {
        LOG_I("[Logger] Log initialize success, and log will not save to file.");
        return 0;
    }
    if (InitLogFile(LogType::RUN,
        GetLogPath(runLogPath, LogType::RUN, option.subModule, logFileConfig, configFilePath)) != 0 ||
        InitLogFile(LogType::OPERATION,
        GetLogPath(operationLogPath, LogType::OPERATION, option.subModule, logFileConfig, configFilePath)) != 0) {
        return -1;
    }
    mIsLogFileReady = true;
    LOG_I("[Logger] Log initialize success, run log file path is %s, operation log file path is %s.",
        mLogPath[LogType::RUN].c_str(),  mLogPath[LogType::OPERATION].c_str());
    return 0;
}

static std::string GetFileName(const std::string &fileName, int32_t index)
{
    if (index == 0) {
        return fileName;
    }
    std::size_t pos = fileName.rfind(".");
    // 生成索引字符串，1位数补零
    std::string indexStr = (index >= 1 && index <= 9)
                            ? "0" + std::to_string(index)
                            : std::to_string(index);
    if (pos != std::string::npos) {
        std::string suffix = fileName.substr(pos + 1);
        std::string name = fileName.substr(0, pos);
        return name + "." + indexStr + "." + suffix;
    }
    return fileName + "." + indexStr;
}

bool Logger::GetMIsLogVerbose()
{
    return this->mIsLogVerbose;
}
uint32_t Logger::GetMaxLogStrSize()
{
    return this->mMaxLogStrSize;
}

void Logger::SetLogLevel(LogLevel level)
{
    mLogLevel = level;
}

int32_t Logger::RotateWrite(LogType type)
{
    for (uint32_t i = mMaxLogFileNum - 1; i > 0; --i) {
        std::string srcFile = GetFileName(mLogPath[type], i - 1);
        std::ifstream f(srcFile.c_str());
        if (!f.good()) {
            continue;
        }
        std::string dstFile = GetFileName(mLogPath[type], i);
        if (std::ifstream(dstFile.c_str()).good() && remove(dstFile.c_str()) != 0) {
            std::cout << "Error: failed to remove log file " << FilterLogStr(dstFile) << std::endl;
            return -1;
        }
        if (rename(srcFile.c_str(), dstFile.c_str()) != 0) {
            std::cout << "Error: failed to rename log file " << FilterLogStr(srcFile) << " to " <<
                FilterLogStr(dstFile) << std::endl;
            return -1;
        }
        if (chmod(dstFile.c_str(), S_IRUSR | S_IRGRP) != 0) {
            std::cout << "Error: failed to chmod log file " << FilterLogStr(dstFile) << std::endl;
            return -1;
        }
    }
    mOutStream[type].close();
    mOutStream[type].open(mLogPath[type], std::ios::app | std::ios::binary);
    if (!mOutStream[type].is_open()) {
        std::cout << "Error: Failed to open log file " << mLogPath[type] << std::endl;
        return -1;
    }
    uint32_t mode = 0640; // 修改文件权限为0640
    if (chmod(mLogPath[type].c_str(), mode) != 0) {
        std::cout << "Error: failed to chmod log file " << mLogPath[type] << std::endl;
        return -1;
    }
    std::cout << "Info: success to rotate log file." << std::endl;
    return 0;
}

std::string FilterLogStr(std::string logStr)
{
    std::regex special = std::regex("\t|\n|\v|\f|\r|\b|\u007F");
    std::regex space = std::regex("\\s+");
    logStr = std::regex_replace(logStr, special, "");
    logStr = std::regex_replace(logStr, space, " ");
    return logStr;
}

static std::string GetHostName()
{
    struct utsname buf;
    if (uname(&buf) != 0) {
        if (buf.nodename == nullptr) {
            return "localhost";
        }
        *buf.nodename = '\0';
    }

    return buf.nodename;
}

std::string Logger::GetLogPrefix(LogType type, LogLevel level)
{
    std::stringstream retStream;
    retStream << GetTimeStrNow();
    if (type == LogType::RUN) {
        if (this->mIsLogVerbose || level == LogLevel::MINDIE_LOG_DEBUG) {
            retStream << "[";
            retStream << getpid();
            retStream << "] [";
            retStream << std::this_thread::get_id();
            retStream << "] [ms] ";
        }
        retStream << GetLevelStr(level);
    } else if (type == LogType::OPERATION) {
        retStream << "[";
        retStream << getpid();
        retStream << "] [";

        retStream << GetHostName() << "] [";

        uid_t uid = geteuid();
        struct passwd *pw = getpwuid(uid);
        if (pw != nullptr) {
            retStream << pw->pw_name;
        }

        retStream << "] [ms] ";
    }
    return retStream.str();
}

int32_t Logger::WriteToFile(LogType type, const std::string &logStr)
{
    std::lock_guard<std::mutex> lock(mLogMtx);
    uint32_t newSize = mCurrentSize[type] + logStr.size();
    bool rotateFlag = newSize > mMaxLogFileSize;

    if (rotateFlag) {
        std::cout << "Warninng: the log file is full, now is going to rotate it" << std::endl;
        if (RotateWrite(type) != 0) {
            std::cout << "Error: Failed to rotate log file!" << std::endl;
            return -1;
        }
        newSize = logStr.size();
    }
    if (mOutStream[type].is_open()) {
        mOutStream[type] << logStr.c_str() << std::endl;
    } else {
        std::cout << "Error: Failed to open log file" << std::endl;
        return -1;
    }
    mCurrentSize[type] = newSize + 1;
    return 0;
}

int32_t Logger::Log(LogType type, LogLevel level, const char *message, ...)
{
    if (!this->mIsLogEnable || (!this->mToFile && !this->mToStdout)) {
        return 0;
    }
    if (type == LogType::RUN && level > mLogLevel && level != LogLevel::MINDIE_LOG_PERF) {
        return 0;
    }

    size_t bufferSize = mMaxLogStrSize * MAX_SPLIT_NUM;
    bufferSize = bufferSize > LOG_MSG_STR_SIZE_MAX ? LOG_MSG_STR_SIZE_MAX : bufferSize;

    char outputStr[bufferSize] = {0};
    if (strcat_s(outputStr, bufferSize, GetLogPrefix(type, level).c_str()) != 0) {
        std::cout << "Error: Failed to strcat_s" << std::endl;
        std::cout << "max log str size " << bufferSize << std::endl;
        return -1;
    }
    size_t headOffset = strlen(outputStr);

    va_list vaList;
    va_start(vaList, message);

    int result = vsnprintf_s(outputStr + headOffset, bufferSize - headOffset,
        bufferSize - headOffset - 1, message, vaList);

    va_end(vaList);

    size_t totalcachedLogLen = strlen(outputStr);

    // mMaxLogStrSize为单次日志打印时, 每行的最大打印量
    for (size_t i = 0; i < totalcachedLogLen; i += mMaxLogStrSize) {
        size_t remain = totalcachedLogLen - i;
        size_t nextLogLen = remain > mMaxLogStrSize ? mMaxLogStrSize : remain;
        std::string nextLog(outputStr + i, nextLogLen);
        std::string logStr = FilterLogStr(nextLog);

        if (this->mToFile && mIsLogFileReady) {
            if (WriteToFile(type, logStr) != 0) {
                return -1;
            }
        } else if (!this->mToStdout && !mIsLogFileReady) {
            std::cout << logStr.c_str() << std::endl;
        }
        if (this->mToStdout) {
            std::cout << logStr.c_str() << std::endl;
        }
    }

    if (result < 0) {
        std::cout << "Logger Warning: Exception accurs when put input string to buffer. "
        "Maybe the length of the input string exceeds the maximum allowed log print length "
        << bufferSize << std::endl;
        return -1;
    }

    return 0;
}
}
}