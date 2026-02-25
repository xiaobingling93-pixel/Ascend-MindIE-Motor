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
#include <sys/stat.h>
#include <system_error>
#include "SharedMemoryUtils.h"

using namespace MINDIE::MS;

SharedMemoryUtils::SharedMemoryUtils(const std::string& shmName, const std::string& semName, size_t bufferSize)
    : bufferSize_(bufferSize),
      data_(nullptr),
      cb_(nullptr),
      sem_(nullptr),
      shmName_(shmName),
      semName_(semName),
      shmFd_(-1),
      isOwner_(false),
      mmapAddr_(MAP_FAILED),
      mmapLength_(0)
{
    size_t totalSize = sizeof(CircularBufferHeader) + bufferSize_;
    mmapLength_ = totalSize;
    shmFd_ = shm_open(shmName_.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    if (shmFd_ == -1) {
        if (errno != ENOENT) {
            throw std::system_error(errno, std::system_category(),
                "[SharedMemoryUtils] Failed to open shared memory: " + shmName_);
        }

        shmFd_ = shm_open(shmName_.c_str(), O_CREAT | O_EXCL | O_RDWR,
                          S_IRUSR | S_IWUSR);
        if (shmFd_ == -1) {
            throw std::system_error(errno, std::system_category(),
                "[SharedMemoryUtils] Failed to create shared memory: " + shmName_);
        }
        
        try {
            if (ftruncate(shmFd_, totalSize) == -1) {
                throw std::system_error(errno, std::system_category(),
                    "[SharedMemoryUtils] Failed to set shared memory size");
            }
            isOwner_ = true;
        } catch (...) {
            close(shmFd_);
            shm_unlink(shmName_.c_str());
            throw;
        }
    }

    mmapAddr_ = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd_, 0);
    if (mmapAddr_ == MAP_FAILED) {
        close(shmFd_);
        shmFd_ = -1;
        throw std::system_error(errno, std::system_category(),
            "[SharedMemoryUtils] Failed to map shared memory: " + shmName_);
    }

    cb_ = static_cast<CircularBufferHeader*>(mmapAddr_);
    data_ = static_cast<int8_t*>(mmapAddr_) + sizeof(CircularBufferHeader);

    if (isOwner_) {
        new (cb_) CircularBufferHeader();
        std::fill(data_, data_ + bufferSize_, 0);
    }

    int semFlags = O_CREAT;
    if (!isOwner_) {
        semFlags = 0;
    }

    sem_ = sem_open(semName_.c_str(), semFlags, S_IRUSR | S_IWUSR, 1);
    if (sem_ == SEM_FAILED) {
        LOG_E("[SharedMemoryUtils] Failed to create semaphore");
        Cleanup();
    }

    CheckPermission();
}

SharedMemoryUtils::~SharedMemoryUtils()
{
    Cleanup();
}

void SharedMemoryUtils::CheckPermission()
{
    // 检查共享存储与命名信号量的属主, 属主正确时同步显式设置权限为0600
    uint32_t mode = 0600;

    std::string name = semName_[0] == '/' ? semName_.substr(1) : semName_;
    std::string semPath;
    std::string shmPath;
#ifdef __linux__
    semPath = "/dev/shm/sem." + name;
    shmPath = "/dev/shm" + shmName_;
#else
    throw std::runtime_error("[SharedMemoryUtils] POSIX semaphore metadata not accessible on this OS.");
#endif

    struct stat stShm;
    if (stat(shmPath.c_str(), &stShm) != 0) {
        Cleanup();
        throw std::system_error(errno, std::system_category(),
            "[SharedMemoryUtils] stat failed for shared memory: " + shmName_);
    }
    if (stShm.st_uid != getuid()) {
        Cleanup();
        throw std::runtime_error("[SharedMemoryUtils] Shared memory owner mismatch: "
            + shmName_);
    }
    if (chmod(shmPath.c_str(), mode) != 0) {
        Cleanup();
        throw std::runtime_error("[SharedMemoryUtils] Shared memory permission set failed: "
            + shmName_);
    }
    
    if (sem_ == SEM_FAILED) {
        return;
    }
    
    struct stat stSem;
    if (stat(semPath.c_str(), &stSem) != 0) {
        sem_ = SEM_FAILED;
        LOG_E("[SharedMemoryUtils] Failed to stat semaphore file: " + semPath);
        Cleanup();
    }
    if (stSem.st_uid != getuid()) {
        sem_ = SEM_FAILED;
        LOG_E("[SharedMemoryUtils] Semaphore owner mismatch: " + semName_);
        Cleanup();
    }
    if (chmod(semPath.c_str(), mode) != 0) {
        sem_ = SEM_FAILED;
        LOG_E("[SharedMemoryUtils] Semaphore permission set failed: " + shmName_);
        Cleanup();
    }
}

void SharedMemoryUtils::Cleanup()
{
    if (sem_) {
        sem_close(sem_);
        if (isOwner_) {
            sem_unlink(semName_.c_str());
        }
        sem_ = nullptr;
    }

    if (mmapAddr_ != MAP_FAILED) {
        munmap(mmapAddr_, mmapLength_);
        mmapAddr_ = MAP_FAILED;
    }

    if (shmFd_ != -1) {
        close(shmFd_);
        shmFd_ = -1;
    }
}

