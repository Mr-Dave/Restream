ACLOCAL_AMFLAGS = -I m4

sysconfdir = @sysconfdir@/$(PACKAGE)

AM_CPPFLAGS= -Dsysconfdir=\"$(sysconfdir)/restream\" -DLOCALEDIR=\"$(DESTDIR)$(localedir)\"

LIBS = @LIBS@

bin_PROGRAMS = restream

restream_SOURCES = \
	restream.h restream.c \
	guide.h guide.c  \
	infile.h infile.c \
	playlist.h playlist.c \
	reader.h reader.c \
	writer.h writer.c

###################################################################
## Create pristine directories to match exactly distributed files
###################################################################
cleanall: distclean
	@rm -rf autom4te.cache m4
	@rm -f config.h.in config.h.in~ aclocal.m4 config.sub ABOUT-NLS missing
	@rm -f compile config.guess config.rpath configure depcomp install-sh
	@rm -f Makefile.in
