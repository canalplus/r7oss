/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not   */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights, trade secrets or other intellectual property rights.   */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/*
   @file     cec_test_main.c
   @brief
*/
/* Requires   MODULE         defined on the command line */
/* Requires __KERNEL__       defined on the command line */
/* Requires __SMP__          defined on the command line for SMP systems   */
/* Requires EXPORT_SYMTAB    defined on the command line to export symbols */

#include <linux/init.h>    /* Initiliasation support */
#include <linux/module.h>  /* Module support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/kallsyms.h>
#include "stm_cec.h"
#include "infra_test_interface.h"
#include "cec_core.h"
#include "stm_event.h"
#include "cec_debug.h"
#include "cec_test.h"
#include "cec_test_ops.h"
#include "stm_display_link.h"
/*** MODULE INFO *************************************************************/

MODULE_AUTHOR("STM");
MODULE_DESCRIPTION("Test Application for CEC testing");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");

#define    CEC_TV	 0
#define    CEC_TUNER1    3
#define    CEC_TUNER2    6
#define    CEC_TUNER3    7
#define    CEC_TUNER4    10

static int MenuStatus = 0;

enum cec_test_evt_action {
	CEC_TEST_IGNORE = -1,
	CEC_TEST_READ_CEC = 1,
	CEC_TEST_HANDLE_HDMI_HPD = 2
};

/**************************************************************/
struct cec_test_control_param_s{
	stm_event_subscription_h	subscription;
	stm_event_info_t		evt_info;
	enum cec_test_evt_action	evt_action;
	stm_cec_h			cec_device;
	uint8_t				cec_address[4];
	uint8_t				logical_addr;
	stm_cec_msg_t			cur_msg;
	stm_cec_msg_t			cur_reply;
	uint8_t				reply;
	uint32_t			send_type;
	uint8_t				cur_initiater;
	uint8_t                         active_route[2];
	uint8_t				cur_phy_addr[4];
	uint8_t				exit;
	infra_os_thread_t		thread;
	infra_os_mutex_t		lock_p;
	/*This sema will be used when subscriber will call the wait_evt with a timeout>0 val.*/
	infra_os_semaphore_t		sem_p;
	uint8_t                         power_status;
	stm_display_link_h              link;
	stm_display_link_hpd_state_t    hpd_state;
};

/*** PROTOTYPES **************************************************************/
/*Entry exit functions*/
static int  cec_test_main_init_module(void);
static void cec_test_main_term_module(void);

/*****function definition for test commands**********************************/
static int cec_test_drv_init(void* data_p, void* user_data_p);
static int cec_test_allocate_logical_addr(void* data_p, void* user_data_p);
static int cec_test_send_msg(void* data_p, void* user_data_p);
static int cec_test_set_logical_addr(void* data_p, void* user_data_p);
static int cec_test_get_reg_status(void* data_p, void* user_data_p);
static int cec_test_get_msg(struct cec_test_control_param_s *control_p);
static void cec_test_report_physical(void* data_p, void* user_data_p);
static void cec_test_one_touch_play(void* data_p, void* user_data_p);
static void cec_test_standby(void* data_p, void* user_data_p);
static void cec_test_logic_addr_setup(void* data_p, void* user_data_p);
static void cec_test_send_routing_info(void* data_p, void* user_data_p);
static int cec_test_wait_on_event(struct cec_test_control_param_s *control_p);
static int __open_display_link(struct cec_test_control_param_s *control_p);
static int __get_hpd_state(struct cec_test_control_param_s *control_p);
static int __read_edid(struct cec_test_control_param_s *control_p,
			uint32_t block_num,
			stm_display_link_edid_block_t edid_block);


/*Global Parameters */

/*** GLOBAL VARIABLES *********************************************************/
struct cec_test_control_param_s		cec_test_control;
uint32_t device;
/**************************************************************/

/*Internal Functions*********************************************************/
static infra_error_code_t cec_test_entry(void);
static int cec_test_exit(void);
static void cec_test_read_task(void *data_p);
static int cec_test_app_init(void );
static int cec_test_app_term(void );
static int cec_test_subscribe_cec_evt(struct cec_test_control_param_s  *cec_test_p);
static int cec_test_process_msg(struct cec_test_control_param_s  *control_p);
static int cec_test_prepare_reply(struct cec_test_control_param_s  *control_p);
static int cec_test_unsubscribe_cec_evt(struct cec_test_control_param_s *control_p);
static int cec_test_drv_term(void* data_p, void* user_data_p);
static void cec_test_display_msg(stm_cec_msg_t *cur_msg_p, uint8_t is_reply);
static int cec_test_send_reply(struct cec_test_control_param_s  *control_p,
				uint8_t retries);
static int
cec_test_handle_hpd_event(struct cec_test_control_param_s *control_p);
/**************************************************************/


/*************************************************************************
   cec_main_init_modules
   Initialise the module
   Aquire any resources needed
   setup internal structures.
***************************************************************************/
static int __open_display_link(struct cec_test_control_param_s *control_p)
{
	int (*display_link_open)(uint32_t id, stm_display_link_h *link);

	display_link_open = (void *)kallsyms_lookup_name(
						"stm_display_link_open");
	if (display_link_open != 0) {
		if (display_link_open(0, &control_p->link) < 0) {
			cec_test_etrace(CEC_TEST_API,
					"stm_display_link_open failure\n");
			return -1;
		}
		return 0;
	} else
		cec_test_etrace(CEC_TEST_API,
				"\nHDMI module is not available\n"
				"Hence no HPD support and no EDID info\n"
				"Using hard coded physical address of:"
				"0x1 0x0 0x0 0x1\n");
	/*HDMI interfaces are not available.
	 * So run cec test without HPD support*/
	return 0;
}

static int __get_hpd_state(struct cec_test_control_param_s *control_p)
{
	int ret = 0;
	int (*hpd_get_state)(stm_display_link_h link,
			stm_display_link_hpd_state_t *state);

	hpd_get_state = (void *)kallsyms_lookup_name(
					"stm_display_link_hpd_get_state");

	if (hpd_get_state != NULL)
		ret = hpd_get_state(control_p->link, &control_p->hpd_state);

	return ret;
}

static int __read_edid(struct cec_test_control_param_s *control_p,
			uint32_t block_num,
			stm_display_link_edid_block_t edid_block)
{
	int ret = 0;
	int (*read_edid)(stm_display_link_h link,
			uint8_t block_number,
			stm_display_link_edid_block_t edid_block);

	read_edid = (void *)kallsyms_lookup_name("stm_display_link_edid_read");

	if (read_edid)
		ret = read_edid(control_p->link, 1, edid_block);

	return ret;
}

