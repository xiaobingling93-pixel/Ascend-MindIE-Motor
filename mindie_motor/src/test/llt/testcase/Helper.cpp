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
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "ConfigParams.h"
#include "ServerManager.h"
#include "JsonFileManager.h"
#include "Logger.h"
#include "Helper.h"

using namespace MINDIE::MS;

HttpClientParams SetClientParams()
{
    HttpClientParams params;

    // Default values for HttpClientParams
    params.k8sIP = "127.0.0.1"; // Default IP address
    params.k8sPort = 8080;      // 8080 Default Kubernetes API Server port
    params.prometheusPort = 9090; // 9090 Default Prometheus port

    // Default values for TlsItems - TLS is enabled by default
    // Note: The constructor initializes tlsEnable to true, so we don't need to set it again here.
    params.k8sClientTlsItems.caCert = "path/to/k8s-ca-cert.pem"; // Example CA certificate path
    params.k8sClientTlsItems.tlsCert = "path/to/k8s-client-cert.pem"; // Example client certificate path
    params.k8sClientTlsItems.tlsKey = "path/to/k8s-client-key.pem"; // Example client key path
    params.k8sClientTlsItems.tlsPasswd = ""; // No password by default
    params.k8sClientTlsItems.tlsCrl = "path/to/k8s-crl.pem"; // Example CRL path

    params.mindieClientTlsItems.caCert = "path/to/mindie-ca-cert.pem"; // Example CA certificate path
    params.mindieClientTlsItems.tlsCert = "path/to/mindie-client-cert.pem"; // Example client certificate path
    params.mindieClientTlsItems.tlsKey = "path/to/mindie-client-key.pem"; // Example client key path
    params.mindieClientTlsItems.tlsPasswd = ""; // No password by default
    params.mindieClientTlsItems.tlsCrl = "path/to/mindie-crl.pem"; // Example CRL path

    params.promClientTlsItems.caCert = "path/to/prom-ca-cert.pem"; // Example CA certificate path
    params.promClientTlsItems.tlsCert = "path/to/prom-client-cert.pem"; // Example client certificate path
    params.promClientTlsItems.tlsKey = "path/to/prom-client-key.pem"; // Example client key path
    params.promClientTlsItems.tlsPasswd = ""; // No password by default
    params.promClientTlsItems.tlsCrl = "path/to/prom-crl.pem"; // Example CRL path

    return params;
}

// Function to set default values for HttpServerParams
HttpServerParams SetServerParams()
{
    HttpServerParams params;

    // Default values for HttpServerParams
    params.ip = "127.0.0.1"; // Default IP address
    params.port = 8080;      // 8080 Default port number

    // Default values for TlsItems - TLS is enabled by default
    // Note: The constructor initializes tlsEnable to true, so we don't need to set it again here.
    params.serverTlsItems.caCert = "path/to/server-ca-cert.pem"; // Example CA certificate path
    params.serverTlsItems.tlsCert = "path/to/server-cert.pem"; // Example server certificate path
    params.serverTlsItems.tlsKey = "path/to/server-key.pem"; // Example server key path
    params.serverTlsItems.tlsPasswd = ""; // No password by default
    params.serverTlsItems.tlsCrl = "path/to/server-crl.pem"; // Example CRL path

    return params;
}

LoadServiceParams SetLoadServiceParams()
{
    LoadServiceParams params;

    // Default values for LoadServiceParams
    params.name = "default-service-name";
    params.nameSpace = "default-namespace";
    params.replicas = 1;
    params.crossNodeNum = 1;
    params.serverImage = "default-server-image:latest";
    params.servicePort = 8080; // 8080 服务端口
    params.mindieServerPort = 8000; // 8000 mindie服务端口
    params.mindieServerMngPort = 8001; // 8001 mindie服务信息端口
    params.initDealy = 10; // 10 seconds
    params.mindieUseHttps = true;
    params.serverType = "NodePort"; // Default server type
    params.startupCmd = "start-service.sh";
    params.memRequest = 1024; // 1024 MB
    params.cpuRequest = 1; // Cores
    params.npuType = "Ascend910"; // Default NPU type
    params.npuNum = 1;
    params.mountMap = {
        {"data-volume", {"host-path", "container-path"}}
    };
    params.npuFaultReschedule = "default-npu-fault-reschedule-policy";
    params.scheduler = "default-scheduler";
    params.modelName = "default-model-name";
    params.worldSize = 1; // Number of processes
    params.cpuMemSize = 1024; // 1024 MB
    params.createTime = "2024-08-07T00:00:00Z";
    params.updateTime = "2024-08-07T00:00:00Z";

    return params;
}

std::string GetExecutablePath()
{
    std::vector<char> path;
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, &path[0], path.size());
    if (len == 0) {
        return "";
    }
    path.resize(len);
#else
    path.resize(PATH_MAX);
    ssize_t len = readlink("/proc/self/exe", &path[0], path.size());
    if (len == -1) {
        return "";
    }
    path.resize(len);
#endif
    return std::string(path.begin(), path.end());
}

