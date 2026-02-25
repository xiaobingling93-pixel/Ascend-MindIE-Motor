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
#ifndef MINDIE_MS_JSON_FILE_LOADER_H
#define MINDIE_MS_JSON_FILE_LOADER_H
#include <string>
#include "nlohmann/json.hpp"
namespace MINDIE::MS {
nlohmann::json FileToJsonObj(const std::string& filePath, uint32_t mode = 0777, bool checkOwner = true);
}
#endif // MINDIE_MS_JSON_FILE_LOADER_H