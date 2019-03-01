#!/bin/sh
#################################
# Makefile for libmsf_rpc_srv.so
#  by luotang
#  2018.3.18
#  project github:
#################################
ROOTDIR = ../../..
include $(ROOTDIR)/Rules.make

TMP_PATH =lib
TARGET	 =$(LIBPATH)/libmsf_rpc_srv.so

IFLAGS	 +=-I inc -I $(ROOTDIR)/inc -I../../libmsf/msf/inc -I../client/inc 
CFLAGS	 +=-fPIC -Wall -Wextra -W -O2 -pipe -Wswitch-default -Wpointer-arith -Wno-unused -ansi -ftree-vectorize -std=gnu11
DFLAGS	 +=#-D_GNU_SOURCE 
LDFLAGS	 +=-shared 
GFLAG 	 += $(IFLAGS) $(DFLAGS) $(CFLAGS)

SRCS = $(wildcard  src/*.c)
OBJS = $(SRCS:%.c=$(TMP_PATH)/%.o)

.PHONY:clean

all:$(TARGET) make_conf

$(TARGET): $(OBJS)
	@$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)
	@echo $(STRIP) $@
	@$(STRIP)  -x -R .note -R .comment $@
	@cp -f $@ /lib
	@rm -rf $(TMP_PATH)
	@echo "====Makefile GFLAG==="
	@echo $(GFLAG)
	@echo "======================"
$(TMP_PATH)/%.o:%.c
	@mkdir -p $(TMP_PATH)
	@mkdir -p $(dir $@)
	@echo $(CC) $@
	@$(CC) $(GFLAG) -c $< -o $@

make_conf:
	cp -f doc/msf_rpc_srv.conf $(CONFPATH)
	cp -f doc/msf_rpc_srv_svc.json $(CONFPATH)

test:
	@echo $(CC) $(BINPATH)/rpc_agent
	@$(CC) $(IFLAGS) $(DFLAGS) -o $(BINPATH)/rpc_agent doc/agent.c -lm -lpthread -L$(LIBPATH) -lrpc 
	@$(STRIP)  -x -R .note -R .comment $(BINPATH)/rpc_agent

clean :
	rm -rf $(TMP_PATH)
