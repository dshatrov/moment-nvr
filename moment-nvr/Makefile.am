COMMON_CFLAGS =			\
	-ggdb			\
	-Wno-long-long -Wall    \
	$(THIS_CFLAGS)

if PLATFORM_WIN32
    COMMON_CFLAGS += -DPLATFORM_WIN32
else
    COMMON_CFLAGS += -pthread # -rdynamic
endif

AM_CFLAGS   += -std=gnu99 $(COMMON_CFLAGS)
AM_CXXFLAGS += $(COMMON_CFLAGS)

INCLUDES = -I$(top_srcdir)

moment_nvr_private_headers =    \
        types.h                 \
        naming_scheme.h         \
        nvr_file_iterator.h     \
        nvr_cleaner.h           \
        channel_recorder.h      \
        media_recorder.h        \
        media_reader.h          \
        media_viewer.h          \
        get_file_session.h      \
        moment_nvr_module.h     \
        inc.h

moment_nvr_target_headers =

moment_nvrdir = $(libdir)/moment-1.0
moment_nvr_LTLIBRARIES = libmoment-nvr-1.0.la
libmoment_nvr_1_0_la_SOURCES =	\
        types.cpp               \
        naming_scheme.cpp       \
        nvr_file_iterator.cpp   \
        nvr_cleaner.cpp         \
        channel_recorder.cpp    \
        media_recorder.cpp      \
        media_reader.cpp        \
        media_viewer.cpp        \
        get_file_session.cpp    \
        moment_nvr_module.cpp   \
        mod_nvr.cpp

moment_nvr_extra_dist =

libmoment_nvr_1_0_la_LDFLAGS = -no-undefined -version-info "0:0:0"
libmoment_nvr_1_0_la_LIBADD = $(THIS_LIBS)
if PLATFORM_WIN32
libmoment_nvr_1_0_la_LIBADD += -lws2_32
endif

EXTRA_DIST = $(moment_nvr_private_headers) $(moment_nvr_extra_dist)

