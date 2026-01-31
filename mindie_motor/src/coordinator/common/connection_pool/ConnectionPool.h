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
#ifndef MINDIE_MS_COORDINATOR_CONNECTION_POOL_H
#define MINDIE_MS_COORDINATOR_CONNECTION_POOL_H

#include <mutex>
#include <cstdint>
#include "HttpClientAsync.h"
#include "ServerConnection.h"

namespace MINDIE::MS {

class ConnectionPool {
public:
    explicit ConnectionPool(HttpClientAsync& clientInit);
    ~ConnectionPool() = default;
    std::shared_ptr<ClientConnection> ApplyConn(boost::beast::string_view ip, boost::beast::string_view port,
        const ClientHandler& clientHandler = {}, uint32_t timeout = 0);

private:
    HttpClientAsync& client;
    std::mutex mtx;
    std::shared_ptr<ClientConnection> NewConnection(boost::beast::string_view ip, boost::beast::string_view port,
        const ClientHandler& clientHandler = {}, uint32_t timeout = 0);
};

}
#endif