static int cec_test_app_init(void )
{
	struct cec_test_control_param_s	*control_p;
	int error;

	control_p = (struct cec_test_control_param_s*)&(cec_test_control);

	error = infra_os_sema_initialize(&(control_p->sem_p), 1);
	error |= infra_os_mutex_initialize(&(control_p->lock_p));
#ifdef CEC_DEVICE_TV
  control_p->cur_phy_addr[0] = 0;
#else
  control_p->cur_phy_addr[0] = 1;
#endif
	control_p->cur_phy_addr[1] = 0;
	control_p->cur_phy_addr[2] = 0;
	control_p->cur_phy_addr[3] = 0;
	control_p->power_status = 0;
	infra_os_thread_create( &(control_p->thread),
	cec_test_read_task,
	(void*)control_p,
	"INF-testCecRead",
	52);

	if(control_p->thread == NULL){
		return -ENOMEM;
	}
	cec_test_entry();

	return 0;
}



static int cec_test_app_term(void )
{
	struct cec_test_control_param_s	*control_p;

	control_p = (struct cec_test_control_param_s*)&(cec_test_control);
	control_p->exit=1;
	infra_os_wait_thread(&(control_p->thread));
	infra_os_sema_terminate(control_p->sem_p);
	infra_os_mutex_terminate(control_p->lock_p);
	return 0;
}

static void cec_test_read_task(void *data_p)
{
	struct cec_test_control_param_s	*control_p;
	uint8_t				sleep_time = 5;
	int				ret = 0;

	control_p = (struct cec_test_control_param_s*)&(cec_test_control);

	while(!control_p->exit){
		infra_os_sleep_milli_sec(sleep_time);
		sleep_time=5;
		ret = cec_test_wait_on_event(control_p);
		switch (control_p->evt_action) {
			case CEC_TEST_READ_CEC:
				ret = cec_test_get_msg(control_p);
				if(ret == 0)
					ret = cec_test_process_msg(control_p);
				if(ret == 0){
					cec_test_display_msg(&control_p->cur_msg,
								0);
					ret = cec_test_prepare_reply(control_p);
					cec_test_send_reply(control_p,
							ret);
					control_p->send_type = CEC_TEST_SEND_NOTHING;
				}
				break;
			case CEC_TEST_HANDLE_HDMI_HPD:
				ret = cec_test_handle_hpd_event(control_p);
				break;
			default:
				ret = -EINVAL;
				break;
		}
		if(ret!=0){
			sleep_time=100;
		}
	}
}

static int cec_test_drv_init(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	int error;

	control_p = (struct cec_test_control_param_s*)(user_data_p);

	/*Doing Cec Init */
	error=stm_cec_new(0,&(control_p->cec_device));
	cec_test_etrace(CEC_TEST_API,
			"cec_device:0x%p\n",
			control_p->cec_device);
	device = (uint32_t)control_p->cec_device;
	if(error!=0){
		pr_err(" Cec Init failure\n");
		return -1;
	}

	error = __open_display_link(control_p);
	if (error < 0)
		return -1;

	/*Subscribing for Event on cec Recieve msg*/
	error=cec_test_subscribe_cec_evt(control_p);
	if(error!=0){
		cec_test_etrace(CEC_TEST_API,
				"unable to subscribe events for cec\n");
		return -1;
	}
	cec_test_itrace(CEC_TEST_API,
			"cec_device:0x%p initialization is done\n",
			control_p->cec_device);
	return (0);  /* If we get here then we have succeeded */
}

static int cec_test_drv_term(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	int				error;

	control_p = (struct cec_test_control_param_s*)(user_data_p);

	/* Doing CEC term */
	error=stm_cec_delete(control_p->cec_device);
	control_p->cec_device = NULL;
	if(error!=0){
		pr_err(" Cec term failure\n");
		return -1;
	}
	/*Subscribing for Event on CEC Recieve msg*/
	error=cec_test_unsubscribe_cec_evt(control_p);
	if(error!=0){
		cec_test_etrace(CEC_TEST_API,
				"unable to unsubscribe events for cec\n");
		return -1;
	}
	cec_test_itrace(CEC_TEST_API,
			">>>  device termination is done sucessfully >>>\n");
	return (0);  /* If we get here then we have succeeded */
}

static int cec_test_allocate_logical_addr(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p=NULL;
	int				error=0;
	uint8_t				max_logical=0;
	uint8_t				logical_addr_arr[5];
	stm_cec_ctrl_type_t		cec_ctrl;
	stm_cec_msg_t			cec_msg;
	uint8_t				i=0;

	control_p = (struct cec_test_control_param_s*)&(cec_test_control);
	if(control_p->logical_addr!=0){
		cec_ctrl.logic_addr_param.logical_addr = control_p->logical_addr;
		cec_ctrl.logic_addr_param.enable = 0;
	    /*set the logical address into cec_ctrl structure */
	    error = cec_test_set_logical_addr(NULL, (void*)control_p);
	    if(error!=0)
            cec_test_etrace(CEC_TEST_API,
			"call to stm_cec_set_compund_control failed\n");
	}
	/* Try to get logical address by sending broadcast message */
#ifdef CEC_DEVICE_TV
	logical_addr_arr[0]=CEC_TV;
	max_logical=1;
#else
	logical_addr_arr[0]=CEC_TUNER1;
	logical_addr_arr[1]=CEC_TUNER2;
	logical_addr_arr[2]=CEC_TUNER3;
	logical_addr_arr[3]=CEC_TUNER4;
	max_logical=4;
#endif

	for(i=0;i<max_logical;i++){
		/* Preparing Broadcast Message */
		cec_msg.msg[0] = (0xF0)|(logical_addr_arr[i] & 0x0F);
		cec_msg.msg_len = 1;
		error = stm_cec_send_msg(control_p->cec_device,
				2,
				(stm_cec_msg_t *)&cec_msg);
		if(0 == error){
			control_p->logical_addr = logical_addr_arr[i];
			break;
		}else{
			cec_test_etrace(CEC_TEST_API,
					"stm_cec_send_msg failed(%d)\n",
					error);
		}
	}
	/*check if any tuner address got allocated*/
	if(error!=0)
		cec_test_etrace(CEC_TEST_API,
				"Logical address allocation failed\n");
	else{
		cec_test_itrace(CEC_TEST_API,
				"Sucessfully allocated logical address(%d)\n",
				control_p->logical_addr);
	}
	return 0;
}


