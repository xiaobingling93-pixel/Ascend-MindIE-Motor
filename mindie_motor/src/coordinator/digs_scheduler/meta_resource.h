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
#ifndef MINDIE_DIGS_META_RESOURCE_H
#define MINDIE_DIGS_META_RESOURCE_H

#include "common.h"
#include "Logger.h"


namespace MINDIE::MS {

class MetaResource {
public:
    explicit MetaResource(std::vector<uint64_t>&& attributes) : attributes_(std::move(attributes)) {};

    explicit MetaResource();

    explicit MetaResource(uint64_t defaultValue);

    explicit MetaResource(const std::unique_ptr<MetaResource>& metaRes) : attributes_(metaRes->attributes_) {};

    ~MetaResource() = default;

    size_t Size() const { return attributes_.size(); }

    uint64_t At(size_t idx) { return attributes_[idx]; }

    bool CompareTo(const MetaResource& other);

    bool IncResource(const MetaResource& other);

    bool DecResource(MetaResource& other);

    uint64_t Slots();

    uint64_t Blocks();

    uint64_t UpdateBlocks(uint64_t updateValue);

    void UpdateTokens(uint64_t updateValue);

    static void InitAttrs(std::string& names, std::string& attrs, std::string& weights);

    static void InitWeights(std::string& weights);

    const static std::vector<double>& ResWeight() { return resWeight_; }

    const static std::vector<std::string>& AttrName() { return attrName_; }

    static int32_t ResMul(std::unique_ptr<MetaResource> &result, const std::unique_ptr<MetaResource>& src,
                          double_t mul);

    static uint64_t TotalLoad(const std::unique_ptr<MetaResource>& res);

    static double ComputeAwareLoad(const std::unique_ptr<MetaResource>& res, size_t maxSlots, size_t reportedBlocks,
                                   const std::unique_ptr<MetaResource>& demand);

    static size_t GetTokenSum(const std::unique_ptr<MetaResource>& res, size_t maxSlots,
                              const std::unique_ptr<MetaResource>& demand);

private:
    friend std::ostream& operator<<(std::ostream& os, const MetaResource& res)
    {
        if (!res.attributes_.empty()) {
            os << common::Dims2Str(&res.attributes_[0], res.attributes_.size());
        } else {
            os << "[]";
        }
        return os;
    }

    friend bool operator<(const MetaResource& res1, const MetaResource& res2)
    {
        for (size_t i = 0; i < res1.attributes_.size(); i++) {
            if (res1.attributes_[i] > res2.attributes_[i]) { // 每一维均小于视为小于
                return false;
            }
        }
        return true;
    }

private:
    static std::vector<uint64_t> defaultAttr_;
    static std::vector<std::string> attrName_;
    static std::vector<double> resWeight_;

    std::vector<uint64_t> attributes_;
    // 默认将对应的Seq tokens按从小到大排序
    std::multiset<uint64_t> computeAttributes_;
};

}
#endif
