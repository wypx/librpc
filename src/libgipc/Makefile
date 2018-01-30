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
LIB      =../../lib
TARGET	 =$(LIB)/libgipc.so

IFLAGS	 +=-I inc -I $(ROOTDIR)/inc  -I ../liblog/inc -I../libbase/inc  -I../libdict/inc -I../../3rdparty/libevent-2.1.8-stable/include 
CFLAGS	 +=-fPIC -Wall -W -O2 -Wswitch-default -Wpointer-arith -Wno-unused
DFLAGS	 +=-D_GNU_SOURCE -DGIPC_HAVE_SCHED_YIELD -DGIPC_HAVE_MEMALIGN -DGIPC_HAVE_POSIX_MEMALIGN -DHAVE_LIBEVENT

LDFLAGS	 +=-shared -lpthread 
GFLAG += $(IFLAGS) $(DFLAGS) $(CFLAGS)

SRCS = $(wildcard  src/*.c)
OBJS = $(SRCS:%.c=$(LIB)/%.o)

.PHONY:clean

all:$(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS) -lpthread -lz -L ../../lib -llog
	$(STRIP)  -x -R .note -R .comment $@
	rm -rf $(LIB)/src
	@echo "====Makefile GFLAG==="
	@echo $(GFLAG)
	@echo "======================"
$(LIB)/%.o:%.c
	@mkdir -p $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(GFLAG) -c $< -o $@

test:
	$(CC) -o $(BIN)/gipc_dlna doc/gipc_dlna.c -I $(ROOTDIR)/inc -I inc -I../libbase/inc -I../libgipc/inc   -lcrypto -L ../../lib -lgipc -lbase -ldict -lcrypto -levent -levent_pthreads
	$(STRIP)  -x -R .note -R .comment $(BIN)/gipc_dlna

	$(CC) -o $(BIN)/gipc_upnp doc/gipc_upnp.c -I $(ROOTDIR)/inc -I inc -I../libbase/inc  -I../libgipc/inc  -lcrypto -L ../../lib -lgipc  -lbase -ldict -lcrypto -levent -levent_pthreads
	$(STRIP)  -x -R .note -R .comment $(BIN)/gipc_upnp
clean:
	rm -rf $(TARGET) $(LIB)/src
	
	



