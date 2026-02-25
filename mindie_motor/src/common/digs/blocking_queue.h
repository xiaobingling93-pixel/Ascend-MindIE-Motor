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
#ifndef MINDIE_DIGS_BLOCKING_QUEUE_H
#define MINDIE_DIGS_BLOCKING_QUEUE_H

#include <semaphore.h>
#include <mutex>
#include <shared_mutex>
#include <queue>

namespace MINDIE::MS {
template<typename T>
class BlockingQueue {
public:
    BlockingQueue()
    {
        sem_init(&sem_, 0, 0);
    }

    ~BlockingQueue()
    {
        sem_destroy(&sem_);
    }

    int Push(T item)
    {
        std::unique_lock<std::shared_mutex> lock(queueMutex_);
        queue_.push(std::move(item));
        return sem_post(&sem_);
    }

    T Take()
    {
        sem_wait(&sem_);
        std::unique_lock<std::shared_mutex> lock(queueMutex_);
        T value(std::move(queue_.front()));
        queue_.pop();
        return value;
    }

private:
    sem_t sem_{};

    std::queue<T> queue_;

    std::shared_mutex queueMutex_;
};
}
#endif
