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
#include <fstream>
#include <string>
#include <memory>
#include <pthread.h>
#include "gtest/gtest.h"
#include "ServerManager.h"
#define private public
#define protected public
#define main __main_server__
#define  g_maxPort g_maxServerPort
#define  g_minPort g_minServerPort
#define  PrintHelp ServerPrintHelp
#include "deployer/server/main.cpp"
#undef g_maxPort
#undef g_minPort
#define main __main_client__
#define  PrintHelp ClientPrintHelp
#include "deployer/msctl/main.cpp"
#include "ConfigParams.h"
#include "Helper.h"
#include "stub.h"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "CrossNodeServer.cpp"
#include "StatusHandler.h"
#include "StatusHandler.cpp"
#include "ServerManager.cpp"
#include "HttpServer.cpp"

class TestServer : public testing::Test {
protected:
    static std::size_t RunStub()
    {
        std::cout << "RunStub" << std::endl;
    }
    static boost::system::error_code UseCertificateChainFileStub(std::string &path, boost::system::error_code& ec)
    {
        std::cout << "UseCertificateChainFileStub" << std::endl;
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }
    static boost::system::error_code UsePrivateKeyFileStub(
        const std::string &path,
        boost::asio::ssl::context::file_format format,
        boost::system::error_code& ec)
    {
        std::cout << "UsePrivateKeyFileStub" << std::endl;
    }

    static boost::system::error_code AddCertificateAuthorityStub(const boost::asio::const_buffer &ca,
        boost::system::error_code& ec)
    {
        std::cout << "AddCertificateAuthorityStub" << std::endl;
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }
    static bool ServerDecryptStub(
        boost::asio::ssl::context &mSslCtx,
        const MINDIE::MS::HttpServerParams &httpParams,
        std::pair<std::string, int32_t> &mPassword)
    {
        std::cout << "ServerDecryptStub" << std::endl;
        return true;
    }
    static int32_t SendStub(MINDIE::MS::HttpClient selfObj, const Request &request,
        int timeoutSeconds, int retries, std::string& responseBody, int32_t &code)
    {
        std::cout << "Run_Stub" << std::endl;
        code = 200; // 200 成功
        if (request.target.find("configmaps") != std::string::npos) {
            responseBody = R"({
                "kind": "ConfigMap",
                "apiVersion": "v1",
                "metadata": {
                    "name": "rings-config-hanxueli-deployment-0",
                    "namespace": "mindie",
                    "uid": "9f5e8d09-1b3f-4ea9-be51-140080f07a68",
                    "resourceVersion": "111449",
                    "creationTimestamp": "2024-08-17T04:19:52Z",
                    "labels": {
                        "ring-controller.atlas": "ascend-910b"
                    },
                    "managedFields": [
                        {
                            "manager": "unknown",
                            "operation": "Update",
                            "apiVersion": "v1",
                            "time": "2024-08-17T04:19:52Z",
                            "fieldsType": "FieldsV1",
                            "fieldsV1": {
                                "f:data": {
                                    ".": {},
                                    "f:hccl.json": {}
                                },
                                "f:metadata": {
                                    ".": {},
                                    "f:labels": {
                                        ".": {},
                                        "f:ring-controller.atlas": {}
                                    }
                                }
                            }
                        }
                    ]
                },
                "data": {
                    "hccl.json": "{\"status\":\"completed\"}"
                }
            })";
        }
        return 0;
    }

    static void AssociateServiceStub(MINDIE::MS::CrossNodeServer selfObj, const LoadServiceParams &serverParams)
    {
        std::cout << "AssociateServiceStub" << std::endl;
        return;
    }

    void SetUp()
    {
        CopyDefaultConfig();
        stub.set((boost::system::error_code(boost::asio::ssl::context::*)(const std::string&,
            boost::system::error_code&))
            ADDR(boost::asio::ssl::context, use_certificate_chain_file),
            UseCertificateChainFileStub);

        stub.set((boost::system::error_code(boost::asio::ssl::context::*)(const boost::asio::const_buffer &,
            boost::system::error_code&))
            ADDR(boost::asio::ssl::context, add_certificate_authority),
            AddCertificateAuthorityStub);

        stub.set((boost::system::error_code(boost::asio::ssl::context::*)(const std::string&,
            boost::asio::ssl::context::file_format,
            boost::system::error_code&))
            ADDR(boost::asio::ssl::context, use_private_key_file),
            UsePrivateKeyFileStub);

        stub.set((boost::system::error_code(boost::asio::ssl::context::*)(const std::string&,
            boost::asio::ssl::context::file_format,
            boost::system::error_code&))
            ADDR(boost::asio::ssl::context, use_private_key_file),
            UsePrivateKeyFileStub);

        stub.set((std::size_t(boost::asio::io_context::*)())
            ADDR(boost::asio::io_context, run), RunStub);
        stub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SendStub);
        stub.set(ADDR(MINDIE::MS::CrossNodeServer, AssociateService), AssociateServiceStub);
        stub.set(ServerDecrypt, ServerDecryptStub);
    }

    void TearDown()
    {
        stub.reset((boost::system::error_code(boost::asio::ssl::context::*)(const std::string&,
            boost::system::error_code&))
            ADDR(boost::asio::ssl::context, use_certificate_chain_file));
        stub.reset((boost::system::error_code(boost::asio::ssl::context::*)(const std::string&,
            boost::asio::ssl::context::file_format,
            boost::system::error_code&))
            ADDR(boost::asio::ssl::context, use_private_key_file));
        stub.reset((std::size_t(boost::asio::io_context::*)())
            ADDR(boost::asio::io_context, run));
        stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
        stub.reset(ADDR(MINDIE::MS::CrossNodeServer, AssociateService));
    }

    Stub stub;
};

// 测试PrintHelp
TEST_F(TestServer, TestServerTC01)
{
    // Expected output string
    const std::string expectedOutput =
        "Usage: ./ms_server [config_file]\n\n"
        "Description:\n"
        "  ms_server is a management server used to manage mindie inference server.\n\n"
        "Arguments:\n"
        "  config_file    Path to the configuration file (in JSON format) that specifies the server settings,"
        " for more detail, see the product documentation.\n\n"
        "Examples:\n"
        "  Run the management server with the specified configuration file:\n"
        "    ./ms_server ms_server.json\n"
        "\n";

    // Capture the actual output from PrintHelp()
    std::ostringstream ss;
    std::streambuf* prev = std::cout.rdbuf(ss.rdbuf());
    // Call the function whose output we want to capture
    ServerPrintHelp();
    // Restore the original stream buffer
    std::cout.rdbuf(prev);
    std::string actualOutput = ss.str();
    // Compare the actual output with the expected output
    std::cout << "Actual Output:\n" << actualOutput << std::endl;
    EXPECT_EQ(actualOutput, expectedOutput);
}

int PrintfStub(const char* fileName, HttpClientParams &clientParams,
    HttpServerParams &httpServerParams, std::string &statusPath)
    {
    std::cout << "I am PrintfStub:\n" << std::endl;
    statusPath = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    return 5; // 5 stub返回值
}

TEST_F(TestServer, TestServerTC04)
{
    Stub stub;
    stub.set(ParseCommandArgs, PrintfStub);
    HttpClientParams clientParams;
    HttpServerParams httpServerParams;
    std::string statusPath;

    auto ret = ParseCommandArgs("test.json", clientParams, httpServerParams, statusPath);
    std::cout << "ret:\n" << ret << std::endl;
    std::cout << "statusPath:\n" << statusPath << std::endl;
}