std::string GetParentPath(const std::string& path)
{
    auto pos = path.rfind('/');
    if (pos == std::string::npos) {
        return "";
    }
    return path.substr(0, pos);
}

std::string GetAbsolutePath(const std::string& base, const std::string& relative)
{
    std::vector<std::string> baseParts;
    std::stringstream ss(base);
    std::string item;
    while (std::getline(ss, item, '/')) {
        baseParts.push_back(item);
    }
    std::vector<std::string> relativeParts;
    ss = std::stringstream(relative);
    while (std::getline(ss, item, '/')) {
        relativeParts.push_back(item);
    }
    std::vector<std::string> parts;
    for (const auto& part : baseParts) {
        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else if (part != ".") {
            parts.push_back(part);
        }
    }
    for (const auto& part : relativeParts) {
        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else if (part != ".") {
            parts.push_back(part);
        }
    }
    std::string result;
    for (const auto& part : parts) {
        result += "/" + part;
    }
    if (result.empty()) {
        result = "/";
    }
    return result;
}

std::string GetJsonPath()
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(exePath);
    std::string jsonPath = GetAbsolutePath(parentPath, "a.json");
    return jsonPath;
}

std::string GetMSServerConfigJsonPath()
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string jsonPath = GetAbsolutePath(parentPath, "mindie_motor/src/config/ms_server.json");
    return jsonPath;
}

std::string GetMSClientConfigJsonPath()
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string jsonPath = GetAbsolutePath(parentPath, "mindie_motor/src/config/msctl.json");
    return jsonPath;
}

std::string GetMSDeployConfigJsonPath()
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string jsonPath = GetAbsolutePath(parentPath, "mindie_motor/src/config/infer_server.json");
    return jsonPath;
}

std::string GetMSServerTestJsonPath()
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string jsonPath = GetAbsolutePath(parentPath,
        "mindie_motor/src/test/common/.mindie_ms/ms_server.json");
    return jsonPath;
}

std::string GetMSClientTestJsonPath()
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string jsonPath = GetAbsolutePath(parentPath,
        "mindie_motor/src/test/common/.mindie_ms/msctl.json");
    return jsonPath;
}

std::string GetMSDeployTestJsonPath()
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string jsonPath = GetAbsolutePath(parentPath,
        "mindie_motor/src/test/common/.mindie_ms/infer_server.json");
    return jsonPath;
}


std::string SetMSServerJsonDefault(const std::string& jsonPath)
{
    JsonFileManager manager(jsonPath);
    manager.Load();
    manager.Set("ip", "127.0.0.1");
    manager.Set("port", 9789); // 9789 端口
    manager.Set("k8s_apiserver_ip", "127.0.0.1");
    manager.Set("k8s_apiserver_port", 6443); // 6443 k8s端口
    manager.SetList({"log_info", "log_level"}, "DEBUG");
    manager.SetList({"log_info", "run_log_path"}, "./log/run_log.txt");
    manager.SetList({"log_info", "operation_log_path"}, "./log/operation_log.txt");
    manager.Set("ms_status_file", "./log/status.json");

    std::string exeDir = GetParentPath(GetExecutablePath());
    std::string certDir = JoinPathComponents({exeDir, "cert_dir"});
    CreateDirectory(certDir);

    auto lambdaFun = [exeDir, &manager](std::string items) mutable {
        auto ca_cert = JoinPathComponents({exeDir, "cert_dir", items + "_ca_cert.crt"});
        CreateFile(ca_cert, "ca_cert");
        auto tls_cert = JoinPathComponents({exeDir, "cert_dir", items + "_tls_cert.crt"});
        CreateFile(tls_cert, "tls_cert");
        auto tls_key = JoinPathComponents({exeDir, "cert_dir", items + "_tls_key.key"});
        CreateFile(tls_key, "tls_key");
        auto tls_crl = JoinPathComponents({exeDir, "cert_dir", items + "_tls_crl.crl"});
        CreateFile(tls_crl, "tls_crl");
        manager.SetList({items, "ca_cert"}, ca_cert);
        manager.SetList({items, "tls_cert"}, tls_cert);
        manager.SetList({items, "tls_key"}, tls_key);
        manager.SetList({items, "tls_passwd"}, "aaaa");
        manager.SetList({items, "tls_crl"}, tls_crl);
    };

    mode_t oldMask = umask(0277);
    lambdaFun("server_tls_items");

    manager.Set("client_k8s_tls_enable", true);
    lambdaFun("client_k8s_tls_items");

    manager.Set("client_mindie_server_tls_enable", true);

    lambdaFun("client_mindie_tls_items");
    umask(oldMask);
    manager.Save();
    return manager.Dump();
}


// 设置 MS 客户端 JSON 文件的默认值
std::string SetMSClientJsonDefault(const std::string& jsonPath)
{
    JsonFileManager manager(jsonPath);
    manager.Load();

    // 设置 http 部分
    manager.Set("http.dstPort", 9789); // 9789 目标端口
    manager.Set("http.dstIP", "127.0.0.1");
    manager.Set("http.ca_cert", "/path/to/ca.crt");
    manager.Set("http.tls_cert", "/path/to/ca.crt");
    manager.Set("http.tls_key", "/path/to/ca.key");
    manager.Set("http.tls_passwd", "");
    manager.Set("http.tls_crl", "");
    manager.Set("http.timeout", 5); // 5 过期时间

    // 设置 log_level
    manager.Set("log_level", "INFO");

    manager.Save();
    return manager.Dump();
}

