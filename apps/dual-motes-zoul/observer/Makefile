CONTIKI = ../../..

ifndef TARGET
TARGET=zoul
BOARD = firefly
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif


all: observer-sender observer-sink

CFLAGS += -DNETSTACK_CONF_RADIO=cc1200_driver -DNETSTACK_CONF_MAC=nullmac_driver -DNETSTACK_CONF_RDC=nullrdc_driver

CONTIKI_WITH_RIME = 1
include $(CONTIKI)/Makefile.include
