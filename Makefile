CC=gcc
CFLAGS=-c -Wall -g -O2
LDFLAGS=-ljpeg

MANDEL_SOURCES=mandel.c jpegrw.c
MOVIE_SOURCES=mandelmovie.c
MANDEL_OBJECTS=$(MANDEL_SOURCES:.c=.o)
MOVIE_OBJECTS=$(MOVIE_SOURCES:.c=.o)
ALL_OBJECTS=$(MANDEL_OBJECTS) $(MOVIE_OBJECTS)

all: mandel mandelmovie

# pull in dependency info for *existing* .o files
-include $(ALL_OBJECTS:.o=.d)

mandel: $(MANDEL_OBJECTS)
	$(CC) $(MANDEL_OBJECTS) $(LDFLAGS) -o $@

# mandelmovie only forks/execs mandel, so it does not need libjpeg
mandelmovie: $(MOVIE_OBJECTS)
	$(CC) $(MOVIE_OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@
	$(CC) -MM $< > $*.d

# Stitch the frames into a movie. MPEG-1 only allows standard frame
# rates, so frames are read at 10 fps and output at 25 fps.
movie: all
	./mandelmovie -p 4
	ffmpeg -y -framerate 10 -i mandel%d.jpg -r 25 -q:v 2 mandel.mpg

clean:
	rm -rf $(ALL_OBJECTS) mandel mandelmovie *.d

# remove generated frames too (keeps mandel.mpg)
cleanframes:
	rm -f mandel*.jpg

.PHONY: all clean cleanframes movie