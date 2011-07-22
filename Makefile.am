pkgconfigdir = $(libdir)/pkgconfig
nodist_pkgconfig_DATA = libenet2.pc

enetincludedir=$(includedir)/enet2
enetinclude_HEADERS = \
	include/enet2/callbacks.h \
	include/enet2/enet.h \
	include/enet2/list.h \
	include/enet2/protocol.h \
	include/enet2/time.h \
	include/enet2/types.h \
	include/enet2/unix.h \
	include/enet2/utility.h \
	include/enet2/win32.h

lib_LTLIBRARIES = libenet2.la
libenet2_la_SOURCES = callbacks.c compress.c host.c list.c packet.c peer.c protocol.c unix.c win32.c
# see info '(libtool) Updating version info' before making a release
libenet2_la_LDFLAGS = $(AM_LDFLAGS) -version-info 1:0:0
INCLUDES = -Iinclude

ACLOCAL_AMFLAGS = -Im4