/*
测试描述: 服务端配置文件测试，正常配置文件
测试步骤:
    1. 配置serverTestJsonPath为正常文件，内容有效，预期结果1
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
*/
TEST_F(TestServer, ServerStartTest)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
    EXPECT_EQ(ret, 0);
    JsonFileManager manager(serverTestJsonPath);
    manager.Load();

    EXPECT_EQ(clientParams.k8sIP, manager.Get<std::string>("k8s_apiserver_ip"));
    EXPECT_EQ(clientParams.k8sPort, manager.Get<int64_t>("k8s_apiserver_port"));
    EXPECT_EQ(clientParams.k8sClientTlsItems.tlsEnable,
        manager.Get<bool>("client_k8s_tls_enable"));
    EXPECT_EQ(clientParams.k8sClientTlsItems.caCert,
        manager.GetList<std::string>({"client_k8s_tls_items", "ca_cert"}));
    EXPECT_EQ(clientParams.k8sClientTlsItems.tlsCert,
        manager.GetList<std::string>({"client_k8s_tls_items", "tls_cert"}));
    EXPECT_EQ(clientParams.k8sClientTlsItems.tlsKey,
        manager.GetList<std::string>({"client_k8s_tls_items", "tls_key"}));
    EXPECT_EQ(clientParams.k8sClientTlsItems.tlsCrl,
        manager.GetList<std::string>({"client_k8s_tls_items", "tls_crl"}));

    EXPECT_EQ(clientParams.mindieClientTlsItems.tlsEnable,
        manager.Get<bool>("client_mindie_server_tls_enable"));
    EXPECT_EQ(clientParams.mindieClientTlsItems.caCert,
        manager.GetList<std::string>({"client_mindie_tls_items", "ca_cert"}));
    EXPECT_EQ(clientParams.mindieClientTlsItems.tlsCert,
        manager.GetList<std::string>({"client_mindie_tls_items", "tls_cert"}));
    EXPECT_EQ(clientParams.mindieClientTlsItems.tlsKey,
        manager.GetList<std::string>({"client_mindie_tls_items", "tls_key"}));

    EXPECT_EQ(clientParams.mindieClientTlsItems.tlsCrl,
        manager.GetList<std::string>({"client_mindie_tls_items", "tls_crl"}));
    EXPECT_EQ(serverParams.ip, manager.Get<std::string>("ip"));
    EXPECT_EQ(serverParams.port, manager.Get<int64_t>("port"));
    EXPECT_EQ(serverParams.serverTlsItems.caCert,
        manager.GetList<std::string>({"server_tls_items", "ca_cert"}));
    EXPECT_EQ(serverParams.serverTlsItems.tlsCert,
        manager.GetList<std::string>({"server_tls_items", "tls_cert"}));
    EXPECT_EQ(serverParams.serverTlsItems.tlsKey,
        manager.GetList<std::string>({"server_tls_items", "tls_key"}));
    EXPECT_EQ(serverParams.serverTlsItems.tlsCrl,
        manager.GetList<std::string>({"server_tls_items", "tls_crl"}));

    EXPECT_EQ(statusPath, manager.Get<std::string>("ms_status_file"));

    MINDIE::MS::ServerManager serverManager(clientParams);
    HttpServer server(&serverManager, 1);

    ret = server.Run(serverParams);
    EXPECT_EQ(ret, 0);
}

