#
# Makefile for gbus process 
# by luotang 2017.8.11
#

ROOTDIR = ../../../..
include $(ROOTDIR)/Rules.make

BIN		= ../../bin
TARGET	= $(BIN)/bus

SRCS 	= $(wildcard src/*.c)
OBJS	= $(SRCS:%.c=$(BIN)/%.o)

IFLAGS 		= -I./inc -I $(ROOTDIR)/inc -I../libgipc/inc -I../libbase/inc  \
			  -I ../liblog/inc -I../threadpool/inc  -I../../3rdparty/libevent-2.1.8-stable/include 
CFLAGS 		= -Wall -W -O2 -Wswitch-default -Wpointer-arith -Wno-unused -g
DFLAGS 		= -D_GNU_SOURCE -DHAVE_LIBEVENT -DHAVE_OPENSSL
LD_FLAGS 	= -lpthread -lcrypto -levent -levent_pthreads -lz -L ../../lib -llog -lbase

GFLAG 		= $(CFLAGS) $(DFLAGS) $(IFLAGS)

.PHONY: all clean

all : $(TARGET)

$(TARGET): $(OBJS)
	$(LINK) -o $@ $^ $(LD_FLAGS) 
	$(STRIP) $@
	$(STRIP) -x -R .note -R .comment $@
	rm -rf $(BIN)/src/
	@echo "====Makefile GFLAG==="
	@echo $(GFLAG)
	@echo "======================"
$(BIN)/%.o:%.c
	@mkdir -p $(BIN)
	@mkdir -p $(dir $@)
	$(CC) $(GFLAG) -c $^ -o $@

clean :
	rm -rf $(BIN)/*