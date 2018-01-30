#!/bin/sh
#################################
# Makefile for libgipc.so
#  by luotang
#  2017.8.9
#################################
ROOTDIR = ../../../..
include $(ROOTDIR)/Rules.make
CROSS_COMPILE = $(TOOL_PREFIX)
CHAIN_HOST  ?= $(patsubst %-,%,$(TOOL_PREFIX))

BIN		 =../../bin
LIB      = ../../lib
TARGET	 =$(LIB)/libgevent.so

IFLAGS	 +=-I inc -I $(ROOTDIR)/inc  
CFLAGS	 +=-fPIC -Wall -W -O2 -Wswitch-default -Wpointer-arith -Wno-unused
DFLAGS	 +=-D_GNU_SOURCE 

LDFLAGS	 +=-shared -lpthread 
CPPFLAGS += $(IFLAGS) $(DFLAGS) $(CFLAGS)

SRCS = $(wildcard  src/*.c)
OBJS = $(SRCS:%.c=$(LIB)/%.o)

.PHONY:clean

all:$(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)
	$(STRIP)  -x -R .note -R .comment $@
	rm -rf $(LIB)/src
	@echo "====Makefile GFLAG==="
	@echo $(GFLAG)
	@echo "======================"
$(LIB)/%.o:%.c
	@mkdir -p $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -c $< -o $@

test:
		$(CC) -o lib/test_libgevent doc/test_libgevent.c -I $(ROOTDIR)/inc -I ./inc -L ./lib -lgevent
		$(STRIP)  -x -R .note -R .comment lib/test_libgevent
clean:
		rm -rf $(LIBPATH)/*
