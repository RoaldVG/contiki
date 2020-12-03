#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define RF_CHANNEL 20
#define NETSTACK_CONF_MAC nullmac_driver
#define NETSTACK_CONF_RDC nullrdc_driver
/* UART intereferes with parallel comm. Set to 0 on observer-sender, 1 on sink. */
#define UART_CONF_ENABLE 0

//#define TCPIP_CONF_ANNOTATE_TRANSMISSIONS 1
#endif /* PROJECT_CONF_H_ */
