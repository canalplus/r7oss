/* CommandIR transceivers driver 2.0 CVS $Revision: 5.9 $
 * Supporting CommandIR II and CommandIR Mini (and multiple of both)
 * April-June 2008, Matthew Bodkin
 * Feb-Aug 2010, Added Support for CommandIR III
 */


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/utsname.h>

#include "hardware.h"
#include "ir_remote.h"
#include "lircd.h"
#include "receive.h"
#include "transmit.h"
#include "hw_commandir.h"


struct hardware hw_commandir =
{
	0,					 	/* default device */
	-1,                 		/* fd */
	LIRC_CAN_SET_SEND_CARRIER|
	LIRC_CAN_SEND_PULSE|
	LIRC_CAN_SET_TRANSMITTER_MASK|
	LIRC_CAN_REC_MODE2,
	LIRC_MODE_PULSE,			/* send_mode */
	LIRC_MODE_MODE2,            /* rec_mode */
	sizeof(lirc_t),        		/* code_length in BITS */
	commandir_init,       		/* init_func */
	commandir_deinit,     		/* deinit_func */
	commandir_send,				/* send_func */
 	commandir_rec,        		/* rec_func  */
	commandir_receive_decode,   /* decode_func */
	commandir_ioctl,            /* ioctl_func */
	commandir_readdata,		    /* readdata */
	"commandir"
};

lirc_t lirc_zero_buffer[2] = {0, 0};

struct commandir_tx_order
{
  struct commandir_device * the_commandir_device;
  struct commandir_tx_order * next;
};

struct detected_commandir
{
  unsigned int busnum;
  int	devnum;
  struct detected_commandir *next;
};


static int child_pipe_write = 0;
static char haveInited = 0;

// 'commandir' event signal values
static unsigned int signal_base[2][2] = { {100|PULSE_BIT, 200},
                 {1000|PULSE_BIT, 200} };

// Pipes to and from the child/parent
static int pipe_fd[2] = { -1, -1 };
static pid_t child_pid = -1;
static int pipe_tochild[2] = { -1, -1 };
static int tochild_read = -1, tochild_write = -1;
static int current_transmitter_mask = 0xff;
static char unsigned commandir_data_buffer[512];
static int last_mc_time = -1;
static int commandir_rx_num = 0;


/***   LIRC Interface Functions - Non-blocking parent thread ***/
static int commandir_receive_decode(struct ir_remote *remote,
                   ir_code *prep,ir_code *codep,ir_code *postp,
                   int *repeat_flagp,
                   lirc_t *min_remaining_gapp, lirc_t *max_remaining_gapp) {

	int i, j;
	i = receive_decode(remote,
                   prep,codep,postp,
                   repeat_flagp,
                   min_remaining_gapp, max_remaining_gapp);

	if(i > 0){
		static char rx_char[3] = {3, 0, RXDECODE_HEADER_LIRC};
		j = write(tochild_write, rx_char, 3);
	}
	
	return i;
}


static int commandir_init()
{
	long fd_flags;
	int j;
	if(haveInited){
		static char init_char[3] = {3, 0, INIT_HEADER_LIRC};
		j = write(tochild_write, init_char, 3);
		return 1;
	}
	
	init_rec_buffer();	// LIRC's rec
	init_send_buffer();	// LIRC's send
	
	/* A separate process will be forked to read data from the USB
	 * receiver and write it to a pipe. hw.fd is set to the readable
	 * end of this pipe. */
	if (pipe(pipe_fd) != 0)
	{
		logprintf(LOG_ERR, "couldn't open pipe 1");
		return 0;
	}
	
	hw.fd = pipe_fd[0];	// the READ end of the Pipe
	
	if (pipe(pipe_tochild) != 0)
	{
		logprintf(LOG_ERR, "couldn't open pipe 1");
		return 0;
	}
	
	tochild_read = pipe_tochild[0];	// the READ end of the Pipe
	tochild_write = pipe_tochild[1];	// the WRITE end of the Pipe
	
	fd_flags = fcntl(pipe_tochild[0], F_GETFL);
	if(fcntl(pipe_tochild[0], F_SETFL, fd_flags | O_NONBLOCK) == -1)
	{
		logprintf(LOG_ERR, "can't set pipe to non-blocking");
		return 0;
	}
	
	signal(SIGTERM, shutdown_usb);	// Set early so child can catch this
	child_pid= fork();
	if (child_pid== -1)
	{
		logprintf(LOG_ERR, "couldn't fork child process");
		return 0;
	}
	else if (child_pid== 0)
	{
		child_pipe_write = pipe_fd[1];
		commandir_child_init();
		commandir_read_loop();
		return 0;
	} else {
		signal(SIGTERM, SIG_IGN);
	}
	haveInited = 1;
	
	logprintf(LOG_ERR, "CommandIR driver initialized");
	return 1;
}


static int commandir_deinit(void)
{
	/* Trying something a bit new with this driver. Keeping the driver
	 * connected so in the future we can still monitor in the client */
	int j;
	if(USB_KEEP_WARM && (!strncmp(progname, "lircd", 5)))
	{
		static char deinit_char[3] = {3, 0, DEINIT_HEADER_LIRC};
		j = write(tochild_write, deinit_char, 3);
		logprintf(LOG_ERR, "LIRC_deinit but keeping warm");
	}
	else
	{
		if (tochild_read >= 0)
		{
			if (close(tochild_read) < 0)
			{
				logprintf(LOG_ERR, "Error closing pipe2");;
			}
			tochild_read = tochild_write = -1;
		}
		
		if(haveInited){
			// shutdown all USB
			if(child_pid > 0)
			{
				logprintf(LOG_ERR, "Closing child process");
				kill(child_pid, SIGTERM);
				waitpid(child_pid, NULL, 0);
				child_pid = -1;
				haveInited = 0;
			}
		}
		
		if (hw.fd >= 0)
		{
			if (close(hw.fd) < 0) logprintf(LOG_ERR, "Error closing pipe");
			hw.fd = -1;
		}
		
		logprintf(LOG_ERR, "commandir_deinit()");
	}
	return(1);
}

static int commandir_send(struct ir_remote *remote,struct ir_ncode *code)
{
	int length, j;
	lirc_t *signals;

	if(!init_send(remote,code)) {
		return 0;
	}

	length = send_buffer.wptr;
	signals = send_buffer.data;

	if (length <= 0 || signals == NULL) {
		return 0;
	}
	
	int cmdir_cnt =0;
	char cmdir_char[70];
	
	// Set the frequency of the signal along with the signal + transmitters
	cmdir_char[0] = 7;
	cmdir_char[1] = 0;

	cmdir_char[2] = FREQ_HEADER_LIRC;
	cmdir_char[3] = (remote->freq >> 24) & (0xff);
	cmdir_char[4] = (remote->freq >> 16) & (0xff);
	cmdir_char[5] = (remote->freq >> 8) & (0xff);
	cmdir_char[6] = (remote->freq & 0xff);
	
	j = write(tochild_write, cmdir_char, cmdir_char[0]);
	
	cmdir_cnt = 3;
	
	unsigned char * send_signals = malloc(sizeof(signals) * length + 4);
	
	send_signals[0] = (sizeof(lirc_t) * length + 4) & 0xff;
	send_signals[1] = ((sizeof(lirc_t) * length + 4) >> 8) & 0xff;
	
	send_signals[2] = TX_LIRC_T;
	send_signals[3] = (char)current_transmitter_mask;
	
	memcpy(&send_signals[4], signals, sizeof(lirc_t) * length);
	
	if(write(tochild_write, send_signals,
		send_signals[0] + send_signals[1] * 256) < 0)
	{
		logprintf(LOG_ERR, "Error writing to child_write");
	}
	
	free(send_signals);
	return(length);
}

static char *commandir_rec(struct ir_remote *remotes)
{
	char * returnit;
	if (clear_rec_buffer()==0) 
	{
		return NULL;
	}
	returnit = decode_all(remotes);
	return returnit;
}

static int commandir_ioctl(unsigned int cmd, void *arg)
{
	struct send_tx_mask send_this_mask;
	int j;
	
	switch(cmd)
	{
	case LIRC_SET_TRANSMITTER_MASK:
		/* Support the old way of setting the frequency of the signal along
		 * with the signal + transmitters
		 */
		send_this_mask.numBytes[0] = sizeof(struct send_tx_mask);
		send_this_mask.numBytes[1] = 0;
		send_this_mask.idByte = CHANNEL_EN_MASK;
		send_this_mask.new_tx_mask = *(unsigned int*)arg;
		
		j = write(tochild_write, &send_this_mask, sizeof(send_this_mask));
		return (0);
		
	default:
		logprintf(LOG_ERR, "Unknown ioctl - %d", cmd);
		return(-1);
	}
	
	return 1;
}

static lirc_t commandir_readdata(lirc_t timeout)
{
	lirc_t code = 0;
	if (!waitfordata(timeout/2)) {
		return 0;
	}
	
	/* if we failed to get data return 0 */
	/* Keep trying if we are mode2, but return immediately if we are the others */
	if(strncmp(progname, "mode2", 5)==0 )
	{
	  while(code == 0) {
		if (read(hw.fd, &code, sizeof(lirc_t)) <= 0) {
			commandir_deinit();
			return -1;
		}
	  }
	}
	else
	{
	  if (read(hw.fd, &code, sizeof(lirc_t)) <= 0) {
		  commandir_deinit();
		  return -1;
	  }
	}
	return code;
}

/***  End of parent fork / LIRC accessible functions  */















