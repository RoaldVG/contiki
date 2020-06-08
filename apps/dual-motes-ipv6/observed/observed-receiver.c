/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Unicast receiving test program
 *         Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Roald Van Glabbeek
 *              March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly) and IPv6
 */

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/rpl/rpl.h"
#include "project-conf.h"

#include "net/netstack.h"
#include <stdio.h>
#include <string.h>

#include "dev/gpio.h"

#define DEBUG DEBUG_FULL
#include "net/ip/uip-debug.h"

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define UDP_LOCAL_PORT 5678
#define UDP_ENERGEST_PORT 4567

static uip_ipaddr_t energest_ipaddr;

// Bit-width of IO communication with observer
#define IO_WIDTH 11

/* Data structure of messages sent from sender
 *
 */
struct testmsg {       
    uint16_t  blackseqno;
    uint16_t  timestamp_app;
    char      padding[44];
    int16_t   timestamp_mac;        
};

struct energestmsg {
    uint32_t     cpu;
    uint32_t     lpm;
    uint32_t     transmit;
    uint32_t     listen;
    uint16_t     seqno;
    uint32_t    totaltime;
};

struct energestmsg prev_energest_vals;

static struct uip_udp_conn *server_conn;
static struct uip_udp_conn *energest_conn;
static uip_ipaddr_t energest_ipaddr;

int received = 0;

/*---------------------------------------------------------------------------*/
PROCESS(observed_receiver_process, "observed receiver process");
AUTOSTART_PROCESSES(&observed_receiver_process);
/*---------------------------------------------------------------------------*/
void
GPIOS_init(void)
{    
    GPIO_SET_OUTPUT(GPIO_A_BASE,GPIO_PIN_MASK(6));        //GPIO PA6
    GPIO_SET_OUTPUT(GPIO_A_BASE,GPIO_PIN_MASK(7));        //GPIO PA7
  
     GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(0));        //GPIO PC0
    GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(1));        //GPIO PC1
    GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(2));        //GPIO PC2
    GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(3));        //GPIO PC3
    GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(4));        //GPIO PC4
    GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(5));        //GPIO PC5
    GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(6));        //GPIO PC6

    GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(0));        //GPIO PD0
    GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(1));        //GPIO PD1
    GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(2));        //GPIO PD2
    
}
/*---------------------------------------------------------------------------*/
void
clear_GPIOS(void)
{    
    GPIO_CLR_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6));        //GPIO PA6
    
     GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(0));        //GPIO PC0
    GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(1));        //GPIO PC1
    GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(2));        //GPIO PC2
    GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(3));        //GPIO PC3
    GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(4));        //GPIO PC4
    GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(5));        //GPIO PC5
    GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(6));        //GPIO PC6

    GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(0));        //GPIO PD0
    GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(1));        //GPIO PD1
    GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(2));        //GPIO PD2
    
}
/*---------------------------------------------------------------------------*/
static void
send_energest()
{
    struct energestmsg energest_values;

    PRINTF("Sending to energest sink %x\n",energest_ipaddr.u8[sizeof(energest_ipaddr.u8) - 1]);

    energest_values.totaltime = RTIMER_NOW() - prev_energest_vals.totaltime;

    energest_flush();
    energest_values.cpu = energest_type_time(ENERGEST_TYPE_CPU) - prev_energest_vals.cpu;
    energest_values.lpm = energest_type_time(ENERGEST_TYPE_LPM) - prev_energest_vals.lpm;
    energest_values.transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - prev_energest_vals.transmit;
    energest_values.listen = energest_type_time(ENERGEST_TYPE_LISTEN) - prev_energest_vals.listen;
    energest_values.seqno = received;
    
    uip_udp_packet_sendto(energest_conn, &energest_values, sizeof(energest_values),
                        &energest_ipaddr, UIP_HTONS(UDP_ENERGEST_PORT));

    energest_flush();
    prev_energest_vals.cpu = energest_type_time(ENERGEST_TYPE_CPU);
    prev_energest_vals.lpm = energest_type_time(ENERGEST_TYPE_LPM);
    prev_energest_vals.transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
    prev_energest_vals.listen = energest_type_time(ENERGEST_TYPE_LISTEN);
    prev_energest_vals.totaltime = RTIMER_NOW();
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler()
{
    struct testmsg msg;
    received++;
    if(uip_newdata()) {
        memcpy(&msg, uip_appdata, sizeof(msg));
        PRINTF("DATA recv from %d\n", UIP_IP_BUF->srcipaddr.u8[sizeof(UIP_IP_BUF->srcipaddr.u8) - 1]);
    }

    static uint8_t seqno_bits[IO_WIDTH];            
    uint8_t i;
    for (i = 0; i < IO_WIDTH; i++) {
        seqno_bits[i] = msg.blackseqno & (1 << i) ? 1 : 0;
    }        //least significant bit in seqno_bits[0]

    clear_GPIOS();
    
    if ( seqno_bits[0]==1 )        GPIO_SET_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6));       //  write a 1 in A6
    if ( seqno_bits[1]==1 )        GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(0));       //  write a 1 in C0
    if ( seqno_bits[2]==1 )        GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(1));       //  write a 1 in C1
    if ( seqno_bits[3]==1 )        GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(2));       //  write a 1 in C2
    if ( seqno_bits[4]==1 )        GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(3));       //  write a 1 in C3
    if ( seqno_bits[5]==1 )        GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(4));       //  write a 1 in C4
    if ( seqno_bits[6]==1 )        GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(5));       //  write a 1 in C5
    if ( seqno_bits[7]==1 )        GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(6));       //  write a 1 in C6
    if ( seqno_bits[8]==1 )        GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(0));       //  write a 1 in D0
    if ( seqno_bits[9]==1 )        GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(1));       //  write a 1 in D1
    if ( seqno_bits[10]==1 )    GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(2));       //  write a 1 in D2
    
    if (GPIO_READ_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7)) == 0)
        GPIO_SET_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7));
    else
        GPIO_CLR_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7));

}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
    int i;
    uint8_t state;

    PRINTF("Client IPv6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
        (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
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
PROCESS_THREAD(observed_receiver_process, ev, data)
{
    uip_ipaddr_t ipaddr;

    PROCESS_BEGIN();

    PROCESS_PAUSE();

    PRINTF("UDP server started. nbr:%d routes:%d\n",
         NBR_TABLE_CONF_MAX_NEIGHBORS, UIP_CONF_MAX_ROUTES);

    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0x00ff, 0xfe00, 2);
    uip_ip6addr(&energest_ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0x00ff, 0xfe00, 1);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);

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
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 2);
#else
/* Mode 3 - derived from link local (MAC) address */
  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
