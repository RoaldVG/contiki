/**
 * \file
 *          Unicast receiving test program
 *          Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Tested and modified on September 8, 2012. Includes a useless but necessary timer!
 *         Writes with a fixed format in the serialdump to facilitate transfer of data to excel.
 *         Ack power is 0dBm, regardless of the sender settings (???).
 *         Version used for measurements on May 18, 2013 in Poix.
 *         Adapted by Maite Bezunartea to serve in the white (observer) sink of a dual network.
 *         Version used by JT in Poix on April 26, 2015.
 *         Refactored by JT on May 5, 2015
 *         
 *          
 */

/**
 * \file
 *          Unicast receiving test program
 *          Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Roald Van Glabbeek
 * 		     March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly) and IPv6
 */

// imported libraries.

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/rpl/rpl.h"

#include <stdio.h>
#include <inttypes.h>
#include "dev/leds.h"
#include "core/net/netstack.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define UDP_LOCAL_PORT	4567
//#define UDP_REMOTE_PORT	5678

#define UDP_EXAMPLE_ID  190

static struct uip_udp_conn *server_conn;
//static struct simple_udp_connection conn;

PROCESS(white_sink_process, "White sink process");
AUTOSTART_PROCESSES(&white_sink_process);

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

// message counters
static uint16_t received = 0 ;

//variables for channel quality
//int8_t lqi_val ;
//int16_t rssi_val ;

// Writes a title on the console
struct energestmsg {
	uint32_t 	cpu;
	uint32_t 	lpm;
	uint32_t 	transmit;
	uint32_t 	listen;
	uint16_t 	seqno;
	uint32_t	totaltime;
};

// This is the receiver function
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
    rtimer_clock_t rtime = RTIMER_NOW();		//received time (for the latency)
	struct energestmsg *msg;
	//memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
    msg = (struct energestmsg *)uip_appdata;
	received ++;
	uint16_t timestamp = packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP);        

	printf("%x,%" PRIu16 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32"\n\r",
		    UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1], msg->seqno, 
            msg->cpu, msg->lpm, msg->transmit, msg->listen, msg->totaltime);
}
static void
print_local_addresses(void)
{
    int i;
    uint8_t state;

    PRINTF("Server IPv6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
        PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
        PRINTF("\n");
        /* hack to make address "final" */
        if (state == ADDR_TENTATIVE) {
            uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
        }
        }
    }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(white_sink_process, ev, data)
{
    uip_ipaddr_t ipaddr;
    struct uip_ds6_addr *root_if;

    PROCESS_BEGIN();

    PROCESS_PAUSE();

    PRINTF("UDP server started. nbr:%d routes:%d\n",
            NBR_TABLE_CONF_MAX_NEIGHBORS, UIP_CONF_MAX_ROUTES);

#if UIP_CONF_ROUTER
/* The choice of server address determines its 6LoWPAN header compression.
 * Obviously the choice made here must also be selected in udp-client.c.
 *
 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the
 * 6LowPAN protocol preferences,
 * e.g. set Context 0 to fd00::. At present Wireshark copies Context/128 and
 * then overwrites it.
 * (Setting Context 0 to fd00::1111:2222:3333:4444 will report a 16 bit
 * compressed address of fd00::1111:22ff:fe33:xxxx)
 * Note Wireshark's IPCMV6 checksum verification depends on the correct
 * uncompressed addresses.
 */
 
#if 0
/* Mode 1 - 64 bits inline */
   uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 1);
#elif 1
/* Mode 2 - 16 bits inline */
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
#else
/* Mode 3 - derived from link local (MAC) address */
  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
#endif

    uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
    /*root_if = uip_ds6_addr_lookup(&ipaddr);
    if(root_if != NULL) {
        rpl_dag_t *dag;
        dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
        uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
        rpl_set_prefix(dag, &ipaddr, 64);
        PRINTF("created a new RPL dag\n");
    } else {
        PRINTF("failed to create a new RPL DAG\n");
    }
    */
#endif /* UIP_CONF_ROUTER */
  
    print_local_addresses();

    /* The data sink runs with a 100% duty cycle in order to ensure high 
        packet reception rates. */
    NETSTACK_MAC.off(1);
    
    server_conn = udp_new(NULL, 0, NULL);
    if(server_conn == NULL) {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(server_conn, UIP_HTONS(UDP_LOCAL_PORT));

    PRINTF("Created a server connection with remote address ");
    PRINT6ADDR(&server_conn->ripaddr);
    PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
            UIP_HTONS(server_conn->rport));
    
    //simple_udp_register(&conn, UDP_LOCAL_PORT, NULL, UDP_LOCAL_PORT, tcpip_handler);
    while(1) {
        PROCESS_YIELD();
        /*if(ev == tcpip_event) {
        tcpip_handler();
        }*/
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/

