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

#include "Outputs.h"
#include <string>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <cassert>

namespace Output {

using namespace std;

void Base::update_audio_levels(int16_t audiolevel_left, int16_t audiolevel_right)
{
    m_audio_left = audiolevel_left;
    m_audio_right = audiolevel_right;
}

File::File(const char *filename)
{
    m_fd = fopen(filename, "wb");
    if (m_fd == nullptr) {
        throw runtime_error(string("Error opening output file: ") + strerror(errno));
    }
}

File::File(FILE *fd) : m_fd(fd) { }

File::~File() {
    if (m_fd) {
        fclose(m_fd);
        m_fd = nullptr;
    }
}

bool File::write_frame(const uint8_t *buf, size_t len)
{
    if (m_fd == nullptr) {
        throw logic_error("Invalid usage of closed File output");
    }

    return fwrite(buf, len, 1, m_fd) == 1;
}

ZMQ::ZMQ() :
    m_ctx(),
    m_sock(m_ctx, ZMQ_PUB)
{
    // Do not wait at teardown to send all data out
    int linger = 0;
    m_sock.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
}

ZMQ::~ZMQ() {}

void ZMQ::connect(const char *uri, const char *keyfile)
{
    if (keyfile) {
        fprintf(stderr, "Enabling encryption\n");

        int rc = readkey(keyfile, m_secretkey);
        if (rc) {
            throw runtime_error("Error reading secret key");
        }

        const int yes = 1;
        m_sock.setsockopt(ZMQ_CURVE_SERVER,
                &yes, sizeof(yes));

        m_sock.setsockopt(ZMQ_CURVE_SECRETKEY,
                m_secretkey, CURVE_KEYLEN);
    }
    m_sock.connect(uri);
}

void ZMQ::set_encoder_type(codec_selection_t& enc, int bitrate)
{
    m_encoder = enc;
    m_bitrate = bitrate;
}

bool ZMQ::write_frame(const uint8_t *buf, size_t len)
{
    if (m_framebuf.size() != ZMQ_HEADER_SIZE + len) {
        m_framebuf.resize(ZMQ_HEADER_SIZE + len);
    }

    zmq_frame_header_t *zmq_frame_header = (zmq_frame_header_t*)m_framebuf.data();

    try {
        switch (m_encoder) {
            case codec_selection_t::dabplus:
                zmq_frame_header->encoder = ZMQ_ENCODER_AACPLUS;
                break;
            case codec_selection_t::mpeg_layer_2:
                zmq_frame_header->encoder = ZMQ_ENCODER_MPEG_L2;
                break;
        }

        zmq_frame_header->version = 1;
        zmq_frame_header->datasize = len;
        zmq_frame_header->audiolevel_left = m_audio_left;
        zmq_frame_header->audiolevel_right = m_audio_right;

        assert(ZMQ_FRAME_SIZE(zmq_frame_header) <= m_framebuf.size());

        memcpy(ZMQ_FRAME_DATA(zmq_frame_header), buf, len);

        m_sock.send(
                zmq::const_buffer{m_framebuf.data(), ZMQ_FRAME_SIZE(zmq_frame_header)},
                zmq::send_flags::dontwait);
    }
    catch (zmq::error_t& e) {
        fprintf(stderr, "ZeroMQ send error !\n");
        return false;
    }

    return true;
}

EDI::EDI() :
    m_time_last_version_sent(chrono::steady_clock::now()),
    m_clock_tai({})
{ }

EDI::~EDI() { }

void EDI::set_odr_version_tag(const std::string& odr_version_tag)
{
    m_odr_version_tag = odr_version_tag;
}

void EDI::add_udp_destination(const std::string& host, unsigned int port)
{
    auto dest = make_shared<edi::udp_destination_t>();
    dest->dest_addr = host;
    dest->dest_port = port;
    m_edi_conf.destinations.push_back(dest);

    // We cannot carry AF packets over UDP, because they would be too large.
    m_edi_conf.enable_pft = true;
}

void EDI::add_tcp_destination(const std::string& host, unsigned int port)
{
    auto dest = make_shared<edi::tcp_client_t>();
    dest->dest_addr = host;
    if (dest->dest_port != 0 and dest->dest_port != port) {
        throw runtime_error("All EDI UDP outputs must be to the same destination port");
    }
    dest->dest_port = port;
    m_edi_conf.destinations.push_back(dest);

    m_edi_conf.dump = false;
}

void EDI::set_fec(int fec)
{
    m_edi_conf.enable_pft = true;
    m_edi_conf.fec = fec;
}

bool EDI::enabled() const
{
    return not m_edi_conf.destinations.empty();
}

void EDI::set_tist(bool enable, uint32_t delay_ms, const chrono::system_clock::time_point& ts)
{
    m_tist = enable;
    m_delay_ms = delay_ms;

    const auto ts_with_delay = ts + chrono::milliseconds(m_delay_ms);

    const auto ts_s = chrono::time_point_cast<chrono::seconds>(ts_with_delay);
    const auto remainder = ts_with_delay - ts_s;
    if (remainder < chrono::milliseconds(0)) {
        throw logic_error("EDI::set_tist remainder duration negative!");
    }
    const uint32_t remainder_ms = chrono::duration_cast<chrono::milliseconds>(remainder).count();

    m_edi_time = chrono::system_clock::to_time_t(ts_s);
    m_timestamp += remainder_ms << 14; // Shift ms by 14 to Timestamp level 2
}

bool EDI::write_frame(const uint8_t *buf, size_t len)
{
    if (not m_edi_sender) {
        m_edi_sender = make_shared<edi::Sender>(m_edi_conf);
    }

    edi::TagStarPTR edi_tagStarPtr("DSTI");

    m_edi_tagDSTI.stihf = false;
    m_edi_tagDSTI.atstf = m_tist;

    m_edi_tagDSTI.set_edi_time(m_edi_time, m_clock_tai.get_offset());
    m_edi_tagDSTI.tsta = m_timestamp & 0xffffff;

    m_edi_tagDSTI.rfadf = false;
    // DFCT is handled inside the TagDSTI

    edi::TagSSm edi_tagPayload;
    // TODO make edi_tagPayload.stid configurable
    edi_tagPayload.istd_data = buf;
    edi_tagPayload.istd_length = len;

    edi::TagODRAudioLevels edi_tagAudioLevels(m_audio_left, m_audio_right);

    // We always send in 24ms interval
    const size_t num_seconds_sent = m_num_frames_sent * 1000 / 24;
    edi::TagODRVersion edi_tagVersion(m_odr_version_tag, num_seconds_sent);

    // The above Tag Items will be assembled into a TAG Packet
    edi::TagPacket edi_tagpacket(m_edi_conf.tagpacket_alignment);

    // put tags *ptr, DETI and all subchannels into one TagPacket
    edi_tagpacket.tag_items.push_back(&edi_tagStarPtr);
    edi_tagpacket.tag_items.push_back(&m_edi_tagDSTI);
    edi_tagpacket.tag_items.push_back(&edi_tagPayload);
    edi_tagpacket.tag_items.push_back(&edi_tagAudioLevels);

    // Send version information only every 10 seconds to save bandwidth
    if (m_time_last_version_sent + chrono::seconds(10) < chrono::steady_clock::now()) {
        m_time_last_version_sent += chrono::seconds(10);
        edi_tagpacket.tag_items.push_back(&edi_tagVersion);
    }

    m_edi_sender->write(edi_tagpacket);

    m_num_frames_sent++;

    // TODO Handle TCP disconnect
    return true;
}

}
