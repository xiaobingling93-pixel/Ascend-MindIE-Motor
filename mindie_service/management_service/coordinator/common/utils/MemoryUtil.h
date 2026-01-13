/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#ifndef MINDIE_MS_COORDINATOR_MEMORY_UTIL_H
#define MINDIE_MS_COORDINATOR_MEMORY_UTIL_H

#include <cstdint>
#include <string>

namespace MINDIE::MS {

/**
 * @brief Memory information structure
 */
struct MemoryInfo {
    int64_t limitBytes = -1;      ///< Memory limit in bytes, -1 indicates unlimited or unavailable
    int64_t usageBytes = -1;      ///< Current usage in bytes, -1 indicates unavailable
    int64_t availableBytes = -1;  ///< Available memory in bytes, -1 indicates unavailable
    std::string source;           ///< Data source (e.g. "cgroup_v2", "cgroup_v1", "proc")
};

/**
 * @brief Memory utility class
 *
 * Retrieves Pod/container memory limit and usage, supports cgroup v1 and v2
 */
class MemoryUtil {
public:
    /**
     * @brief Check if request config may exceed Pod memory limit
     *
     * Calculates maxReqs * bodyLimit and compares with Pod memory limit,
     * prints warning log if theoretical max memory demand exceeds Pod limit.
     * This function is marked noexcept to ensure it never throws exceptions
     * and does not affect the caller's process stability.
     */
    static void CheckRequestConfigMemoryRisk() noexcept;

private:
    /// Get complete memory information
    static MemoryInfo GetMemoryInfo();

    /// Get current memory usage
    static int64_t GetMemoryUsage();

    /// Format bytes to human-readable string
    static std::string FormatBytes(int64_t bytes);

    /// Get memory limit from cgroup v2
    static int64_t GetMemoryLimitFromCgroupV2();

    /// Get memory limit from cgroup v1
    static int64_t GetMemoryLimitFromCgroupV1();

    /// Get total system memory from /proc/meminfo
    static int64_t GetMemoryLimitFromProc();

    /// Get memory usage from cgroup v2
    static int64_t GetMemoryUsageFromCgroupV2();

    /// Get memory usage from cgroup v1
    static int64_t GetMemoryUsageFromCgroupV1();
};

} // namespace MINDIE::MS

#endif // MINDIE_MS_COORDINATOR_MEMORY_UTIL_H
