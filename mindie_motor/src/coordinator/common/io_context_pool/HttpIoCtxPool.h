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
#ifndef MINDIE_MS_COORDINATOR_IO_CONTEXT_POOL_H
#define MINDIE_MS_COORDINATOR_IO_CONTEXT_POOL_H

#include <list>
#include <memory>
#include <vector>
#include <thread>
#include "boost/asio.hpp"

namespace MINDIE::MS {
class HttpIoCtxPool {
public:
    HttpIoCtxPool();
    int32_t Init(uint32_t poolSize);
    void Run();
    void Stop();
    boost::asio::io_context& GetHttpIoCtx();

private:
    HttpIoCtxPool(const HttpIoCtxPool& other) = delete;
    HttpIoCtxPool& operator=(const HttpIoCtxPool& other) = delete;
    uint32_t nextHttpIoCtx;
    std::mutex mtx;
    std::vector<std::shared_ptr<boost::asio::io_context>> httpIoCtxs;
    std::list<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workVec;
    std::vector<std::thread> threadVec;
};
}
#endif