/*** CommandIR USB Operations and Signal Management ***/
static struct commandir_device * first_commandir_device = NULL;
static struct detected_commandir * detected_commandirs = NULL;
static struct commandir_tx_order * ordered_commandir_devices = NULL;
static struct commandir_device * rx_device = NULL;

int rx_hold = 0;
int shutdown_pending = 0;
int read_delay = WAIT_BETWEEN_READS_US;
int insert_fast_zeros = 0;	// changed from 2, Aug 4/2010

// Interface Functions:

static void commandir_child_init()
{
	unsigned int emitter_set_test[4];
	emitter_set_test[0] = 2; //	0101
	emitter_set_test[1] = 4;
	emitter_set_test[2] = 5; // 1100
	emitter_set_test[3] = 6;
	
	logprintf(LOG_ERR, "Child Initializing CommandIR Hardware");
	
	first_commandir_device = NULL;
	
	alarm(0);
	signal(SIGTERM, shutdown_usb);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, shutdown_usb);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	
	usb_init();
	hardware_scan();
	
	commandir_read_loop();
}

int do_we_know_device(unsigned int bus_num, int devnum) {
  struct commandir_device * pcd;
	
  for(pcd = first_commandir_device; pcd; pcd = pcd->next_commandir_device) {
		if( (pcd->busnum==bus_num) && (pcd->devnum==devnum) ) {
			 return TRUE;
		}
  }
  return FALSE;
}

void commandir_iii_update_status(struct commandir_device * cd)
{
	int receive_status;
	struct commandirIII_status * sptr;
	
	// Ready the first status that will tell us all the above info
  receive_status = usb_bulk_read(
		cd->cmdir_udev,
		1,	// endpoint 1
		(char *)commandir_data_buffer,
		cd->endpoint_max[1],
		1500);
	
	if(receive_status==8)
	{
		// Update the commandir_device with what we just received.
		sptr = (struct commandirIII_status*)commandir_data_buffer;
		cd->num_transmitters = (sptr->tx_status & 0x1F) + 1;
		cd->num_receivers = (sptr->rx_status & 0x60) >> 5;
		cd->tx_jack_sense = sptr->jack_status[3] * 0x01000000 
			+ sptr->jack_status[2] * 0x010000 
			+ sptr->jack_status[1] * 0x0100 
			+ sptr->jack_status[0];
		cd->rx_jack_sense = sptr->rx_status & 0x03;
		cd->commandir_tx_available[0] = (sptr->tx_status & 0x80 ? 1024 : 0);		
		cd->rx_data_available = sptr->rx_status & 0x80;
		cd->hw_revision = sptr->versionByte >> 5;			// 3 high bits
		cd->hw_subversion = sptr->versionByte & 0x1F;	// 5 low bits
		
		// Now we're update to date, see if the pipeline has something to send
		pipeline_check(cd);
	}
}

// Create a new commandir_device, set it up, add it to the linked list
int claim_and_setup_commandir(unsigned int bus_num, 
														int devnum, struct usb_device *dev) 
{
	struct commandir_device * new_commandir;
	int x;

	new_commandir = malloc(sizeof(struct commandir_device));
	new_commandir->busnum = bus_num;
	new_commandir->devnum = devnum;
	new_commandir->next_commandir_device = NULL;
	new_commandir->interface = 0;
	new_commandir->next_tx_signal = NULL;
	new_commandir->last_tx_signal = NULL;
	new_commandir->signalid = 0;
	new_commandir->lastSendSignalID = 0;
	
	new_commandir->cmdir_udev = usb_open(dev);
	
	if(new_commandir->cmdir_udev==NULL)
	{
	 logprintf(LOG_ERR,
		"Error opening commandir - bus %d, device %d.",
		bus_num, devnum);
		free(new_commandir);
		return FALSE;
	}
	
	int r = 0;
	
	r = usb_claim_interface(new_commandir->cmdir_udev, new_commandir->interface);

  if(r < 0)
  {
		free(new_commandir);
		logprintf(LOG_ERR,
			"Unable to claim CommandIR (error %d) - Is it already busy?", r);
		logprintf(LOG_ERR,
			"Try 'rmmod commandir' or check for other lircds");
		return FALSE;
	}
	
	struct usb_config_descriptor *config = &dev->config[0];
	struct usb_interface *interface = &config->interface[0];
	struct usb_interface_descriptor *ainterface = &interface->altsetting[0];
	
	int i;
	
	/*** Hardware Specific Setup ***/
	
	switch(dev->descriptor.iProduct)
	{
		case 3:
		logprintf(LOG_ERR, "Product identified as CommandIR III");
		
		for (i = 0; i < ainterface->bNumEndpoints; i++)
		{
			new_commandir->endpoint_max[
				(ainterface->endpoint[i].bEndpointAddress >= 0x80)
				? (ainterface->endpoint[i].bEndpointAddress-0x80)
				: (ainterface->endpoint[i].bEndpointAddress)]
				= ainterface->endpoint[i].wMaxPacketSize;
		}
		
		new_commandir->hw_type = HW_COMMANDIR_3;
		new_commandir->hw_revision = 0;
		new_commandir->hw_subversion = 0;
		new_commandir->num_transmitters = 0;
		new_commandir->num_receivers = 0;
		new_commandir->num_timers = 0;
		new_commandir->flush_buffer = 5;
		
		commandir_iii_update_status(new_commandir);
		
		break;
		
	case 2:
		logprintf(LOG_INFO, "Product identified as CommandIR II");
		new_commandir->hw_type = HW_COMMANDIR_2;
		new_commandir->hw_revision = 0;
		new_commandir->hw_subversion = 0;
		
		new_commandir->num_transmitters = 4;
		new_commandir->num_receivers = 1;
		new_commandir->num_timers = 4;
		// tx timer to emitter map is FIXED on CommandIR II and Mini
		// timer 0 used by emitter 1 (firmware assigned) ...
		new_commandir->tx_timer_to_channel_map[0] = 1;
		new_commandir->tx_timer_to_channel_map[1] = 2;
		new_commandir->tx_timer_to_channel_map[2] = 3;
		new_commandir->tx_timer_to_channel_map[3] = 4;
		
		for (i = 0; i < ainterface->bNumEndpoints; i++)
		{
			new_commandir->endpoint_max[
				(ainterface->endpoint[i].bEndpointAddress >= 0x80)
				? (ainterface->endpoint[i].bEndpointAddress-0x80)
				: (ainterface->endpoint[i].bEndpointAddress)]
				= 64;
		}
		
		int send_status = 0, tries=20;
		static char get_version[] = {2, GET_VERSION};
		send_status = 4;	// just to start the while()
		
		while(tries--){
			usleep(USB_TIMEOUT_US);	// wait a moment
			
			// try moving this below:
			send_status = usb_bulk_write(
			new_commandir->cmdir_udev,
				2, // endpoint2
				get_version,
				2,
				1500);
			if(send_status < 0)
			{
				logprintf(LOG_ERR,
					"Unable to write version request - Is CommandIR busy? Error %d", 
					send_status);
				break;
			}
			
			send_status = usb_bulk_read(
				new_commandir->cmdir_udev,
				1,
				(char *)commandir_data_buffer,
				new_commandir->endpoint_max[1],
				1500);
			
			if(send_status < 0)
			{
				logprintf(LOG_ERR,
					"Unable to read version request - Is CommandIR busy? Error %d", 
						send_status);
				usb_release_interface(new_commandir->cmdir_udev,
					new_commandir->interface);
				usb_close(new_commandir->cmdir_udev);
				free(new_commandir);
				return FALSE;
			}
			if(send_status==3)
			{
				if(commandir_data_buffer[0]==GET_VERSION)
				{
					// Sending back version information.
					new_commandir->hw_revision =
					 commandir_data_buffer[1];
					new_commandir->hw_subversion =
					 commandir_data_buffer[2];
					logprintf(LOG_ERR, "Hardware revision is %d.%d.",
					 commandir_data_buffer[1], commandir_data_buffer[2]);
					break;
				}
				else
				{
					continue;
				}
			}

		}
		break;
		
	default:
		logprintf(LOG_ERR, "Product identified as Original CommandIR Mini");
		new_commandir->hw_type = HW_COMMANDIR_MINI;
		new_commandir->num_transmitters = 4;
		new_commandir->num_receivers = 1;
		new_commandir->num_timers = 4;
		new_commandir->hw_revision = 2;	// never set by the Mini

		for (i = 0; i < ainterface->bNumEndpoints; i++)
		{
			new_commandir->endpoint_max[
				(ainterface->endpoint[i].bEndpointAddress >= 0x80)
				? (ainterface->endpoint[i].bEndpointAddress-0x80)
				: (ainterface->endpoint[i].bEndpointAddress)]
				= 64;
		}
		
		// tx timer to emitter map is FIXED on CommandIR II and Mini
		new_commandir->tx_timer_to_channel_map[0] = 1;
		new_commandir->tx_timer_to_channel_map[1] = 2;
		new_commandir->tx_timer_to_channel_map[2] = 3;
		new_commandir->tx_timer_to_channel_map[3] = 4;
	}

	if(new_commandir->hw_type ==	HW_COMMANDIR_UNKNOWN)
	{
		logprintf(LOG_ERR, "Product UNKNOWN - cleanup");
		usb_release_interface(new_commandir->cmdir_udev,
			new_commandir->interface);
		usb_close(new_commandir->cmdir_udev);
		free(new_commandir);
		return FALSE;
	}
	else
	{
		new_commandir->lastSendSignalID = 0;
		new_commandir->commandir_last_signal_id = 0;
	}

	new_commandir->next_enabled_emitters_list = malloc(sizeof(int) * 
		new_commandir->num_transmitters);
	for(x=0; x<new_commandir->num_transmitters; x++)
	{
		new_commandir->next_enabled_emitters_list[x] = x+1;
		new_commandir->commandir_tx_available[x] = 0;
	}
	new_commandir->num_next_enabled_emitters = new_commandir->num_transmitters;
	
	// Lastly, we attach this item onto our list:
	struct commandir_device * pcd = first_commandir_device;
	
	if(pcd==NULL) {
		first_commandir_device = new_commandir;
		first_commandir_device->next_commandir_device = NULL;
		// Automatically receive from the first commandir:
		rx_device = first_commandir_device;	
		return TRUE;
	}
	
	while(pcd) {
		if(pcd->next_commandir_device==NULL) {
			pcd->next_commandir_device = new_commandir;
			return TRUE;
		}
		pcd = pcd->next_commandir_device;
	}
	
	return TRUE;
}


