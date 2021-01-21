#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define ENERGEST_FREQ 10     // every x messages a message is sent to the energest sink
#define IO_WIDTH 11
#define RF_CHANNEL 26

#define NETSTACK_CONF_RDC nullrdc_driver 
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 16 
#define NETSTACK_CONF_MAC csma_driver   
#define ENERGEST_CONF_ON 1

/* UART interferes with the parallel comm. Set to 0 on observed motes, 1 on energest sink */
#define UART_CONF_ENABLE 0

/* code is written to have receiver as router. Set to 1 for receiver, 0 for energest sink and sender */
#define UIP_CONF_ROUTER 1

//#define TCPIP_CONF_ANNOTATE_TRANSMISSIONS 1
#endif /* PROJECT_CONF_H_ */
