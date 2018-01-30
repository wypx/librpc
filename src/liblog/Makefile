#!/bin/sh
#################################
# Makefile for libglog.a
#  by luotang
#  2017.1.13
#################################
ROOTDIR = ../../../..
include $(ROOTDIR)/Rules.make
CROSS_COMPILE = $(TOOL_PREFIX)
CHAIN_HOST  ?= $(patsubst %-,%,$(TOOL_PREFIX))

BIN		=../../bin
LIBPATH	=../../lib
TARGET	=$(LIBPATH)/liblog.a

IFLAGS	+=-I inc -I $(ROOTDIR)/inc 
CFLAGS	+=-Wall -W -O2 -Wswitch-default -Wpointer-arith -unused -Wno-unused
DFLAGS	+=-D_GNU_SOURCE

LDFLAGS	+=
CPPFLAGS += $(IFLAGS)  $(DFLAGS) $(CFLAGS) $(LDFLAGS)

SRCS = $(wildcard  ./src/*.c)
OBJS = $(SRCS:%.c=$(LIBPATH)/%.o)

.PHONY:clean

all:$(TARGET)

#ar命令是用来将若干 .o 文件打包成（静态）库文件
#参数r:在库中插入模块(替换).当插入的模块名已经在库中存在,则替换同名的模块.
#参数c:创建一个库。不管库是否存在，都将创建。
#参数s:创建目标文件索引，这在创建较大的库时能加快时间 (补充:如果不需要创建索引,可改成大写S参数;如果.a文件缺少索引,可以使用ranlib命令添加)
#参数v:让 ar命令在执行的时候打印更多的提示信息
$(TARGET):$(OBJS)
		$(AR) cr $@ $(OBJS)
		$(RANLIB) $@
		$(STRIP)  -x -R .note -R .comment $@
		rm -rf $(LIBPATH)/src
		@echo "#############Makefile CFLAGS#############"
		@echo $(CPPFLAGS)
		@echo "#########################################"

$(LIBPATH)/%.o:%.c
		@mkdir -p $(LIBPATH)
		@mkdir -p $(dir $@)
		$(CC) $(CPPFLAGS) -c $^ -o $@

test:
		$(CC) -o lib/logtest doc/logtest.c -I ./inc  -L ../lib -lglog -lpthread -lz
		$(STRIP)  -x -R .note -R .comment lib/logtest
clean:
		rm -rf $(LIBPATH)/*
