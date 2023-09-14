#/*
# *    This file is part of Restream.
# *
# *    Restream is free software: you can redistribute it and/or modify
# *    it under the terms of the GNU General Public License as published by
# *    the Free Software Foundation, either version 3 of the License, or
# *    (at your option) any later version.
# *
# *    Restream is distributed in the hope that it will be useful,
# *    but WITHOUT ANY WARRANTY; without even the implied warranty of
# *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# *    GNU General Public License for more details.
# *
# *    You should have received a copy of the GNU General Public License
# *    along with Restream.  If not, see <https://www.gnu.org/licenses/>.
# *
# */

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src

pkgsysconfdir = $(sysconfdir)/@PACKAGE@

###################################################################
## Create pristine directories to match exactly distributed files
###################################################################
cleanall: distclean
	@rm -rf autom4te.cache m4
	@rm -f config.hpp.in config.hpp.in~ aclocal.m4 config.sub ABOUT-NLS missing
	@rm -f compile config.guess config.rpath configure depcomp install-sh
	@rm -f Makefile.in src/Makefile.in

