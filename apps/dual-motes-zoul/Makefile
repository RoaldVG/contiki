CONTIKI = ../..

ifndef TARGET
TARGET=zoul
BOARD = firefly
endif

all: observed-receiver-unicast-etnew observed-receiver observed-sender-energest \
	 observer-sender observer-sink

CONTIKI_WITH_RIME = 1
include $(CONTIKI)/Makefile.include