// Scan for hardware changes
static void hardware_scan()
{
	struct usb_bus *bus = 0;
	struct usb_device *dev = 0;
	struct detected_commandir * p = NULL;
	
	int located = 0, changed = 0, disconnect = 0;
	
	while(detected_commandirs) {
		p = detected_commandirs;
		detected_commandirs = detected_commandirs->next;
		free(p);
  }
	
	usb_find_busses();
	if(!usb_find_devices()) {
		return;
	}
	
	// Main Scan:
	for (bus = usb_busses; bus; bus = bus->next)
	{
		for (dev = bus->devices; dev; dev = dev->next)
		{
			if ( (dev->descriptor.idVendor == USB_CMDIR_VENDOR_ID) && 
				(dev->descriptor.idProduct == 3) )
			{
				// eg. 004
				unsigned int bus_num = bus->dirname[2] - '0' + (bus->dirname[1]-'0') * 
					10 + (bus->dirname[0]-'0') * 100;
				located++;
				
				if(do_we_know_device(bus_num, dev->devnum)) {
					if(claim_and_setup_commandir(bus_num, dev->devnum, dev)==TRUE) {
						set_detected(bus_num, dev->devnum);
						changed++;
					}
				} else {
					set_detected(bus_num, dev->devnum);
				}
			}// if it's a CommandIR
		}// for bus dev's
	}// for bus's
	
	if(!located)
	{
		// Have we already printed this message recently?  TODO
		logprintf(LOG_ERR, "No CommandIRs found");
	}

	/* Check if any we currently know about have been removed
	 * (Usually, we get a read/write error first)
	 */
	
	struct commandir_device * a;
	for(a = first_commandir_device; a; a = a->next_commandir_device) 
	{
		// Did we detect this?
		int found = 0;
		struct detected_commandir * check_detected_commandir;
		for(check_detected_commandir = detected_commandirs; 
				check_detected_commandir; 
				check_detected_commandir=check_detected_commandir->next) 
			{
			if( (a->busnum==check_detected_commandir->busnum) && 
				(a->devnum==check_detected_commandir->devnum) ) 
			{
				found = 1;
				break;
			}
		}
		
		if(!found) 
		{
			// We no longer detect this CommandIR:
			logprintf(LOG_ERR, "Commandir removed from [%d][%d].",
				a->busnum,
				a->devnum);
			hardware_disconnect(a);
			disconnect++;
			commandir_rx_num = -1;
			changed++;
		}
	}
	
	if(disconnect) 
	{
		software_disconnects();
	}
	
	if(changed)
	{
		hardware_setorder();
		raise_event(COMMANDIR_REORDERED);
	}
}


static void set_detected(unsigned int bus_num, int devnum) {
	struct detected_commandir * newdc;
	struct detected_commandir * last_detected_commandir = NULL;

	newdc = malloc(sizeof(struct detected_commandir));
	newdc->busnum = bus_num;
	newdc->devnum = devnum;
	newdc->next = NULL;
	if(detected_commandirs==NULL) {
		detected_commandirs = newdc;
	} 
	else 
	{
		for(last_detected_commandir = detected_commandirs; 
			last_detected_commandir->next; 
			last_detected_commandir = last_detected_commandir->next) 
		{
		}
		last_detected_commandir->next = newdc;
	}
	last_detected_commandir = newdc;
}


static void hardware_setorder() {
	// This has been rewritten to use linked lists.
	// Each object has a busnum and devnum; we sort those to preserve order
	struct commandir_device * pcd;
	struct commandir_tx_order * ptx;
	struct commandir_tx_order * last_ptx;
	int done = 0;
	
	ptx = ordered_commandir_devices;
	
	struct commandir_tx_order * next_ptx;  //   = ptx->next;
	
	while(ptx) {
		next_ptx = ptx->next;
		free(ptx);
		ptx = next_ptx;
	}
	ordered_commandir_devices = NULL;	// we just deleted them all
	
	if( (rx_device==NULL) && (first_commandir_device!=NULL) )
	{
		rx_device = first_commandir_device;
	}
	
	for(pcd = first_commandir_device; pcd; pcd = pcd->next_commandir_device) 
	{
		// Add pcd to the ptx tree, in the right spot.
		struct commandir_tx_order * new_ptx;
		new_ptx = malloc(sizeof(struct commandir_tx_order));
		
		new_ptx->the_commandir_device = pcd;
		new_ptx->next = NULL;
		
		if(ordered_commandir_devices == NULL) {
			ordered_commandir_devices = new_ptx;
			continue;
		}
		
		int this_pcd_value = pcd->busnum * 128 + pcd->devnum;
		ptx = ordered_commandir_devices;
		last_ptx = NULL;
		
		int this_ptx_value = ptx->the_commandir_device->busnum * 128 + 
			ptx->the_commandir_device->devnum;
		
		if(this_pcd_value > this_ptx_value) 
		{
			 ptx->next = new_ptx;
			 continue;
		}
		if(this_pcd_value < this_ptx_value) 
		{
			new_ptx->next = ordered_commandir_devices;
			ordered_commandir_devices = new_ptx;
			continue;
		}
		
		while(this_pcd_value < this_ptx_value) {
		 // keep moving up the ptx chain until pcd is great, then insert
		 if(ptx->next == NULL) {
			 last_ptx->next = new_ptx;
			 new_ptx->next = ptx;
			 done = 1;
			 break; // we're at the end
		 }
		 last_ptx = ptx;
		 ptx = ptx->next;
		 if(ptx==NULL) {
			 ptx->next = new_ptx;
			 done = 1;
			 break; // we're at the end
		 }
		 this_ptx_value = ptx->the_commandir_device->busnum * 128 + 
		 	ptx->the_commandir_device->devnum;
		}
		
		if(!done) 
		{
			if(last_ptx==NULL) 
			{
				// This one is the new head:
				new_ptx->next = ordered_commandir_devices;
				ordered_commandir_devices = new_ptx;
				continue;
			}
			last_ptx->next = new_ptx;
			new_ptx->next = ptx;
		}
	}
	
	int CommandIR_Num = 0;
	int emitters = 1;
	if(first_commandir_device && first_commandir_device->next_commandir_device)
	{
		logprintf(LOG_INFO, "Re-ordered Multiple CommandIRs:");
		for(pcd = first_commandir_device; pcd; pcd = pcd->next_commandir_device) {
			logprintf(LOG_INFO, 
				" CommandIR Index: %d (Type: %d, Revision: %d), Emitters #%d-%d", 
				CommandIR_Num, pcd->hw_type, pcd->hw_revision, emitters, 
				emitters + pcd->num_transmitters-1);
			CommandIR_Num++;
			emitters += pcd->num_transmitters;
		}
	}
}


static void hardware_disconnect(struct commandir_device * a)
{
	// To disconnect, uninit the USB object, and set the pointer to NULL
	// The cleanup function will remove this item from the chain
	usb_release_interface(a->cmdir_udev,
		a->interface);
	usb_close(a->cmdir_udev);
	a->cmdir_udev = NULL;
}

static void software_disconnects() {
	struct commandir_device * previous_dev = NULL;
	struct commandir_device * next_dev;
	
	struct commandir_device * a;
	a = first_commandir_device;
	
	struct detected_commandir * pdc;
	struct detected_commandir * last_pdc;
	
	struct commandir_tx_order * ptx;
	struct commandir_tx_order * last_ptx;
	
	while(a) 
	{
		if(a->cmdir_udev==NULL) 
		{
			last_pdc = NULL;
			for(pdc = detected_commandirs; pdc; pdc=pdc->next) 
			{
				if( (pdc->busnum==a->busnum) && (pdc->devnum==a->devnum)) 
				{
					if(last_pdc==NULL) {
						detected_commandirs=pdc->next;
					} 
					else 
					{
						last_pdc->next = pdc->next;
			 		}
				 free(pdc);
				 break;
				}
			}
			
			last_ptx = NULL;
			for(ptx = ordered_commandir_devices; ptx; ptx=ptx->next) 
			{
				if(ptx->the_commandir_device==a) 
				{
					if(last_ptx==NULL) 
					{
						ordered_commandir_devices=ptx->next;
					} 
					else 
					{
						last_ptx->next = ptx->next;
					}
					free(ptx);
					break;
				}
			}
			
			if(previous_dev==NULL) 
			{
				if(a->next_commandir_device) {
					first_commandir_device = a->next_commandir_device;
				} 
				else 
				{
					first_commandir_device = NULL;
				}
			} 
			else 
			{
				if(a->next_commandir_device) 
				{
					previous_dev->next_commandir_device = a->next_commandir_device;
				} 
				else 
				{
					previous_dev->next_commandir_device = NULL;
				}
			}
			next_dev = a->next_commandir_device;
			
			free(a);
			if(previous_dev) 
			{
				if(a==rx_device)
				{
					rx_device = previous_dev;	
				}
				a = next_dev;
				previous_dev->next_commandir_device = next_dev;
			} 
			else 
			{
				if(a==rx_device)
				{
					rx_device = first_commandir_device;
				}
				a = next_dev;
				first_commandir_device = a;
			}
			continue;
		} 
		else 
		{	
			// If this one is empty:
			previous_dev = a;
		}
		a = a->next_commandir_device;
	} // check all of them
}




/*** Reading and Writing Functions ***/
static void commandir_read_loop()
{
	// Read from CommandIR, Write to pipe
	static unsigned char rx_decode_led[7] = {7, PROC_SET, 0x40, 0, 0,4, 2};
	static unsigned char init_led[7] = {7, PROC_SET, 0x00, 0x01, 3, 55, 2};
	static unsigned char deinit_led[7] = {7, PROC_SET, 0x0, 0x02, 3, 45, 2};
	static unsigned int LIRC_frequency = 38000;
	
	unsigned char commands[MAX_COMMAND];
	int curCommandStart = 0;
	int curCommandLength = 0;
	int bytes_read;
	unsigned char periodic_checks = 0;
	int send_status = 0;
	int repeats = 0;
	
	struct commandir_device * pcd;
	struct send_tx_mask * incoming_new_mask;
	
	raise_event(COMMANDIR_READY);
	
	for(;;)
	{
		/*** This is the main loop the monitors control and TX events from
		  * the parent, and monitors the CommandIR RX buffer
		  */
		
		curCommandStart = 0;
		curCommandLength = 0;
		
		bytes_read = read(tochild_read, commands, MAX_COMMAND);
		
		if(bytes_read > 0)
		{
			while(curCommandStart < bytes_read)
			{
				curCommandLength = commands[curCommandStart] +
					commands[curCommandStart + 1] * 256;

				switch(commands[curCommandStart + 2]){	// the control value
					case DEINIT_HEADER_LIRC:
						for(pcd = first_commandir_device; pcd; 
							pcd=pcd->next_commandir_device) 
						{
							if(pcd->hw_type == HW_COMMANDIR_2)
							{
								if(pcd->cmdir_udev > 0)
								{
									send_status=usb_bulk_write(
										pcd->cmdir_udev,
										2, // endpoint2
										(char*)deinit_led,
										7, // bytes
										USB_TIMEOUT_MS);
								}
								rx_hold = 1;	// Put a hold on RX, but queue events
							}
						}
						break;
						
					case INIT_HEADER_LIRC:
					  for(pcd = first_commandir_device; pcd; 
					  	pcd=pcd->next_commandir_device)
						{
							if(pcd->hw_type == HW_COMMANDIR_2)
							{
								if(pcd->cmdir_udev > 0)
								{
									send_status=usb_bulk_write(
										pcd->cmdir_udev,
										2, // endpoint2
										(char*)init_led,
										7, // bytes
										USB_TIMEOUT_MS);
								}
								rx_hold = 0;	// Resume RX after queue events
							}
						}
						break;
						
					case RXDECODE_HEADER_LIRC:
					  // Successful RX decode: show it on CommandIR II LED
					  for(pcd = first_commandir_device; pcd; 
					  	pcd=pcd->next_commandir_device)
						{
						  if(rx_device && (rx_device->cmdir_udev > 0) && 
						  	(pcd->hw_type == HW_COMMANDIR_2) )
						  {
								send_status=usb_bulk_write(
									rx_device->cmdir_udev,
									2, // endpoint2
									(char*)rx_decode_led,
									7, // bytes
									USB_TIMEOUT_MS);
						  }
						}
					  break;
						
					case FREQ_HEADER_LIRC:
						LIRC_frequency = (commands[curCommandStart + 6] & 0x000000ff) |
							((commands[curCommandStart + 5] << 8) & 0x0000ff00) |
							((commands[curCommandStart + 4] << 16) & 0x00ff0000) |
							((commands[curCommandStart + 3] << 24) & 0xff000000);
						if(!LIRC_frequency)
							LIRC_frequency = DEFAULT_FREQ;
						break;
						
					case TX_HEADER_NEW:
					case TX_LIRC_T:
						if(curCommandLength==64)
						{
							if(check_irsend_commandir(&commands[curCommandStart + 4]))
							{
								break;
							}
						}
						add_to_tx_pipeline(&commands[curCommandStart + 4], 
							curCommandLength - 4, LIRC_frequency);
						break;
						
					case CHANNEL_EN_MASK:
						// This is also in check_irsend_commandir
						incoming_new_mask = (void*)&commands[curCommandStart];
						set_convert_int_bitmask_to_list_of_enabled_bits(
							&incoming_new_mask->new_tx_mask, 
							sizeof(incoming_new_mask->new_tx_mask));
						break;
				}
				curCommandStart += curCommandLength;
			}
		} 
		else 
		{
			// shutdown check moved down here to ensure the read loop is done once
			if(shutdown_pending > 0)
				shutdown_usb();
		}
		
		repeats = 5;	// Up to 5 immediate repeats if we have alot to receive
		while((commandir_read() > 63 ) && (repeats-- > 0) )
		{
			// Don't remove this loop, need to call commandir_read()
		}
		if(repeats > 0)
		{
			// once in a while, but never while we're receiving a signal
			if(++periodic_checks>100)
			{
				hardware_scan();
				periodic_checks = 0;
			}
			else
			{
 				usleep(read_delay);
 			}
		}
	}
}



/*** New Add_to_tx_pipeline ***/
static void add_to_tx_pipeline(unsigned char *buffer, int bytes,
	unsigned int frequency)
{
	/* *buffer points to a buffer that will be OVERWRITTEN; malloc our copy.
	 * buffer is a LIRC_T packet for CommandIR
	 */
	struct tx_signal * new_tx_signal = NULL;
	
	new_tx_signal = malloc(sizeof(struct tx_signal));
	
	new_tx_signal->raw_signal = malloc(bytes);
	new_tx_signal->raw_signal_len = bytes;
	new_tx_signal->raw_signal_frequency = frequency;
	new_tx_signal->next = NULL;
	
	lirc_t *oldsignal, *newsignal;
	int x, pulse_now = 1;
	int projected_signal_length;
	short aPCAFOM = 0;
	float afPCAFOM = 0.0;
	int difference = 0;
	
	afPCAFOM = (6000000.0 / ((frequency > 0) ? frequency : DEFAULT_FREQ)  ) ;
	aPCAFOM = afPCAFOM;
	
	// Trim off mid-modulation pulse fragments, add to space for exact signals
	for(x=0; x<(bytes/sizeof(lirc_t)); x++)
	{
		oldsignal = (lirc_t *)&buffer[x*sizeof(lirc_t)];
		newsignal = (lirc_t *)new_tx_signal->raw_signal;
		newsignal += x;
		
		if(pulse_now==1){
			projected_signal_length =
				(((int)( (*oldsignal * 12)/( afPCAFOM ) ))  * aPCAFOM) / 12;
			difference = *oldsignal - projected_signal_length;
			// take off difference plus 1 full FOM cycle
			*newsignal = *oldsignal - difference - (aPCAFOM / 12);
		}
		else
		{
			if(difference != 0)
			{
				// Add anything subtracted from the pulse to the space
				*newsignal = *oldsignal + difference + (aPCAFOM / 12);
				difference = 0;
			}
		}
		pulse_now++;
		if(pulse_now > 1) pulse_now = 0;
	}

	// Deliver this signal to any CommandIRs with next_tx_mask set
	struct commandir_device * pcd;

	for(pcd = first_commandir_device; pcd; pcd=pcd->next_commandir_device) 
	{
		if(pcd->num_next_enabled_emitters) 
		{
			// Make a local copy
			struct tx_signal * copy_new_signal;
			copy_new_signal = malloc(sizeof(struct tx_signal));
			memcpy(copy_new_signal, new_tx_signal, sizeof(struct tx_signal));
			
			copy_new_signal->raw_signal = malloc(bytes);
			memcpy(copy_new_signal->raw_signal, new_tx_signal->raw_signal, bytes);
			
			copy_new_signal->bitmask_emitters_list = 
				malloc(pcd->num_transmitters * sizeof(int));
			
			memcpy(copy_new_signal->bitmask_emitters_list, 
				pcd->next_enabled_emitters_list,
				sizeof(int) * pcd->num_next_enabled_emitters);
			
			copy_new_signal->num_bitmask_emitters_list = 
				pcd->num_next_enabled_emitters;
			copy_new_signal->raw_signal_tx_bitmask = 
				get_hardware_tx_bitmask(pcd);
			
			copy_new_signal->next = NULL;
			set_new_signal_bitmasks(pcd, copy_new_signal);
			
			// Add to units' TX tree:
			if(pcd->last_tx_signal) {
				pcd->last_tx_signal->next = copy_new_signal;
				pcd->last_tx_signal = copy_new_signal;
			} 
			else 
			{
				// The first and the last (ie the only) signal
				pcd->last_tx_signal = copy_new_signal;
				pcd->next_tx_signal = copy_new_signal;
			}
		}
	}
	return;
}


