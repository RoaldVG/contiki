/**
 * \file
 *         Unicast sending test program
 * \author
 *         Marie-Paule Uwase
 *         August 7, 2012
 *         Roald Van Glabbeek
 * 		   March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly)
 */

// imported libraries.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "contiki.h"
#include "net/rime/rime.h"
#include "sys/energest.h" // Module to measure ON time of hardware component
#include "dev/leds.h"
#include "lib/random.h"
#include "core/net/netstack.h"
#include "cpu/cc2538/dev/gpio.h"

#define DEBUG DEBUG_NULL
#include "net/ip/uip-debug.h"


/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;
// sender mote id
uint8_t sender;          // set automatically

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

// receiver mote id
uint8_t receiver = 211;
linkaddr_t destination;

linkaddr_t energest_rx;
void prepare_energest( void );

// message counters
uint16_t send = 0;

static void recv_uc(struct unicast_conn *c, const linkaddr_t *from);
static void sent_uc(struct unicast_conn *c, int status, int num_tx);
static const struct unicast_callbacks unicast_callbacks = {sent_uc, recv_uc};
static struct unicast_conn uc;

// Bit-width of IO communication with observer
#define IO_WIDTH 11

/* Selection of the time interval between messages */

#define AVERAGE_SEND_INTERVAL CLOCK_SECOND
#define RANDOM 0
#define MIN_SEND_INTERVAL 1
#define INTERVAL_RANGE (AVERAGE_SEND_INTERVAL - MIN_SEND_INTERVAL) * 2 

#if RANDOM
#define SEND_INTERVAL  ((INTERVAL_RANGE + random_rand()) % INTERVAL_RANGE) + MIN_SEND_INTERVAL
#else
#define SEND_INTERVAL  AVERAGE_SEND_INTERVAL
#endif

/* Data structure of messages sent from sender
 *
 */


struct testmsg {       
	uint16_t  blackseqno;
	uint16_t  timestamp_app;
    char      padding[82];
    uint16_t  timestamp_mac;
};

struct energestmsg {
	uint32_t 	cpu;
	uint32_t 	lpm;
	uint32_t 	transmit;
	uint32_t 	listen;
	uint16_t 	seqno;
	uint32_t	totaltime;
};

struct energestmsg prev_energest_vals;

uint16_t seqno=0;

PROCESS(temp_process, "Sending numbered messages");
AUTOSTART_PROCESSES(&temp_process);

/*--------------------------------------------------------------------------------
 * SETTING THE GPIOS
 *-------------------------------------------------------------------------------*/

void
GPIOS_init(void)
{

	GPIO_SET_OUTPUT(GPIO_A_BASE,GPIO_PIN_MASK(6));		//GPIO PA6
	GPIO_SET_OUTPUT(GPIO_A_BASE,GPIO_PIN_MASK(7));		//GPIO PA7
  
 	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(0));		//GPIO PC0
	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(1));		//GPIO PC1
  	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(2));		//GPIO PC2
  	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(3));		//GPIO PC3
	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(4));		//GPIO PC4
	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(5));		//GPIO PC5
  	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(6));		//GPIO PC6

	GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(0));		//GPIO PD0
  	GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(1));		//GPIO PD1
	GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(2));		//GPIO PD2
}

void
clear_GPIOS(void)
{
//clear all output pins

	GPIO_CLR_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6));		//GPIO PA6
	
 	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(0));		//GPIO PC0
	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(1));		//GPIO PC1
  	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(2));		//GPIO PC2
  	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(3));		//GPIO PC3
	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(4));		//GPIO PC4
	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(5));		//GPIO PC5
  	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(6));		//GPIO PC6

	GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(0));		//GPIO PD0
  	GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(1));		//GPIO PD1
	GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(2));		//GPIO PD2
}


/*--------------------------------------------------------------------------------
 * Receiver function which seems necessary to operate the sender with a normal MAC
 *-------------------------------------------------------------------------------*/

static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	//PRINTF("unicast message received from %d.%d\n",
	//		from->u8[0], from->u8[1]);
}

