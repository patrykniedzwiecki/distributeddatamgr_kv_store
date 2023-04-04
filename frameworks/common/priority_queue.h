/*
* Copyright (c) 2023 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include <map>
#include <memory>
#include <mutex>
#include <queue>

#ifndef OHOS_DISTRIBUTED_DATA_FRAMEWORKS_COMMON_PRIORITY_QUEUE_H
#define OHOS_DISTRIBUTED_DATA_FRAMEWORKS_COMMON_PRIORITY_QUEUE_H
namespace OHOS {
template<typename _Tsk, typename _Tme, typename _Tid>
class PriorityQueue {
public:
    struct Index {
        using TskIndex = typename std::map<_Tid, _Tsk>::iterator;
        _Tme time;
        TskIndex index;
        bool isValid = true;
        Index(_Tme time, TskIndex index) : time(time), index(index) {}
    };
    
    struct cmp {
        bool operator()(std::shared_ptr<Index> a, std::shared_ptr<Index> b)
        {
            return a->time > b->time;
        }
    };
    
    PriorityQueue() {}
    ~PriorityQueue() {}

    _Tsk Pop()
    {
        std::unique_lock<decltype(pqMtx_)> lock(pqMtx_);
        _Tsk res;
        while(!queue_.empty()) {
            while (!queue_.empty() && !queue_.top()->isValid) {
                queue_.pop();
            }
            if (!queue_.empty()) {
                res = queue_.top()->index->second;
                if (res.startTime > std::chrono::steady_clock::now()) {
                    popCv_.wait_until(lock, res.startTime);
                    continue;
                }
                queue_.pop();
                tasks_.erase(res.GetId());
                indexes_.erase(res.GetId());
                break;
            }
        }
        return res;
    }

    bool Push(_Tsk tsk)
    {
        std::unique_lock<std::mutex> lock(pqMtx_);
        if (!tsk.Valid()) {
            return false;
        }
        auto index = tasks_.emplace(tsk.GetId(), std::move(tsk)).first;
        auto item = std::make_shared<Index>(tsk.startTime, index);
        queue_.push(item);
        indexes_.emplace(tsk.GetId(), item);
        return true;
    }

    size_t Size()
    {
        std::unique_lock<decltype(pqMtx_)> lock(pqMtx_);
        return tasks_.size();
    }

    bool Empty()
    {
        std::unique_lock<decltype(pqMtx_)> lock(pqMtx_);
        return tasks_.size() == 0;
    }

    _Tsk Find(_Tid id)
    {
        std::unique_lock<decltype(pqMtx_)> lock(pqMtx_);
        _Tsk res = tasks_[id];
        if (!res.Valid()) {
            tasks_.erase(id);
        }
        return res;
    }

    bool Remove(_Tid id)
    {
        std::unique_lock<decltype(pqMtx_)> lock(pqMtx_);
        bool res = true;
        _Tsk task = tasks_[id];
        if (!task.Valid()) {
            res = false;
        } else {
            indexes_.at(id)->isValid = false;
            indexes_.erase(id);
        }
        tasks_.erase(id);
        return res;
    }

private:
    std::mutex pqMtx_;
    _Tsk tsk_ = _Tsk();
    std::condition_variable popCv_;
    std::map<_Tid, _Tsk> tasks_;
    std::map<_Tid, std::shared_ptr<Index>> indexes_;
    std::priority_queue<std::shared_ptr<Index>, std::vector<std::shared_ptr<Index>>, cmp> queue_;
};
} // namespace OHOS
#endif //OHOS_DISTRIBUTED_DATA_FRAMEWORKS_COMMON_PRIORITY_QUEUE_H