static int get_hardware_tx_bitmask(struct commandir_device * pcd)
{
	int mini_tx_mask = 0;
	int x;
	
	switch(pcd->hw_type)
	{
		case HW_COMMANDIR_2:
		mini_tx_mask = 0;
		for(x=0; x<pcd->num_next_enabled_emitters; x++)
		{
			switch(pcd->next_enabled_emitters_list[x])
			{
				case 1:	mini_tx_mask |= 0x10; break;
				case 2:	mini_tx_mask |= 0x20; break;
				case 3:	mini_tx_mask |= 0x40; break;
				case 4:	mini_tx_mask |= 0x80; break;
			}
		}
		return mini_tx_mask;
		
	case HW_COMMANDIR_MINI:
		mini_tx_mask = 0;
		for(x=0; x<pcd->num_next_enabled_emitters; x++)
		{
			switch(pcd->next_enabled_emitters_list[x])
			{
				case 1:	mini_tx_mask |= 0x80; break;
				case 2:	mini_tx_mask |= 0x40; break;
				case 3:	mini_tx_mask |= 0x20; break;
				case 4:	mini_tx_mask |= 0x10; break;
			}
		}
		return mini_tx_mask;
		
	case HW_COMMANDIR_3:
		mini_tx_mask = 0;
		for(x=0; x<pcd->num_next_enabled_emitters; x++)
		{
			mini_tx_mask |= (1 << (pcd->next_enabled_emitters_list[x]-1));
		}
		return mini_tx_mask;
	}
	// All cases should be covered, but:
	return 0;	
}


// Shutdown everything and terminate
static void shutdown_usb()
{
	// Check for queued signals before shutdown
	struct commandir_device * a;
	a = first_commandir_device;
	
  if( (haveInited == 0) && (shutdown_pending==0) )
  {
  	// Shutdown before init?  Let's wait for an incoming event first
		shutdown_pending++;
		return;
  }
	
	for(a = first_commandir_device; a; a = a->next_commandir_device)
	{
		if(a->next_tx_signal)
		{
			shutdown_pending++;
			logprintf(LOG_ERR, "Waiting for signals to finish transmitting before shutdown");
			return;
		}
	}
	logprintf(LOG_ERR, "No more signal for transmitting, CHILD _EXIT()");
	_exit(EXIT_SUCCESS);
}


static int commandir_decode(char *command)
{
	int i;
	int code = 0;
	
	lirc_t *codes;
	codes = (lirc_t *)command;
	
	for(i=0; i<15; i+=2)
	{
		code = code << 1;
		if(codes[i]==100)
			code |= 1;
	}
	return code;
}


static int check_irsend_commandir(unsigned char *command)
{
	int commandir_code = 0;
	commandir_code = commandir_decode((char*)command);
	
	if(commandir_code > 0xef)
	{
		// It's a settransmitters command; convert channel number to bitmask
		unsigned long channel = 0x01 << (commandir_code & 0x0f);
		set_convert_int_bitmask_to_list_of_enabled_bits(&channel, sizeof(unsigned long));
		return commandir_code;
	}
	
	switch(commandir_code)
	{
		case 0x53:
			read_delay /= 2;	// "faster" means less time
			if(read_delay < MIN_WAIT_BETWEEN_READS_US)
				read_delay = MIN_WAIT_BETWEEN_READS_US;
			break;
		case 0x54:
			read_delay *= 2;	// "slower" means more time
			if(read_delay > MAX_WAIT_BETWEEN_READS_US)
				read_delay = MAX_WAIT_BETWEEN_READS_US;
			break;
		case 0x09:
		case 0x0A:
			logprintf(LOG_ERR, "Re-selecting RX not implemented yet");
			break;
		case 0xe6:	//	disable-fast-decode
			logprintf(LOG_ERR, "Fast decoding disabled");
			insert_fast_zeros = 0;
			break;
		case 0xe7:	//	enable-fast-decode
		case 0xe9:	//	force-fast-decode-2
			logprintf(LOG_ERR, "Fast decoding enabled");
			insert_fast_zeros = 2;
			break;
		case 0xe8:	//	force-fast-decode-1
			logprintf(LOG_ERR, "Fast decoding enabled (1)");
			insert_fast_zeros = 1;
			break;
		default:
			if(commandir_code > 0x60 && commandir_code < 0xf0)
			{
				int ledhigh = 0, ledlow = 0, ledprog = -1;
				// CommandIR II LED Command
				switch(commandir_code >> 4)
				{
					case 0x6: ledlow = 0x80; break;
					case 0x7: ledlow = 0x40; break;
					case 0x8: ledlow = 0x20; break;
					case 0x9: ledlow = 0x10; break;
					case 0xa: ledlow = 0x04; break;
					case 0xb: ledhigh = 0x80; break;
					case 0xc: ledlow = 0x01; break;
					case 0xd: ledlow = 0x02; break;
					case 0xe: ledlow = 0x08; break;
				}
				ledprog = (commandir_code & 0x0f) - 1;
				
				if( ((ledhigh + ledlow) > 0) && ledprog > -1)
				{
					static unsigned char lightchange[7] = {7,
					PROC_SET, 0, 0, 0, 0, 3};
					lightchange[2] = ledhigh;
					lightchange[3] = ledlow;
					lightchange[4] = ledprog;
					int send_status = 0;
					
					if(first_commandir_device)
					{
					  send_status=usb_bulk_write(
							first_commandir_device->cmdir_udev,
							2, // endpoint2
							(char *)lightchange,
							7, // bytes
							USB_TIMEOUT_MS);
					}
				}
				return commandir_code;
			}
	}
	return commandir_code;
}





/*** CommandIR RX Functions ***/

static void lirc_pipe_write(lirc_t * one_item)
{
	int bytes_w;	// Pipe write
	
	bytes_w = write(child_pipe_write, one_item, sizeof(lirc_t));
	if (bytes_w < 0)
	{
		logprintf(LOG_ERR, "Can't write to LIRC pipe! %d", child_pipe_write);
	}
}


static int commandir_read()
{
	/***  Which CommandIRs do we have to read from?  Poll RX CommandIRs
		* regularly, but non-receiving CommandIRs should be more periodically
		*/
	
	int j;
	int read_received = 0;
	int read_retval = 0;
	int conv_retval = 0;
	int max_read = 5;
	static int zeroterminated = 0;
	int commandir_num = 0;
	static int bandwidth_saver_cycle = -1;
	
	struct commandir_device * pcd;
	
	if(++bandwidth_saver_cycle > 50)
		bandwidth_saver_cycle = 0;
	
	for(pcd = first_commandir_device; pcd; pcd = pcd->next_commandir_device)
	{
		commandir_num++;
		// Skip if this unit doesn't RX, and it have no signal waiting to send:
		if( (pcd->next_tx_signal==NULL) && 
			(pcd != rx_device) && 
			(bandwidth_saver_cycle != 0) )
		{
			continue;
		}
		
		switch(pcd->hw_type)
		{
			case HW_COMMANDIR_3:
				commandir_iii_update_status(pcd);
				if(pcd->rx_data_available)
				{
					read_retval = usb_bulk_read(
						pcd->cmdir_udev,
						3,
						(char *)commandir_data_buffer,
						pcd->endpoint_max[3],
						5000);
					if(read_retval==0)
					{
						break;
					}
					
					// Flush any stale data on the CommandIR on initial connect
					if(pcd->flush_buffer>0)
					{
						pcd->flush_buffer--;
					}
					
					if(read_retval < 1)
					{
						if(read_retval == -19){
							logprintf(LOG_ERR, "Read Error - CommandIR probably unplugged");
						}
						else
						{
							logprintf(LOG_ERR,
								"Error %d trying to read %d bytes",
								read_retval, pcd->endpoint_max[3]);
						}
						hardware_disconnect(pcd);
						software_disconnects();
						hardware_scan();
						hardware_setorder();
						return 0;
					}
					
					read_received = read_retval;
					if(read_received <7) break;
					
					if(pcd->flush_buffer==0)
					{
						conv_retval = commandir3_convert_RX(
							commandir_data_buffer, read_retval-1);
					}
				}
			break;
			
			case HW_COMMANDIR_2:
				read_retval = usb_bulk_read(
					pcd->cmdir_udev,
					1,
					(char *)commandir_data_buffer,
					pcd->endpoint_max[1],
					5000);
				
				if(read_retval==0)
					break;
				
				if(read_retval < 1)
				{
					if(read_retval < 0)
					{
						if(read_retval == -19){
							logprintf(LOG_ERR, "Read Error - CommandIR probably unplugged");
						}
						else
						{
							logprintf(LOG_ERR,
								"Didn't receive a full packet from a CommandIR II! - err %d ."
								, read_retval);
						}
						hardware_disconnect(pcd);
						software_disconnects();
						hardware_scan();
						hardware_setorder();
						return 0;
					}
					break;
				}
				
				if(commandir_data_buffer[0]==RX_HEADER_TXAVAIL)
				{
					update_tx_available(pcd);
					
					int tmp4 = 0;
					if(zeroterminated>1001)
					{
						if(insert_fast_zeros > 0)
						{
							tmp4 = write(child_pipe_write, lirc_zero_buffer, 
								sizeof(lirc_t)*insert_fast_zeros);
						}
						zeroterminated = 0;
					}
					else
					{
						if((zeroterminated < 1000) && (zeroterminated > 0))
							zeroterminated += 1000;
						if(zeroterminated > 1000)
							zeroterminated++;
					}
					
					break;
				}
				
				if(commandir_data_buffer[0]==RX_HEADER_EVENTS)
				{
					for(j=1; j<(read_retval); j++)
					{
 						raise_event(commandir_data_buffer[j]+(commandir_num-1)*0x10);
					}
				}
				else
				{
					if( (commandir_data_buffer[0]==RX_HEADER_DATA) &&
						(rx_device==pcd) )
					{
						if(rx_hold==0)
						{
							zeroterminated = 1;
							conv_retval = commandir2_convert_RX(
								(unsigned short *)&commandir_data_buffer[2],
								commandir_data_buffer[1]);
							read_received = conv_retval;
						}
					} else {
							// Ignoring RX data from this unit
					}
				}
				break;
				
			case HW_COMMANDIR_MINI:
				max_read = 5;
				while(max_read--){
					read_retval = usb_bulk_read(
						pcd->cmdir_udev,
						1,
						(char *)commandir_data_buffer,
						64,
						USB_TIMEOUT_MS);
					
					if (!(read_retval == MAX_HW_MINI_PACKET))
					{
						if(read_retval == -19){
							logprintf(LOG_ERR, "Read Error - CommandIR probably unplugged");
						}
						else
						{
							logprintf(LOG_ERR,
								"Didn't receive a full packet from a Mini! - err %d .",
								read_retval);
						}
						hardware_disconnect(pcd);
						software_disconnects();
						hardware_scan();
						hardware_setorder();
						return 0;
					}
					
					if ( (commandir_data_buffer[1] > 0)  &&
						(rx_device==pcd) )
					{
						conv_retval = cmdir_convert_RX(commandir_data_buffer);
						read_received += commandir_data_buffer[1];
						
						if(commandir_data_buffer[1] < 20)
						{
							break;
						}
					}
					else
					{
						break;
					}
				} // while; should only repeat if there's more RX data
				update_tx_available(pcd);
				break;
			case HW_COMMANDIR_UNKNOWN:
					break;
		} // end switch
	} // for each attached hardware device
	return read_received;
}