/*---------------------------------------------------------------------------*/
void
prepare_payload( void )
{
	struct testmsg msg;

		//seqno = (seqno < ((2<<(IO_WIDTH+1))-1) ? seqno++ : 0);
	if (seqno < ((2<<(IO_WIDTH))-1))
		seqno++;
	else
		seqno=0;

	/*Set general info*/
	msg.blackseqno=seqno;		
	msg.timestamp_app= clock_time();
	/*
	*      convert seqno into bits and set the GPIO bits accordingly
	*/

	static uint8_t seqno_bits[IO_WIDTH];			
	uint8_t i;
	for (i = 0; i < IO_WIDTH; i++) {
		seqno_bits[i] = msg.blackseqno & (1 << i) ? 1 : 0;
	}		//least significant bit in seqno_bits[0]

	clear_GPIOS();

	if ( seqno_bits[0]==1 )		GPIO_SET_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6));       //  write a 1 in A6
	if ( seqno_bits[1]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(0));       //  write a 1 in C0
	if ( seqno_bits[2]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(1));       //  write a 1 in C1
	if ( seqno_bits[3]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(2));       //  write a 1 in C2
	if ( seqno_bits[4]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(3));       //  write a 1 in C3
	if ( seqno_bits[5]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(4));       //  write a 1 in C4
	if ( seqno_bits[6]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(5));       //  write a 1 in C5
	if ( seqno_bits[7]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(6));       //  write a 1 in C6
	if ( seqno_bits[8]==1 )		GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(0));       //  write a 1 in D0
	if ( seqno_bits[9]==1 )		GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(1));       //  write a 1 in D1
	if ( seqno_bits[10]==1 )	GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(2));       //  write a 1 in D2
	
	if (GPIO_READ_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7)) == 0)
		GPIO_SET_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7));
	else
		GPIO_CLR_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7));

	packetbuf_copyfrom(&msg, sizeof(msg));
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);
}
/*---------------------------------------------------------------------------*/
static void
sent_uc(struct unicast_conn *c, int status, int num_tx)
{
	const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  	if(linkaddr_cmp(dest, &linkaddr_null)) {
    	return;
  	}
}
/*-------------------------------------------------------------------------------
 * Send packets with all needed statistic information
 *-------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------*/
void
prepare_energest( void )
{
	struct energestmsg energest_values;

	PRINTF("Sending to energest sink %x:%x\n",energest_rx.u8[0],energest_rx.u8[1]);

	energest_values.totaltime = RTIMER_NOW() - prev_energest_vals.totaltime;

	energest_flush();
	energest_values.cpu = energest_type_time(ENERGEST_TYPE_CPU) - prev_energest_vals.cpu;
	energest_values.lpm = energest_type_time(ENERGEST_TYPE_LPM) - prev_energest_vals.lpm;
	energest_values.transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - prev_energest_vals.transmit;
	energest_values.listen = energest_type_time(ENERGEST_TYPE_LISTEN) - prev_energest_vals.listen;
	energest_values.seqno = seqno;
	
	
	packetbuf_copyfrom(&energest_values, sizeof(energest_values));
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);

	prev_energest_vals.cpu = energest_values.cpu;
	prev_energest_vals.lpm = energest_values.lpm;
	prev_energest_vals.transmit = energest_values.transmit;
	prev_energest_vals.listen = energest_values.listen;
	prev_energest_vals.totaltime = energest_values.totaltime;
}

static struct etimer et;

// Sending thread

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

	// adjust power
	NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER,power);

	GPIOS_init();
	unicast_open(&uc, channel, &unicast_callbacks);

	destination.u8[0] = 0xe5;
	destination.u8[1] = 0xe4;

	energest_rx.u8[0] = 0x9b;
	energest_rx.u8[1] = 0xf3;

	prev_energest_vals.cpu = 0;
	prev_energest_vals.lpm = 0;
	prev_energest_vals.transmit = 0;
	prev_energest_vals.listen = 0;
	prev_energest_vals.seqno = 0;
	prev_energest_vals.totaltime = 0;


	while(1)
	{
		//    wait for the SEND_INTERVAL time
		etimer_set(&et, SEND_INTERVAL);
  
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		PRINTF("Send interval: %u; Seqno: %u\n",SEND_INTERVAL, seqno);

		if (seqno%100==0){
			prepare_energest();
			unicast_send(&uc,&energest_rx);
		}

		//clear_GPIOS();
		prepare_payload();
		if(!linkaddr_cmp(&destination, &linkaddr_node_addr)) {
      		unicast_send(&uc, &destination);
		}
    }

	PROCESS_END();
}
