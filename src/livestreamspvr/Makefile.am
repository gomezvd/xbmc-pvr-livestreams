LIBS   =
LIBDIR = ../../addons/pvr.livestreams
LIB    = ../../addons/pvr.livestreams/$(ADDONNAME)

lib_LTLIBRARIES = liblivestreamspvraddon.la

liblivestreamspvraddon_la_SOURCES = \
	client.cpp \
	client.h \
	PVRLiveStreamsData.cpp \
	PVRLiveStreamsData.h \
	slre.h \
	slre.cpp
	
liblivestreamspvraddon_la_LDFLAGS = \
	-avoid-version

liblivestreamspvraddon_la_LIBADD = \
	$(ZLIB_LIBS)

AM_CPPFLAGS = \
	$(ARCH_DEFINES) \
	-I. \
	-I$(top_srcdir)/include -I$(top_srcdir)/include/xbmc -I$(top_srcdir)/src \
	-DUSE_DEMUX

EXTRA_DIST = \
	include

$(ADDONNAME): liblivestreamspvraddon.la
	cp -f .libs/liblivestreamspvraddon.$(SOEXT) $(ADDONNAME)

$(LIB): $(ADDONNAME)
	cp -f $(ADDONNAME) $(LIB)

all: $(LIB)

clean-local:
	-rm -f $(LIB) $(ADDONNAME)

install:
