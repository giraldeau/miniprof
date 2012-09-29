EXTRA_DIST = miniprof.h

bin_PROGRAMS = miniprofdemo

lib_LTLIBRARIES = libminiprof.la
include_HEADERS = miniprof.h
libminiprof_la_SOURCES = miniprof.c
libminiprof_la_LDFLAGS = -version-info 0:0:0 -lrt

miniprofdemo_SOURCES = demo.c
miniprofdemo_CFLAGS = -finstrument-functions
miniprofdemo_LDADD = libminiprof.la
