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
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include "gtest/gtest.h"
#define main __main_coordinator__
#include "coordinator/main.cpp"
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "stub.h"
#include "MyThreadPool.h"
#include "HttpClientAsync.h"

using namespace MINDIE::MS;

class TestMaxConnection : public testing::Test {
protected:
    void SetUp()
    {
        CopyDefaultConfig();
        for (auto i = 0; i < numMindIEServer; i++) {
            predictIPList.emplace_back("127.0.0.1");
        }
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.SetList({"request_limit", "max_requests"}, 1);
        manager.Save();
    }
    uint8_t numMindIEServer = 4;
    ThreadSafeVector<std::string> predictIPList;
    ThreadSafeVector<std::string> predictPortList;
    ThreadSafeVector<std::string> managePortList;
    ThreadSafeVector<std::string> interCommonPortLists;
    std::string manageIP = "127.0.0.1";
    std::string managePort = "1026";
    std::string predictIP = "127.0.0.1";
    std::string predictPort = "1025";
};

/*
测试描述: 客户端与coordinator建连，连接数超过上限
测试步骤:
    1. 将coordinator的最大请求数设置为1，最大连接数为最大请求数
    2. 客户端向coordinator建立连接，超过1条连接
    3. 检查客户端的链接数量
预期结果:
    1. 客户端的连接数为1
*/
TEST_F(TestMaxConnection, TestMaxConnectionTC01)
{
    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto predictIP = predictIPList[i];
        auto manageIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto pdInstance =
        SetPDRole(predictIPList, predictPortList, managePortList,
            interCommonPortLists, 2, 2, manageIP, managePort); // 2表示PD两种类型
    WaitCoordinatorReady(manageIP, managePort);
    std::mutex mtx;
    HttpClientAsync httpClient;
    TlsItems tls;
    tls.tlsEnable = false;
    httpClient.Init(tls);
    for (size_t i = 0; i < 5; ++i) { // 建立5条链接
        uint32_t id;
        auto ret = httpClient.AddConnection(predictIP, predictPort, id);
        ASSERT_TRUE(ret);
    }
    sleep(1);
    EXPECT_EQ(httpClient.GetConnSize(), 1); // 最多1条链接
    EXPECT_TRUE(threadObj.joinable());
    boost::beast::http::request<boost::beast::http::dynamic_body> req;
    req.method(boost::beast::http::verb::get);
    req.target("/");
    req.keep_alive(false);
    auto connIds = httpClient.FindId(predictIP, predictPort);
    for (auto connId : connIds) {
        auto conn = httpClient.GetConnection(connId);
        ASSERT_NE(conn, nullptr);
        conn->SendReq(req);
    }
    sleep(1);
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}