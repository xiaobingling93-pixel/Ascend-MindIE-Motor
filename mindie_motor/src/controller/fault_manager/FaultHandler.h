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
#ifndef MINDIE_MS_FAULT_HANDLER_H
#define MINDIE_MS_FAULT_HANDLER_H

#include <cstdint>
#include <memory>
#include "NodeStatus.h"
namespace MINDIE::MS {
int32_t SubHealthyHardwareFaultHandler(std::shared_ptr<NodeStatus> nodeStatus, uint64_t id);
int32_t UnhealthyHardwareFaultHandler(std::shared_ptr<NodeStatus> nodeStatus, uint64_t id);
} // namespace MINDIE::MS
#endif // MINDIE_MS_CONTROLLER_FAULT_HANDLER_H