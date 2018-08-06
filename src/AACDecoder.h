/* ------------------------------------------------------------------
 * Copyright (C) 2011 Martin Storsjo
 * Copyright (C) 2017 Matthias P. Braendli
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

/*!
 *  \file AACDecoder.h
 *  \brief Uses FDK-AAC to decode the AAC format for loopback tests and
 *         to measure the audio level
 */

#pragma once

#include <fdk-aac/aacdecoder_lib.h>
#include <cstdint>
#include <cstddef>
#include <vector>

class AACDecoder {
    public:
        AACDecoder();
        ~AACDecoder();
        AACDecoder(const AACDecoder&) = delete;
        AACDecoder& operator=(const AACDecoder&) = delete;
        void decode_frame(uint8_t *data, size_t len);

        struct peak_t { int16_t peak_left; int16_t peak_right; };
        peak_t get_peaks();

    private:
        void decode_au(uint8_t *data, size_t len);
        bool m_decoder_set_up = false;
        int m_channels = 0;

        peak_t m_peak;

        HANDLE_AACDECODER m_handle;
        std::vector<uint8_t> m_output_frame;
};

