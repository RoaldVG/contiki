CONTIKI = ../..

ifndef TARGET
TARGET=zoul
BOARD=firefly
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif

all: observed-receiver observed-sender \
	 observer-sender observer-sink

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
