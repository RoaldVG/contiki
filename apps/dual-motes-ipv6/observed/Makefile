CONTIKI = ../../..

ifndef TARGET
TARGET=zoul
BOARD=firefly
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif

all: observed-receiver observed-sender energest-sink

CFLAGS += -DNETSTACK_CONF_RDC=contikimac_driver -DNETSTACK_CONF_RDC_CHANNEL_CHECK_RATE=4 -DUART_CONF_ENABLE=1 -DUIP_CONF_RPL=0

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
