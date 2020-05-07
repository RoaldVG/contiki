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
#include "contiki.h"
//#include "dev/i2cmaster.h"
//#include "dev/tmp102.h"
#include "net/rime/rime.h"
//#include "dev/cc2420/cc2420.h"          // To use RSSI and LQI variables
//#include "sys/energest.h"      // Module to measure ON time of hardware component
#include "dev/leds.h"
#include "core/net/netstack.h"
#include "cpu/cc2538/dev/gpio.h"


/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel_unicast =       134;

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

// message counters
static uint16_t received = 0 ;

/* Timer for delay in black>white communication  */

 static uint8_t flag_white=0;
 static struct etimer et2;

//variables for channel quality
//int8_t lqi_val ;
int16_t rssi_val ;

// Bit-width of IO communication with observer
#define IO_WIDTH 11

void tell_white (void);

// Writes a title on the console

PROCESS(temp_process, "receiving numbered black unicast messages");
AUTOSTART_PROCESSES(&temp_process);

struct testmsg {       
    uint16_t  blackseqno;
    uint16_t  timestamp_app;
    char      padding[44];
    uint16_t  timestamp_mac;
};
struct testmsg msg;

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
	GPIO_CLR_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6));		//GPIO PA6
	//GPIO_CLR_PIN(GPIO_A_BASE,GPIO_PIN_MASK(7));		//GPIO PA7
	
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

// This is the receiver interrupt handler

recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
/* note the time of arrival of the message at application level.
	unsigned long rtime; // received time (long)
	rtime=clock_time();                             // Low resolution real-time clock.
	rtimer_clock_t now = RTIMER_NOW();		// Higher resolution real time clock
*/
	
	memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
	received ++;
	tell_white();
/*
 *    Print received message for control purposes
 */
	NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, rssi_val);
	//rssi_val = ( cc2420_last_rssi ) - 45;
	printf("unicast Rcvd = %5d from %4d, blcksqno = %5d length = % 3d, RSSI= %4d\n",received,from->u8[0], msg.blackseqno, sizeof(msg), rssi_val);

}

void tell_white (void)
{
/*
 *      convert seqno into bits and set the GPIO bits accordingly
 */
	static uint8_t seqno_bits[IO_WIDTH];			
	uint8_t i;
	for (i = 0; i < IO_WIDTH; i++) {
		seqno_bits[i] = msg.blackseqno & (1 << i) ? 1 : 0;
	}		//least significant bit in seqno_bits[0]

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
	     
        flag_white=1;
/*
 *      Clear the GPIO pins.
 */    
        //clear_GPIOS();
}


   // printf(" seqno %u %u %u %u %u %u \n",seqno_bits[0],seqno_bits[1],seqno_bits[2],seqno_bits[3],seqno_bits[4],seqno_bits[5]);
   // printf(" READ_GPIOS %u \n",read_GPIOS());

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;


PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

        printf("Begin of process \n");

	// adjust power
	NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER,power);
 	//cc2420_set_txpower(power);

	GPIOS_init();

	unicast_open(&uc, channel_unicast, &unicast_callbacks);
//        while(1){
//		if(flag_white){
//		  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et2));
//        	  clear_GPIOS();
//		  flag_white=0;
//                }
//        }
	PROCESS_END();
}
