AM_CFLAGS = -Wall -Wextra -pedantic -std=c99

bin_PROGRAMS = euclid
dist_bin_SCRIPTS = euclid_with_resolution \
					euclid_1024x768 \
					euclid_1280x1024 \
					euclid_1280x720 \
					euclid_1600x1200 \
					euclid_1920x1080 \
					euclid_1920x1200 \
					euclid_640x360 \
					euclid_640x480 \
					euclid_800x600

EXTRA_DIST = euclid.ogg euclid.png

euclid_CFLAGS = -Wall -Wextra -pedantic -std=c99 -pthread
euclid_LDADD = -lm -lGL -lglut -lasound -lvorbisfile
euclid_SOURCES = main.c audio.c audio.h
