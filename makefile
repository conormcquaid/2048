#
#
# makefile for gcc
#
#

CC=gcc
CFLAGS=-g -Wall -D__USE_FIXED_PROTOTYPES__ 
#-ansi

#
# -g enable debugging
#
# -Wall all warnings
#
# -D__USE_FIXED_PROTOTYPES__ force compiler to use correct headers(?)
#
# -ansi no GNU extensions, just ansi c
#

thing: thing.c
	$(CC) $(CFLAGS) -o thing thing.c

clean:
	rm -f thing
    