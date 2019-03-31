#!/bin/sh
#################################
# Makefile for libipc.so
#  by luotang
#  2017.8.9
#  project github:
#################################
ROOTDIR = ../../..
include $(ROOTDIR)/Rules.make

TARGET	 =$(LIBPATH)/libipc.so

TMP_PATH =lib

IFLAGS	 +=-I inc -I $(ROOTDIR)/inc -I../../libmsf/msf/inc
CFLAGS	 +=-fPIC -Wall -W -O2 -Wswitch-default -Wpointer-arith -Wno-unused
DFLAGS	 +=-D_GNU_SOURCE -DHAVE_GCC_ATOMICS

LDFLAGS	 +=-shared -lpthread 
GFLAG += $(IFLAGS) $(DFLAGS) $(CFLAGS)

SRCS = $(wildcard  src/*.c)
OBJS = $(SRCS:%.c=$(TMP_PATH)/%.o)

.PHONY:clean 3rdparty

all:$(TARGET) test

$(TARGET): $(OBJS)
	@$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)
	@echo $(STRIP) $@
	@$(STRIP)  -x -R .note -R .comment $@
	@cp -f $@ $(TOOL_LIB_DIR)
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

3rdparty:
	cd 3rdparty && make


test:
	@echo $(CC) $(BINPATH)/msf_upnp
	@$(CC) -o $(BINPATH)/msf_upnp doc/msf_upnp.c -I $(ROOTDIR)/inc -I inc -I../../libmsf/msf/inc -lpthread -ldl -lnuma -L $(LIBPATH) -lmsf -lipc 
	@$(STRIP)  -x -R .note -R .comment $(BINPATH)/msf_upnp
	@echo $(CC) $(BINPATH)/msf_dlna
	@$(CC) -o $(BINPATH)/msf_dlna doc/msf_dlna.c -I $(ROOTDIR)/inc -I inc -I../../libmsf/msf/inc -lpthread -ldl -lnuma -L $(LIBPATH) -lmsf -lipc 
	@$(STRIP)  -x -R .note -R .comment $(BINPATH)/msf_dlna
clean:
	rm -rf $(TARGET) $(TMP_PATH)
	
	



