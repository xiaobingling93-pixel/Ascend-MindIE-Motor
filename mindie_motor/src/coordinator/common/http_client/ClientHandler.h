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
#ifndef MINDIE_MS_COORDINATOR_CLIENT_HANDLER_H
#define MINDIE_MS_COORDINATOR_CLIENT_HANDLER_H

#include <map>
#include <memory>
#include <functional>

namespace MINDIE::MS {
class ClientConnection;

using CHFun = std::function<void(std::shared_ptr<ClientConnection>)>;

enum class ClientHandlerType : int8_t {
    HEADER_RES, // 消息头响应回调
    RES, // 普通http响应回调
    CHUNK_BODY_RES, // chunked body响应回调
    REQ, // 写请求的回调
    HEADER_RES_ERROR, // 消息头响应出错回调
    CHUNK_BODY_RES_ERROR, // chunked body响应出错回调
    REQ_ERROR, // 写出错的回调
};

class ClientHandler {
public:
    ClientHandler() = default;
    ~ClientHandler() = default;
    void RegisterFun(ClientHandlerType type, const CHFun& fun);
    CHFun GetFun(ClientHandlerType type);
    bool Empty() const;

private:
    std::map<ClientHandlerType, CHFun> callBackMap;
};

}
#endif