#endif

    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
#if UIP_CONF_IPV6_RPL
    struct uip_ds6_addr *root_if;
    root_if = uip_ds6_addr_lookup(&ipaddr);
    if(root_if != NULL) {
        rpl_dag_t *dag;
        dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)&ipaddr);
        uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
        rpl_set_prefix(dag, &ipaddr, 64);
        PRINTF("created a new RPL dag\n");
    } else {
        PRINTF("failed to create a new RPL DAG\n");
    }
#endif /* UIP_CONF_IPV6_RPL */
#endif /* UIP_CONF_ROUTER */
  
    print_local_addresses();

    GPIOS_init();
    
    server_conn = udp_new(NULL, 0, NULL);
    if(server_conn == NULL) {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(server_conn, UIP_HTONS(UDP_LOCAL_PORT));

    PRINTF("Created main server connection with remote address ");
    PRINT6ADDR(&server_conn->ripaddr);
    PRINTF(" local/remote port %u/%u\n", UIP_HTONS(server_conn->lport),
            UIP_HTONS(server_conn->rport));
    
    energest_conn = udp_new(NULL, 0, NULL);
    if(energest_conn == NULL) {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(energest_conn, UIP_HTONS(UDP_LOCAL_PORT));

    PRINTF("Created energest server connection with remote address ");
    PRINT6ADDR(&energest_conn->ripaddr);
    PRINTF(" local/remote port %u/%u\n", UIP_HTONS(energest_conn->lport),
            UIP_HTONS(energest_conn->rport));

    prev_energest_vals.cpu = 0;
    prev_energest_vals.lpm = 0;
    prev_energest_vals.transmit = 0;
    prev_energest_vals.listen = 0;
    prev_energest_vals.seqno = 0;
    prev_energest_vals.totaltime = 0;
    printf("IPV6 %d, RPL %d, ND6 %d, ROUTER %d\n",NETSTACK_CONF_WITH_IPV6, UIP_CONF_IPV6_RPL, UIP_CONF_ND6_SEND_NS, UIP_CONF_ROUTER);

    while(1) {
        PROCESS_YIELD();
        
        if(ev == tcpip_event) {
            tcpip_handler();
            if (received%7 == 0)
                send_energest();
        }
        
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
