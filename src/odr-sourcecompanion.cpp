/* ------------------------------------------------------------------
 * Copyright (C) 2019 Matthias P. Braendli
 * Copyright (C) 2017 AVT GmbH - Fabien Vercasson
 * Copyright (C) 2011 Martin Storsjo
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

/*! \mainpage Introduction
 *  \file odr-sourcecompanion.cpp
 *  \brief The main file for the audio encoder
 */

#include "config.h"
#include "zmq.hpp"

#include "AVTInput.h"
#include "Outputs.h"
#include "AACDecoder.h"
#include "StatsPublish.h"
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
#include "encryption.h"
#include "utils.h"
}

#include <stdexcept>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <string>
#include <getopt.h>
#include <cstdio>
#include <cstdint>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>


using namespace std;

void usage(const char* name) {
    fprintf(stderr,
    "ODR-SourceCompanion %s\n"
    "\nUsage:\n"
    "%s [INPUT SELECTION] [OPTION...]\n",
#if defined(GITVERSION)
    GITVERSION
#else
    PACKAGE_VERSION
#endif
    , name);
    fprintf(stderr,
    "   For the AVT input:\n"
    "   Using the option -I will switch to AVT encoder reception mode:\n"
    "        * The internal encoder is not used any more, all input related options are ignored\n"
    "        * The audio mode and bitrate will be sent to the encoder if option --control-uri\n"
    "          and DAB+ specific options are set (-b -c -r --aaclc --sbr --ps)\n"
    "        * PAD Data can be send to the encoder with the options --pad-port --pad --pad-fifo\n"
    "     -I, --input-uri=URI                      Input URI. (Supported: 'udp://...')\n"
    "         --control-uri=URI                    Output control URI (Supported: 'udp://...')\n"
    "         --timeout=ms                         Maximum frame waiting time, in milliseconds (def=2000)\n"  
    "         --pad-port=port                      Port opened for PAD Frame requests (def=0 not opened)\n"
    "         --jitter-size=nbFrames               Jitter buffer size, in 24ms frames (def=40)\n"
    "   Encoder parameters:\n"
    "     -b, --bitrate={ 8, 16, ..., 192 }    Output bitrate in kbps. Must be a multiple of 8.\n"
    "     -c, --channels={ 1, 2 }              Nb of input channels (default: 2).\n"
    "     -r, --rate={ 32000, 48000 }          Input sample rate (default: 48000).\n"
    "         --aaclc                          Force the usage of AAC-LC (no SBR, no PS)\n"
    "         --sbr                            Force the usage of SBR\n"
    "         --ps                             Force the usage of PS\n"
    "   Output and pad parameters:\n"
    "     -o, --output=URI                     Output ZMQ uri. (e.g. 'tcp://localhost:9000')\n"
    "                                          If more than one ZMQ output is given, the socket\n"
    "                                          will be connected to all listed endpoints.\n"
    "     -e, --edi=URI                        EDI output uri, (e.g. 'tcp://localhost:7000')\n"
    "     -T, --timestamp-delay=DELAY_MS       Enabled timestamps in EDI (requires TAI clock bulletin download) and\n"
    "     -k, --secret-key=FILE                Enable ZMQ encryption with the given secret key.\n"
    "     -p, --pad=BYTES                      Set PAD size in bytes.\n"
    "     -P, --pad-fifo=FILENAME              Set PAD data input fifo name"
    "                                          (default: /tmp/pad.fifo).\n"
    "     -l, --level                          Show peak audio level indication.\n"
    "     -S, --stats=SOCKET_NAME              Connect to the specified UNIX Datagram socket and send statistics.\n"
    "                                          This allows external tools to collect audio and drift compensation stats.\n"
    "\n"
    "Only the tcp:// zeromq transport has been tested until now,\n"
    " but epgm:// and pgm:// are also accepted\n"
    );

}


#define no_argument 0
#define required_argument 1
#define optional_argument 2

