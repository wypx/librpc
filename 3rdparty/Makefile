#############################
# Makefile for libevent lib
# by luotang
# 2017.8.5
#############################
ROOTDIR = ../../..
include $(ROOTDIR)/Rules.make

CHAIN_HOST  ?= $(patsubst %-,%,$(TOOL_PREFIX))


DIR_libevent = libevent-2.1.8-stable

INCPATH		= $(ROOTDIR)/inc
LIBPATH		= $(ROOTDIR)/lib

all: make_libevent

make_libevent: #--disable-shared --disable-thread-support
	rm -rf lib/*
	cd $(DIR_libevent)  && chmod +x ./configure && \
	./configure --prefix="$(PWD)/lib" --host=$(CHAIN_HOST) \
				--enable-static --enable-shared \
				--disable-FEATURE --enable-function-sections \
				--disable-dependency-tracking \
				--enable-samples --disable-debug-mode && \
	make && \
	make install
	#$(STRIP) -x -R .note -R .comment ./lib/lib/libevent.a
	cp -rf ./lib/lib/* $(TOOL_LIB_DIR)
	cp -rf ./lib/include/ $(TOOL_INC_DIR)
clean:
	cd $(DIR_libevent)  && make clean
	rm -rf lib/*
