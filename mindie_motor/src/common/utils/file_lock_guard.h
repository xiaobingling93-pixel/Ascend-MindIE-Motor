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
#ifndef FILE_LOCK_GUARD_H
#define FILE_LOCK_GUARD_H

#include <string>
#include <fcntl.h>      // POSIX file operations: open(), O_CREAT, O_RDWR
#include <sys/file.h>   // POSIX file locking: flock(), LOCK_EX, LOCK_UN
#include <unistd.h>     // POSIX standard: close()
#include "Logger.h"
#include "Util.h"

namespace MINDIE {
namespace MS {

/**
 * @brief RAII file lock guard for automatic file locking and unlocking
 *
 * This class provides automatic management of file locks using POSIX flock().
 * The lock is acquired in the constructor and automatically released in the destructor.
 *
 * Example usage:
 * @code
 *   std::string lockPath = "/path/to/file.lock";
 *   FileLockGuard lockGuard(lockPath);
 *   if (!lockGuard.IsLocked()) {
 *       // Failed to acquire lock
 *       return -1;
 *   }
 *   // Perform operations under lock protection
 *   // Lock is automatically released when lockGuard goes out of scope
 * @endcode
 */
class FileLockGuard {
public:
    /**
     * @brief Construct a new FileLockGuard object and acquire exclusive lock
     *
     * @param lockPath Path to the lock file (will be created if not exists)
     * @param blocking Whether to block waiting for lock (default: true)
     *                 If false, uses LOCK_NB for non-blocking lock attempt
     */
    explicit FileLockGuard(const std::string& lockPath, bool blocking = true)
        : fd_(-1), locked_(false), lockPath_(lockPath)
    {
        std::string path = lockPath;
        if (!PathCheckForCreate(path)) {
            LOG_E("[FileLockGuard] Invalid path of LockGuard file: %s", lockPath.c_str());
            return;
        }
        // Create lock file with read/write permission for owner only
        fd_ = open(lockPath.c_str(), O_CREAT | O_RDWR, 0600); // 0600 means read and write permission for owner only
        if (fd_ == -1) {
            LOG_E("[FileLockGuard] Failed to create lock file: %s (errno: %d)",
                  lockPath.c_str(), errno);
            return;
        }

        // Acquire exclusive lock (blocking or non-blocking)
        int lockFlags;
        if (blocking) {
            lockFlags = LOCK_EX; // blocking mode
        } else {
            lockFlags = LOCK_EX | LOCK_NB; // non-blocking mode
        }

        if (flock(fd_, lockFlags) == -1) {
            if (!blocking && errno == EWOULDBLOCK) {
                LOG_W("[FileLockGuard] Lock is held by another process: %s", lockPath.c_str());
            } else {
                LOG_E("[FileLockGuard] Failed to acquire file lock: %s (errno: %d)",
                      lockPath.c_str(), errno);
            }
            close(fd_);
            fd_ = -1;
            return;
        }

        locked_ = true;
        LOG_D("[FileLockGuard] Successfully acquired lock on %s", lockPath.c_str());
    }

    /**
     * @brief Destroy the FileLockGuard object and release lock
     *
     * Automatically releases the lock and closes the file descriptor.
     * This ensures proper cleanup even in case of exceptions.
     */
    ~FileLockGuard()
    {
        if (locked_) {
            flock(fd_, LOCK_UN);  // Release lock
            LOG_D("[FileLockGuard] Released lock on %s", lockPath_.c_str());
        }
        if (fd_ != -1) {
            close(fd_);  // Close file descriptor
        }
    }

    // Delete copy constructor and copy assignment operator
    FileLockGuard(const FileLockGuard&) = delete;
    FileLockGuard& operator=(const FileLockGuard&) = delete;

    // Delete move constructor and move assignment operator
    FileLockGuard(FileLockGuard&&) = delete;
    FileLockGuard& operator=(FileLockGuard&&) = delete;

    /**
     * @brief Check if the lock was successfully acquired
     *
     * @return true if lock is held, false otherwise
     */
    bool IsLocked() const { return locked_; }

    /**
     * @brief Get the file descriptor of the lock file
     *
     * @return int File descriptor, or -1 if not opened
     */
    int GetFd() const { return fd_; }

    /**
     * @brief Get the path of the lock file
     *
     * @return const std::string& Path to the lock file
     */
    const std::string& GetLockPath() const { return lockPath_; }

private:
    int fd_;                    // File descriptor
    bool locked_;               // Whether lock is successfully acquired
    std::string lockPath_;      // Path to the lock file
};

} // namespace MS
} // namespace MINDIE

#endif // FILE_LOCK_GUARD_H
