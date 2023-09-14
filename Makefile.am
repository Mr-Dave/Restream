ACLOCAL_AMFLAGS = -I m4

sysconfdir = @sysconfdir@/$(PACKAGE)

AM_CPPFLAGS= -Dsysconfdir=\"$(sysconfdir)/restream\" -DLOCALEDIR=\"$(DESTDIR)$(localedir)\"

LDADD = $(LIBINTL)

LIBS = @LIBS@

bin_PROGRAMS = restream

restream_SOURCES = \
	restream.hpp restream.cpp \
	guide.hpp    guide.cpp  \
	infile.hpp   infile.cpp \
	playlist.hpp playlist.cpp \
	reader.hpp   reader.cpp \
	writer.hpp   writer.cpp

###################################################################
## Create pristine directories to match exactly distributed files
###################################################################
cleanall: distclean
	@rm -rf autom4te.cache m4
	@rm -f config.h.in config.h.in~ aclocal.m4 config.sub ABOUT-NLS missing
	@rm -f compile config.guess config.rpath configure depcomp install-sh
	@rm -f Makefile.in
