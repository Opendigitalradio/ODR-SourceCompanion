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
#include <cstdint>
#include <cstdio>

class OrderedQueue
{
    public:
        OrderedQueue(int32_t countModulo, size_t capacity);

        void push(int32_t count, const uint8_t* buf, size_t size);
        bool availableData() const;
        size_t pop(std::vector<uint8_t>& buf, int32_t *retCount=nullptr);

        using OrderedQueueData = std::vector<uint8_t>;

    private:
        int32_t     _countModulo;
        size_t      _capacity;
        uint64_t    _duplicated = 0;
        uint64_t    _overruns = 0;
        int32_t     _lastCount = -1;

        std::map<int, OrderedQueueData> _stock;
};

