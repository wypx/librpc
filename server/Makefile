#!/bin/sh
########################################################
# Makefile for libmsf_agent
#  by luotang
#  2018.3.18
#  project github: https://github.com/wypx/librpc/server
########################################################
ROOTDIR = ../../..
include $(ROOTDIR)/Rules.make

TMP_PATH =bin
TARGET	 =$(BINPATH)/msf_agent

IFLAGS	 +=-I inc -I $(ROOTDIR)/inc -I../../libmsf/msf/inc -I../comm
CFLAGS	 +=-fPIC -Wall -Wextra -W -O2 -pipe -Wswitch-default -Wpointer-arith -g -Wno-unused -ansi -ftree-vectorize -std=gnu11
DFLAGS 	 =  
LDFLAGS  = -lpthread -ldl -lnuma -L$(LIBPATH) -lmsf

GFLAG 	 += $(IFLAGS) $(DFLAGS) $(CFLAGS)

SRCS = $(wildcard  src/*.c)
OBJS = $(SRCS:%.c=$(TMP_PATH)/%.o)

.PHONY:clean

all : $(TARGET)

$(TARGET): $(OBJS)
	@echo $(LINK) $@
	@$(LINK) -o $@ $^ $(LDFLAGS) 
	@echo $(STRIP) $@
	@$(STRIP) -x -R .note -R .comment $@
	@rm -rf $(TMP_PATH)
	cp -f doc/msf_agent.conf $(CONFPATH)
	@echo "====Makefile GFLAG==="
	@echo $(GFLAG)
	@echo "======================"
$(TMP_PATH)/%.o:%.c
	@mkdir -p $(BINPATH)
	@mkdir -p $(dir $@)
	@echo $(CC) $@
	@$(CC) $(GFLAG) -c $^ -o $@

clean :
	rm -rf $(TMP_PATH)