int main(int argc, char *argv[])
{
    std::string avt_input_uri = "";
    std::string avt_output_uri = "";
    int32_t avt_timeout = 2000;
    uint32_t avt_pad_port = 0;
    size_t avt_jitterBufferSize = 40;

    std::vector<std::string> output_uris;

    AACDecoder decoder;
    unique_ptr<StatsPublisher> stats_publisher;

    /* For MOT Slideshow and DLS insertion */
    const char* pad_fifo = "/tmp/pad.fifo";
    int pad_fd = -1;
    int padlen = 0;

    /* Whether to show the 'sox'-like measurement */
    bool show_level = false;

    /* If not empty, send stats over UNIX DGRAM socket */
    string send_stats_to = "";

    /* Data for ZMQ CURVE authentication */
    char *keyfile = nullptr;

    const struct option longopts[] = {
        {"bitrate",                required_argument,  0, 'b'},
        {"channels",               required_argument,  0, 'c'},
        {"edi",                    required_argument,  0, 'e'},
        {"timestamp-delay",        required_argument,  0, 'T'},
        {"output",                 required_argument,  0, 'o'},
        {"pad",                    required_argument,  0, 'p'},
        {"pad-fifo",               required_argument,  0, 'P'},
        {"rate",                   required_argument,  0, 'r'},
        {"stats",                  required_argument,  0, 'S'},
        {"secret-key",             required_argument,  0, 'k'},
        {"input-uri",              required_argument,  0, 'I'},
        {"control-uri",            required_argument,  0,  6 },
        {"timeout",                required_argument,  0,  7 },
        {"pad-port",               required_argument,  0,  8 },
        {"jitter-size",            required_argument,  0,  9 },
        {"aaclc",                  no_argument,        0,  0 },
        {"help",                   no_argument,        0, 'h'},
        {"level",                  no_argument,        0, 'l'},
        {"ps",                     no_argument,        0,  2 },
        {"sbr",                    no_argument,        0,  1 },
        {0, 0, 0, 0},
    };

    fprintf(stderr,
            "Welcome to %s %s, compiled at %s, %s",
            PACKAGE_NAME,
#if defined(GITVERSION)
            GITVERSION,
#else
            PACKAGE_VERSION,
#endif
            __DATE__, __TIME__);
    fprintf(stderr, "\n");
    fprintf(stderr, "  http://opendigitalradio.org\n\n");


    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    bool allowSBR = false;
    bool allowPS  = false;

    vector<string> edi_output_uris;
    bool tist_enabled = false;
    uint32_t tist_delay_ms = 0;

    int bitrate = 0;
    int channels = 2;
    int sample_rate = 48000;
    char ch = 0;
    int index;
    while(ch != -1) {
        ch = getopt_long(argc, argv, "hlb:c:e:T:k:o:r:p:P:I:", longopts, &index);
        switch (ch) {
        case 0: // AAC-LC
            allowPS = false;
            allowSBR = false;
            break;
        case 1: // SBR
            allowPS = false;
            allowSBR = true;
            break;
        case 2: // PS
            allowPS = true;
            allowSBR = true;
            break;
        case 'b':
            bitrate = stoi(optarg);
            break;
        case 'c':
            channels = stoi(optarg);
            break;
        case 'e':
            edi_output_uris.push_back(optarg);
            break;
        case 'T':
            tist_enabled = true;
            tist_delay_ms = std::stoi(optarg);
            break;
        case 'k':
            keyfile = optarg;
            break;
        case 'l':
            show_level = true;
            break;
        case 'o':
            output_uris.push_back(optarg);
            break;
        case 'p':
            padlen = stoi(optarg);
            break;
        case 'P':
            pad_fifo = optarg;
            break;
        case 'r':
            sample_rate = stoi(optarg);
            break;
        case 'S':
            send_stats_to = optarg;
            break;
        case 'I':
            avt_input_uri = optarg;
            fprintf(stderr, "AVT Encoder Mode\n");
            break;
        case 6:
            avt_output_uri = optarg;
            break;
        case 7:
            avt_timeout = stoi(optarg);
            if (avt_timeout < 0) {
                avt_timeout = 2000;
            }
            break;
        case 8:
            avt_pad_port = stoi(optarg);
            break;
        case 9:
            avt_jitterBufferSize = stoi(optarg);
            break;
        case '?':
        case 'h':
            usage(argv[0]);
            return 1;
        }
    }

    if (padlen < 0) {
        fprintf(stderr, "Invalid PAD length specified\n");
        return 1;
    }

    if (avt_input_uri.empty()) {
        fprintf(stderr, "No input URI defined\n");
        return 1;
    }

    shared_ptr<Output::ZMQ> zmq_output;
    Output::EDI edi_output;

    if (output_uris.empty() and edi_output_uris.empty()) {
        fprintf(stderr, "No output URIs defined\n");
        return 1;
    }

    for (const auto& uri : output_uris) {
        if (not zmq_output) {
            zmq_output = make_shared<Output::ZMQ>();
        }

        zmq_output->connect(uri.c_str(), keyfile);
    }

    for (const auto& uri : edi_output_uris) {
        if (uri.compare(0, 6, "tcp://") == 0 or
            uri.compare(0, 6, "udp://") == 0) {
            auto host_port_sep_ix = uri.find(':', 6);
            if (host_port_sep_ix != string::npos) {
                auto host = uri.substr(6, host_port_sep_ix - 6);
                auto port = std::stoi(uri.substr(host_port_sep_ix + 1));

                auto proto = uri.substr(0, 3);
                if (proto == "tcp") {
                    edi_output.add_tcp_destination(host, port);
                }
                else if (proto == "udp") {
                    edi_output.add_udp_destination(host, port);
                }
                else {
                    throw logic_error("unhandled proto");
                }
            }
            else {
                fprintf(stderr, "Invalid EDI URL host!\n");
            }
        }
        else {
            fprintf(stderr, "Invalid EDI protocol!\n");
        }
    }

    if (not edi_output_uris.empty()) {
        edi_output.set_tist(tist_enabled, tist_delay_ms);
    }

    if (padlen != 0) {
        int flags;
        if (mkfifo(pad_fifo, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH) != 0) {
            if (errno != EEXIST) {
                fprintf(stderr, "Can't create pad file: %d!\n", errno);
                return 1;
            }
        }
        pad_fd = open(pad_fifo, O_RDONLY | O_NONBLOCK);
        if (pad_fd == -1) {
            fprintf(stderr, "Can't open pad file!\n");
            return 1;
        }
        flags = fcntl(pad_fd, F_GETFL, 0);
        if (fcntl(pad_fd, F_SETFL, flags | O_NONBLOCK)) {
            fprintf(stderr, "Can't set non-blocking mode in pad file!\n");
            return 1;
        }
    }

    AVTInput avtinput(avt_input_uri, avt_output_uri, avt_pad_port, avt_jitterBufferSize);

    if (avt_input_uri != "") {
        if (avtinput.prepare() != 0) {
            fprintf(stderr, "Fail to connect to AVT encoder in:'%s' out:'%s'\n", avt_input_uri.c_str(), avt_output_uri.c_str());
            return 1;
        }

        // Audio parameters
        if (avtinput.setDabPlusParameters(bitrate, channels, sample_rate, allowSBR, allowPS) != 0) {
            fprintf(stderr, "Wrong audio parameters for AVT encoder\n");
            return 1;
        }
    }
    else {
        fprintf(stderr, "No input defined\n");
        return 1;
    }

    if (not send_stats_to.empty()) {
        StatsPublisher *s = nullptr;
        try {
            s = new StatsPublisher(send_stats_to);
            stats_publisher.reset(s);
        }
        catch (const runtime_error& e) {
            fprintf(stderr, "Failed to initialise Stats Publisher: %s", e.what());
            if (s != nullptr) {
                delete s;
            }
            return 1;
        }
    }

    int outbuf_size;
    std::vector<uint8_t> outbuf;

    outbuf_size = bitrate/8*120;
    outbuf.resize(24*120);

    if (outbuf_size % 5 != 0) {
        fprintf(stderr, "Warning: (outbuf_size mod 5) = %d\n", outbuf_size % 5);
    }

    unsigned char pad_buf[padlen + 1];

    fprintf(stderr, "Starting encoding\n");

    int retval = 0;
    int send_error_count = 0;

    int peak_left = 0;
    int peak_right = 0;

    ssize_t read_bytes = 0;
    do {
        size_t numOutBytes = 0;
        read_bytes = 0;

        // -------------- Read Data
        memset(&outbuf[0], 0x00, outbuf_size);

        const auto timeout_start = std::chrono::steady_clock::now();
        const auto timeout_duration = std::chrono::milliseconds(avt_timeout);
        bool timedout = false;

        while (!timedout and numOutBytes == 0) {
            // Fill the PAD Frame queue because multiple PAD frame requests
            // can come for each DAB+ Frames (up to 6),
            if (padlen != 0 and pad_fd != -1) {
                bool no_data = false;
                while (!no_data and !avtinput.padQueueFull()) {
                    const ssize_t pad_ret = read(pad_fd, pad_buf, padlen + 1);

                    if ((pad_ret < 0 and errno == EAGAIN) or pad_ret == 0) {
                        no_data = true;
                    }
                    else if (pad_ret == padlen + 1) {
                        const int calculated_padlen = pad_buf[padlen];
                        avtinput.pushPADFrame(pad_buf + (padlen - calculated_padlen), calculated_padlen);
                    }
                    else {
                        // Some other error occurred during read.
                        fprintf(stderr, "Unable to read from PAD!\n");
                        break;
                    }
                }
            }

            numOutBytes = avtinput.getNextFrame(outbuf);
            if (numOutBytes == 0) {
                const auto curTime = std::chrono::steady_clock::now();
                const auto diff = curTime - timeout_start;
                if (diff > timeout_duration) {
                    fprintf(stderr, "timeout reached\n");
                    timedout = true;
                } else {
                    const int wait_ms = 1;
                    usleep(wait_ms * 1000);
                }
            }
        }

        if (numOutBytes != 0) {
            try {
                if (numOutBytes % 120 != 0) {
                    throw runtime_error("Invalid data length " + to_string(numOutBytes));
                }

                // Drop the Reed-Solomon data
                decoder.decode_frame(outbuf.data(), numOutBytes / 120 * 110);

                auto p = decoder.get_peaks();
                peak_left = p.peak_left;
                peak_right = p.peak_right;
            }
            catch (const runtime_error &e) {
                fprintf(stderr, "AAC decoding failed with: %s\n", e.what());
                peak_left = 0;
                peak_right = 0;
            }

            if (stats_publisher) {
                stats_publisher->update_audio_levels(peak_left, peak_right);
            }
        }

        read_bytes = numOutBytes;

        if (numOutBytes != 0) {
            bool success = false;
            if (zmq_output) {
                zmq_output->update_audio_levels(peak_left, peak_right);
                success = zmq_output->write_frame(outbuf.data(), outbuf.size());
            }
            else if (edi_output.enabled()) {
                edi_output.update_audio_levels(peak_left, peak_right);
                success = edi_output.write_frame(outbuf.data(), outbuf.size());
            }

            if (not success) {
                send_error_count++;
            }

            if (send_error_count > 10) {
                fprintf(stderr, "Send failed ten times, aborting!\n");
                retval = 4;
                break;
            }
        }

        if (numOutBytes != 0) {
            if (show_level) {
                if (channels == 1) {
                    fprintf(stderr, "\rIn: [%-6s]",
                            level(1, MAX(peak_right, peak_left)));
                }
                else if (channels == 2) {
                    fprintf(stderr, "\rIn: [%6s|%-6s]",
                            level(0, peak_left),
                            level(1, peak_right));
                }
            }

            peak_right = 0;
            peak_left = 0;

            if (stats_publisher) {
                stats_publisher->send_stats();
            }
        }
    } while (read_bytes > 0);

    fprintf(stderr, "\n");

    return retval;
}
