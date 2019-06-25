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

#include "AVTInput.h"
#include <cstring>
#include <cstdio>
#include <stdint.h>
#include <limits.h>
#include <algorithm>


//#define PRINTF(fmt, A...)   fprintf(stderr, fmt, ##A)
#define PRINTF(x ...)
#define INFO(fmt, A...)   fprintf(stderr, "AVT: " fmt, ##A)
//#define DEBUG(fmt, A...)   fprintf(stderr, "AVT: " fmt, ##A)
#define DEBUG(X...)
#define ERROR(fmt, A...)   fprintf(stderr, "AVT: ERROR " fmt, ##A)

#define MAX_AVT_FRAME_SIZE  (1500)  /* Max AVT MTU = 1472 */

#define MAX_PAD_FRAME_QUEUE_SIZE  (6)

//#define DISTURB_INPUT

// ETSI EN 300 797 V1.2.1 ch 8.2.1.2
uint8_t STI_FSync0[3] = { 0x1F, 0x90, 0xCA };
uint8_t STI_FSync1[3] = { 0xE0, 0x6F, 0x35 };

static void _dump(const uint8_t* buf, int size)
{
    for (int i = 0 ; i < size ; i++) {
        PRINTF("%02X ", buf[i]);
        if( (i+1) % 16 == 0 ) PRINTF("\n");
    }
    if (size % 16 != 0 ) PRINTF("\n");
}

static uint32_t unpack2(const uint8_t* buf)
{
    return (buf[0] << 8) | buf[1];
}

AVTInput::AVTInput(const std::string& input_uri, const std::string& output_uri, uint32_t pad_port, size_t jitterBufferSize)
    :   _input_uri(input_uri),
        _output_uri(output_uri),
        _pad_port(pad_port),
        _jitterBufferSize(jitterBufferSize),

        _output_packet(2048),
        _input_pad_packet(2048),
        _ordered(5000, _jitterBufferSize),
        _lastInfoFrameType(_typeCantExtract)
{

}

int AVTInput::prepare(void)
{
    UdpSocket::init();

    INFO("Open input socket\n");
    int ret = _openSocketSrv(&_input_socket, _input_uri.c_str());

    if (ret == 0 && !_output_uri.empty()) {
        INFO("Open output socket\n");
        ret = _openSocketCli();
    }

    if ( ret == 0 && _pad_port > 0) {
        INFO("Open PAD Port %d\n", _pad_port);
        char uri[50];
        sprintf(uri, "udp://:%d", _pad_port);
        ret = _openSocketSrv(&_input_pad_socket, uri);
        _purgeMessages();
    }

    return ret;
}

int AVTInput::setDabPlusParameters(int bitrate, int channels, int sample_rate, bool sbr, bool ps)
{
    int ret = 0;

    _subChannelIndex = bitrate / 8;
    _bitRate = bitrate * 1000;
    _dab24msFrameSize = bitrate * 3;
    if (_subChannelIndex * 8 != bitrate || _subChannelIndex < 1 || _subChannelIndex > 24) {
        ERROR("Bad bitrate for DAB+ (8..192)");
        return 1;
    }

    if ( sample_rate != 48000 && sample_rate != 32000 ) {
        ERROR("Bad sample rate for DAB+ (32000,48000)");
        return 1;
    }
    _dac = sample_rate == 48000 ? AVT_DAC_48 : AVT_DAC_32;

    if ( channels != 1 && channels != 2 ) {
        ERROR("Bad channel number for DAB+ (1,2)");
        return 1;
    }
    _audioMode =
        channels == 1
            ? (sbr ? AVT_Mono_SBR : AVT_Mono)
            : ( ps ? AVT_Stereo_SBR_PS : sbr ? AVT_Stereo_SBR : AVT_Stereo );

    _ordered = OrderedQueue(5000, _jitterBufferSize);

    _currentFrame.clear();
    _currentFrame.resize(_subChannelIndex*8*5*3);
    _currentFrameSize = 0;
    _nbFrames = 0;

    _sendCtrlMessage();

    return ret;
}

bool AVTInput::_parseURI(const char* uri, std::string& address, long& port)
{
    // Skip the udp:// part if it is present
    if (strncmp(uri, "udp://", 6) == 0) {
        address = uri + 6;
    }
    else {
        address = uri;
    }

    size_t pos = address.find(':');
    if (pos == std::string::npos) {
        fprintf(stderr,
                "\"%s\" is an invalid format for udp address: "
                "should be [udp://][address]:port - > aborting\n", uri);
        return false;
    }

    port = strtol(address.c_str()+pos+1, (char **)NULL, 10);
    if ((port == LONG_MIN) || (port == LONG_MAX)) {
        fprintf(stderr,
                "can't convert port number in udp address %s\n",
                uri);
        return false;
    }

    if ((port <= 0) || (port >= 65536)) {
        fprintf(stderr, "can't use port number %ld in udp address\n", port);
        return false;
    }
    address.resize(pos);

    DEBUG("_parseURI <%s> -> <%s> : %ld\n", uri, address.c_str(), port);

    return true;
}

