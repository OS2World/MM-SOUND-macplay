#
#  macplay.dll makefile for gnu MAKE and g++
#

# build environment
#-----------------------
# gcc -v
#
#Reading specs from X:/usr/lib/gcc-lib/i386-pc-os2-emx/3.3.5/specs
#Configured with: G:/GCCOS2/TREE/src/gcc/configure --enable-clh --enable-threads=
#os2 --enable-shared=libgcc,bfd,opcodes --enable-nls --without-included-gettext -
#-with-local-prefix=G:/GCCOS2/TREE/TOOLS/x86.os2/gcc/staged --prefix=/gcc --with-
#gnu-as --disable-libgcj --enable-languages=c,c++
#Thread model: os2
#gcc version 3.3.5 (Innotek Build 2005-07-18 03:46)
#
# make -v
#
#GNU Make version 3.76.1, by Richard Stallman and Roland McGrath.
#Copyright (C) 1988, 89, 90, 91, 92, 93, 94, 95, 96, 97
#        Free Software Foundation, Inc.
#This is free software; see the source for copying conditions.
#There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
#PARTICULAR PURPOSE.
#

TARGET = ../macplay

CC = g++ -c
LD = g++
RM = del

MACLIB = ./maclib


CFLAGS   = -Zmts -O3
INCLUDE  = -I./include -I$(MACLIB)
LIB      =

all: $(TARGET).dll

DEF      = macplay.def
OBJECTS  = macplay.o wav.o
LIBS     = $(MACLIB)/libmac.a

$(TARGET).dll : $(OBJECTS) $(DEF)
	$(LD) -Zdll $(CFLAGS) -o $(TARGET).dll $(OBJECTS) $(LIBS) $(DEF)

clean:
	-@$(RM) ..\*.dll $(OBJECTS) *.bak ..\*.bak 2> nul

macplay.o : macplay.cpp wav.h
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ macplay.cpp

wav.o : wav.cpp wav.h
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ wav.cpp

