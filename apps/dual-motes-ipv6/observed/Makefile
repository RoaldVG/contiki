CONTIKI = ../../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
all: observed-receiver observed-sender energest-sink observed-sender-bc
ifndef TARGET
TARGET=zoul
BOARD=firefly
endif

ifneq ($(TARGET),zoul)
$(error Software is platform specific, only for Zolertia Zoul.)
endif

CONTIKI_WITH_RPL = 0
#CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
