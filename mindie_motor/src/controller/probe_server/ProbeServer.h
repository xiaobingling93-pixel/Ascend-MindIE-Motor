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
#ifndef MINDIE_MS_PROBE_SERVER_H
#define MINDIE_MS_PROBE_SERVER_H
#include <memory>
#include <cstdint>
#include "HttpServer.h"
namespace MINDIE::MS {
/// The server of the controller.
///
/// This class offers HTTP service for startup status checks and health checks.
class ProbeServer {
public:
    ProbeServer(const ProbeServer &obj) = delete;
    ProbeServer &operator=(const ProbeServer &obj) = delete;
    ProbeServer(ProbeServer &&obj) = delete;
    ProbeServer &operator=(ProbeServer &&obj) = delete;
    ProbeServer() = default;

    /// Destructor.
    ///
    /// This destructor call the Stop method, which stops the server and the thread.
    ~ProbeServer();

    /// Initialize the server.
    ///
    /// \return The result of the initialization. 0 indicates success. -1 indicates failure.
    int32_t Init();

    /// Run the server.
    ///
    /// \return The result of the running. 0 indicates success. -1 indicates failure.
    int32_t Run();

    /// Stop server and threads.
    void Stop();

    /// Handle Get requests for startup status.
    ///
    /// \return The result of the startup status check. It always returns the code OK.
    std::pair<ErrorCode, Response> GetStartupHandler(const Http::request<Http::string_body> &req) const;

    /// Handle Get requests for health status.
    ///
    /// \return The result of the health check. It always returns the code OK.
    std::pair<ErrorCode, Response> GetHealthHandler(const Http::request<Http::string_body> &req) const;
private:
    std::unique_ptr<HttpServer> mServer = nullptr;       /// The HTTP server.
    std::atomic<bool> mRun = { true };                   /// The run status of the HTTP server.
    std::unique_ptr<std::thread> mMainThread = nullptr;  /// The thread that executes the Run method.
};
}
#endif // MINDIE_MS_PROBE_SERVER_H