static int cec_test_send_msg(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	stm_cec_msg_t			cec_msg;
	uint8_t				retries=5;
	int				error;
	int number_of_tokens;
	int src_addr;
	int des_addr;

	number_of_tokens = get_number_of_tokens((char*)data_p );
	if(number_of_tokens == 0){
		error = -EINVAL;
		cec_test_etrace(CEC_TEST_API,
				"Error:%d No Arguments Found.",
				error);
		return error;
	}
	error = get_arg(&data_p, INTEGER ,(void *)&src_addr, sizeof(int));
	if(error < 0 || src_addr > 0xF){

		pr_err("Error:%d Invalid source addr(%d)\n",
				error,
				src_addr);
		return -EINVAL;
	}
	error = get_arg(&data_p, INTEGER ,(void *)&des_addr, sizeof(int));
	if(error < 0 || des_addr > 0xF){

		pr_err("Error:%d Invalid Instance(%d)\n",
				error,
				des_addr);
		return -EINVAL;
	}

	cec_test_dtrace(CEC_TEST_API,
			">>> %s@%d >>> \n");
	control_p = (struct cec_test_control_param_s*)user_data_p;
	cec_msg.msg[0]=((src_addr << 4)&0xF0)|(0x0F & des_addr);
	cec_msg.msg[1] =0x90; /**/
	cec_msg.msg[2] = 0x0;
	cec_msg.msg_len=3;
	/*Prepare and send cec msg */
	error=stm_cec_send_msg(
			(stm_cec_h )control_p->cec_device,
			retries,
			(stm_cec_msg_t *)&cec_msg);
	if(error < 0)
		cec_test_etrace(CEC_TEST_API,
				"failed to send cec msg\n");
	return error;
}

static int cec_test_read_edid(struct cec_test_control_param_s *control_p,
				stm_display_link_edid_block_t edid_block)
{
	uint32_t idx = 0;
	uint8_t do_read = 0;

	for (idx = 0; idx < 128; idx++)
		if ((edid_block[idx] == 0x03) &&
			(edid_block[idx+1] == 0x0c) &&
			(edid_block[idx+2] == 0x0)) {
				do_read = 1;
				break;
		}
	if (do_read == 1) {
		control_p->cur_phy_addr[0] = (edid_block[idx+3] >> 4) & 0xF;
		control_p->cur_phy_addr[1] = edid_block[idx+3] & 0xF;
		control_p->cur_phy_addr[2] = (edid_block[idx+4] >> 4) & 0xF;
		control_p->cur_phy_addr[3] = edid_block[idx+4] & 0xF;
		pr_info("\r\ncec_test: Physical Address0 = 0x%02x\r\n",
				control_p->cur_phy_addr[0]);
		pr_info("cec_test: Physical Address1 = 0x%02x\r\n",
				control_p->cur_phy_addr[1]);
		pr_info("cec_test: Physical Address2 = 0x%02x\r\n",
				control_p->cur_phy_addr[2]);
		pr_info("cec_test: Physical Address3 = 0x%02x\r\n",
				control_p->cur_phy_addr[3]);
	}
	return 0;

}

static int cec_test_handle_hpd_event(struct cec_test_control_param_s *control_p)
{
	int ret = 0;
	int trial = 2;
	stm_display_link_edid_block_t edid_block;


	ret = __get_hpd_state(control_p);
	if (ret != 0 ) {
		pr_err("%s: stm_display_link_hpd_get_state \
			error 0x%08x\r\n",
			__func__,
			ret);
		return ret;
	}
	if (cec_test_control.hpd_state == STM_DISPLAY_LINK_HPD_STATE_HIGH) {
		do {
			ret = __read_edid(control_p, 1, edid_block);
			if(ret == 0)
				break;
			mdelay(100);
		} while(!trial--);
		cec_test_read_edid(control_p, edid_block );
		cec_test_control.logical_addr = 0;
		cec_test_allocate_logical_addr(NULL, control_p);
		cec_test_set_logical_addr(NULL, control_p);
		cec_test_report_physical(NULL, control_p);
		pr_info("Logical Addr : 0x%02x\r\n",
			control_p->logical_addr);
	}
	return 0;
}

static int cec_test_wait_on_event(struct cec_test_control_param_s *control_p)
{

	int			error;
	uint32_t		number_of_events;
	stm_event_info_t	*evt_info_p;

	memset(&(control_p->cur_msg),0,sizeof(stm_cec_msg_t));
	/* evt wait call for Receive event to happen */
	error = stm_event_wait(
			control_p->subscription,
			-1/*infinite wait*/,
			1,
			&number_of_events,
			&control_p->evt_info);

	if(error!=0){
		control_p->evt_action = CEC_TEST_IGNORE;
		return error;
	}

	evt_info_p = &control_p->evt_info;
	/*handle events from CEC driver*/
	if(evt_info_p->event.object == control_p->cec_device) {
		switch(evt_info_p->event.event_id) {
			case STM_CEC_EVENT_MSG_RECEIVED:
				control_p->evt_action = CEC_TEST_READ_CEC;
				break;
			default:
				control_p->evt_action = CEC_TEST_IGNORE;
				cec_test_etrace(CEC_TEST_API,
					"Unhandled Event(%d)\n",
					evt_info_p->event.event_id);
				break;
		}
	}

	if (evt_info_p->event.object == control_p->link) {
		switch(evt_info_p->event.event_id) {
			case STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT:
				control_p->evt_action =
						CEC_TEST_HANDLE_HDMI_HPD;
				break;
			default:
				control_p->evt_action = CEC_TEST_IGNORE;
				cec_test_etrace(CEC_TEST_API,
					"Unhandled Event(%d)\n",
					evt_info_p->event.event_id);
				break;
		}

	}
	return 0;

}

static int cec_test_get_msg(struct cec_test_control_param_s *control_p)
{
	int error = 0;
	stm_event_info_t        *evt_info_p;

	memset(&(control_p->cur_msg),0,sizeof(stm_cec_msg_t));
	evt_info_p = &control_p->evt_info;

	do{
		/*Consume all the CEC messages,
		 * in case there was event miss.*/
		error=stm_cec_receive_msg(control_p->cec_device,
				&(control_p->cur_msg));
	}while(!error && evt_info_p->events_missed == 1);
	/*reset*/
	error=0;

	if(control_p->cur_msg.msg_len == 0){
		cec_test_etrace(CEC_TEST_API,
				"failed to read cec msg \n");
		return -1;
	}
	return 0;
}

static void cec_test_display_msg(stm_cec_msg_t *cur_msg_p, uint8_t is_reply)
{
	uint32_t idx = 0;

	cec_test_itrace(CEC_TEST_API,
			"CEC Msg(Size:%d):\n",
			cur_msg_p->msg_len);
	if(!is_reply)
		pr_info("\033[4;32m Response:");
	else
		pr_info("\033[4;32m Reply:");

	for(; idx < cur_msg_p->msg_len; idx++) {
		pr_info("\033[4;32m0x%x ",
			cur_msg_p->msg[idx]);
	}
	pr_info("\033[0m\n");
	return;

}

