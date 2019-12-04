bin_PROGRAMS = wmlenovo

wmlenovo_SOURCES =   \
	files.h \
	main.c \
	dockapp.c \
	dockapp.h \
	backlight_off.xpm \
	backlight_on.xpm \
	parts.xpm

AM_CPPFLAGS = @CPPFLAGS@ @DFLAGS@

INCLUDES = @HEADER_SEARCH_PATH@

wmlenovo_LDADD = @LIBRARY_SEARCH_PATH@ @XLIBS@

INSTALL_PROGRAM = \
	@INSTALL_PROGRAM@ \
	@SETGID_FLAGS@ \
	$(AM_INSTALL_PROGRAM_FLAGS)
