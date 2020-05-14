/**
 * \file
 *          Unicast receiving test program
 *          Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Roald Van Glabbeek
 * 		   March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly)
 */

// imported libraries.

#include <stdio.h>
#include <inttypes.h>
#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include "core/net/netstack.h"
#include "platform/zoul/contiki-conf.h"

#define DEBUG DEBUG_FULL
#include "net/ip/uip-debug.h"


/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 

 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

// message counters
static uint16_t received = 0 ;

//variables for channel quality
//int8_t lqi_val ;
//int16_t rssi_val ;

/* Altough it doesn't have anything to do every 10 seconds this timer is needed in order 
 * for the receiver to work ???
 */
//#define TMP102_READ_INTERVAL (CLOCK_SECOND*10)

// Writes a title on the console

PROCESS(temp_process, "receiving energest messages from observed motes");
AUTOSTART_PROCESSES(&temp_process);

struct energestmsg {
	uint32_t 	cpu;
	uint32_t 	lpm;
	uint32_t 	transmit;
	uint32_t 	listen;
	uint16_t 	seqno;
	uint32_t	totaltime;
};

// This is the receiver function

recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{ 
        rtimer_clock_t rtime = RTIMER_NOW();		//received time (for the latency)
	struct energestmsg msg;
	memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
	received ++;
	uint16_t timestamp = packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP);

        

        /*output format:Msg_from|sink_seqno|black_seqno,|white_seqno|ENERGY|time_interval|*/
	printf("%x:%x,%" PRIu16 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32"\n\r",
		from->u8[0], from->u8[1], msg.seqno, msg.cpu, msg.lpm, msg.transmit, msg.listen, msg.totaltime);

}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};

static struct unicast_conn uc;

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

/*	  Initialisation of the useless but needed timer ???   */
/*        adjust power                                         */
		  NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, power);
/*        Start receiver                                       */
	  unicast_open(&uc, channel, &unicast_callbacks);
	  printf("Channel: %d\n", CC2538_RF_CONF_CHANNEL);
	
	PROCESS_END();
}
