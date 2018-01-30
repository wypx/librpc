#!/bin/sh
#################################
# Makefile for libgbase.so
#  by luotang
#  2017.8.9
#################################
ROOTDIR = ../../../..
include $(ROOTDIR)/Rules.make
CROSS_COMPILE = $(TOOL_PREFIX)
CHAIN_HOST  ?= $(patsubst %-,%,$(TOOL_PREFIX))

BIN		 =../../bin
LIB      =../../lib
TARGET	 =$(LIB)/libbase.so

IFLAGS	 +=-I inc -I $(ROOTDIR)/inc 
CFLAGS	 +=-fPIC -Wall -W -O2 -Wswitch-default -Wpointer-arith -Wno-unused
DFLAGS	 +=-D_GNU_SOURCE -DHAVE_OPENSSL

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
	