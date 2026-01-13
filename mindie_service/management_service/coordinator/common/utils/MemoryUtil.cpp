/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include "MemoryUtil.h"
#include "Logger.h"
#include "Configure.h"

#include <fstream>
#include <sstream>
#include <iomanip>

namespace MINDIE::MS {

namespace {
// cgroup v2 file paths
constexpr const char* CGROUP_V2_MEMORY_MAX = "/sys/fs/cgroup/memory.max";
constexpr const char* CGROUP_V2_MEMORY_CURRENT = "/sys/fs/cgroup/memory.current";

// cgroup v1 file paths
constexpr const char* CGROUP_V1_MEMORY_LIMIT = "/sys/fs/cgroup/memory/memory.limit_in_bytes";
constexpr const char* CGROUP_V1_MEMORY_USAGE = "/sys/fs/cgroup/memory/memory.usage_in_bytes";

// System memory info
constexpr const char* PROC_MEMINFO = "/proc/meminfo";

// Unit conversion constants
constexpr int64_t ONE_KB = 1024LL;
constexpr int64_t ONE_MB = 1024LL * 1024LL;
constexpr int64_t ONE_GB = 1024LL * 1024LL * 1024LL;

// cgroup v1 "unlimited" threshold (close to 2^63)
constexpr int64_t CGROUP_V1_UNLIMITED_THRESHOLD = 1LL << 60;

/**
 * @brief Read first line from file
 */
bool ReadFirstLine(const char* filePath, std::string& content)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    std::getline(file, content);
    file.close();
    return !content.empty();
}

/**
 * @brief Safely convert string to int64_t
 */
bool SafeStringToInt64(const std::string& str, int64_t& value)
{
    try {
        value = std::stoll(str);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // anonymous namespace

int64_t MemoryUtil::GetMemoryLimitFromCgroupV2()
{
    std::string content;
    if (!ReadFirstLine(CGROUP_V2_MEMORY_MAX, content)) {
        return -1;
    }

    // "max" indicates no limit
    if (content == "max") {
        LOG_D("[MemoryUtil] cgroup v2 memory limit is 'max' (unlimited)");
        return -1;
    }

    int64_t limit = -1;
    if (SafeStringToInt64(content, limit)) {
        LOG_D("[MemoryUtil] Memory limit from cgroup v2: %ld bytes", limit);
        return limit;
    }

    LOG_W("[MemoryUtil] Failed to parse cgroup v2 memory limit: %s", content.c_str());
    return -1;
}

int64_t MemoryUtil::GetMemoryLimitFromCgroupV1()
{
    std::string content;
    if (!ReadFirstLine(CGROUP_V1_MEMORY_LIMIT, content)) {
        return -1;
    }

    int64_t limit = -1;
    if (!SafeStringToInt64(content, limit)) {
        LOG_W("[MemoryUtil] Failed to parse cgroup v1 memory limit: %s", content.c_str());
        return -1;
    }

    // cgroup v1 "unlimited" is usually a very large number
    if (limit > CGROUP_V1_UNLIMITED_THRESHOLD) {
        LOG_D("[MemoryUtil] cgroup v1 memory limit appears unlimited: %ld", limit);
        return -1;
    }

    LOG_D("[MemoryUtil] Memory limit from cgroup v1: %ld bytes", limit);
    return limit;
}

int64_t MemoryUtil::GetMemoryLimitFromProc()
{
    std::ifstream file(PROC_MEMINFO);
    if (!file.is_open()) {
        return -1;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip lines that don't start with "MemTotal:"
        if (line.find("MemTotal:") != 0) {
            continue;
        }

        // Format: "MemTotal:       16384000 kB"
        size_t pos = line.find_first_of("0123456789");
        if (pos == std::string::npos) {
            break;
        }

        int64_t memKB = -1;
        if (!SafeStringToInt64(line.substr(pos), memKB)) {
            break;
        }

        int64_t memBytes = memKB * ONE_KB;
        LOG_D("[MemoryUtil] System total memory from /proc/meminfo: %ld bytes", memBytes);
        file.close();
        return memBytes;
    }

    file.close();
    return -1;
}

int64_t MemoryUtil::GetMemoryUsageFromCgroupV2()
{
    std::string content;
    if (!ReadFirstLine(CGROUP_V2_MEMORY_CURRENT, content)) {
        return -1;
    }

    int64_t usage = -1;
    if (SafeStringToInt64(content, usage)) {
        return usage;
    }

    return -1;
}

int64_t MemoryUtil::GetMemoryUsageFromCgroupV1()
{
    std::string content;
    if (!ReadFirstLine(CGROUP_V1_MEMORY_USAGE, content)) {
        return -1;
    }

    int64_t usage = -1;
    if (SafeStringToInt64(content, usage)) {
        return usage;
    }

    return -1;
}

int64_t MemoryUtil::GetMemoryUsage()
{
    // Try cgroup v2 first
    int64_t usage = GetMemoryUsageFromCgroupV2();
    if (usage >= 0) {
        return usage;
    }

    // Try cgroup v1
    usage = GetMemoryUsageFromCgroupV1();
    if (usage >= 0) {
        return usage;
    }

    return -1;
}

MemoryInfo MemoryUtil::GetMemoryInfo()
{
    MemoryInfo info;

    // Get memory limit
    info.limitBytes = GetMemoryLimitFromCgroupV2();
    if (info.limitBytes > 0) {
        info.source = "cgroup_v2";
    } else {
        info.limitBytes = GetMemoryLimitFromCgroupV1();
        if (info.limitBytes > 0) {
            info.source = "cgroup_v1";
        } else {
            info.limitBytes = GetMemoryLimitFromProc();
            if (info.limitBytes > 0) {
                info.source = "proc_meminfo";
            }
        }
    }

    // Get memory usage
    info.usageBytes = GetMemoryUsage();

    // Calculate available memory
    if (info.limitBytes > 0 && info.usageBytes >= 0) {
        info.availableBytes = info.limitBytes - info.usageBytes;
        if (info.availableBytes < 0) {
            info.availableBytes = 0;
        }
    }

    return info;
}

std::string MemoryUtil::FormatBytes(int64_t bytes)
{
    if (bytes < 0) {
        return "N/A";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2); // 2: decimal places

    if (bytes >= ONE_GB) {
        oss << (static_cast<double>(bytes) / ONE_GB) << " GB";
    } else if (bytes >= ONE_MB) {
        oss << (static_cast<double>(bytes) / ONE_MB) << " MB";
    } else if (bytes >= ONE_KB) {
        oss << (static_cast<double>(bytes) / ONE_KB) << " KB";
    } else {
        oss << bytes << " B";
    }

    return oss.str();
}

void MemoryUtil::CheckRequestConfigMemoryRisk() noexcept
{
    try {
        // Get Pod memory limit
        MemoryInfo info = GetMemoryInfo();
        if (info.limitBytes <= 0) {
            LOG_W("[MemoryUtil] Unable to get memory limit, skipping request config memory risk check");
            return;
        }

        // Get configuration parameters
        auto configure = Configure::Singleton();
        if (configure == nullptr) {
            LOG_W("[MemoryUtil] Configure not available, skipping request config memory risk check");
            return;
        }

        size_t maxReqs = configure->reqLimit.maxReqs;
        size_t bodyLimit = configure->reqLimit.bodyLimit;

        // Calculate theoretical max memory demand: maxReqs * bodyLimit
        // Use int64_t to avoid size_t multiplication overflow
        int64_t theoreticalMaxMemory = static_cast<int64_t>(maxReqs) * static_cast<int64_t>(bodyLimit);

        // Add 20% margin
        int64_t theoreticalMaxMemoryWithMargin = static_cast<int64_t>(theoreticalMaxMemory * 1.2); // 1.2: 20% margin

        LOG_M("[MemoryUtil] Request Config Check:");
        LOG_M("[MemoryUtil]   maxReqs:                %zu", maxReqs);
        LOG_M("[MemoryUtil]   bodyLimit:              %s (%zu bytes)", FormatBytes(bodyLimit).c_str(), bodyLimit);
        LOG_M("[MemoryUtil]   Theoretical Max Memory: %s (%ld bytes)",
              FormatBytes(theoreticalMaxMemory).c_str(), theoreticalMaxMemory);
        LOG_M("[MemoryUtil]   With 20%% Margin:        %s (%ld bytes)",
              FormatBytes(theoreticalMaxMemoryWithMargin).c_str(), theoreticalMaxMemoryWithMargin);
        LOG_M("[MemoryUtil]   Pod Memory Limit:       %s (%ld bytes)",
              FormatBytes(info.limitBytes).c_str(), info.limitBytes);

        // Compare theoretical max memory (with margin) against Pod memory limit
        if (theoreticalMaxMemoryWithMargin > info.limitBytes) {
            double ratio = static_cast<double>(theoreticalMaxMemoryWithMargin) / static_cast<double>(info.limitBytes);
            LOG_W("[MemoryUtil] ==================== MEMORY CONFIG WARNING ====================");
            LOG_W("[MemoryUtil] Request configuration (with 20%% margin) may exceed Pod memory limit!");
            LOG_W("[MemoryUtil]   maxReqs (%zu) * bodyLimit (%s) * 1.2 = %s",
                  maxReqs, FormatBytes(bodyLimit).c_str(), FormatBytes(theoreticalMaxMemoryWithMargin).c_str());
            LOG_W("[MemoryUtil]   This is %.2fx of Pod memory limit (%s)",
                  ratio, FormatBytes(info.limitBytes).c_str());
            LOG_W("[MemoryUtil] Recommendation: Reduce 'max_requests' or 'body_limit' in config,");
            LOG_W("[MemoryUtil]                 or increase Pod memory limit.");
            LOG_W("[MemoryUtil] ================================================================");
            return;
        }

        // Calculate config (with margin) usage percentage of memory limit
        double usagePercent = (static_cast<double>(theoreticalMaxMemoryWithMargin) / info.limitBytes) * 100.0;
        LOG_M("[MemoryUtil]   Config (with margin) uses %.2f%% of Pod memory limit", usagePercent);
    } catch (const std::exception& e) {
        LOG_W("[MemoryUtil] Exception during memory config risk check: %s", e.what());
    } catch (...) {
        LOG_W("[MemoryUtil] Unknown exception during memory config risk check");
    }
}

} // namespace MINDIE::MS