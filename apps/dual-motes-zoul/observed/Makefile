CONTIKI = ../../..

ifndef TARGET
TARGET = zoul
BOARD = firefly
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif


all: observed-receiver-unicast-etnew observed-receiver observed-sender-energest

CFLAGS += -DNETSTACK_CONF_RDC=contikimac_driver -DNETSTACK_CONF_RDC_CHANNEL_CHECK_RATE=4

CONTIKI_WITH_RIME = 1
include $(CONTIKI)/Makefile.include
