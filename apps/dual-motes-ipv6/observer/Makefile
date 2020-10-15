CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
CONTIKI = ../../..

ifndef TARGET
TARGET=zoul
BOARD=firefly
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif

all: observer-sender observer-sink

#CFLAGS += -DRF_CHANNEL=20 -DNETSTACK_CONF_MAC=nullmac_driver -DNETSTACK_CONF_RDC=nullrdc_driver -DUART_CONF_ENABLE=1

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
