#!/bin/sh
#################################
# Makefile for libdict.so
#  by luotang
#  2017.12.27
#  project github: https://github.com/fmela/libdict
#################################
ROOTDIR = ../../../..
include $(ROOTDIR)/Rules.make
CROSS_COMPILE = $(TOOL_PREFIX)
CHAIN_HOST  ?= $(patsubst %-,%,$(TOOL_PREFIX))

BIN		 =../../bin
LIB      =../../lib
TARGET	 =$(LIB)/libdict.so

IFLAGS	 +=-I inc -I inc/inner -I $(ROOTDIR)/inc 
CFLAGS	 +=-fPIC -Wall -Wextra -W -O2 -pipe -Wswitch-default -Wpointer-arith -Wno-unused  -ansi -pedantic -std=c11
DFLAGS	 +=-D_GNU_SOURCE -DHAVE_OPENSSL -DHAVE_LIBEVENT

LDFLAGS	 +=-shared 
GFLAG 	 += $(IFLAGS) $(DFLAGS) $(CFLAGS)

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
	$(CC) $(GFLAG) -c $< -o $@

clean:
	rm -rf $(TARGET) $(LIB)/src
	