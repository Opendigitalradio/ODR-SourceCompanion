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

#include "OrderedQueue.h"
#include <cstring>
#include <cstdio>
#include <stdint.h>

using namespace std;

#define DEBUG(fmt, A...)   fprintf(stderr, "OrderedQueue: " fmt, ##A)
//#define DEBUG(x...)
#define ERROR(fmt, A...)   fprintf(stderr, "OrderedQueue: ERROR " fmt, ##A)

OrderedQueue::OrderedQueue(int maxIndex, size_t capacity) :
    _maxIndex(maxIndex),
    _capacity(capacity)
{
}

void OrderedQueue::push(int32_t index, const uint8_t* buf, size_t size, const timestamp_t& ts)
{
    // DEBUG("OrderedQueue::push index=%d\n", index);
    index = (index + _maxIndex) % _maxIndex;

    // First frame makes the index initialisation.
    if (_lastIndexPop == -1) {
        // Equivalent to index - 1 in modulo arithmetic:
        _lastIndexPop = (index + _maxIndex-1) % _maxIndex;
    }

    if (_stock.size() < _capacity) {
        if (_stock.find(index) != _stock.end()) {
            // index already exists, duplicated frame
            // Replace the old one by the new one.
            // the old one could a an old frame from the previous index loop
            _duplicated++;
            DEBUG("Duplicated index=%d\n", index);
        }

        OrderedQueueData oqd;
        oqd.buf.resize(size);
        oqd.capture_timestamp = ts;

        copy(buf, buf + size, oqd.buf.begin());
        _stock[index] = move(oqd);
    }
    else {
        _overruns++;
        if (_overruns < 100) {
            DEBUG("Overruns (size=%zu) index=%d not inserted\n", _stock.size(), index);
        }
        else if (_overruns == 100) {
            DEBUG("stop displaying Overruns\n");
        }
    }
}

bool OrderedQueue::availableData() const
{
    // TODO Wait for filling gaps
    return _stock.size() > 0;
}

OrderedQueueData OrderedQueue::pop(int32_t *returnedIndex)
{
    OrderedQueueData oqd;
    uint32_t gap = 0;

    if (_stock.size() > 0) {
        int32_t nextIndex = (_lastIndexPop+1) % _maxIndex;
        bool found = false;
        while (not found) {
            try {
                oqd = move(_stock.at(nextIndex));
                _stock.erase(nextIndex);
                _lastIndexPop = nextIndex;
                if (returnedIndex) *returnedIndex = _lastIndexPop;
                found = true;
            }
            catch (const std::out_of_range&) {
                if (_stock.size() < _capacity) {
                    break;
                }
                else {
                    // Search for the new index, starting from the current one
                    // This could be optimised, but the modulo makes things
                    // not easy.
                    gap++;
                    nextIndex = (nextIndex+1) % _maxIndex;
                }
            }
        }
    }

    if (gap > 0) {
        DEBUG("index jump of %d\n", gap);
    }

    return oqd;
}

