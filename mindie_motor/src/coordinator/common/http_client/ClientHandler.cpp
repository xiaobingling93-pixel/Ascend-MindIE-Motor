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
#include "ClientHandler.h"

namespace MINDIE::MS {
void ClientHandler::RegisterFun(ClientHandlerType type, const CHFun& fun)
{
    callBackMap[type] = fun;
}

CHFun ClientHandler::GetFun(ClientHandlerType type)
{
    auto iter = callBackMap.find(type);
    if (iter != callBackMap.end()) {
        return iter->second;
    }
    return nullptr;
}

bool ClientHandler::Empty() const
{
    return callBackMap.empty();
}

}