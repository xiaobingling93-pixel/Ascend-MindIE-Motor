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
#include <arpa/inet.h>
#include "Logger.h"
#include "ConnectionPool.h"

namespace MINDIE::MS {

ConnectionPool::ConnectionPool(HttpClientAsync& clientInit) : client(clientInit) {}

std::shared_ptr<ClientConnection> ConnectionPool::ApplyConn(boost::beast::string_view ip,
    boost::beast::string_view port, const ClientHandler& clientHandler, uint32_t timeout)
{
    LOG_D("[ConnectionPool] Apply connection. Connection size is %lu.", client.GetConnSize());
    std::unique_lock<std::mutex> lock(mtx);
    auto connIds = client.FindId(ip, port);
    for (auto &connId : connIds) {
        auto conn = client.GetConnection(connId);
        if (conn == nullptr || conn->IsClose() || !conn->IsAvailable()) {
            LOG_D("[ConnectionPool] conn is closed or not available, address: %s:%s", ip.data(), port.data());
            continue;
        }
        conn->SetAvailable(false);
        conn->SetHandler(clientHandler);
        conn->SetTimeout(timeout);
        LOG_D("[ConnectionPool] find a connection in pool, connection id: %u", connId);
        return conn;
    }
    return NewConnection(ip, port, clientHandler, timeout);
}

std::shared_ptr<ClientConnection> ConnectionPool::NewConnection(boost::beast::string_view ip,
    boost::beast::string_view port, const ClientHandler& clientHandler, uint32_t timeout)
{
    uint32_t connId;
    auto ret = client.AddConnection(ip, port, connId, clientHandler, timeout);
    if (!ret) {
        LOG_E("[%s] [ConnectionPool] Add connection failed.",
            GetErrorCode(ErrorType::CALL_ERROR, CoordinatorFeature::CONNECTION_POOL).c_str());
        return nullptr;
    }
    auto conn = client.GetConnection(connId);
    if (conn == nullptr) {
        LOG_E("[%s] [ConnectionPool] Get connection failed, connection id is %u.",
            GetErrorCode(ErrorType::NOT_FOUND, CoordinatorFeature::CONNECTION_POOL).c_str(),
            connId);
        return nullptr;
    }
    conn->SetAvailable(false);
    LOG_D("[ConnectionPool] new connection succeed for %s:%s.", ip.data(), port.data());
    return conn;
}

}