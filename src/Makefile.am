EXTRA_DIST = miniprof.h

bin_PROGRAMS = miniprofdemo

noinst_LTLIBRARIES = libfoo.la
lib_LTLIBRARIES = libminiprof.la
include_HEADERS = miniprof.h
libminiprof_la_SOURCES = miniprof.c
libminiprof_la_LDFLAGS = -version-info 0:0:0
libminiprof_la_LIBADD = -lrt -ldl $(GLIB_LIBS) 
libminiprof_la_CFLAGS = $(GLIB_CFLAGS)

miniprofdemo_SOURCES = demo.c
miniprofdemo_LDADD = libfoo.la

libfoo_la_SOURCES = foo.c foo.h
libfoo_la_CFLAGS = -finstrument-functions
libfoo_la_LIBADD = libminiprof.la