static int cec_test_prepare_reply(struct cec_test_control_param_s  *control_p)
{
	stm_cec_msg_t	*cec_msg;
	stm_cec_msg_t	*cur_msg = &control_p->cur_msg;
	uint8_t		retries=2, intr_addr, foll_addr=0xF;

	control_p->reply=0;
	cec_msg = &(control_p->cur_reply);
	foll_addr = (control_p->cur_initiater & 0x0F);
	intr_addr = (control_p->logical_addr<<4)&0xF0;

	switch(control_p->send_type){
		case CEC_TEST_SEND_NOTHING:
			control_p->reply=0;
			break;
		case CEC_TEST_SEND_ABORT:
			cec_msg->msg[CEC_TEST_OPCODE_INDEX]= CEC_TEST_OPCODE_ABORT_MESSAGE; /*Abort message opcode*/
			cec_msg->msg[CEC_TEST_ADDR_INDEX]=intr_addr|foll_addr;
			cec_msg->msg_len=2;
			control_p->reply=1;
			break;
		case CEC_TEST_SEND_FEATURE_ABORT:
			cec_msg->msg[CEC_TEST_OPCODE_INDEX]= CEC_TEST_OPCODE_FEATURE_ABORT; /*Abort message opcode*/
			cec_msg->msg[CEC_TEST_ADDR_INDEX]=intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX] = cur_msg->msg[CEC_TEST_OPCODE_INDEX];
			cec_msg->msg[CEC_TEST_OPERNAD_2_INDEX]= 0x3;
			cec_msg->msg_len=4;
			control_p->reply=1;
			retries=5;
			break;
		case CEC_TEST_OPCODE_REPORT_PHYSICAL_ADDRESS:
			foll_addr = 0xF;
			cec_msg->msg[CEC_TEST_OPCODE_INDEX]= CEC_TEST_OPCODE_REPORT_PHYSICAL_ADDRESS;
			cec_msg->msg[CEC_TEST_ADDR_INDEX]=intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX]= (control_p->cur_phy_addr[0]<<4)|
								control_p->cur_phy_addr[1];
			cec_msg->msg[CEC_TEST_OPERNAD_2_INDEX]= (control_p->cur_phy_addr[2]<<4)|
								control_p->cur_phy_addr[3];
			cec_msg->msg[CEC_TEST_OPERNAD_3_INDEX]= control_p->logical_addr;
			cec_msg->msg_len=5;
			control_p->reply=1;
			break;
		case CEC_TEST_SET_OSD_NAME:
			cec_msg->msg[CEC_TEST_OPCODE_INDEX]= CEC_TEST_OPCODE_SET_OSD_NAME;
			cec_msg->msg[CEC_TEST_ADDR_INDEX]=intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX]= 'S';
			cec_msg->msg[CEC_TEST_OPERNAD_2_INDEX]= 'T';
			cec_msg->msg[CEC_TEST_OPERNAD_3_INDEX]= 'B';
			cec_msg->msg[CEC_TEST_OPERNAD_4_INDEX]= ' ';
			cec_msg->msg[CEC_TEST_OPERNAD_5_INDEX]= 'T';
			cec_msg->msg[CEC_TEST_OPERNAD_6_INDEX]= 'U';
			cec_msg->msg[CEC_TEST_OPERNAD_7_INDEX]= 'N';
			cec_msg->msg[CEC_TEST_OPERNAD_8_INDEX]= 'E';
			cec_msg->msg[CEC_TEST_OPERNAD_9_INDEX]= 'R';
			cec_msg->msg_len=11;
			control_p->reply=1;
                        break;
		case CEC_TEST_OPCODE_DEVICE_VENDOR_ID:
			foll_addr = 0xF;
			cec_msg->msg[CEC_TEST_OPCODE_INDEX]= CEC_TEST_OPCODE_DEVICE_VENDOR_ID;
			cec_msg->msg[CEC_TEST_ADDR_INDEX]=intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX]= 0x00; /* STM iee id */
			cec_msg->msg[CEC_TEST_OPERNAD_2_INDEX]= 0x80;
			cec_msg->msg[CEC_TEST_OPERNAD_3_INDEX]= 0xE1;
			cec_msg->msg_len=5;
			control_p->reply=1;
			break;
		case CEC_TEST_OPCODE_REPORT_POWER_STATUS:
			cec_msg->msg[CEC_TEST_OPCODE_INDEX]= CEC_TEST_OPCODE_REPORT_POWER_STATUS;
			cec_msg->msg[CEC_TEST_ADDR_INDEX]=intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX]= control_p->power_status;
			control_p->power_status = 0; /* for tests */
			cec_msg->msg_len=3;
			control_p->reply=1;
                        break;
		case CEC_TEST_OPCODE_ACTIVE_SOURCE:
			foll_addr = 0xF;
			cec_msg->msg[CEC_TEST_OPCODE_INDEX] = CEC_TEST_OPCODE_ACTIVE_SOURCE;
			cec_msg->msg[CEC_TEST_ADDR_INDEX] = intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX] = (control_p->cur_phy_addr[0]<<4)|
								control_p->cur_phy_addr[1];
			cec_msg->msg[CEC_TEST_OPERNAD_2_INDEX] = (control_p->cur_phy_addr[2]<<4)|
								control_p->cur_phy_addr[3];
			cec_msg->msg_len=4;
			control_p->reply=1;
			break;
		case CEC_TEST_OPCODE_CEC_VERSION:
			cec_msg->msg[CEC_TEST_OPCODE_INDEX] = CEC_TEST_OPCODE_CEC_VERSION;
			cec_msg->msg[CEC_TEST_ADDR_INDEX] = intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX] = 0x05; /* 1.4b */
			cec_msg->msg_len=3;
			control_p->reply=1;
                        break;
                case CEC_TEST_OPCODE_MENU_STATUS:
			cec_msg->msg[CEC_TEST_OPCODE_INDEX] = CEC_TEST_OPCODE_MENU_STATUS;
			cec_msg->msg[CEC_TEST_ADDR_INDEX] = intr_addr|foll_addr;
			switch(cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]){
                            case 0:
                            case 1:
                                cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX] = cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]; /* deactivated */
			        cec_msg->msg_len=3;
			        control_p->reply=1;
                                MenuStatus = cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX];
                                break;
                            case 2:
                                cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX] = MenuStatus; /* deactivated */
			        cec_msg->msg_len=3;
			        control_p->reply=1;
                                break;
                            default:
			        control_p->reply=0;
                                break;
                        }
                        break;
		case CEC_TEST_OPCODE_SET_MENU_LANGUAGE:
			foll_addr = 0xF;
			cec_msg->msg[CEC_TEST_OPCODE_INDEX] = CEC_TEST_OPCODE_SET_MENU_LANGUAGE;
			cec_msg->msg[CEC_TEST_ADDR_INDEX] = intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX] = 'e'; /* english */
			cec_msg->msg[CEC_TEST_OPERNAD_2_INDEX] = 'n';
			cec_msg->msg[CEC_TEST_OPERNAD_3_INDEX] = 'g';
			cec_msg->msg_len=5;
			control_p->reply=1;
			break;
		case CEC_TEST_OPCODE_STANDBY:
			foll_addr = 0xF;
			cec_msg->msg[CEC_TEST_OPCODE_INDEX] = CEC_TEST_OPCODE_STANDBY;
			cec_msg->msg[CEC_TEST_ADDR_INDEX] = intr_addr|foll_addr;
			cec_msg->msg_len=2;
			control_p->reply=1;
			break;
		case CEC_TEST_OPCODE_ROUTING_INFORMATION:
			foll_addr = 0xF;
			cec_msg->msg[CEC_TEST_OPCODE_INDEX] = CEC_TEST_OPCODE_ROUTING_INFORMATION;
			cec_msg->msg[CEC_TEST_ADDR_INDEX] = intr_addr|foll_addr;
			cec_msg->msg[CEC_TEST_OPERNAD_1_INDEX] = control_p->active_route[0];
			cec_msg->msg[CEC_TEST_OPERNAD_2_INDEX] = control_p->active_route[1];
			cec_msg->msg_len=4;
			control_p->reply=1;
			break;

		default:
			control_p->reply=0;
			cec_test_etrace(CEC_TEST_API,"WRONG OPCODE\n");
			break;

	}
	return retries;
}

