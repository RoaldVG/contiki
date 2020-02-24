CONTIKI = ../..

ifndef TARGET
TARGET=zoul
BOARD = firefly
endif

all: black-receiver black-sender \
	 white-sender white-sink

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
