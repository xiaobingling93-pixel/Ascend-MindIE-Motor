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
#ifndef MINDIE_MS_SECURITY_UTILS_H
#define MINDIE_MS_SECURITY_UTILS_H

#include <string>
#include <nlohmann/json.hpp>

namespace MINDIE {
namespace MS {

/**
 * @brief Validates if a string is safe to be used in JSON
 * @param input The string to validate
 * @return true if the string is safe, false otherwise
 */
bool IsStringSafeForJson(const std::string& input);

/**
 * @brief Sanitizes a string for safe JSON usage
 * @param input The string to sanitize
 * @return The sanitized string
 */
std::string SanitizeStringForJson(const std::string& input);

/**
 * @brief Validates and sanitizes an IP address string
 * @param ip The IP address string to validate
 * @return The sanitized IP address or empty string if invalid
 */
std::string ValidateAndSanitizeIP(const std::string& ip);

/**
 * @brief Validates and sanitizes a device ID string
 * @param deviceId The device ID string to validate
 * @return The sanitized device ID or empty string if invalid
 */
std::string ValidateAndSanitizeDeviceId(const std::string& deviceId);

/**
 * @brief Validates a MODEL_ID string format
 * @param modelID The MODEL_ID string to validate
 * @return true if the MODEL_ID is valid, false otherwise
 */
bool IsValidModelID(const std::string& modelID);

/**
 * @brief Validates metrics info data size
 * @param metricsInfo The metrics info string to validate
 * @return true if the metrics info is valid, false otherwise
 */
bool IsValidMetricsInfo(const std::string& metricsInfo);

/**
 * @brief Validates gRPC rank table content
 * @param rankTable The rank table string to validate
 * @return true if the rank table is valid, false otherwise
 */
bool IsValidRankTable(const std::string& rankTable);

/**
 * @brief Validates gRPC fault signal type
 * @param signalType The signal type string to validate
 * @return true if the signal type is valid, false otherwise
 */
bool IsValidFaultSignalType(const std::string& signalType);

/**
 * @brief Validates gRPC fault level
 * @param faultLevel The fault level string to validate
 * @return true if the fault level is valid, false otherwise
 */
bool IsValidFaultLevel(const std::string& faultLevel);

/**
 * @brief Validates gRPC node name
 * @param nodeName The node name string to validate
 * @return true if the node name is valid, false otherwise
 */
bool IsValidNodeName(const std::string& nodeName);

} // namespace MS
} // namespace MINDIE

#endif // MINDIE_MS_SECURITY_UTILS_H
