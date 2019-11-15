ODR-SourceCompanion
===================

The ODR-SourceCompanion is a tool used to connect audio encoders to
ODR-DabMux through its EDI and ZeroMQ interfaces.

For detailed usage, see the usage screen of the tool with the *-h* option.

More information is available on the
[Opendigitalradio wiki](http://opendigitalradio.org)

How to build
=============

Requirements:

* A C++11 compiler
* ZeroMQ 4.0.4 or more recent
* fdk-aac

This package:

    ./bootstrap
    ./configure
    make
    sudo make install


How to use
==========

We assume that you have a ODR-DabMux configured for an EDI
input on port 9000.

Also, assuming you have an AVT encoder on the IP address 192.168.128.111 and a fifo for ODR-PadEnc

    odr-sourcecompanion -c 2 -b 80 -r 48000 --sbr -p 32 -P fifo.pad --pad-port=9405 \
    --input-uri=udp://:32010 --control-uri=udp://192.168.128.111:9325 --jitter-size=80 \
    -e tcp://yourserver:9000

