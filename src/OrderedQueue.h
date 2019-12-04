/* ------------------------------------------------------------------
 * Copyright (C) 2017 AVT GmbH - Fabien Vercasson
 * Copyright (C) 2019 Matthias P. Braendli
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <cstdint>
#include <cstdio>

using timestamp_t = std::chrono::system_clock::time_point;
using vec_u8 = std::vector<uint8_t>;

struct OrderedQueueData {
    vec_u8 buf;
    timestamp_t capture_timestamp;
};

/* An queue that receives indexed frames, potentially out-of-order,
 * which returns the frames in-order.
 */
class OrderedQueue
{
    public:
        /* Indexes of frames must be between 0 and maxIndex.
         * The queue will fill to capacity if there is a gap.
         */
        OrderedQueue(int32_t maxIndex, size_t capacity);

        void push(int32_t index, const uint8_t* buf, size_t size, const timestamp_t& ts);
        bool availableData() const;

        /* Return the next buffer, or an empty buffer if none available */
        OrderedQueueData pop(int32_t *returnedIndex=nullptr);

    private:
        int32_t     _maxIndex;
        size_t      _capacity;
        uint64_t    _duplicated = 0;
        uint64_t    _overruns = 0;
        int32_t     _lastIndexPop = -1;

        std::map<int, OrderedQueueData> _stock;
};

