/* ------------------------------------------------------------------
 * Copyright (C) 2011 Martin Storsjo
 * Copyright (C) 2020 Matthias P. Braendli
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
#include <vector>
#include <chrono>
#include <deque>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "common.h"
#include "zmq.hpp"
#include "ClockTAI.h"
#include "edioutput/TagItems.h"
#include "edioutput/TagPacket.h"
#include "edioutput/AFPacket.h"
#include "edioutput/Transport.h"
extern "C" {
#include "encryption.h"
}

namespace Output {

/*! \file Outputs.h
 *
 * Declaration of all outputs
 */

class Base {
    public:
        virtual ~Base() {};

        /*! Write a buffer of encoded data to the output */
        virtual bool write_frame(const uint8_t *buf, size_t len) = 0;

        /*! Update peak audio level information */
        virtual void update_audio_levels(
                int16_t audiolevel_left, int16_t audiolevel_right);

    protected:
        int16_t m_audio_left = 0;
        int16_t m_audio_right = 0;
};

class File : public Base {
    public:
        File(const char *filename);
        File(FILE *file);
        File(const File&) = delete;
        File& operator=(const File&) = delete;
        virtual ~File() override;

        virtual bool write_frame(const uint8_t *buf, size_t len) override;

    private:
        FILE *m_fd = nullptr;
};

/*! This defines the on-wire representation of a ZMQ message header.
 * It must be compatible with the definition in ODR-DabMux.
 *
 * The data follows right after this header */
struct zmq_frame_header_t
{
    uint16_t version; // we support version=1 now
    uint16_t encoder; // see ZMQ_ENCODER_XYZ

    /* length of the 'data' field */
    uint32_t datasize;

    /* Audio level, peak, linear PCM */
    int16_t audiolevel_left;
    int16_t audiolevel_right;

    /* Data follows this header */
} __attribute__ ((packed));

#define ZMQ_ENCODER_AACPLUS 1
#define ZMQ_ENCODER_MPEG_L2 2

#define ZMQ_HEADER_SIZE sizeof(struct zmq_frame_header_t)

/* The expected frame size incl data of the given frame */
#define ZMQ_FRAME_SIZE(f) (sizeof(struct zmq_frame_header_t) + f->datasize)

#define ZMQ_FRAME_DATA(f) ( ((uint8_t*)f)+sizeof(struct zmq_frame_header_t) )


class ZMQ: public Base {
    public:
        ZMQ();
        ZMQ(const ZMQ&) = delete;
        ZMQ& operator=(const ZMQ&) = delete;
        virtual ~ZMQ() override;

        void connect(const char *uri, const char *keyfile);
        void set_encoder_type(codec_selection_t& enc, int bitrate);

        virtual bool write_frame(const uint8_t *buf, size_t len) override;

    private:
        zmq::context_t m_ctx;
        zmq::socket_t m_sock;

        int m_bitrate = 0;
        char m_secretkey[CURVE_KEYLEN+1];
        codec_selection_t m_encoder = codec_selection_t::dabplus;
        using vec_u8 = std::vector<uint8_t>;
        vec_u8 m_framebuf;
};


class EDI: public Base {
    public:
        EDI();
        EDI(const EDI&) = delete;
        EDI& operator=(const EDI&) = delete;
        virtual ~EDI() override;

        void set_odr_version_tag(const std::string& odr_version_tag);

        void add_udp_destination(const std::string& host, unsigned int port);
        void add_tcp_destination(const std::string& host, unsigned int port);

        // Enables PFT layer and sets FEC
        void set_fec(int fec);

        void set_tist(bool enable, uint32_t delay_ms, const std::chrono::system_clock::time_point& ts);

        bool enabled() const;

        virtual bool write_frame(const uint8_t *buf, size_t len) override;

    private:
        std::string m_odr_version_tag;

        edi::configuration_t m_edi_conf;
        std::shared_ptr<edi::Sender> m_edi_sender;

        uint32_t m_timestamp = 0;
        uint32_t m_num_frames_sent = 0;
        std::time_t m_edi_time = 0;
        std::chrono::steady_clock::time_point m_time_last_version_sent;

        edi::TagDSTI m_edi_tagDSTI;

        ClockTAI m_clock_tai;
        bool m_tist = false;
        uint32_t m_delay_ms = 0;
};

}