static void set_new_signal_bitmasks(struct commandir_device * pcd, 
	struct tx_signal * ptx)
{
	ptx->raw_signal_tx_bitmask = get_hardware_tx_bitmask(pcd);
	ptx->num_bitmask_emitters_list = pcd->num_next_enabled_emitters;
	ptx->bitmask_emitters_list = malloc(
		sizeof(int) * ptx->num_bitmask_emitters_list);
	memcpy(ptx->bitmask_emitters_list, pcd->next_enabled_emitters_list, 
		sizeof(int) * ptx->num_bitmask_emitters_list);
}



static void pipeline_check(struct commandir_device * pcd)
{
  /* Transmit from the pipeline if it's time and there's space
	* what's available should now be updated in the main_loop
	*/
	int j, oktosend = 1;
	
	if(!pcd->next_tx_signal) 
		return;
	
	switch(pcd->hw_type)
	{
		case HW_COMMANDIR_3:
			commandir_2_transmit_next(pcd);
			break;
		case HW_COMMANDIR_2:
		case HW_COMMANDIR_MINI:

		for(j=0; j<pcd->next_tx_signal->num_bitmask_emitters_list; j++)
		{
			if(pcd->commandir_tx_available[ 
				pcd->next_tx_signal->bitmask_emitters_list[j]-1 ] <
				(36 + (pcd->next_tx_signal->raw_signal_len)/sizeof(lirc_t)))
			{
				oktosend = 0;
				break;
			}
		}
		
		if(oktosend)
		{
			for(j = 0; j<pcd->next_tx_signal->num_bitmask_emitters_list; j++)
			{
				pcd->commandir_tx_available[j] = 0;
			}
			commandir_2_transmit_next(pcd);
		}
	}
}



static void commandir_2_transmit_next(struct commandir_device * pcd)
{
	/*** Send a TX command to 1 or more CommandIRs.
	 * Keep in mind: TX frequency, TX channels, TX signal length,
	 * which CommandIR, & what hardware version
	 */
	
	int send_status;
	unsigned char packet[TX_BUFFER_SIZE];
	/* So we know where there should be gaps between signals and more
	 * importantly, where there shouldn't be
	 */
	
	/* Depending on the tx channels, then depending on what hardware it is,
	 * set the freq if needed, and send the buffer with the channel header
	 * that's right for that CommandIR
	 */
	
	int sent = 0, tosend = 0;
	int total_signals = 0;
	lirc_t * signals;	// have bytes/sizeof(lirc_t) signals
	int i;
	char cmdir_char[66];
	int which_signal = 0;
	
	struct tx_signal * ptx;
	struct commandir_3_tx_signal * ptxiii;
	
	char freqPulseWidth = DEFAULT_PULSE_WIDTH;
	
	total_signals = pcd->next_tx_signal->raw_signal_len / sizeof(lirc_t);
	signals = (lirc_t *)pcd->next_tx_signal->raw_signal;
	
  switch(pcd->hw_type)
  {
	  case HW_COMMANDIR_3:
			ptxiii = (struct commandir_3_tx_signal *)&packet;
			ptxiii->tx_bit_mask1 = 
				(unsigned short)(pcd->next_tx_signal->raw_signal_tx_bitmask);
			ptxiii->tx_bit_mask2 = 
				(unsigned short)(pcd->next_tx_signal->raw_signal_tx_bitmask >> 16);
			ptxiii->tx_min_gap = 100000/20;
			ptxiii->tx_signal_count = 
				pcd->next_tx_signal->raw_signal_len/sizeof(lirc_t);
			ptxiii->pulse_width = 
				48000000 / pcd->next_tx_signal->raw_signal_frequency;
			ptxiii->pwm_offset = ptxiii->pulse_width / 2;
			
		  int sendNextSignalsCounter = 0;
		  int packetCounter;
		  packetCounter = sizeof(struct commandir_3_tx_signal);
		  short tx_value;
		  char pulse_toggle = 1;
		  while(sendNextSignalsCounter < 
		  	(pcd->next_tx_signal->raw_signal_len/sizeof(lirc_t)))
			{
				while( (packetCounter < (pcd->endpoint_max[2]-1) ) && 
			  	(sendNextSignalsCounter < 
			  	(pcd->next_tx_signal->raw_signal_len/sizeof(lirc_t)) ) )
			  {
					tx_value =  signals[sendNextSignalsCounter++] * 
						pcd->next_tx_signal->raw_signal_frequency/1000000;
					packet[packetCounter++] = tx_value & 0xff;	// low byte
					packet[packetCounter++] = (tx_value >> 8) | (pulse_toggle<<7);
					pulse_toggle = !pulse_toggle;
				}
				if(packetCounter > sizeof(struct commandir_3_tx_signal))
				{
					send_status=usb_bulk_write(
						pcd->cmdir_udev,
						2,
						(char*)packet,
						packetCounter,
						USB_TIMEOUT_MS);
					packetCounter = 0;
					if(send_status < 0)
					{
						hardware_scan();
						return;
					}
			  }
		  }
		  break;
		  
	  case HW_COMMANDIR_2:
			packet[1] = TX_COMMANDIR_II;
			packet[2] = pcd->next_tx_signal->raw_signal_tx_bitmask;
			
			short PCAFOM = 0;
			float fPCAFOM = 0.0;
			
			if(pcd->next_tx_signal->raw_signal_len/sizeof(lirc_t) > 255)
			{
				logprintf(LOG_ERR, "Error: signal over max size");
				break;
			}
			
			fPCAFOM = (6000000 / ((pcd->next_tx_signal->raw_signal_frequency > 0) ? 
				pcd->next_tx_signal->raw_signal_frequency :	DEFAULT_FREQ)  ) ;
			PCAFOM = fPCAFOM;
			
			pcd->lastSendSignalID = packet[5] = (getpid() + pcd->signalid++) + 1;
			
			packet[4] = PCAFOM & 0xff;
			packet[3] = (PCAFOM >> 8) & 0xff;
			
			short packlets_to_send = 0, sending_this_time = 0;
			
			packlets_to_send = pcd->next_tx_signal->raw_signal_len / sizeof(lirc_t);
			
			int attempts;
			for(attempts = 0; attempts < 10; attempts++)
			{
				// Force CommandIR II packet sizes to 64 byte max
				if((packlets_to_send*3 + 7) > 64)
				{
					sending_this_time =64/3 - 3;
				}
				else
				{
					sending_this_time = packlets_to_send;
				}
				int sending;
				
				for(i=0; i<sending_this_time; i++)
				{
					sending = signals[which_signal++];
					packet[i*3+7] = sending >> 8; // high1
					packet[i*3+8] = sending & 0xff; // low
					packet[i*3+9] = sending >> 16 & 0xff; // high2
				}
				
				packet[0] = (sending_this_time * 3 + 7);
				packet[6] = (sending_this_time == packlets_to_send)  ? 0xcb :  0x00;
				
				send_status=usb_bulk_write(
					pcd->cmdir_udev,
					2,
					(char*)packet,
					packet[0],
					USB_TIMEOUT_MS);
				
				if(send_status < 0)
				{
					hardware_scan();
					return;
				}
				
				packlets_to_send -= ((send_status - 7) / 3);
				if(!packlets_to_send)
				{
					break;
				}
			}
		  break;

	  case HW_COMMANDIR_MINI:
			freqPulseWidth = (unsigned char)((1000000 /
				((pcd->next_tx_signal->raw_signal_frequency > 0) ? 
				pcd->next_tx_signal->raw_signal_frequency: DEFAULT_FREQ)  ) / 2);
			
			if(freqPulseWidth == 0)
			{
				freqPulseWidth = DEFAULT_PULSE_WIDTH;
			}
			
			if(pcd->mini_freq != freqPulseWidth)
			{
				cmdir_char[0] = FREQ_HEADER;
				cmdir_char[1] = freqPulseWidth;
				cmdir_char[2] = 0;
				pcd->mini_freq = freqPulseWidth;
				send_status=usb_bulk_write(
					pcd->cmdir_udev,
					2, 
					cmdir_char,
					2, // 2 bytes
					USB_TIMEOUT_MS);
				if(send_status < 2)
				{
					hardware_scan();
					return ;
				}
			}
			
			unsigned int mod_signal_length=0;
			
			cmdir_char[0] = TX_HEADER_NEW;
			cmdir_char[1] = pcd->next_tx_signal->raw_signal_tx_bitmask;
			
			unsigned int hibyte, lobyte;
			sent = 0;
			which_signal = 0;
			while(sent < (pcd->next_tx_signal->raw_signal_len / 
				sizeof(lirc_t) * 2 ) )
			{
				tosend = (pcd->next_tx_signal->raw_signal_len / 
					sizeof(lirc_t) * 2 ) - sent;
				
				if(tosend > (MAX_HW_MINI_PACKET - 2))
				{
					tosend = MAX_HW_MINI_PACKET - 2;
				}
				
				for(i=0;i<(tosend/2);i++) // 2 bytes per CommandIR pkt
				{
					mod_signal_length = signals[which_signal++] >> 3;
					hibyte = mod_signal_length/256;
					lobyte = mod_signal_length%256;
					cmdir_char[i*2+3] = lobyte;
					cmdir_char[i*2+2] = hibyte;
				}
				send_status=usb_bulk_write(
					pcd->cmdir_udev,
					2,
					cmdir_char,
					tosend + 2,
					USB_TIMEOUT_MS);
				if(send_status < 1)
				{
					hardware_scan();
					return;
				}
				sent += tosend;
			} // while unsent data
			break;
		default:
			logprintf(LOG_ERR, "Unknown hardware: %d", pcd->hw_type);
	}
	
	ptx = pcd->next_tx_signal->next;
	
	if(pcd->last_tx_signal == pcd->next_tx_signal) {
		pcd->last_tx_signal = NULL;
	}
	
	free(pcd->next_tx_signal->raw_signal);
	pcd->next_tx_signal->raw_signal = NULL;
	
	free(pcd->next_tx_signal);
	
	pcd->next_tx_signal = ptx;
}


