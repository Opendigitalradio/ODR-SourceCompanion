ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

if IS_GIT_REPO
GITVERSION_FLAGS = -DGITVERSION="\"`git describe --dirty`\""
else
GITVERSION_FLAGS =
endif

odr_sourcecompanion_LDADD       = -lzmq
odr_sourcecompanion_CFLAGS      = $(GITVERSION_FLAGS) -ggdb -O2 -Wall
odr_sourcecompanion_CXXFLAGS    = $(GITVERSION_FLAGS) -ggdb -O2 -Wall -Isrc/fec -Ilib
odr_sourcecompanion_SOURCES     = src/odr-sourcecompanion.cpp \
								  src/AACDecoder.h src/AACDecoder.cpp \
								  src/AVTInput.h src/AVTInput.cpp \
								  src/OrderedQueue.h src/OrderedQueue.cpp \
								  src/StatsPublish.h src/StatsPublish.cpp \
								  src/encryption.h src/encryption.c \
								  src/utils.h src/utils.c \
								  lib/fec/char.h \
								  lib/fec/decode_rs_char.c \
								  lib/fec/decode_rs.h \
								  lib/fec/encode_rs_char.c \
								  lib/fec/encode_rs.h \
								  lib/fec/fec.h \
								  lib/fec/init_rs_char.c \
								  lib/fec/init_rs.h \
								  lib/fec/rs-common.h \
								  lib/ClockTAI.h lib/ClockTAI.cpp \
								  lib/Globals.cpp \
								  lib/Log.h lib/Log.cpp \
								  lib/ReedSolomon.h lib/ReedSolomon.cpp \
								  lib/RemoteControl.h lib/RemoteControl.cpp \
								  lib/Socket.h lib/Socket.cpp \
								  lib/ThreadsafeQueue.h \
								  lib/crc.h lib/crc.c \
								  lib/edi/AFPacket.h lib/edi/AFPacket.cpp \
								  lib/edi/EDIConfig.h \
								  lib/edi/Interleaver.h lib/edi/Interleaver.cpp \
								  lib/edi/PFT.h lib/edi/PFT.cpp \
								  lib/edi/TagItems.h lib/edi/TagItems.cpp \
								  lib/edi/TagPacket.h lib/edi/TagPacket.cpp \
								  lib/edi/Transport.h lib/edi/Transport.cpp

bin_PROGRAMS =  odr-sourcecompanion$(EXEEXT)

EXTRA_DIST = $(top_srcdir)/bootstrap \
			 $(top_srcdir)/README.md \
			 $(top_srcdir)/LICENCE \
			 $(top_srcdir)/ChangeLog \
			 $(top_srcdir)/Doxyfile \
			 $(top_srcdir)/lib/fec/LICENSE
			 $(top_srcdir)/lib/fec/README.md

doc: export PROJECT_NUMBER:=$(shell git describe --dirty)

.PHONY: doc
doc:
	doxygen