std::string SetInferJsonDefault(const std::string& jsonPath)
{
    JsonFileManager manager(jsonPath);
    manager.Load();
    manager.Set("server_name", "mindie-ie");
    manager.Set("replicas", 1);
    manager.Set("service_port", 31005); // 31005 service_port
    manager.Set("cross_node_num", 2); // 2 cross_node_num
    manager.Set("service_type", "NodePort");
    manager.Set("server_type", "mindie_cross_node");
    manager.Set("scheduler", "default");
    manager.Set("init_delay", 180); // 180 init_delay
    manager.SetList({"resource_requests", "memory"}, 500000); // 500000 memory
    manager.SetList({"resource_requests", "cpu_core"}, 32000); // 32000 cpu_core
    manager.SetList({"resource_requests", "npu_type"}, "Ascend910");
    manager.SetList({"resource_requests", "npu_chip_num"}, 8); // 8 npu核心数
    manager.SetList({"mindie_server_config", "infer_port"}, 1025); // 1025 infer端口
    manager.SetList({"mindie_server_config", "management_port"}, 1026); // 1026 management_port
    manager.SetList({"mindie_server_config", "enable_tls"}, true);
    manager.Save();

    return manager.Dump();
}

// 拷贝指定文件到另一个路径，若目的路径文件存在，则删除覆盖
bool CopyFile(const std::string& sourcePath, const std::string& destPath)
{
    std::ifstream sourceFile(sourcePath, std::ios::binary);
    if (!sourceFile.is_open()) {
        std::cerr << "Error opening source file: " << sourcePath << std::endl;
        return false;
    }

    // 如果目标文件已存在，则删除它
    if (std::remove(destPath.c_str()) != 0) {
        std::cerr << "Error removing existing destination file: " << destPath << std::endl;
        sourceFile.close();
        return false;
    }

    std::ofstream destFile(destPath, std::ios::binary);
    if (!destFile.is_open()) {
        std::cerr << "Error creating destination file: " << destPath << std::endl;
        sourceFile.close();
        return false;
    }

    // 逐字节拷贝文件内容
    destFile << sourceFile.rdbuf();

    // 关闭文件
    sourceFile.close();
    destFile.close();

    return true;
}

void CopyDefaultConfig()
{
    // Logger::Singleton()->Init("DEBUG", false,"","");
    std::string serverConfigJsonPath = GetMSServerConfigJsonPath();
    std::string clientConfigJsonPath = GetMSClientConfigJsonPath();
    std::string deployConfigJsonPath = GetMSDeployConfigJsonPath();
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    std::string clientTestJsonPath = GetMSClientTestJsonPath();
    std::string deployTestJsonPath = GetMSDeployTestJsonPath();

    mode_t oldMask = umask(027);
    CopyFile(serverConfigJsonPath, serverTestJsonPath);
    umask(oldMask);
    CopyFile(clientConfigJsonPath, clientTestJsonPath);
    CopyFile(deployConfigJsonPath, deployTestJsonPath);

    SetMSServerJsonDefault(serverTestJsonPath);
    SetMSClientJsonDefault(clientTestJsonPath);
    SetInferJsonDefault(deployTestJsonPath);
}

// Function to join path components into a full path
std::string JoinPathComponents(const std::vector<std::string>& components)
{
    std::string path;
    for (const auto& component : components) {
        if (!component.empty()) {
            // Avoid adding duplicate separators
            if (!path.empty() && path.back() != '/' && component.front() != '/') {
                path += '/';
            }
            path += component;
        }
    }
    return path;
}

bool CreateDirectory(const std::string& path)
{
    struct stat info;

    // 检查目录是否存在
    if (stat(path.c_str(), &info) != 0) {
        // 目录不存在，创建目录
        if (mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
            std::cerr << "Error creating directory: " << path << std::endl;
            return false;
        }
    } else if (!(info.st_mode & S_IFDIR)) {
        std::cerr << "Error creating directory: " << path << " (not a directory)" << std::endl;
        return false;
    }

    return true;
}

void CreateFile(std::string filename, std::string content)
{
    // 检查文件是否存在
    if (remove(filename.c_str()) != 0) {
        // 文件不存在或删除失败，不做处理
    }

    // 创建文件并写入内容
    std::ofstream outfile(filename);
    outfile << content;
    outfile.close();
}

static bool ReturnFalseStub(const char * format)
{
    std::cout << "ReturnFalseStub" << std::endl;
    return false;
}

static bool ReturnTrueStub(const char * format)
{
    std::cout << "ReturnTrueStub" << std::endl;
    return false;
}

static void *ReturnNullptrStub(const char * format)
{
    std::cout << "ReturnNullptrStub" << std::endl;
    return nullptr;
}