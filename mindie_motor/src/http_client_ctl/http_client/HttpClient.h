/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MINDIE_MS_HTTP_CLIENT_H
#define MINDIE_MS_HTTP_CLIENT_H

#include <string>
#include "boost/asio/ssl/stream.hpp"
#include "boost/asio.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/asio/ssl/error.hpp"
#include "boost/beast/version.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/ssl.hpp"
#include "boost/beast/http.hpp"
#include "ConfigParams.h"
namespace MINDIE {
namespace MS {
namespace Net = boost::asio;
namespace Beast = boost::beast;
namespace Http = Beast::http;
using Tcp = Net::ip::tcp;
namespace Ssl = Net::ssl;

struct Request {
    std::string target;
    Http::verb method;
    std::map<boost::beast::http::field, std::string> header;
    std::string body;
};

class HttpClient {
public:
    HttpClient() = default;
    int32_t Init(const std::string &host, const std::string &port, const TlsItems &tlsConfig, bool useKMC = true);
    void SetHostAndPort(const std::string &host, const std::string &port);
    int32_t SendRequest(const Request &request, int timeoutSeconds, int retries,
        std::string& responseBody, int32_t &code);
private:
    int32_t SendRequestTls(const Request &request, int timeoutSeconds, int retries,
        std::string& responseBody, std::string &contentType);

    int32_t SendRequestNonTls(const Request &request, int timeoutSeconds, int retries,
        std::string& responseBody, std::string &contentType);

private:
    int32_t SetTlsCtx();
    Ssl::context mContext {Ssl::context::tlsv13_client};
    std::string mHost;
    std::string mPort;
    TlsItems mTlsConfig;
    Net::io_context mIoc;
    bool mUseKMC = true;
    std::pair<char *, int32_t> mPassword = {nullptr, 0};
    VerifyItems verifyItems = {false, "", "", "", "", true};
};

}
}

#endif
