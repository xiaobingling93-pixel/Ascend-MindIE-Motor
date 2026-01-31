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
#ifndef MINDIE_MS_HTTP_SERVER_H
#define MINDIE_MS_HTTP_SERVER_H

#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <boost/asio.hpp>
#include <cstdlib>
#include <csignal>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <set>
#include <deque>
#include "ConfigParams.h"
#include "Logger.h"

namespace Beast = boost::beast;
namespace Http = Beast::http;
namespace Net = boost::asio;
namespace Ssl = boost::asio::ssl;
using Tcp = boost::asio::ip::tcp;

namespace MINDIE::MS {
struct Response {
    std::string message {};
    std::string data {};
};
using UrlCallBack = std::function<std::pair<ErrorCode, Response>(const Http::request<Http::string_body>
        &req)>;
class HttpServer {
public:
    HttpServer(const HttpServer &) = delete;
    HttpServer &operator=(const HttpServer &) = delete;
    explicit HttpServer(size_t ioContextPoolSize = 1, int maxConnections = 1) : mIoc(ioContextPoolSize),
        mMaxConnections(maxConnections) {}

    Http::message_generator HandleRequest(Http::request<Http::string_body> &&req, const std::string &ip);

    int Run(const MINDIE::MS::HttpServerParams &httpParams);

    int Stop();
    void AddConnection()
    {
        mCurrentConnections.fetch_add(1);
    }
    void SubConnection()
    {
        mCurrentConnections.fetch_sub(1);
    }
    bool ReachMaxConnection() const
    {
        return mCurrentConnections.load() >= mMaxConnections;
    }

    int RegisterGetUrlHandler(const std::string &url, UrlCallBack func)
    {
        if (mGetUrlHandler.find(url) != mGetUrlHandler.end()) {
            LOG_I("[HttpServer] Register GET URL handler failed, URL %s is registered.", url.c_str());
            return -1;
        }
        mGetUrlHandler[url] = func;
        return 0;
    }

    int RegisterPostUrlHandler(const std::string &url, UrlCallBack func)
    {
        if (mPostUrlHandler.find(url) != mPostUrlHandler.end()) {
            LOG_I("[HttpServer] Register POST URL handler failed, URL %s is registered.", url.c_str());
            return -1;
        }
        mPostUrlHandler[url] = func;
        return 0;
    }

    int RegisterDeleteUrlHandler(const std::string &url, UrlCallBack func)
    {
        if (mDeleteUrlHandler.find(url) != mDeleteUrlHandler.end()) {
            LOG_I("[HttpServer] Register DELETE URL handler failed, URL %s is registered.", url.c_str());
            return -1;
        }
        mDeleteUrlHandler[url] = func;
        return 0;
    }

private:
    std::map<std::string, UrlCallBack>& GetUrlHandler(Http::request<Http::string_body> &req);
    void ConnectionRun(const Tcp::socket &socket, Ssl::context &ctx);
    void OnAccept(Beast::error_code ec, Tcp::socket socket);
    Net::io_context mIoc;
    std::map<std::string, UrlCallBack> mGetUrlHandler {};
    std::map<std::string, UrlCallBack> mPostUrlHandler {};
    std::map<std::string, UrlCallBack> mDeleteUrlHandler {};
    bool mStop = false;
    uint32_t mTimeOut = 10; // 一次请求处理的最大时间
    std::mutex mMutex;
    std::condition_variable mCondition;
    Ssl::context mSslCtx = Ssl::context(Ssl::context::tlsv13_server);
    bool isConnecting = false;
    std::atomic<int> mCurrentConnections{0};
    int mMaxConnections = 1; // 1最大支持1个并发链接
    std::pair<char *, int32_t> mPassword = {nullptr, 0};
    VerifyItems verifyItems = {false, "", "", "", "", true};
};

}  // namespace MINDIE::MS

#endif