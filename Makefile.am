ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

if IS_GIT_REPO
GITVERSION_FLAGS = -DGITVERSION="\"`git describe --dirty`\""
else
GITVERSION_FLAGS =
endif

odr_sourcecompanion_LDADD       = -lzmq
odr_sourcecompanion_CFLAGS      = $(GITVERSION_FLAGS) -ggdb -O2 -Wall
odr_sourcecompanion_CXXFLAGS    = $(GITVERSION_FLAGS) -ggdb -O2 -Wall -Isrc/fec
odr_sourcecompanion_SOURCES     = src/odr-sourcecompanion.cpp \
								  src/AACDecoder.h src/AACDecoder.cpp \
								  src/AVTInput.h  src/AVTInput.cpp \
								  src/InetAddress.h src/InetAddress.cpp \
								  src/OrderedQueue.h src/OrderedQueue.cpp \
								  src/UdpSocket.h src/UdpSocket.cpp \
								  src/crc.h src/crc.c \
								  src/encryption.h src/encryption.c \
								  src/utils.h src/utils.c \
								  src/fec/char.h \
								  src/fec/decode_rs_char.c \
								  src/fec/decode_rs.h \
								  src/fec/encode_rs_char.c \
								  src/fec/encode_rs.h \
								  src/fec/fec.h \
								  src/fec/init_rs_char.c \
								  src/fec/init_rs.h \
								  src/fec/rs-common.h


bin_PROGRAMS =  odr-sourcecompanion$(EXEEXT)

EXTRA_DIST = $(top_srcdir)/bootstrap \
			 $(top_srcdir)/README.md \
			 $(top_srcdir)/LICENCE \
			 $(top_srcdir)/ChangeLog \
			 $(top_srcdir)/Doxyfile \
			 $(top_srcdir)/src/fec/LICENSE
			 $(top_srcdir)/src/fec/README.md

doc: export PROJECT_NUMBER:=$(shell git describe --dirty)

.PHONY: doc
doc:
	doxygen