static int cec_test_send_reply(struct cec_test_control_param_s  *control_p,
				uint8_t retries)
{
	stm_cec_msg_t	*cec_msg;
	int		error=0;

	cec_msg = &(control_p->cur_reply);
	if(control_p->reply==1){
		cec_test_display_msg(cec_msg, 1);

		error=stm_cec_send_msg((stm_cec_h )control_p->cec_device,
				retries,
				cec_msg);
		if(error!=0)
			pr_err("%s:: failed(%d) to send cec msg\n",
				__func__,
				error);
		else
			cec_test_dtrace(CEC_TEST_API, "\n");
	}
	return error;
}


static int cec_test_process_msg(struct cec_test_control_param_s  *control_p)
{
	uint8_t int_addr, foll_addr, opcode, data[15];
	stm_cec_msg_t *cur_msg = &control_p->cur_msg;
	int error= 0;
	uint8_t print_msg=0;
	uint8_t phy_addr_msb, phy_addr_lsb;

	control_p->send_type = CEC_TEST_SEND_NOTHING;
	int_addr = (cur_msg->msg[CEC_TEST_ADDR_INDEX]&0xF0)>>4;
	foll_addr = (cur_msg->msg[CEC_TEST_ADDR_INDEX]&0xF);
	control_p->cur_initiater = int_addr;

	phy_addr_msb = ((control_p->cur_phy_addr[0]<<4)|
			control_p->cur_phy_addr[1]);
	phy_addr_lsb = ((control_p->cur_phy_addr[2]<<4)|
			control_p->cur_phy_addr[3]);
	opcode = (cur_msg->msg[CEC_TEST_OPCODE_INDEX]);

    switch (opcode)
     {
         case CEC_TEST_OPCODE_ABORT_MESSAGE                   :
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_ABORT_MESSAGE\n");
             if(foll_addr==control_p->logical_addr){
	 control_p->send_type = CEC_TEST_SEND_FEATURE_ABORT;
             }else{
                 cec_test_dtrace(CEC_TEST_API,"Invalid Addr\n");
             }
             break;
         case CEC_TEST_OPCODE_STANDBY                         :
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_STANDBY\n");
	     control_p->power_status = 1; /* standby */
	     pr_info("STB goes to standby...\r\n");
	 break;
         case CEC_TEST_OPCODE_GIVE_DEVICE_POWER_STATUS        :
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_GIVE_DEVICE_POWER_STATUS\n");
	     if(foll_addr == 0xF){
		     cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_GIVE_DEVICE_POWER_STATUS bad destination address\n");
		     error = -EINVAL;
	     }else{
		     cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_GIVE_DEVICE_POWER_STATUS\n");
		     control_p->send_type = CEC_TEST_OPCODE_REPORT_POWER_STATUS;
	     }
             break;
	 case CEC_TEST_OPCODE_SET_STREAM_PATH                 :
	     print_msg=1;
	     if(((cur_msg->msg_len-1) < 3) || (foll_addr != 0xF)){
		     cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_SET_STREAM_PATH bad or missing parameters!\n");
		     error = -EINVAL;
		     break;
	     }else{
		     data[0] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]);
		     data[1] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]);
		     if((data[0] != phy_addr_msb ) ||
				     (data[1] != phy_addr_lsb))
			     break;
		     cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_SET_STREAM_PATH:0x%x 0x%x\n",data[0],data[1]);
	     }
         case CEC_TEST_OPCODE_REQUEST_ACTIVE_SOURCE           :
             if(foll_addr != 0xF){
                 cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_REQUEST_ACTIVE_SOURCE bad destination address\n");
                 error = -EINVAL;
             }else{
                 cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_REQUEST_ACTIVE_SOURCE\n");
                 control_p->send_type = CEC_TEST_OPCODE_ACTIVE_SOURCE;
             }
             break;
         case CEC_TEST_OPCODE_GIVE_OSD_NAME :
	     if((foll_addr == 0xF) || (int_addr == 0xF)){
		     cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_GIVE_OSD_NAME bad initiator/follower address\n");
		     error = -EINVAL;
	     }else{
		     cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_GIVE_OSD_NAME \n");
		     control_p->send_type = CEC_TEST_SET_OSD_NAME;
	     }
             break;
         case CEC_TEST_OPCODE_SET_OSD_NAME:
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_SET_OSD_NAME\n");
             break;
         case CEC_TEST_OPCODE_GET_CEC_VERSION                 :
	     if(foll_addr == 0xF){
		     cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_GET_CEC_VERSION bad destination address\n");
		     error = -EINVAL;
	     }else{
		     cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_GET_CEC_VERSION\n");
		     control_p->send_type = CEC_TEST_OPCODE_CEC_VERSION;
	     }
	     break;
         case CEC_TEST_OPCODE_GIVE_PHYSICAL_ADDRESS           :
	     if(foll_addr == 0xF){
		     cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_GIVE_PHYSICAL_ADDRESS bad destination address\n");
		     error = -EINVAL;
	     }else{
		     cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_GIVE_PHYSICAL_ADDRESS\n");
		     control_p->send_type = CEC_TEST_OPCODE_REPORT_PHYSICAL_ADDRESS;
	     }
	 break;
         case CEC_TEST_OPCODE_MENU_REQUEST               :
             if((cur_msg->msg_len -1 < 1) || (foll_addr == 0xF)){
                cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_MENU_REQUEST bad destination address\n");
                error = -EINVAL;
             }else{
                cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_MENU_REQUEST\n");
		control_p->send_type = CEC_TEST_OPCODE_FEATURE_ABORT;
             }
             break;
         case CEC_TEST_OPCODE_GET_MENU_LANGUAGE               :
             if(foll_addr == 0xF){
                cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_GET_MENU_LANGUAGE bad destination address\n");
                error = -EINVAL;
             }else{
                cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_GET_MENU_LANGUAGE\n");
                control_p->send_type = CEC_TEST_OPCODE_SET_MENU_LANGUAGE;
             }
	 break;
         case CEC_TEST_OPCODE_SET_MENU_LANGUAGE				  :
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_SET_MENU_LANGUAGE\n");
             break;
         case CEC_TEST_OPCODE_VENDOR_COMMAND           :
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_VENDOR_COMMAND\n");
             break;
         case CEC_TEST_OPCODE_GIVE_DEVICE_VENDOR_ID           :
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_GIVE_DEVICE_VENDOR_ID\n");
             control_p->send_type = CEC_TEST_OPCODE_DEVICE_VENDOR_ID;
             break;
         case CEC_TEST_OPCODE_DEVICE_VENDOR_ID           :
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_DEVICE_VENDOR_ID\n");
             break;
         case CEC_TEST_OPCODE_VENDOR_COMMAND_WITH_ID           :
             cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_VENDOR_COMMAND_WITH_ID\n");
             break;
	 case CEC_TEST_OPCODE_ACTIVE_SOURCE                   :
	     if(cur_msg->msg_len -1 < 2){
		cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_ACTIVE_SOURCE missing parameters!\n");
		error = -EINVAL;
	     }
	     else{
		data[0] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]);
		data[1] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]);
		if(((data[0]>>4) & 0xF) == control_p->cur_phy_addr[0]) {
		   if(control_p->cur_phy_addr[1] == 0) {
                      if((data[0] & 0xF) > control_p->cur_phy_addr[1])
                          control_p->send_type = CEC_TEST_OPCODE_ROUTING_INFORMATION;
                      else {
                          if(control_p->cur_phy_addr[2] == 0) {
                             if(((data[1]>>4) & 0xF) > control_p->cur_phy_addr[2])
                                 control_p->send_type = CEC_TEST_OPCODE_ROUTING_INFORMATION;
                             else {
                                  if(control_p->cur_phy_addr[3] == 0)
                                     if((data[1] & 0xF) > control_p->cur_phy_addr[3])
                                         control_p->send_type = CEC_TEST_OPCODE_ROUTING_INFORMATION;
                             }
			  }
                      }
		   }
		 }
                control_p->active_route[0] = data[0];
		control_p->active_route[1] = data[1];
                cec_test_dtrace(CEC_TEST_API,
				     "CEC_TEST_OPCODE_ACTIVE_SOURCE:0x%x \
				     0x%x\n",data[0],data[1]);
	     }
             break;

         case CEC_TEST_OPCODE_INACTIVE_SOURCE                 :
	 if(!print_msg)
                 cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_INACTIVE_SOURCE\n");
	 print_msg=1;
             break;
         case CEC_TEST_OPCODE_ROUTING_INFORMATION             :
             if(foll_addr != 0xF){
                cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_ROUTING_INFORMATION bad destination address\n");
                error = -EINVAL;
             }else{
	           if(cur_msg->msg_len -1 < 2){
	               error = -1;
	           }else{
	               data[0] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]>>4) & 0x0F;
	               data[1] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]) & 0x0F;
	               data[2] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]>>4) & 0x0F;
	               data[3] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]) & 0x0F;
	           }
             }
             break;

         case CEC_TEST_OPCODE_ROUTING_CHANGE                 :
             if(foll_addr != 0xF){
                cec_debug_trace(CEC_API,"CEC_TEST_OPCODE_ROUTING_CHANGE bad destination address\n");
                error = -EINVAL;
             }else{
                cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_ROUTING_CHANGE\n");
                control_p->send_type = CEC_TEST_SEND_NOTHING;
               if(cur_msg->msg_len -1 < 2){
                   error = -1;
               }else{
                    data[0] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]>>4) & 0x0F;
                    data[1] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]) & 0x0F;
                    data[2] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]>>4) & 0x0F;
                    data[3] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]) & 0x0F;
                    data[4] = (cur_msg->msg[CEC_TEST_OPERNAD_3_INDEX]>>4) & 0x0F;
                    data[5] = (cur_msg->msg[CEC_TEST_OPERNAD_3_INDEX]) & 0x0F;
                    data[6] = (cur_msg->msg[CEC_TEST_OPERNAD_4_INDEX]>>4) & 0x0F;
                    data[7] = (cur_msg->msg[CEC_TEST_OPERNAD_4_INDEX]) & 0x0F;
               }
             }
            break;

	 case CEC_TEST_OPCODE_REPORT_PHYSICAL_ADDRESS :
	    cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_REPORT_PHYSICAL_ADDRESS\n");
	    if(cur_msg->msg_len -1 < 3){
		    error = -EINVAL;
	    }else{
		    data[0] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]>>4) & 0x0F;
		    data[1] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]) & 0x0F;
		    data[2] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]>>4) & 0x0F;
		    data[3] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]) & 0x0F;
		    data[4] = (cur_msg->msg[CEC_TEST_OPERNAD_3_INDEX]>>4) & 0x0F;
	    }
	    break;

         case CEC_TEST_OPCODE_CEC_VERSION                    :
             if(cur_msg->msg_len -1 < 1){
		 error = -EINVAL;
             }else{
	 data[0] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]);
                 cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_CEC_VERSION:%d\n",data[0]);
                 if(data[0] > 0x04){
	 error = -EINVAL;
                 }
             }
             break;

         case CEC_TEST_OPCODE_REPORT_POWER_STATUS             :
             if(cur_msg->msg_len -1 < 1){
		 error = -EINVAL;
              }else{
	  data[0] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]);
                 cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_REPORT_POWER_STATUS:0x%x\n",data[0]);
                 if(data[0] > 0x03){
	 error = -EINVAL;
                 }
             }
             break;

         case CEC_TEST_OPCODE_FEATURE_ABORT                  :
             if(cur_msg->msg_len -1 < 3){
		 error = -EINVAL;
               }else{
	   data[0] = (cur_msg->msg[CEC_TEST_OPERNAD_1_INDEX]);
	   data[1] = (cur_msg->msg[CEC_TEST_OPERNAD_2_INDEX]);
                   cec_test_dtrace(CEC_TEST_API,"CEC_TEST_OPCODE_FEATURE_ABORT:0x%x 0x%x\n",data[0],data[1]);
             }
             break;

     }
    return 0;
}


