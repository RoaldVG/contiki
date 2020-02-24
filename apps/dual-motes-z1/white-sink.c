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

// imported libraries.

#include <stdio.h>
#include "contiki.h"
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"
#include "net/rime/rime.h"
#include "dev/cc2420/cc2420.h"        // To use RSSI and LQI variables
#include "dev/leds.h"


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
#define TMP102_READ_INTERVAL (CLOCK_SECOND)
//static struct etimer et;


// Writes a title on the console

PROCESS(temp_process, "receiving messages from white motes");
AUTOSTART_PROCESSES(&temp_process);

struct whitemsg {
	uint8_t  blackseqno;
	uint16_t whiteseqno;
	uint32_t energy;
	uint16_t counter_ADC;
	uint16_t timestamp_app;
        uint16_t timestamp_mac;
};

// This is the receiver function
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{ 
        rtimer_clock_t rtime = RTIMER_NOW();		//received time (for the latency)
	struct whitemsg msg;
	memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
	//packetbuf_copyto(&msg);
	received ++;
	uint16_t timestamp = packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP);

        

        /*output format:Msg_from|sink_seqno|black_seqno,|white_seqno|ENERGY|time_interval|*/
	printf("%d,%d,%d,%d,%li,%d,%u,%u,%u,%u\n",
					from->u8[0], received,msg.blackseqno,msg.whiteseqno,
                                        msg.energy,msg.counter_ADC, 
                                        msg.timestamp_app,msg.timestamp_mac,timestamp,rtime);
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};

static struct unicast_conn uc;

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

/*	  Initialisation of the useless but needed timer ???   */
          tmp102_init();
/*        adjust power                                         */
          cc2420_set_txpower(power);
/*        Start receiver                                       */
	  unicast_open(&uc, channel, &unicast_callbacks);
	
	PROCESS_END();
}
