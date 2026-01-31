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
#ifndef MINDIE_DIGS_COMMON_H
#define MINDIE_DIGS_COMMON_H

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "digs_instance.h"
#include "Logger.h"


namespace MINDIE::MS::common {

const size_t ILLEGAL_INSTANCE_ID = std::numeric_limits<uint64_t>::max();

const uint64_t INVALID_SCORE = std::numeric_limits<uint64_t>::max();

const size_t STAGE_SIZE = 2;

constexpr double DOUBLE_EPS = 1e-6;

constexpr size_t DEFAULT_MAX_SUMMARY_COUNT = 1024 * 1024;

enum class Status {
    OK = 0,
    ILLEGAL_PARAMETER = -1,
    RESOURCE_NOT_FOUND = -2,
    STATE_ERROR = -3,
    TIMEOUT = -4,
    NO_SATISFIED_RESOURCE = -5,
    STATISTICAL_ERROR = -6,
};

size_t GetBlockSize();

void SetBlockSize(const size_t &blockSize);

/**
 * Get current time in date
 * @return uint64
 */
uint64_t GetTimeNow();

/**
 * Calculate block num
 * @param tokens
 * @param blockSize
 * @return blockNum
 */
size_t BlockNum(size_t tokens, size_t blockSize);

void Str2Bool(std::string strValue, bool& boolValue);

bool DoubleIsZero(double dnum);
double MinDouble(double a, double b);
bool DoubleGreater(double a, double b);
bool DoubleLess(double a, double b);

int32_t StrToDouble(const std::string& strValue, const std::string& name, double& dValue);

template<typename T>
std::string Dims2Str(const T* dims, const size_t dimCount)
{
    std::string str("[");
    for (size_t index = 0; index < dimCount; index++) {
        if (index > 0) {
            str += ",";
        }
        if constexpr (std::is_same_v<T, std::string>) {
            str += dims[index];
        } else {
            str += std::to_string(dims[index]);
        }
    }
    str += "]";
    return str;
}

template<typename T>
bool CanConvertConfigType(const std::string& strVal, T& config)
{
    if constexpr (std::is_same_v<T, std::string>) {
        config = strVal;
        return true;
    } else {
        try {
            if constexpr (std::is_same_v<T, uint32_t> || std::is_same_v<T, size_t>) {
                config = std::stoul(strVal);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                config = std::stoi(strVal);
            } else if constexpr (std::is_same_v<T, double>) {
                config = std::stod(strVal);
            }
            return true;
        } catch (const std::invalid_argument& e) {
            LOG_W("[%s] [DIGS] Config cannot convert to size_t from string, use default value.",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        } catch (std::out_of_range& e) {
            LOG_W("[%s] [DIGS] Config out of range, use default value.",
                  GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str());
        }
        return false;
    }
}

template<typename T>
bool GetConfig(const std::string& name, T& config, const std::map<std::string, std::string>& configs)
{
    auto iter = configs.find(name);
    if (iter == configs.end()) {
        LOG_W("[%s] [DIGS] Config %s is not found, use default value.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
              name.c_str());
        return false;
    }

    return CanConvertConfigType(iter->second, config);
}

template<typename T>
std::string ToStr(const T& obj)
{
    std::ostringstream s;
    s << obj;
    return s.str();
}
}
#endif