int AVTInput::_openSocketSrv(UdpSocket* socket, const char* uri)
{
    int returnCode = -1;

    std::string address;
    long port;

    if (_parseURI(uri, address, port)) {
        returnCode = 0;
        if (socket->create(port) == -1) {
            fprintf(stderr, "can't set port %li on Udp input (%s: %s)\n",
                    port, inetErrDesc, inetErrMsg);
            returnCode = -1;
        }

        if (!address.empty()) {
            // joinGroup should accept const char*
            if (socket->joinGroup((char*)address.c_str()) == -1) {
                fprintf(stderr,
                        "can't join multicast group %s (%s: %s)\n",
                        address.c_str(), inetErrDesc, inetErrMsg);
                returnCode = -1;
            }
        }

        if (socket->setBlocking(false) == -1) {
            fprintf(stderr, "can't set Udp input socket in non-blocking mode "
                    "(%s: %s)\n", inetErrDesc, inetErrMsg);
            returnCode = -1;
        }
    }

    return returnCode;
}

/* ------------------------------------------------------------------
 * From ODR-dabMux DabOutputUdp::Open
 */
int AVTInput::_openSocketCli()
{
    std::string address;
    long port;

    if (!_parseURI(_output_uri.c_str(), address, port)) {
        return -1;
    }

    if (_output_packet.getAddress().setAddress(address.c_str()) == -1) {
        fprintf(stderr, "Can't set address %s (%s: %s)\n", address.c_str(),
                inetErrDesc, inetErrMsg);
        return -1;
    }

    _output_packet.getAddress().setPort(port);

    if (_output_socket.create() == -1) {
        fprintf(stderr, "Can't create UDP socket (%s: %s)\n",
                inetErrDesc, inetErrMsg);
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------
 * From ODR-Dabmux dabInputUdp::dabInputUdpRead
 */
ssize_t AVTInput::_read(uint8_t* buf, size_t size, bool onlyOnePacket)
{
    ssize_t nbBytes = 0;

    uint8_t* data = buf;
    UdpPacket _input_packet(2048);

    if (_input_packet.getLength() == 0) {
        _input_socket.receive(_input_packet);
    }

    while (nbBytes < size) {
        unsigned freeSize = size - nbBytes;
        if (_input_packet.getLength() > freeSize) {
            // Not enought place in output
            memcpy(&data[nbBytes], _input_packet.getData(), freeSize);
            nbBytes = size;
            _input_packet.setOffset(_input_packet.getOffset() + freeSize);
        }
        else {
            unsigned length = _input_packet.getLength();
            memcpy(&data[nbBytes], _input_packet.getData(), length);
            nbBytes += length;
            _input_packet.setOffset(0);

            _input_socket.receive(_input_packet);
            if (_input_packet.getLength() == 0 || onlyOnePacket) {
                break;
            }
        }
    }
    bzero(&data[nbBytes], size - nbBytes);

    return nbBytes;
}

/* ------------------------------------------------------------------
 *
 */
bool AVTInput::_isSTI(const uint8_t* buf)
{
    return  (memcmp(buf+1, STI_FSync0, sizeof(STI_FSync0)) == 0) ||
            (memcmp(buf+1, STI_FSync1, sizeof(STI_FSync1)) == 0);
}

/* ------------------------------------------------------------------
 *
 */
const uint8_t* AVTInput::_findDABFrameFromUDP(const uint8_t* buf, size_t size,
                                    int32_t& frameNumber, size_t& dataSize)
{
    const uint8_t* data = NULL;
    uint32_t index = 0;

    bool error = !_isSTI(buf+index);
    bool rtp = false;

    // RTP Header is optionnal, STI is mandatory
    if (error)
    {
        // Assuming RTP header
        if (size-index >= 12) {
            uint32_t version = (buf[index] & 0xC0) >> 6;
            uint32_t payloadType = (buf[index+1] & 0x7F);
            if (version == 2 && payloadType == 34) {
                index += 12; // RTP Header length
                error = !_isSTI(buf+index);
                rtp = true;
            }
        }
    }
    if (!error) {
        index += 4;
        //uint32_t DFS = unpack2(buf+index);
        index += 2;
        //uint32_t CFS = unpack2(buf+index);
        index += 2;

        // FC
        index += 5;
        uint32_t DFCTL = buf[index];
        index += 1;
        uint32_t DFCTH = buf[index] >> 3;
        uint32_t NST   = unpack2(buf+index) & 0x7FF; // 11 bits
        index += 2;

        if (NST >= 1) {
            // Take the first stream even if NST > 1
            uint32_t STL = unpack2(buf+index) & 0x1FFF; // 13 bits
            uint32_t CRCSTF = buf[index+3] & 0x80 >> 7; // 7th bit
            index += NST*4+4;

            data = buf+index;
            dataSize = STL - 2*CRCSTF;
            frameNumber = DFCTH*250 + DFCTL;

            _info(rtp?_typeSTIRTP:_typeSTI, dataSize);
        } else error = true;
    }

    if( error ) ERROR("Nothing detected\n");

    return data;
}


/* ------------------------------------------------------------------
 * Set AAC Encoder Parameter format:
 * Flag             : 1 Byte  : 0xFD
 * Command code     : 1 Byte  : 0x07
 * SubChannelIndex  : 1 Byte  : DataRate / 8000
 * AAC Encoder Mode : 1 Byte  :
 *                       * 0 = Mono
 *                       * 1 = Mono + SBR
 *                       * 2 = Stereo
 *                       * 3 = Stereo + SBR
 *                       * 4 = Stereo + SBR + PS
 * DAC Flag         : 1 Byte  : 0 = 32kHz, 1 = 48kHz
 * Mono mode        : 1 Byte  :
 *                       * 0 = ( Left + Right ) / 2
 *                       * 1 = Left
 *                       * 2 = Right
 */
void AVTInput::_sendCtrlMessage()
{
    if (!_output_uri.empty()) {
        uint8_t data[50];
        uint32_t index = 0;

        data[index++] = 0xFD;
        data[index++] = 0x07;
        data[index++] = _subChannelIndex;
        data[index++] = _audioMode;
        data[index++] = _dac;
        data[index++] = _monoMode;

        _output_packet.setOffset(0);
        _output_packet.setLength(0);
        _output_packet.addData(data, index);
        _output_socket.send(_output_packet);

        INFO("Send control packet to encoder\n");
    }
}

/* ------------------------------------------------------------------
 * PAD Provision Message format:
 * Flag         : 1 Byte  : 0xFD
 * Command code : 1 Byte  : 0x18
 * Size         : 1 Byte  : Size of data (including AD header)
 * AD Header    : 1 Byte  : 0xAD
 *              : 1 Byte  : Size of pad data
 * Pad datas    : X Bytes : In natural order, strating with FPAD bytes
 */
void AVTInput::_sendPADFrame(UdpPacket* packet)
{
    if (packet && _padFrameQueue.size() > 0) {
        std::vector<uint8_t> frame(move(_padFrameQueue.front()));
        _padFrameQueue.pop();

        uint8_t data[500];
        uint32_t index = 0;

        data[index++] = 0xFD;
        data[index++] = 0x18;
        data[index++] = frame.size()+2;
        data[index++] = 0xAD;
        data[index++] = frame.size();
        memcpy( data+index, frame.data(), frame.size());
        index += frame.size();

        packet->setOffset(0);
        packet->setLength(0);
        packet->addData(data, index);

        _input_pad_socket.send(*packet);
    }
}

/* ------------------------------------------------------------------
 * Message format:
 * Flag         : 1 Byte : 0xFD
 * Command code : 1 Byte
 *                  * 0x17 = Request for 1 PAD Frame
 */
void AVTInput::_interpretMessage(const uint8_t* data, size_t size, UdpPacket* packet)
{
    if (size >= 2) {
        if (data[0] == 0xFD) {
            switch (data[1]) {
                case 0x17:
                    _sendPADFrame(packet);
                    break;
            }
        }
    }
}

/* ------------------------------------------------------------------
 *
 */
bool AVTInput::_checkMessage()
{
    bool dataRecevied = false;

    if (_input_pad_packet.getLength() == 0) {
        _input_pad_socket.receive(_input_pad_packet);
    }

    if (_input_pad_packet.getLength() > 0) {
        _interpretMessage((uint8_t*)_input_pad_packet.getData(), _input_pad_packet.getLength(),
                &_input_pad_packet);
        _input_pad_packet.setOffset(0);
        _input_pad_socket.receive(_input_pad_packet);

        dataRecevied = true;
    }

    return dataRecevied;
}

/* ------------------------------------------------------------------
 *
 */
void AVTInput::_purgeMessages()
{
    bool dataRecevied;
    int nb = 0;
    do {
        dataRecevied = false;
        if (_input_pad_packet.getLength() == 0) {
            _input_pad_socket.receive(_input_pad_packet);
        }

        if (_input_pad_packet.getLength() > 0) {
            nb++;
            _input_pad_packet.setOffset(0);
            _input_pad_socket.receive(_input_pad_packet);

            dataRecevied = true;
        }
    } while (dataRecevied);
    if (nb>0) DEBUG("%d messages purged\n", nb);
}


/* ------------------------------------------------------------------
 *
 */
bool AVTInput::_readFrame()
{
    bool dataRecevied = false;

    uint8_t readBuf[MAX_AVT_FRAME_SIZE];
    int32_t frameNumber;
    const uint8_t* dataPtr = NULL;
    size_t dataSize = 0;
    std::vector<uint8_t> data;

    size_t readBytes = _read(readBuf, sizeof(readBuf), true/*onlyOnePacket*/);
    if (readBytes > 0)
    {
        dataRecevied = true;

        if (readBytes > _dab24msFrameSize) {
            // Extract frame data and frame number from buf
            dataPtr = _findDABFrameFromUDP(readBuf, readBytes, frameNumber, dataSize);
        }
//      if (!data) {
//              // Assuming pure RAW data
//              data = buf;
//              dataSize = _dab24msFrameSize;
//              frameNumber = _dummyFrameNumber++;
//      }
        if (!dataPtr) {
            _info(_typeCantExtract, 0);
        }
        if (dataPtr) {
            if (dataSize == _dab24msFrameSize ) {
                if( _frameAligned || frameNumber%5 == 0) {
#if defined(DISTURB_INPUT)
                    // Duplicate a frame
                    if(frameNumber%250==0) _ordered.push(frameNumber, dataPtr, dataSize);

                    // Invert 2 frames (content inverted, audio distrubed by this test))
                    if( frameNumber % 200 == 0) frameNumber += 10;
                    else if( (frameNumber-10) % 200 == 0) frameNumber -= 10;

                    // Remove a frame (audio distrubed, frame missing)
                    if(frameNumber%300 > 5)
#endif
                    _ordered.push(frameNumber, dataPtr, dataSize);
                    _frameAligned = true;
                }
            }
            else ERROR("Wrong frame size from encoder %zu != %zu\n", dataSize, _dab24msFrameSize);
        }
    }

    return dataRecevied;
}

/* ------------------------------------------------------------------
 *
 */
ssize_t AVTInput::getNextFrame(std::vector<uint8_t> &buf)
{
    ssize_t nbBytes = 0;

    //printf("A: _padFrameQueue size=%zu\n", _padFrameQueue.size());

    // Read all messages from encoder (in priority)
    // Read all available frames from input socket
    while (_checkMessage() || _readFrame() );

    //printf("B: _padFrameQueue size=%zu\n", _padFrameQueue.size());

    // Assemble next frame
    int32_t nb = 0;
    std::vector<uint8_t> part;
    while (_nbFrames < 5 && (nb = _ordered.pop(part)) != 0)
    {
        while (_checkMessage());

        memcpy(_currentFrame.data() + _currentFrameSize, part.data(), nb);
        _currentFrameSize += nb;
        _nbFrames ++;
    }

    if (_nbFrames == 5 && _currentFrameSize <= buf.size()) {
        memcpy(&buf[0], _currentFrame.data(), _currentFrameSize);
        nbBytes = _currentFrameSize;
        _currentFrameSize = 0;
        _nbFrames = 0;
    }

    //printf("C: _padFrameQueue size=%zu\n", _padFrameQueue.size());

    return nbBytes;
}

/* ------------------------------------------------------------------
 *
 */
void AVTInput::pushPADFrame(const uint8_t* buf, size_t size)
{
    if (_pad_port == 0) {
        return;
    }

    if (size > 0) {
        std::vector<uint8_t> frame(size);
        std::reverse_copy(buf, buf + size, frame.begin());
        _padFrameQueue.push(frame);
    }
}

/* ------------------------------------------------------------------
 *
 */
bool AVTInput::padQueueFull()
{
    return _padFrameQueue.size() >= MAX_PAD_FRAME_QUEUE_SIZE;
}

/* ------------------------------------------------------------------
 *
 */
void AVTInput::_info(_frameType type, size_t size)
{
    if (_lastInfoFrameType != type || _lastInfoSize != size) {
        switch (type) {
            case _typeSTI:
                INFO("Extracting from UDP/STI frames of size %zu\n", size);
                break;
            case _typeSTIRTP:
                INFO("Extracting from UDP/RTP/STI frames of size %zu\n", size);
                break;
            case _typeCantExtract:
                ERROR("Can't extract data from encoder frame\n");
                break;
        }
        _lastInfoFrameType = type;
        _lastInfoSize = size;
    }
    if (_lastInfoFrameType != _typeCantExtract) {
        _infoNbFrame++;
        if ( (_infoNbFrame == 100) ||
             (_infoNbFrame < 10000 && _infoNbFrame % 1000 == 0) ||
             (_infoNbFrame < 100000 && _infoNbFrame % 10000 == 0) ||
             (_infoNbFrame % 100000 == 0)
           )
        {
            INFO("%zu 24ms-frames received\n", _infoNbFrame);
        }
    }
}