static void recalc_tx_available(struct commandir_device * pcd)
{
	int i;
	int length = 0;
	static int failsafe = 0;
	
	if(pcd->lastSendSignalID !=
		pcd->commandir_last_signal_id)
	{
		if(failsafe++ < 1000)
		{
			return;
		}
		logprintf(LOG_ERR, "Error: required the failsafe %d != %d", 
			pcd->lastSendSignalID, pcd->commandir_last_signal_id);
	}
	
	failsafe = 0;
	for(i=0; i<4; i++)
	{
		length = pcd->commandir_tx_end[i] - pcd->commandir_tx_start[i];
		if(length < 0) length += 0xff;

		if(pcd->commandir_tx_available[i] < 0xff - length)
		{
			pcd->commandir_tx_available[i] = 0xff - length;
		}
	}
}


static unsigned int get_time_value(unsigned int firstint,
	unsigned int secondint, unsigned char overflow)
{
	/* get difference between two MCU timestamps, CommandIR Mini version  */
	unsigned int t_answer = 0;
	
	if (secondint > firstint)
	{
		t_answer = secondint - firstint + overflow*0xffff;
	}
	else
	{
		if (overflow > 0)
		{
			t_answer = (65536 - firstint) + secondint + (overflow - 1)*0xffff - 250;
		}
		else
		{
			t_answer = (65536 - firstint) + secondint;
		}
	}

	/* clamp to long signal  */
	if (t_answer > 16000000) t_answer = PULSE_MASK;
	return t_answer;
}


// Originally from lirc_cmdir.c, CommandIR Mini RX Conversion
static int cmdir_convert_RX(unsigned char *orig_rxbuffer)
{
	unsigned int num_data_values = 0;
	unsigned int num_data_bytes = 0;
	unsigned int asint1 = 0, asint2 = 0, overflows = 0;
	int i;
	int bytes_w;	// Pipe write
	
	lirc_t lirc_data_buffer[256];
	num_data_bytes = orig_rxbuffer[1];
	
	/* check if num_bytes is multiple of 3; if not, error  */
	if (num_data_bytes%3 > 0) return -1;
	if (num_data_bytes > 60) return -3;
	if (num_data_bytes < 3) return -2;
	
	num_data_values = num_data_bytes/3;
	
	asint2 = orig_rxbuffer[3] + orig_rxbuffer[4] * 0xff;
	if(last_mc_time==-1)
	{
		// The first time we run there's no previous time value
		last_mc_time = asint2 - 110000;
		if(last_mc_time < 0) last_mc_time+=0xffff;
	}
	
	asint1 = last_mc_time;
	overflows = orig_rxbuffer[5];
	
	for(i=2; i<num_data_values+2; i++)
	{
		if(overflows < 0xff)
		{
			// space
			lirc_data_buffer[i-2] = get_time_value(asint1,
				 asint2, overflows) - 26;
		}
		else
		{	// pulse
			lirc_data_buffer[i-2] = get_time_value(asint1,
				 asint2, 0) + 26;
			lirc_data_buffer[i-2] |= PULSE_BIT;
		}
		asint1 = asint2;
		asint2 = orig_rxbuffer[i*3] + orig_rxbuffer[i*3+1] * 0xff;
		overflows = orig_rxbuffer[i*3+2];
	}
	last_mc_time = asint1;
	
 	bytes_w = write(child_pipe_write, lirc_data_buffer, 
 		sizeof(lirc_t)*num_data_values);
	
	if (bytes_w < 0)
	{
		logprintf(LOG_ERR, "Can't write to LIRC pipe! %d", child_pipe_write);
		goto done;
	}
	
done:
	return bytes_w;
}


static int commandir3_convert_RX(unsigned char *rxBuffer,
	int numNewValues)
{
	static unsigned char incomingBuffer[MAX_INCOMING_BUFFER + 30];
	static unsigned char switchByte = 0;
	static int incomingBuffer_Write = 0, incomingBuffer_Read = 0;
	static lirc_t	buffer_write;
	static int currentPCA_Frequency = (48000000/38000);	
	static float currentPWM = 0;
	
	int i, mySize, read_num;
	int currentSize, expectingBytes;
	int packet_number;
	static int mcu_rx_top_location;
	
	struct usb_rx_pulse3 * a_usb_rx_pulse;
	struct usb_rx_space3 * a_usb_rx_space;
	struct usb_rx_pulse_def3 * a_usb_rx_pulse_def;
	struct usb_rx_demod_pulse * a_usb_rx_demod_pulse;
	
	if(numNewValues > 0)
	{
		packet_number = rxBuffer[0];
		expectingBytes = rxBuffer[1] + rxBuffer[2] * 256;
		if(numNewValues != (expectingBytes + 5))
			logprintf(LOG_ERR, "MCU top is now: %d (a change of: %d)\n", 
				rxBuffer[3] + rxBuffer[4] * 256, (rxBuffer[3] + rxBuffer[4] * 256) - 
				mcu_rx_top_location);
		mcu_rx_top_location = rxBuffer[3] + rxBuffer[4] * 256;
		
		if(numNewValues != (expectingBytes + 5))
			logprintf(LOG_ERR, 
				"USB received: %d, expectingBytes: %d. Hex data (headers: %d %d %d):\t"
				, numNewValues, expectingBytes, rxBuffer[0], rxBuffer[1], rxBuffer[2]);
		for (i = 5; i < expectingBytes + 5; i++)
		{
/*			if(numNewValues != (expectingBytes + 5)) 	
				printf("%02x@%d\t", rxBuffer[i], incomingBuffer_Write);
 */
			incomingBuffer[incomingBuffer_Write++] = rxBuffer[i];
			if(incomingBuffer_Write >= MAX_INCOMING_BUFFER)
				incomingBuffer_Write = 0;
		}
		
		if(incomingBuffer_Write > incomingBuffer_Read)
		{
			currentSize = incomingBuffer_Write - incomingBuffer_Read;
		} else {
			currentSize = (MAX_INCOMING_BUFFER - incomingBuffer_Read) + 
				incomingBuffer_Write;
		}
		
		while(currentSize > 0)
		{
			// Make sure the entire struct is received before trying to use
			switch(incomingBuffer[incomingBuffer_Read])
			{
				case USB_RX_PULSE:
					mySize = 2;
					break;
				case USB_RX_SPACE:
					mySize = 4;
					break;
				case USB_RX_PULSE_DEMOD:
					mySize = 4;
					break;
				case USB_RX_SPACE_DEMOD:
					mySize = 4;
					break;
				case USB_RX_PULSE_DEF:
					mySize = 4;
					break;
				case USB_NO_DATA_BYTE:
					read_num = write(child_pipe_write, lirc_zero_buffer, 
						sizeof(lirc_t)*insert_fast_zeros);
					mySize = 0;
					break;
				default:
					logprintf(LOG_ERR, "Unknown struct identifier: %02x at %d\n", 
						incomingBuffer[incomingBuffer_Read], incomingBuffer_Read);
					mySize = 0;
					break;
			}
			
			if(currentSize >= (mySize+1))	
			{
				if(incomingBuffer_Read + mySize >= MAX_INCOMING_BUFFER)
				{
					memcpy(&incomingBuffer[MAX_INCOMING_BUFFER], &incomingBuffer[0], 
						mySize + 1);	// +1 to compensate for header
				}
				
				switchByte = incomingBuffer[incomingBuffer_Read++];
				if(incomingBuffer_Read >= MAX_INCOMING_BUFFER)
					incomingBuffer_Read -= MAX_INCOMING_BUFFER;
				currentSize--;

				switch(switchByte)
				{
				case USB_NO_DATA_BYTE:
					// Hold this feature for now
					//	buffer_write = 0;
					//	lirc_pipe_write(&buffer_write);
					break;
					
				case USB_RX_PULSE:
					a_usb_rx_pulse = (struct usb_rx_pulse3 *)
						&incomingBuffer[incomingBuffer_Read];
					buffer_write =  a_usb_rx_pulse->t0_count * 
						(1000000/ (1/( ((float)(currentPCA_Frequency))/48000000) ) ) ;
					buffer_write |= PULSE_BIT;
					lirc_pipe_write(&buffer_write);
					break;
					
				case USB_RX_SPACE:
					a_usb_rx_space = (struct usb_rx_space3 *)
						&incomingBuffer[incomingBuffer_Read];
					buffer_write = (a_usb_rx_space->pca_offset + 
						(a_usb_rx_space->pca_overflow_count) * 0xffff)/48;
					lirc_pipe_write(&buffer_write);
					break;
					
				case USB_RX_PULSE_DEF:
					a_usb_rx_pulse_def = (struct usb_rx_pulse_def3 *)
						&incomingBuffer[incomingBuffer_Read];
					currentPWM = ((float)(a_usb_rx_pulse_def->pwm))/48;
					/*  We have no way to tell irrecord the detected frequency, do we?
					printf(" Pulse Def, frequency: %f us, pwm: %fus; Duty Cycle: %f%%\n",
							1/( ((float)(a_usb_rx_pulse_def->frequency))/48000000),
							currentPWM,
							100* ((((float)(a_usb_rx_pulse_def->pwm))/48) / 
								(((float)(a_usb_rx_pulse_def->frequency)/48)) ));
					*/
					currentPCA_Frequency = a_usb_rx_pulse_def->frequency;
					break;
					
				case USB_RX_PULSE_DEMOD:
					a_usb_rx_demod_pulse = (struct usb_rx_demod_pulse*)
						&incomingBuffer[incomingBuffer_Read];
					buffer_write = (a_usb_rx_demod_pulse->pca_offset + 
						(a_usb_rx_demod_pulse->pca_overflow_count) * 0xffff)/48;
					buffer_write |= PULSE_BIT;
					lirc_pipe_write(&buffer_write);
					break;
					
				case USB_RX_SPACE_DEMOD:
					a_usb_rx_demod_pulse = (struct usb_rx_demod_pulse*)
						&incomingBuffer[incomingBuffer_Read];
					buffer_write = (a_usb_rx_demod_pulse->pca_offset + 
						(a_usb_rx_demod_pulse->pca_overflow_count) * 0xffff)/48;
					lirc_pipe_write(&buffer_write);
					break;
				}
				
				incomingBuffer_Read +=mySize;
				if(incomingBuffer_Read >= MAX_INCOMING_BUFFER)
					incomingBuffer_Read -= MAX_INCOMING_BUFFER;
				currentSize -= mySize;
			} else {
				break; // from while
			}
		}
	}
	return 0;
}