static int cec_test_set_logical_addr(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	int				error;
	stm_cec_ctrl_type_t		cec_ctrl;

	control_p = (struct cec_test_control_param_s*)user_data_p;
	cec_ctrl.logic_addr_param.logical_addr = control_p->logical_addr;
	cec_ctrl.logic_addr_param.enable = 1;

	/*set the logical address into cec_ctrl structure */
	error=stm_cec_set_compound_control((stm_cec_h )device ,
			STM_CEC_CTRL_FLAG_UPDATE_LOGICAL_ADDR,
			&cec_ctrl);
	if(error!=0)
		cec_test_etrace(CEC_TEST_API,
		"call to stm_cec_set_compund_control failed\n");

	memset(&cec_ctrl, 0, sizeof(stm_cec_ctrl_type_t));
	error=stm_cec_get_compound_control((stm_cec_h )device ,
			STM_CEC_CTRL_FLAG_UPDATE_LOGICAL_ADDR,
			&cec_ctrl);
	if(error!=0)
		cec_test_etrace(CEC_TEST_API,
		"call to stm_cec_get_compund_control failed\n");

	cec_test_dtrace(CEC_TEST_API, "Logical Addr: 0x%x\n",
			cec_ctrl.logic_addr_param.logical_addr);

	cec_test_dtrace(CEC_TEST_API,
	">>>  Sucessfully cec_set_logical_address  is done >>>\n");

    return error;
}


