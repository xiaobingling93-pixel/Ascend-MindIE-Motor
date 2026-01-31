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
#include <string>
#include "ServerHandler.h"

namespace MINDIE::MS {
static constexpr uint32_t URL_LIMIT = 4096;
static bool MatchCore(const std::string &pattern, const std::string &str)
{
    if (pattern.empty()) {
        return str.empty();
    }
    if (pattern[0] == '*') {
        // 跳过'*'，并检查剩余的模式和字符串
        return MatchCore(pattern.substr(1), str) ||
            (!str.empty() && (MatchCore(pattern, str.substr(1)) || MatchCore(pattern.substr(1), str.substr(1))));
    } else {
        return !str.empty() && (pattern[0] == str[0] || pattern[0] == '?') && MatchCore(pattern.substr(1),
            str.substr(1));
    }
}

static bool Match(const std::string &pattern, const std::string &str)
{
    return MatchCore(pattern, str);
}

void ServerHandler::RegisterFun(boost::beast::http::verb method, boost::beast::string_view target, const SHFun& fun)
{
    callBackMap[method][target] = fun;
}

SHFun ServerHandler::GetFun(boost::beast::http::verb method, boost::beast::string_view target)
{
    if (target.size() > URL_LIMIT) {
        return nullptr;
    }
    auto iter = callBackMap.find(method);
    if (iter != callBackMap.end()) {
        auto& funs = iter->second;
        for (auto iter1 = funs.cbegin(); iter1 != funs.cend(); ++iter1) {
            if (Match(iter1->first, target)) {
                return iter1->second;
            }
        }
    }
    return nullptr;
}

void ServerHandler::RegisterFun(ServerHandlerType type, const SHFun& fun)
{
    callBackMap2[type] = fun;
}

SHFun ServerHandler::GetFun(ServerHandlerType type)
{
    auto iter = callBackMap2.find(type);
    if (iter != callBackMap2.end()) {
        return iter->second;
    }
    return nullptr;
}
}