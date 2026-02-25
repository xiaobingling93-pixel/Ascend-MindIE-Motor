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
#ifndef CONCURRENT_SET_H
#define CONCURRENT_SET_H
#include <unordered_set>
#include <pthread.h>
#include <vector>

namespace MINDIE {
namespace MS {
template <typename T> class ConcurrentSet {
public:
    ConcurrentSet() { pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE); }

    ~ConcurrentSet() { pthread_spin_destroy(&spinlock); }

    // 插入元素
    bool Insert(const T &value)
    {
        pthread_spin_lock(&spinlock);
        auto result = set.insert(value);
        pthread_spin_unlock(&spinlock);
        return result.second;  // 返回是否成功插入（true表示新插入，false表示已存在）
    }

    // 删除元素
    bool Erase(const T &value)
    {
        pthread_spin_lock(&spinlock);
        size_t count = set.erase(value);
        pthread_spin_unlock(&spinlock);
        return count > 0;  // 返回是否成功删除
    }

    // 检查元素是否存在
    bool Contains(const T &value) const
    {
        pthread_spin_lock(&spinlock);
        bool exists = set.find(value) != set.end();
        pthread_spin_unlock(&spinlock);
        return exists;
    }

    // 清空集合
    void Clear()
    {
        pthread_spin_lock(&spinlock);
        set.clear();
        pthread_spin_unlock(&spinlock);
    }

    // 判断是否为空
    bool Empty() const
    {
        pthread_spin_lock(&spinlock);
        bool is_empty = set.empty();
        pthread_spin_unlock(&spinlock);
        return is_empty;
    }

    // 获取集合大小
    size_t Size() const
    {
        pthread_spin_lock(&spinlock);
        size_t ret = set.size();
        pthread_spin_unlock(&spinlock);
        return ret;
    }

    // 获取所有元素（返回vector）
    std::vector<T> ToVector() const
    {
        pthread_spin_lock(&spinlock);
        std::vector<T> result(set.begin(), set.end());
        pthread_spin_unlock(&spinlock);
        return result;
    }

    // 遍历集合中的所有元素
    template <typename Func>
    void ForEach(Func func) const
    {
        pthread_spin_lock(&spinlock);
        for (const auto &item : set) {
            func(item);
        }
        pthread_spin_unlock(&spinlock);
    }

    // 批量插入
    template <typename Iterator>
    size_t InsertRange(Iterator begin, Iterator end)
    {
        pthread_spin_lock(&spinlock);
        size_t count = 0;
        for (auto it = begin; it != end; ++it) {
            if (set.insert(*it).second) {
                count++;
            }
        }
        pthread_spin_unlock(&spinlock);
        return count;  // 返回实际插入的元素数量
    }

    // 批量删除
    template <typename Iterator>
    size_t EraseRange(Iterator begin, Iterator end)
    {
        pthread_spin_lock(&spinlock);
        size_t count = 0;
        for (auto it = begin; it != end; ++it) {
            count += set.erase(*it);
        }
        pthread_spin_unlock(&spinlock);
        return count;  // 返回实际删除的元素数量
    }

private:
    std::unordered_set<T> set;
    mutable pthread_spinlock_t spinlock;
};
} // namespace MS
} // namespace MINDIE
#endif