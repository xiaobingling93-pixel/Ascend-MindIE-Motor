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
#ifndef MINDIE_MS_COORDINATOR_SERVER_HANDLER_H
#define MINDIE_MS_COORDINATOR_SERVER_HANDLER_H

#include <map>
#include <functional>
#include <memory>
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"

namespace MINDIE::MS {
class ServerConnection;

enum class ServerHandlerType : int8_t {
    RES, // 写响应的回调
    RES_CHUNK, // 写chunk响应的回调
    REQ_ERROR, // 读出错
    RES_ERROR, // 写普通响应出错
    RES_CHUNK_ERROR, // 写chunk响应出错
    CLOSE, // 正常断连回调
    EXCEPTION_CLOSE, // 异常断连回调
};

using SHFun = std::function<void(std::shared_ptr<ServerConnection>)>;

class ServerHandler {
public:
    ServerHandler() = default;
    ~ServerHandler() = default;
    void RegisterFun(boost::beast::http::verb method, boost::beast::string_view target, const SHFun& fun);
    SHFun GetFun(boost::beast::http::verb method, boost::beast::string_view target);
    void RegisterFun(ServerHandlerType type, const SHFun& fun);
    SHFun GetFun(ServerHandlerType type);

private:
    std::map<boost::beast::http::verb, std::map<std::string, SHFun>> callBackMap;
    std::map<ServerHandlerType, SHFun> callBackMap2;
};
}
#endif