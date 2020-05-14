CONTIKI = ../../..

ifndef TARGET
TARGET = zoul
BOARD = firefly
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif

MODULES += core/net/mac/lpp

all: observed-receiver-unicast-etnew observed-receiver observed-sender-energest observed-receiver-energest\
	energest-sink observed-bc-sender-energest observed-bc-receiver-energest

CFLAGS += -DRF_CHANNEL=25 -DNETSTACK_CONF_RDC=contikimac_driver -DNETSTACK_CONF_RDC_CHANNEL_CHECK_RATE=16 -DENERGEST_CONF_ON=1 -DUART_CONF_ENABLE=1

CONTIKI_WITH_RIME = 1
include $(CONTIKI)/Makefile.include
