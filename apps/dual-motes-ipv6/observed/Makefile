CONTIKI = ../..

ifndef TARGET
TARGET=zoul
BOARD=firefly
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif

all: observed-receiver observed-sender

CFLAGS += -DNETSTACK_CONF_RDC=contiki_mac -DNETSTACK_CONF_RDC_CHANNEL_CHECK_RATE=4

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
