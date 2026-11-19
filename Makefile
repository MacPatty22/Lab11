# Makefile for CPE 2600 Lab 12 - Multithreading
# Author: Mac Patterson
#
# make        builds mandel and mandelmovie
# make clean  removes executables, object files, and generated frames

CC=gcc
CFLAGS=-c -g -Wall -Werror -pthread
LDFLAGS=-pthread

all: mandel mandelmovie

# mandel uses pthreads and libjpeg
mandel: mandel.o jpegrw.o
	$(CC) $(LDFLAGS) mandel.o jpegrw.o -o mandel -ljpeg

# mandelmovie only forks and execs mandel, so no extra libraries
mandelmovie: mandelmovie.o
	$(CC) mandelmovie.o -o mandelmovie

mandel.o: mandel.c jpegrw.h
	$(CC) $(CFLAGS) mandel.c -o mandel.o

mandelmovie.o: mandelmovie.c
	$(CC) $(CFLAGS) mandelmovie.c -o mandelmovie.o

jpegrw.o: jpegrw.c jpegrw.h
	$(CC) $(CFLAGS) jpegrw.c -o jpegrw.o

clean:
	rm -f mandel mandelmovie *.o *.jpg
