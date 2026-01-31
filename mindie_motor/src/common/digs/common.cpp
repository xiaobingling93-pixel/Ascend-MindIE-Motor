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
#include "common.h"
#include <type_traits>

namespace MINDIE::MS::common {

size_t g_blockSize = 128;

size_t GetBlockSize()
{
    return g_blockSize;
}

void SetBlockSize(const size_t &blockSize)
{
    g_blockSize = blockSize;
}

uint64_t GetTimeNow()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

size_t BlockNum(size_t tokens, size_t blockSize)
{
    if (blockSize == 0) {
        LOG_E("[%s] [DIGS] Block size is zero, which causes division by zero.",
              GetErrorCode(ErrorType::INVALID_PARAMETER, CommonFeature::DIGS).c_str());
        return 0;
    }
    return (tokens + blockSize - 1) / blockSize;
}

void Str2Bool(std::string strValue, bool& boolValue)
{
    std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
    if (strValue == "true") {
        boolValue = true;
        return;
    }
    boolValue = false;
}

bool DoubleIsZero(double dnum)
{
    return fabs(dnum) < DOUBLE_EPS;
}

double MinDouble(double a, double b)
{
    if (a < b - DOUBLE_EPS) {
        return a;
    }
    return b;
}

bool DoubleGreater(double a, double b)
{
    return a > b + DOUBLE_EPS;
}

bool DoubleLess(double a, double b)
{
    return a < b - DOUBLE_EPS;
}

int32_t StrToDouble(const std::string& strValue, const std::string& name, double& dValue)
{
    double res;
    try {
        res = std::stod(strValue);
    } catch (const std::invalid_argument &e) {
        LOG_W("[%s] [DIGS] Config %s has an invalid argument.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
              name.c_str());
        return static_cast<int32_t>(Status::ILLEGAL_PARAMETER);
    } catch (const std::out_of_range &e) {
        LOG_W("[%s] [DIGS] Config %s has an out-of-range argument.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
              name.c_str());
        return static_cast<int32_t>(Status::ILLEGAL_PARAMETER);
    } catch (...) {
        LOG_W("[%s] [DIGS] Config %s has other error.",
              GetWarnCode(ErrorType::WARNING, CommonFeature::DIGS).c_str(),
              name.c_str());
        return static_cast<int32_t>(Status::ILLEGAL_PARAMETER);
    }
    dValue = res;
    return static_cast<int32_t>(Status::OK);
}

}