static int cec_test_get_control(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	int				error;
	stm_cec_ctrl_type_t		cec_ctrl;

	control_p = (struct cec_test_control_param_s*)&(user_data_p);

	/*get the logical address from cec_ctrl structure */
	error=stm_cec_get_compound_control(
				(stm_cec_h )control_p->cec_device,
				STM_CEC_CTRL_FLAG_UPDATE_LOGICAL_ADDR,
				&cec_ctrl);
	if(error==0)
		cec_test_dtrace(CEC_TEST_API, "Logical Address = %d",
					cec_ctrl.logic_addr_param.logical_addr);
	else
		cec_test_dtrace(CEC_TEST_API,
				"stm_cec_get_compound_control failed(%d)\n",
				error);
	return 0;
}

static int cec_test_subscribe_cec_evt(struct cec_test_control_param_s *control_p)
{
	int				error;
	uint32_t			number_of_entries=0;
	stm_event_subscription_entry_t	subscription_entry[2];
	stm_event_subscription_entry_t  *subscription_p;

	subscription_p = &subscription_entry[number_of_entries];
	subscription_p->cookie = control_p;
	subscription_p->object = control_p->cec_device;
	subscription_p->event_mask = STM_CEC_EVENT_MSG_RECEIVED |
					STM_CEC_EVENT_MSG_RECEIVED_ERROR;
	number_of_entries++;

	subscription_p = &subscription_entry[number_of_entries];
	subscription_p->cookie = control_p;
	subscription_p->object = (stm_object_h)control_p->link;
	subscription_p->event_mask = STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT;
	number_of_entries++;

	error = stm_event_subscription_create(subscription_entry,
						number_of_entries,
						&(control_p->subscription));

	cec_test_dtrace(CEC_TEST_API,
			"stm_event_subscription_create status(%d)\n",
			error);
	return error;
}

static int cec_test_unsubscribe_cec_evt(struct cec_test_control_param_s *control_p)
{
	int	error;

	error = stm_event_subscription_delete(control_p->subscription);
	cec_test_dtrace(CEC_TEST_API,
			"stm_event_subscription_delete status(%d)\n",
			error);
	return error;
}

