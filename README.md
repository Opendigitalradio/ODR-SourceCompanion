ODR-SourceCompanion
===================

The ODR-SourceCompanion is a tool used to connect audio encoders to
ODR-DabMux through its ZeroMQ interface.

For detailed usage, see the usage screen of the tool with the *-h* option.

More information is available on the
[Opendigitalradio wiki](http://opendigitalradio.org)

How to build
=============

Requirements:

* A C++11 compiler
* Install ZeroMQ 4.0.4 or more recent
  * If your distribution does not include it, take it from
    from http://download.zeromq.org/zeromq-4.0.4.tar.gz

This package:

    ./bootstrap
    ./configure
    make
    sudo make install


How to use
==========

We assume that you have a ODR-DabMux configured for an ZeroMQ
input on port 9000.

    DST="tcp://yourserver:9000"
    BITRATE=64