/*
测试描述: 部署配置测试用例
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestServer, InferServerLoadServerTest)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    std::string deployStr = SetInferJsonDefault(GetMSDeployTestJsonPath());
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
    EXPECT_EQ(ret, 0);

    MINDIE::MS::ServerManager serverManager(clientParams);

    serverManager.LoadServer(deployStr);
    serverManager.GetDeployStatus("mindie-ie");
    serverManager.UnloadServer("mindie-ie");
}

/*
测试描述: 查询服务测试用例
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestServer, InferServerGetDeployStatusTest)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    std::string deployStr = SetInferJsonDefault(GetMSDeployTestJsonPath());
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
    EXPECT_EQ(ret, 0);

    MINDIE::MS::ServerManager serverManager(clientParams);
    serverManager.GetDeployStatus("mindie-ie");
}

/*
测试描述: 加载已部署服务测试用例
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestServer, InferServeFromStatusFileTest)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    std::string deployStr = SetInferJsonDefault(GetMSDeployTestJsonPath());
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
    EXPECT_EQ(ret, 0);

    JsonFileManager manager(serverTestJsonPath);
    manager.Load();
    std::string msStatusFile = manager.Get<std::string>("msStatusFile");

    JsonFileManager status(msStatusFile);
    status.Load();
    json server;
    json server_list;

    server["namespace"] = "mindie";
    server["replicas"] = 1;
    server["server_name"] = "test";
    server["server_type"] = "mindie_cross_node";
    server["use_service"] = true;
    server_list.push_back(server);
    status.Set("server_list", server_list);
    status.Save();
    std::cout << "status: " << status.Dump() << std::endl;
    MINDIE::MS::ServerManager serverManager(clientParams);
    serverManager.FromStatusFile(msStatusFile);
}

/*
测试描述: 服务端配置文件测试，配置异常的ip地址字符串值
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestServer, ParseCommandArgsInvalidIPString)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();

    std::vector<std::string> cfgTypes = {"ip", "k8s_apiserver_ip"};
    std::vector<std::string> testIPs = {"256.0.0.0", "0.0.0.0", "", "127.0.0.1.", "127.0.1", "127.a.0.1"};
    for (auto cfg: cfgTypes) {
        for (auto ip : testIPs) {
            std::cout << "cfg: " << cfg << std::endl;
            std::cout << "ip: " << ip << std::endl;
            {
                CopyDefaultConfig();
                // 2. 修改 ip 字段为异常值，预期结果2
                JsonFileManager manager(serverTestJsonPath);
                manager.Load();
                manager.Set(cfg, ip);
                manager.Save();
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, -1);
            }
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.Set(cfg, 127); // 127 非法配置
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.Erase(cfg);
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
    }
}

/*
测试描述: 服务端配置文件测试，配置异常的端口字符串值
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestServer, ParseCommandArgsInvalidPort)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();

    std::vector<std::string> cfgTypes = {"port", "k8s_apiserver_port"};
    std::vector<int64_t> testPorts = {-20, 0, 1023, 65536, 9999999};
    for (auto cfg: cfgTypes) {
        for (auto port : testPorts) {
            std::cout << "cfg: " << cfg << std::endl;
            std::cout << "port: " << port << std::endl;
            {
                CopyDefaultConfig();
                // 2. 修改 ip 字段为异常值，预期结果2
                JsonFileManager manager(serverTestJsonPath);
                manager.Load();
                manager.Set(cfg, port);
                manager.Save();
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, -1);
            }
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.Set(cfg, "sss");
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.Erase(cfg);
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
    }
}

/*
测试描述: 服务端配置文件测试，配置异常的日志目录不存在
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestServer, ParseCommandArgsInvalidlog)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;
    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    std::vector<std::string> cfgKeys = {"log_level", "run_log_path", "operation_log_path"};
    for (auto item: cfgKeys) {
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.SetList({"log_info", item}, "./AAAAA/BBBB");
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.SetList({"log_info", item}, 127); // 127 不是有效路径
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.EraseList({"log_info", item});
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
    }
}

template<typename T>
void SetMSStatusFile(JsonFileManager& manager, const T& value)
{
    manager.Load();
    manager.Set("ms_status_file", value);
    manager.Save();
}
/*
测试描述: 服务端配置文件测试，配置ms_status_file异常
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestServer, ParseCommandArgsInvalidStatus)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;
    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();

    {
        CopyDefaultConfig();
        // 文件不存在
        JsonFileManager manager(serverTestJsonPath);
        SetMSStatusFile(manager, "AAAAA");
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, 0);
    }
    {
        CopyDefaultConfig();
        // 日志目录不存在
        JsonFileManager manager(serverTestJsonPath);
        SetMSStatusFile(manager, "AAAAA/BBBBB");
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, 0);
    }
    {
        CopyDefaultConfig();
        // 文件内容异常
        JsonFileManager manager(serverTestJsonPath);
        SetMSStatusFile(manager, "testcase");
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, 0);
    }
    {
        CopyDefaultConfig();
        // 文件内容异常
        JsonFileManager manager(serverTestJsonPath);
        SetMSStatusFile(manager, 123); // 123 不合规文件名
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, -1);
    }
    {
        CopyDefaultConfig();
        JsonFileManager manager(serverTestJsonPath);
        manager.Load();
        manager.Erase("ms_status_file");
        manager.Save();
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, -1);
    }
}

template<typename T>
void SetManagerList(JsonFileManager& manager, const std::vector<std::string>& keys, const T& value)
{
    manager.Load();
    manager.SetList(keys, value);
    manager.Save();
}

/*
测试描述: 服务端配置文件测试，配置证书异常
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestServer, ParseCommandArgsInvalidCert)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;
    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    std::vector<std::string> certPath = {"server_tls_items", "client_k8s_tls_items", "client_mindie_tls_items"};
    std::vector<std::string> certType = {"ca_cert", "tls_cert", "tls_key", "tls_crl"};
    // 文件不存在
    for (auto item: certPath) {
        for (auto fileName: certType) {
            std::cout << "item: " << item << std::endl;
            std::cout << "fileName: " << fileName << std::endl;
            // 文件不存在
            {
                CopyDefaultConfig();
                JsonFileManager manager(serverTestJsonPath);
                SetManagerList(manager, {item, fileName}, "AAAAA");
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, 0);
                MINDIE::MS::ServerManager serverManager(clientParams);
                serverManager.LoadServer(serverTestJsonPath);
            }
            // 配置项不存在
            {
                CopyDefaultConfig();
                JsonFileManager manager(serverTestJsonPath);
                manager.Load();
                manager.EraseList({item, fileName});
                manager.Save();
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, -1);
            }
            // 配置项非字符串
            {
                CopyDefaultConfig();
                JsonFileManager manager(serverTestJsonPath);
                SetManagerList(manager, {item, fileName}, 128); // 128 不合规的文件名
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, -1);
            }
            // 配置项内容异常
            {
                CopyDefaultConfig();
                JsonFileManager manager(serverTestJsonPath);
                SetManagerList(manager, {item, fileName}, "testcase");
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, 0);
            }
        }
    }
}