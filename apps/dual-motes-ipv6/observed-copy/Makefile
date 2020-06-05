CONTIKI = ../../..

ifndef TARGET
TARGET=zoul
BOARD=firefly
endif

all: observed-receiver observed-sender energest-sink

CFLAGS += -DUIP_CONF_RPL=0

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