// CommandIR II RX Conversion
static int commandir2_convert_RX(unsigned short *bufferrx,
	unsigned char numvalues)
{
	// convert hardware timestamp values to elapsed time values
	
	int i;
	int curpos = 0;
	int bytes_w = 0;
	lirc_t lirc_data_buffer[256];

	i=0;
	int pca_count = 0;
	int overflows = 0;

	while(curpos < numvalues )
	{
		pca_count = (bufferrx[curpos] & 0x3fff) << 2;
		pca_count = pca_count / 12;
		if(bufferrx[curpos] & COMMANDIR_2_OVERFLOW_MASK)
		{
			overflows = bufferrx[curpos+1];
			lirc_data_buffer[i] =  pca_count + (overflows * 0xffff / 12);

			if(bufferrx[curpos] & COMMANDIR_2_PULSE_MASK)
			{
				lirc_data_buffer[i] |= PULSE_BIT;
			}
			curpos++;
		}
		else
		{
			lirc_data_buffer[i] = pca_count;
			if(bufferrx[curpos] & COMMANDIR_2_PULSE_MASK)
			{
				lirc_data_buffer[i] |= PULSE_BIT;
			}
		}
		
		curpos++;
		i++;
		if(i> 255)
		{
			break;
		}
	}
	
 	bytes_w = write(child_pipe_write, lirc_data_buffer, sizeof(lirc_t)*i);
	
	if (bytes_w < 0)
	{
		logprintf(LOG_ERR, "Can't write to LIRC pipe! %d", child_pipe_write);
		return 0;
	}
	return bytes_w;
}



static void update_tx_available(struct commandir_device * pcd)
{
  // Note the endian change
  switch(pcd->hw_type)
  {
		case HW_COMMANDIR_2:
			pcd->commandir_tx_start[0] = commandir_data_buffer[4];
			pcd->commandir_tx_start[1] = commandir_data_buffer[3];
			pcd->commandir_tx_start[2] = commandir_data_buffer[2];
			pcd->commandir_tx_start[3] = commandir_data_buffer[1];

			pcd->commandir_tx_end[0] = commandir_data_buffer[8];
			pcd->commandir_tx_end[1] = commandir_data_buffer[7];
			pcd->commandir_tx_end[2] = commandir_data_buffer[6];
			pcd->commandir_tx_end[3] = commandir_data_buffer[5];
			pcd->commandir_last_signal_id = commandir_data_buffer[9];
			break;
			
		case HW_COMMANDIR_MINI:
			/* CommandIR Mini only has 1 buffer  */
			pcd->commandir_tx_start[0] = 0;
			pcd->commandir_tx_start[1] = 0;
			pcd->commandir_tx_start[2] = 0;
			pcd->commandir_tx_start[3] = 0;

			pcd->commandir_tx_end[0] = commandir_data_buffer[2];
			pcd->commandir_tx_end[1] = commandir_data_buffer[2];
			pcd->commandir_tx_end[2] = commandir_data_buffer[2];
			pcd->commandir_tx_end[3] = commandir_data_buffer[2];

			pcd->commandir_last_signal_id = pcd->lastSendSignalID;
			break;
  }
  recalc_tx_available(pcd);
  pipeline_check(pcd);
}

static void set_convert_int_bitmask_to_list_of_enabled_bits(
	unsigned long * bitmask, int bitmask_len)
{
	int x, set_next_list_item, bitnum = 1;
	unsigned long tmp_mask = *bitmask;
	int * list_of_bits;
	
	list_of_bits = malloc(sizeof(int) * bitmask_len);	
	set_next_list_item = 0;
	
	for(x=0; x<(sizeof(unsigned long) * 8); x++)
	{
		if(tmp_mask & 0x01)
		{
			list_of_bits[set_next_list_item++] = bitnum;
		}
		bitnum++;
		tmp_mask = tmp_mask >> 1;
	}
	set_all_next_tx_mask(list_of_bits, set_next_list_item, *bitmask);
}



static void set_all_next_tx_mask(int * ar_new_tx_mask_list, int new_tx_len, 
	unsigned long raw_tx_mask)
{
	static int * ar_current_tx_mask_list = NULL;
	int x = 0;
	
	if(ar_current_tx_mask_list) {
		free(ar_current_tx_mask_list);
	}
	
	ar_current_tx_mask_list = malloc(sizeof(int) * new_tx_len);
	memcpy(ar_current_tx_mask_list, ar_new_tx_mask_list, 
		new_tx_len * sizeof(int));
	
	static struct commandir_device * pcd;
	int start_emitter_num = 1;
	int current_enable_these = 0;
	
	for(pcd = first_commandir_device; pcd; pcd = pcd->next_commandir_device)
	{
		pcd->num_next_enabled_emitters = 0;
		while( (ar_current_tx_mask_list[current_enable_these] < 
			start_emitter_num + pcd->num_transmitters) && 
			(current_enable_these < new_tx_len) )
		{
			pcd->next_enabled_emitters_list[pcd->num_next_enabled_emitters++] = 
				ar_current_tx_mask_list[current_enable_these++] - start_emitter_num +1;
		}
		x++;
		start_emitter_num += pcd->num_transmitters;	// 1 becomes 5, or 11
	}
}


static void raise_event(unsigned int eventid)
{
	static lirc_t event_data[18] = 
		{LIRCCODE_GAP, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int i, bytes_w;
	
	// Only for CommandIR II, and never for irrecord or mode2
	if(strncmp(progname, "mode2", 5)==0 || strncmp(progname, "irrecord", 8)==0)
	{
		return;
	}
	
	for(i=0; i<8; i++)
	{
		if( (eventid & 0x80) )
		{
			event_data[i*2+1] = signal_base[0][0];
			event_data[i*2+2] = signal_base[0][1];
		}
		else
		{
			event_data[i*2+1] = signal_base[1][0];
			event_data[i*2+2] = signal_base[1][1];
		}
		eventid = eventid << 1;
	}
	
	event_data[16] = LIRCCODE_GAP*4;
	
	bytes_w = write(child_pipe_write, event_data, sizeof(lirc_t) * 17);
	
	if (bytes_w < 0)
	{
		logprintf(LOG_ERR, "Can't write to LIRC pipe! %d", child_pipe_write);
	}
}