static infra_error_code_t cec_test_get_reg_status(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	volatile uint16_t		reg_data;
	uint8_t				idx=0;

	control_p = (struct cec_test_control_param_s*)&(user_data_p);

	cec_test_etrace(CEC_TEST_API,
			">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\
			>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	for(;idx<0x64;){
		reg_data = INFRA_READ_REG_WORD(
				(((stm_cec_h)device)->base_addr_v
				+ idx));
	        cec_test_dtrace(CEC_TEST_API, "Reg(0x%x): 0x%x\n", idx, reg_data);
		idx+=0x4;
	}
	cec_test_etrace(CEC_TEST_API, "<<<<<<<<<<<<<<<<<<<<<<<<<<<\
				<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
	return 0;
}
/*Test command functions*/
static void cec_test_report_physical(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	stm_cec_msg_t			cec_msg;
	uint8_t				retries=2, intr_addr, foll_addr=0xF;
	int				error;

	control_p = (struct cec_test_control_param_s*)user_data_p;
	foll_addr = (foll_addr & 0x0F);
	intr_addr = (control_p->logical_addr<<4)&0xF0;
	cec_msg.msg[0]=intr_addr|foll_addr;
	cec_msg.msg[1]= CEC_TEST_OPCODE_REPORT_PHYSICAL_ADDRESS;
	cec_msg.msg[2]= (control_p->cur_phy_addr[0]<<4)|
			control_p->cur_phy_addr[1];
	cec_msg.msg[3]= (control_p->cur_phy_addr[2]<<4)|
			control_p->cur_phy_addr[3];
	cec_msg.msg[4]= 3; /* Tuner type */
	cec_msg.msg_len=5;

	/*Prepare and send cec msg */
	error=stm_cec_send_msg((stm_cec_h )control_p->cec_device,
			retries,
			(stm_cec_msg_t *)&cec_msg);
	if(error!=0)
		pr_err("%s:: failed(%d) to send cec msg\n",
			__func__,
			error);
	else
		cec_test_dtrace(CEC_TEST_API, "Physical Address reported\n");
	return ;
}

static void cec_test_one_touch_play(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	stm_cec_msg_t			cec_msg;
	uint8_t				retries=2, intr_addr, foll_addr=0;
	int					error;

	control_p = (struct cec_test_control_param_s*)user_data_p;
	foll_addr = (foll_addr & 0x0F);
	intr_addr = (control_p->logical_addr<<4)&0xF0;
	cec_msg.msg[0]=intr_addr|foll_addr;
	cec_msg.msg[1]= CEC_TEST_OPCODE_IMAGE_VIEW_ON;
	cec_msg.msg_len=2;

	/*Prepare and send cec msg */
	error=stm_cec_send_msg((stm_cec_h )control_p->cec_device,
			retries,
			(stm_cec_msg_t *)&cec_msg);
	if(error!=0)
		pr_err("%s:: failed(%d) to send cec msg\n",
			__func__,
			error);
	else
		cec_debug_trace(CEC_API, "Image view on\n");

	foll_addr = 0x0F;
	intr_addr = (control_p->logical_addr<<4)&0xF0;
	cec_msg.msg[0]=intr_addr|foll_addr;
	cec_msg.msg[1]= CEC_TEST_OPCODE_ACTIVE_SOURCE;
	cec_msg.msg[2]= (control_p->cur_phy_addr[0]<<4)|
			control_p->cur_phy_addr[1];
	cec_msg.msg[3]= (control_p->cur_phy_addr[2]<<4)|
			control_p->cur_phy_addr[3];
	cec_msg.msg_len=4;

	/*Prepare and send cec msg */
	error=stm_cec_send_msg((stm_cec_h )control_p->cec_device,
			retries,
			(stm_cec_msg_t *)&cec_msg);
	if(error!=0)
		pr_err("%s:: failed(%d) to send cec msg\n",
			__func__,
			error);
	else
		cec_debug_trace(CEC_API, "Active source\n");

	return ;
}

static void cec_test_standby(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	stm_cec_msg_t			cec_msg;
	uint8_t				retries=2, intr_addr, foll_addr=0;
	int					error;

	control_p = (struct cec_test_control_param_s*)user_data_p;
	foll_addr = 0x0F;
	intr_addr = (control_p->logical_addr<<4)&0xF0;
	cec_msg.msg[0]=intr_addr|foll_addr;
	cec_msg.msg[1]= CEC_TEST_OPCODE_STANDBY;
	cec_msg.msg_len=2;

	/*Prepare and send cec msg */
	error=stm_cec_send_msg((stm_cec_h )control_p->cec_device,
				retries,
				(stm_cec_msg_t *)&cec_msg);
	if(error!=0)
		pr_err("%s:: failed(%d) to send cec msg\n",
				__func__,
				error);
	else
		cec_debug_trace(CEC_API, "Standby\n");

	return ;
}

static void cec_test_send_routing_info(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;
	stm_cec_msg_t			cec_msg;
	uint8_t				retries=2, intr_addr, foll_addr=0xF;
	int				error;

	control_p = (struct cec_test_control_param_s*)user_data_p;
	foll_addr = (foll_addr & 0x0F);
	intr_addr = (control_p->logical_addr<<4)&0xF0;
	cec_msg.msg[0]=intr_addr|foll_addr;
	cec_msg.msg[1]= CEC_TEST_OPCODE_ROUTING_INFORMATION;
	cec_msg.msg[2]= 0x10;
	cec_msg.msg[3]= 0x00;
	cec_msg.msg_len=4;

	/*Prepare and send cec msg */
	error=stm_cec_send_msg((stm_cec_h )control_p->cec_device,
			retries,
			(stm_cec_msg_t *)&cec_msg);
	if(error!=0)
		cec_test_etrace(CEC_TEST_API, "failed to send cec msg\n");
	else
		cec_test_dtrace(CEC_TEST_API,
				"ROUTING_INFORMATION reported\n");
	return ;
}


static void cec_test_logic_addr_setup(void* data_p, void* user_data_p)
{
	struct cec_test_control_param_s	*control_p;

	control_p = (struct cec_test_control_param_s*)user_data_p;

	cec_test_itrace(CEC_TEST_API,
			"Allocating Logical Address\n");
	cec_test_allocate_logical_addr(data_p, user_data_p);

	cec_test_itrace(CEC_TEST_API,
			"Reporting Physical Address\n");
	cec_test_report_physical(data_p, user_data_p);

	cec_test_itrace(CEC_TEST_API,
			"Setting Logical Address on the H/W\n");
	cec_test_set_logical_addr(data_p, user_data_p);

}

static infra_error_code_t cec_test_entry(void)
{
	infra_test_reg_param_t		cmd_param;
	cmd_param.user_data_p = (void*)&(cec_test_control);

	strlcpy(cmd_param.info ,
			"init cec test module",
			(INFRA_TEST_MAX_CMD_DESC_LEN-1));
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_INIT",
			cec_test_drv_init,
			"init cec test module");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_TERM",
			cec_test_drv_term,
			"term cec test module");
	strlcpy(cmd_param.info ,
			"Set logical address",
			(INFRA_TEST_MAX_CMD_DESC_LEN-1));
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_SET_LOGICAL_ADDR",
			cec_test_set_logical_addr,
			"Set logical address");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_REPORT_PHYSICAL",
			cec_test_report_physical,
			"report physical addr");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_SEND_ROUTING_INFOu",
			cec_test_send_routing_info,
			"send routing info");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_SEND_MSG",
			cec_test_send_msg,
			"send CEC msg on Cec line");
	INFRA_TEST_REG_CMD(cmd_param,
			"GET_CTRL_STRUCT",
			cec_test_get_control,
			"Get logical address");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_REG_STAT",
			cec_test_get_reg_status,
			"cec reg status");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_ALLOC_LOGICAL",
			cec_test_allocate_logical_addr,
			"Allocate logical addr");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_DO_LOGIC_SETUP",
			cec_test_logic_addr_setup,
			"Do complete setup for logical addr");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_ONE_TOUCH_PLAY",
			cec_test_one_touch_play,
			"one touch play");
	INFRA_TEST_REG_CMD(cmd_param,
			"CEC_SEND_STANDBY",
			cec_test_standby,
			"standby broadcast");

	return INFRA_NO_ERROR;
}


static int cec_test_exit(void)
{
    infra_test_reg_param_t		cmd_param;
    INFRA_TEST_DEREG_CMD(cmd_param, cec_test_drv_init);
    INFRA_TEST_DEREG_CMD(cmd_param, cec_test_set_logical_addr);
    INFRA_TEST_DEREG_CMD(cmd_param, cec_test_report_physical);
    INFRA_TEST_DEREG_CMD(cmd_param, cec_test_get_control);
    INFRA_TEST_DEREG_CMD(cmd_param, cec_test_send_msg);
    INFRA_TEST_DEREG_CMD(cmd_param, cec_test_allocate_logical_addr);
    INFRA_TEST_DEREG_CMD(cmd_param, cec_test_one_touch_play);
    INFRA_TEST_DEREG_CMD(cmd_param, cec_test_standby);

    return 0;
}

/*** MODULE LOADING ******************************************************/

void cec_test_modprobe_func(void)
{
	pr_info("This is INFRA's CEC TEST Module\n");
}
EXPORT_SYMBOL(cec_test_modprobe_func);

/*=============================================================================
  cec_main_term_module

  Realease any resources allocaed to this module before the module is
  unloaded.
  ===========================================================================*/
static void __exit cec_test_main_term_module(void)
{
	int error =0;
	struct cec_test_control_param_s *control_p = &cec_test_control;

	cec_test_exit();
	if (control_p->cec_device != NULL) {
		error = stm_cec_delete((stm_cec_h)control_p->cec_device);
		if(error!=0)
			cec_test_etrace(CEC_TEST_API,
					"cec deletion failed(%d)\n",
					error);
	}
	/*Unsubscribing for Events*/
	error = cec_test_unsubscribe_cec_evt(control_p);
	if (error != 0)
		cec_test_etrace(CEC_TEST_API,
				"unable to unsubscribe events for cec\n");

	cec_test_app_term();
	pr_info("Unload module cec_test_main by %s (pid %i)\n",
			current->comm,
			current->pid);
}

static int __init cec_test_main_init_module(void)
{
	cec_test_app_init();
	cec_test_dtrace(CEC_TEST_API,
			">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
	return (0);  /* If we get here then we have succeeded */
}
/* Tell the module system these are our entry points. */

module_init(cec_test_main_init_module);
module_exit(cec_test_main_term_module);
