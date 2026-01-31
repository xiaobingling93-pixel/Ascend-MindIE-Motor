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
#include "Communication.h"
#include "ClientConnection.h"
#include "Configure.h"

namespace MINDIE::MS {

void SendErrorRes(std::shared_ptr<ServerConnection> connection, boost::beast::http::status status,
    boost::beast::string_view body)
{
    if (connection == nullptr) {
        return;
    }
    ServerRes res;
    res.body = body;
    res.state = status;
    res.contentType = "text/plain";
    connection->SendRes(res);
}

}