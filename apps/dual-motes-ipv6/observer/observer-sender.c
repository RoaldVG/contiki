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
 *         Observer sender program for the dual network.
 *         This program sends a messsage to the white sink whenever it is trigered via GPIO pin A7 by the connected observed mote.
 *
 * \author
 *         Marie-Paule Uwase
 *         August 7, 2012
 *         Roald Van Glabbeek
 *         March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly) and IPv6
 */

#include "contiki.h"
#include "sys/ctimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-udp-packet.h"
#include "sys/ctimer.h"

#include "cpu/cc2538/dev/gpio.h"
#include "dev/zoul-sensors.h"
#include "dev/adc-zoul.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "dev/serial-line.h"
#include "net/ipv6/uip-ds6-route.h"

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define DEBUG DEBUG_FULL
#include "net/ip/uip-debug.h"

#define ADC_READ_INTERVAL (CLOCK_SECOND/128)

// Bit-width of IO communication with observer
#define IO_WIDTH 11

/* 
 * Data structure of sent messages
 */
uint16_t  observer_seqno=0;
uint32_t  ADCResult=0;
uint32_t  counter=0;

struct whitemsg {
    uint16_t    observed_seqno;
    uint16_t    observer_seqno;
    uint32_t    energy;
    uint16_t    counter_ADC;
    uint32_t    timestamp_app;
    uint16_t    timestamp_mac;
};

uint32_t prev_time;

/* sender power
 * possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
 *                    -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3;
 */ 
uint8_t power = 31;

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;  // sink address

static void send_packet(void *ptr);

int ADC_val=0;

/*---------------------------------------------------------------------------*/
PROCESS(observer_sender_process, "Observer sender process");
AUTOSTART_PROCESSES(&observer_sender_process);
/*--------------------------------------------------------------------------------
 * SETTING THE GPIOS
 *-------------------------------------------------------------------------------*/
void msg_callback(uint8_t port, uint8_t pin){
    send_packet(NULL);
}
void
GPIOS_init(void)
{
    GPIO_SET_INPUT(GPIO_A_BASE,GPIO_PIN_MASK(6));        //GPIO PA6
  
    GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(0));        //GPIO PC0
    GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(1));        //GPIO PC1
    GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(2));        //GPIO PC2
    GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(3));        //GPIO PC3
    GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(4));        //GPIO PC4
    GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(5));        //GPIO PC5
    GPIO_SET_INPUT(GPIO_C_BASE,GPIO_PIN_MASK(6));        //GPIO PC6

    GPIO_SET_INPUT(GPIO_D_BASE,GPIO_PIN_MASK(0));        //GPIO PD0
    GPIO_SET_INPUT(GPIO_D_BASE,GPIO_PIN_MASK(1));        //GPIO PD1
    GPIO_SET_INPUT(GPIO_D_BASE,GPIO_PIN_MASK(2));        //GPIO PD2

    // PA7 as interrupt pin on rising and any edge
    GPIO_SOFTWARE_CONTROL(GPIO_A_BASE,GPIO_PIN_MASK(7));
    GPIO_SET_INPUT(GPIO_A_BASE,GPIO_PIN_MASK(7));
    GPIO_DETECT_EDGE(GPIO_A_BASE,GPIO_PIN_MASK(7));
    GPIO_TRIGGER_BOTH_EDGES(GPIO_A_BASE,GPIO_PIN_MASK(7));
    GPIO_ENABLE_INTERRUPT(GPIO_A_BASE,GPIO_PIN_MASK(7));
    // interrupt is handled in msg_callback function
    gpio_register_callback(msg_callback, 0, 7);
}
/*---------------------------------------------------------------------------*/
uint16_t
read_GPIOS(void)
{
    //reading the value in each pin
    uint16_t  observed_seqno=0;

    if (GPIO_READ_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6)))    observed_seqno=observed_seqno+1;        
    if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(0)))    observed_seqno=observed_seqno+2;
    if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(1)))    observed_seqno=observed_seqno+4; 
    if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(2)))    observed_seqno=observed_seqno+8;
    if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(3)))    observed_seqno=observed_seqno+16; 
    if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(4)))    observed_seqno=observed_seqno+32; 
    if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(5)))    observed_seqno=observed_seqno+64;
    if (GPIO_READ_PIN(GPIO_C_BASE,GPIO_PIN_MASK(6)))    observed_seqno=observed_seqno+128; 
    if (GPIO_READ_PIN(GPIO_D_BASE,GPIO_PIN_MASK(0)))    observed_seqno=observed_seqno+256;
    if (GPIO_READ_PIN(GPIO_D_BASE,GPIO_PIN_MASK(1)))    observed_seqno=observed_seqno+512; 
    if (GPIO_READ_PIN(GPIO_D_BASE,GPIO_PIN_MASK(2)))    observed_seqno=observed_seqno+1024; 

    return observed_seqno;
}
/*---------------------------------------------------------------------------*/
static void
send_packet(void *ptr)
{
    observer_seqno++;
    struct whitemsg msg;

    msg.observed_seqno = read_GPIOS();    
    msg.observer_seqno = observer_seqno;    
    msg.energy = ADCResult;
    msg.counter_ADC = counter;
    msg.timestamp_app = RTIMER_NOW() - prev_time;
    msg.timestamp_mac = 0;

    PRINTF("DATA sent to %d\n",
            server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1]);
    uip_udp_packet_sendto(client_conn, &msg, sizeof(msg),
                            &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
    ADCResult=0;
    counter=0;
    prev_time = msg.timestamp_app;
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
static void
set_global_address(void)
{
    uip_ipaddr_t ipaddr;

    // mote address
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0x00ff, 0xfe00, 0xe4b0);//0x9c47);//
    uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);

/* The choice of server address determines its 6LoWPAN header compression.
 * (Our address will be compressed Mode 3 since it is derived from our
 * link-local address)
 * Obviously the choice made here must also be selected in udp-server.c.
 *
 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the
 * 6LowPAN protocol preferences,
 * e.g. set Context 0 to fd00::. At present Wireshark copies Context/128 and
 * then overwrites it.
 * (Setting Context 0 to fd00::1111:2222:3333:4444 will report a 16 bit
 * compressed address of fd00::1111:22ff:fe33:xxxx)
 *
 * Note the IPCMV6 checksum verification depends on the correct uncompressed
 * addresses.
 */
 
#if 0
/* Mode 1 - 64 bits inline */
   uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 1);
#elif 1
/* Mode 2 - 16 bits inline */
    uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0x00ff, 0xfe00, 2);
#else
/* Mode 3 - derived from server link-local (MAC) address */
  uip_ip6addr(&server_ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0x0250, 0xc2ff, 0xfea8, 0xcd1a); //redbee-econotag
#endif
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(observer_sender_process, ev, data)
{
    static struct etimer periodic;
    PROCESS_BEGIN();

    PROCESS_PAUSE();

    set_global_address();

    PRINTF("UDP client process started nbr:%d routes:%d\n",
         NBR_TABLE_CONF_MAX_NEIGHBORS, UIP_CONF_MAX_ROUTES);

    print_local_addresses();

    /* new connection with remote host */
    client_conn = udp_new(NULL, 0, NULL); 
    if(client_conn == NULL) {
        PRINTF("No UDP connection available, exiting the process!\n");
        PROCESS_EXIT();
    }
    udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT)); 

    PRINTF("Created a connection with the server ");
    PRINT6ADDR(&client_conn->ripaddr);
    PRINTF(" local/remote port %u/%u\n",
    UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

    NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, power);

    // init ADC on A5
    adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC2);

    GPIOS_init();
    counter = 0;
    prev_time = 0;

    etimer_set(&periodic, ADC_READ_INTERVAL);
    while(1) {
        PROCESS_WAIT_UNTIL(etimer_expired(&periodic));

        counter++;
        ADC_val = adc_zoul.value(ZOUL_SENSORS_ADC2);
        ADCResult += ADC_val;
        etimer_reset(&periodic);
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
