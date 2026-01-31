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
#ifndef HELPER_H
#define HELPER_H

#include <iostream>
#include <fstream>
#include <string>
#include "ConfigParams.h"
#include "ServerManager.h"

MINDIE::MS::HttpClientParams SetClientParams();

// Function to set default values for HttpServerParams
MINDIE::MS::HttpServerParams SetServerParams();

MINDIE::MS::LoadServiceParams SetLoadServiceParams();

std::string GetExecutablePath();

std::string GetParentPath(const std::string& path);

std::string GetAbsolutePath(const std::string& base, const std::string& relative);

std::string GetJsonPath();

std::string SetMSServerJsonDefault(const std::string& jsonPath);
std::string SetMSClientJsonDefault(const std::string& jsonPath);
std::string SetInferJsonDefault(const std::string& jsonPath);

// 拷贝指定文件到另一个路径，若目的路径文件存在，则删除覆盖
bool CopyFile(const std::string& sourcePath, const std::string& destPath);

// 拷贝config目录下的文件MindIE_MS\config至测试配置目录下tests\mindie_ms\common\.mindie_ms

void CopyDefaultConfig();

std::string GetMSServerConfigJsonPath();
std::string GetMSClientConfigJsonPath();
std::string GetMSDeployConfigJsonPath();
std::string GetMSServerTestJsonPath();
std::string GetMSClientTestJsonPath();
std::string GetMSDeployTestJsonPath();

void CreateFile(std::string filename, std::string content);
bool CreateDirectory(const std::string& path);
std::string JoinPathComponents(const std::vector<std::string>& components);

#endif