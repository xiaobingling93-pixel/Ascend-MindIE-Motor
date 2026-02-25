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
#include <stdexcept>
#include "Logger.h"
#include "HttpIoCtxPool.h"

namespace MINDIE::MS {
HttpIoCtxPool::HttpIoCtxPool() : nextHttpIoCtx(0) {}

int32_t HttpIoCtxPool::Init(uint32_t poolSize)
{
    if (poolSize == 0) {
        LOG_E("[%s] [HttpIoCtxPool] Initialze failed. Pool size should not be 0.",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::HTTPIO_CTXPOOL).c_str());
        return -1;
    }
    for (uint32_t i = 0; i < poolSize; ++i) {
        auto ioContext = std::make_shared<boost::asio::io_context>();
        httpIoCtxs.push_back(ioContext);
        workVec.push_back(boost::asio::make_work_guard(*ioContext));
    }
    return 0;
}

void HttpIoCtxPool::Stop()
{
    for (uint32_t i = 0; i < httpIoCtxs.size(); ++i) {
        httpIoCtxs[i]->stop();
    }
    for (uint32_t i = 0; i < threadVec.size(); ++i) {
        if (threadVec[i].joinable()) {
            threadVec[i].join();
        }
    }
    workVec.clear();
    httpIoCtxs.clear();
    threadVec.clear();
}

void HttpIoCtxPool::Run()
{
    if (httpIoCtxs.size() == 0) {
        LOG_E("[%s] [HttpIoCtxPool] Run failed. Pool size is 0.",
            GetErrorCode(ErrorType::INVALID_INPUT, CoordinatorFeature::HTTPIO_CTXPOOL).c_str());
        return;
    }
    for (uint32_t i = 0; i < httpIoCtxs.size(); ++i) {
        threadVec.emplace_back([this, i] { httpIoCtxs[i]->run(); });
    }
}

boost::asio::io_context& HttpIoCtxPool::GetHttpIoCtx()
{
    std::unique_lock<std::mutex> lock(mtx);
    if (httpIoCtxs.size() == 0) {
        throw std::runtime_error("[HttpIoCtxPool] HttpIoCtxs is empty.");
    }
    boost::asio::io_context& ioContext = *httpIoCtxs[nextHttpIoCtx];
    ++nextHttpIoCtx;
    if (nextHttpIoCtx == httpIoCtxs.size()) {
        nextHttpIoCtx = 0;
    }
    return ioContext;
}
}