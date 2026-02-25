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
#ifndef MINDIE_MS_DP_GROUPING_UTIL_H
#define MINDIE_MS_DP_GROUPING_UTIL_H

#include <memory>
#include "digs_instance.h"
#include "cluster_status/NodeStatus.h"

namespace MINDIE::MS {

/**
 * DP grouping utility class, providing common functionality for node DP grouping
 */
class DpGroupingUtil {
public:
    /**
     * Process DP grouping for a single node
     * @param serverNode Server node information
     * @return 0 for success, -1 for failure
     */
    static int32_t ProcessSingleNodeDpGrouping(std::unique_ptr<NodeInfo>& serverNode);
};

} // namespace MINDIE::MS

#endif // MINDIE_MS_DP_GROUPING_UTIL_H