namespace {
void RobustSemWait(sem_t* sem)
{
    int result;
    do {
        result = sem_wait(sem);
    } while (result == -1 && errno == EINTR); // 循环直到不是被信号中断

    if (result == -1) {
        // 如果不是 EINTR，那就是其他致命错误
        throw std::system_error(errno, std::system_category(), "sem_wait");
    }
}

void RobustSemPost(sem_t* sem)
{
    if (sem_post(sem) == -1) {
        throw std::system_error(errno, std::system_category(), "sem_post");
    }
}
}  // namespace

std::string SharedMemoryUtils::Read()
{
    // Check if semaphore is valid before proceeding
    // If SEM_FAILED, it means the semaphore was already unlinked or failed to open
    if (sem_ == SEM_FAILED) {
        LOG_E("[SharedMemoryUtils] Semaphore is invalid, read failed");
        return "";
    }

    try {
        RobustSemWait(sem_);

        uint32_t readPos = cb_->readIdx.load(std::memory_order_relaxed);
        uint32_t writePos = cb_->writeIdx.load(std::memory_order_acquire);
        if (readPos == writePos) {
            RobustSemPost(sem_);
            return "";
        }

        uint32_t len = 0;
        while (len < static_cast<uint32_t>(bufferSize_)) {
            uint32_t idx = (readPos + len) % static_cast<uint32_t>(bufferSize_);
            if (data_[idx] == '\0') {
                break;
            }
            ++len;
        }

        std::string result;
        if (len > 0) {
            result.resize(len);
            for (uint32_t i = 0; i < len; ++i) {
                result[i] = data_[(readPos + i) % static_cast<uint32_t>(bufferSize_)];
            }

            cb_->readIdx.store((readPos + len + 1) % static_cast<uint32_t>(bufferSize_), std::memory_order_release);
        }

        RobustSemPost(sem_);
        return result;
    } catch (const std::system_error& e) {
        std::cerr << "Synchronization error: " << e.what() << std::endl;
        throw;
    }
}

void SharedMemoryUtils::ClearResources(const std::string& shmName, const std::string& semName)
{
    shm_unlink(shmName.c_str());
    sem_unlink(semName.c_str());
}

bool RetainSharedMemoryUtils::Write(const std::string& msg)
{
    // Check if semaphore is valid before proceeding
    // If SEM_FAILED, it means the semaphore was already unlinked or failed to open
    if (sem_ == SEM_FAILED) {
        LOG_E("[SharedMemoryUtils] Semaphore is invalid, write failed");
        return false;
    }

    try {
        RobustSemWait(sem_);

        uint32_t writePos = cb_->writeIdx.load(std::memory_order_relaxed);
        uint32_t readPos = cb_->readIdx.load(std::memory_order_acquire);
        uint32_t totalLen = static_cast<uint32_t>(msg.size()) + 1;
        if (totalLen >= static_cast<uint32_t>(bufferSize_)) {
            LOG_E("[SharedMemoryUtils] Message too long to fit in buffer");
            RobustSemPost(sem_);
            return false;
        }

        uint32_t dataInBuffer = (writePos >= readPos)
            ? writePos - readPos
            : static_cast<uint32_t>(bufferSize_) - readPos + writePos;

        uint32_t availableSpace = static_cast<uint32_t>(bufferSize_) - dataInBuffer - 1;

        if (totalLen > availableSpace) {
            LOG_E("[SharedMemoryUtils] Buffer full, cannot write");
            RobustSemPost(sem_);
            return false;
        }

        uint32_t remain = static_cast<uint32_t>(bufferSize_) - writePos;
        uint32_t copyLen = std::min(remain, totalLen);

        std::copy(msg.c_str(), msg.c_str() + copyLen, data_ + writePos);
        if (copyLen < totalLen) {
            std::copy(msg.c_str() + copyLen, msg.c_str() + totalLen, data_);
        }

        cb_->writeIdx.store((writePos + totalLen) % static_cast<uint32_t>(bufferSize_), std::memory_order_release);
        RobustSemPost(sem_);
        return true;
    } catch (const std::system_error& e) {
        std::cerr << "Synchronization error: " << e.what() << std::endl;
        throw;
    }
}

bool OverwriteSharedMemoryUtils::Write(const std::string& msg)
{
    // Check if semaphore is valid before proceeding
    // If SEM_FAILED, it means the semaphore was already unlinked or failed to open
    if (sem_ == SEM_FAILED) {
        LOG_E("[SharedMemoryUtils] Semaphore is invalid, write failed");
        return false;
    }

    try {
        RobustSemWait(sem_);

        uint32_t totalLen = static_cast<uint32_t>(msg.size()) + 1;
        if (totalLen >= static_cast<uint32_t>(bufferSize_)) {
            LOG_E("[SharedMemoryUtils] Message too long to fit in buffer");
            RobustSemPost(sem_);
            return false;
        }

        std::copy(msg.c_str(), msg.c_str() + totalLen, data_);
        cb_->readIdx.store(0, std::memory_order_release);
        cb_->writeIdx.store(totalLen, std::memory_order_release);
        RobustSemPost(sem_);
        return true;
    } catch (const std::system_error& e) {
        std::cerr << "Synchronization error: " << e.what() << std::endl;
        throw;
    }
}