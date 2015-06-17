/*****************************************************************************/
/* COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided AS IS, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   evt_test_signalerr.c
 @brief



*/

#include <linux/random.h>
#include <linux/string.h>

#include <linux/netfilter.h>	/* For netfilter */
#include <linux/netfilter_ipv4.h>
#include <linux/skbuff.h>

#include "evt_test_signaler.h"
#include "infra_token.h"


#define EVT_TEST_ENABLE_SIG_ALL		0
#define EVT_TEST_ENABLE_SIG_1		1
#define EVT_TEST_ENABLE_SIG_2		1
#define EVT_TEST_ENABLE_SIG_3		1
#define MAX_INSTANCES			EVT_TEST_SIG_TYPE_MAX
#define EVT_TEST_NAME_SIZE		100
#define EVT_TEST_MAP_TABLE_SIZE		100

static infra_dev_name_t evt_signaler_dev_name[] = {
	"EVT_TEST_SIG_1",
	"EVT_TEST_SIG_2",
	"EVT_TEST_SIG_3"
};

struct evt_test_integer_string_map_s {
	char string[EVT_TEST_NAME_SIZE];
	unsigned int val;
};

static uint32_t evt_test_signaler_handle_store[EVT_TEST_SIG_TYPE_MAX];
static struct evt_test_integer_string_map_s map_arr[EVT_TEST_MAP_TABLE_SIZE];
static unsigned int total_map_entries;

#if EVT_TEST_ENABLE_SIG_ALL
#if EVT_TEST_ENABLE_SIG_1
static evt_test_signaler_t		*signaler_1_control_p;
#endif
#if EVT_TEST_ENABLE_SIG_2
static evt_test_signaler_t		*signaler_2_control_p;
#endif
#if EVT_TEST_ENABLE_SIG_3
static evt_test_signaler_t		*signaler_3_control_p;
#endif
#endif

static void evt_test_signaler_task(void *);

static void evt_test_sig_generate_evt(evt_test_signaler_t *control_p);

static int evt_test_init_sig(void *, void *);
static int evt_test_term_sig(void *, void *);
static int evt_test_signal_sig(void *, void *);
static int evt_test_start_sig(void *data_p, void *usr_data);
static int evt_test_stop_sig(void *, void *);


#if EVT_TEST_ENABLE_SIG_ALL
static int evt_test_signaler_init_all(void);
static int evt_test_signaler_term_all(void);
#endif

static int evt_test_sig_term(evt_test_signaler_t *control_p);
static int evt_test_sig_init(evt_test_signaler_t *control_p);
static unsigned int evt_test_ip_v4_nethook(unsigned int hooknum,
					   struct sk_buff *ip_fe_buffer,
					   const struct net_device *in_p,
					   const struct net_device *out_p,
					   int (*okfn)(struct sk_buff *));
static int evt_test_sig_install_intr(void *, void *);
static int evt_test_sig_uninstall_intr(void *, void *);
static int insert_map_entry(const char *, int);
static int get_integer_from_map(const char *str);
static char *get_string_from_map(int);
static int intialize_map_table(void);


static int insert_map_entry(const char *str, int val)
{
	struct evt_test_integer_string_map_s *cur_free_entry_p;

	if (total_map_entries < EVT_TEST_MAP_TABLE_SIZE) {
		cur_free_entry_p = &map_arr[total_map_entries];
		cur_free_entry_p->val = val;
		strncpy(cur_free_entry_p->string,
			      str,
			      EVT_TEST_NAME_SIZE);
		total_map_entries++;
		return 0;
	}
	return -1;
}

static int get_integer_from_map(const char *str)
{
	int index = 0;
	struct evt_test_integer_string_map_s *cur_free_entry_p;

	for (; index < total_map_entries; index++) {
		cur_free_entry_p = &map_arr[index];
		if (strcmp(cur_free_entry_p->string,
			str) == 0) {
			return cur_free_entry_p->val;
		}
	}
	return -1;
}


static char *get_string_from_map(int val)
{
	int index = 0;
	struct evt_test_integer_string_map_s *cur_free_entry_p;

	for (; index < total_map_entries; index++) {
		cur_free_entry_p = &map_arr[index];
		if (cur_free_entry_p->val == val)
			return cur_free_entry_p->string;

	}
	return NULL;
}

static int intialize_map_table(void)
{
	int val;

	insert_map_entry("EVT_TEST_ID_A", 0x1);
	insert_map_entry("EVT_TEST_ID_B", (1 << 2));
	insert_map_entry("EVT_TEST_ID_C", (1 << 3));
	insert_map_entry("EVT_TEST_ID_D", (1 << 4));
	insert_map_entry("EVT_TEST_ID_E", (1 << 5));
	insert_map_entry("EVT_TEST_ID_G", (1 << 6));
	insert_map_entry("EVT_TEST_ID_H", (1 << 7));
	insert_map_entry("EVT_TEST_ID_I", (1 << 8));
	insert_map_entry("EVT_TEST_ID_J", (1 << 9));
	insert_map_entry("EVT_TEST_ID_K", (1 << 10));
	insert_map_entry("EVT_TEST_ID_IPV4", (1 << 11));

	val = get_integer_from_map("EVT_TEST_ID_IPV4");
	return 0;
}


int evt_test_sig_set_state(evt_test_signaler_t *control_p,
			   evt_test_sig_task_state_t state)
{
	int		error = INFRA_NO_ERROR;
	evt_test_sig_task_state_t	cur_state;

	infra_os_mutex_lock(control_p->lock_p);
	cur_state = control_p->cur_state;
	infra_os_mutex_unlock(control_p->lock_p);
	evt_test_dtrace(EVT_TEST_L1, " Cur:%d Desired:%d\n", cur_state, state);

	switch (state) {
	case EVT_TEST_SIG_TASK_STATE_INIT:
		if (cur_state != EVT_TEST_SIG_TASK_STATE_TERM)
			error = -EINVAL;

		break;
	case EVT_TEST_SIG_TASK_STATE_START:
		if ((cur_state != EVT_TEST_SIG_TASK_STATE_STOP) &&
			(cur_state != EVT_TEST_SIG_TASK_STATE_INIT))
			error = -EINVAL;

		break;
	case EVT_TEST_SIG_TASK_STATE_TERM:
		evt_test_dtrace(EVT_TEST_L1, "\n");
		break;
	case EVT_TEST_SIG_TASK_STATE_STOP:
		evt_test_dtrace(EVT_TEST_L1, "\n");
		if ((cur_state != EVT_TEST_SIG_TASK_STATE_START))
			error = -EINVAL;

		break;
	default:
		error = -EINVAL;
		break;
	}

	if (error == INFRA_NO_ERROR) {
		infra_os_mutex_lock(control_p->lock_p);
		control_p->next_state = state;
		infra_os_mutex_unlock(control_p->lock_p);
		infra_os_sema_signal(control_p->sem_p);
		evt_test_dtrace(EVT_TEST_L1, "\n");
	}

	return error;
}

void evt_test_signaler_task(void *data_p)
{
	evt_test_signaler_t		*control_p;
	int		error = INFRA_NO_ERROR;
	uint32_t			timeout;
	uint32_t			time_now;
	control_p = (evt_test_signaler_t *) data_p;


	timeout = ((100 * INFRA_OS_GET_TICKS_PER_SEC) / 1000);
	control_p->timeout_in_millisec = timeout;
	control_p->timeout_in_millisec = INFRA_OS_INFINITE;
	evt_test_dtrace(EVT_TEST_L2,
		      " Ticks:%u\n",
		      (INFRA_OS_GET_TICKS_PER_SEC * 1000 / 1000));


	while (1) {
		error = 0;
		time_now = infra_os_get_time_in_milli_sec();
		evt_test_dtrace(EVT_TEST_L2,
			      " E:%d Time(%u) Tout:%u\n",
			      error,
			      time_now,
			      (time_now + control_p->timeout_in_millisec));
		error = infra_os_sema_wait_timeout(control_p->sem_p,
			      control_p->timeout_in_millisec);
		evt_test_dtrace(EVT_TEST_L2,
			      " E:%d Time(%u) T:%u\n",
			      error,
			      time_now,
			      infra_os_get_time_in_milli_sec());
		infra_os_mutex_lock(control_p->lock_p);
		if (control_p->cur_state != control_p->next_state)
			control_p->cur_state = control_p->next_state;

		infra_os_mutex_unlock(control_p->lock_p);
		control_p->timeout_in_millisec = INFRA_OS_INFINITE;
		switch (control_p->cur_state) {
		case EVT_TEST_SIG_TASK_STATE_INIT:
			evt_test_dtrace(EVT_TEST_L2,
				      ">>> Successfully %s is initiated",
				      control_p->dev_name);
			break;
		case EVT_TEST_SIG_TASK_STATE_START:
			evt_test_sig_generate_evt(control_p);
			control_p->timeout_in_millisec =
				((5000 * INFRA_OS_GET_TICKS_PER_SEC) / 1000);
			break;
		case EVT_TEST_SIG_TASK_STATE_STOP:
			evt_test_dtrace(EVT_TEST_L1, " Stopping\n");
			evt_test_sig_set_cmd(control_p,
				      EVT_TEST_SIG_TASK_STATE_INIT);
			infra_os_sema_signal(control_p->cmd_sem_p);
			break;
		case EVT_TEST_SIG_TASK_STATE_TERM:
			evt_test_dtrace(EVT_TEST_L1, " Terminating\n");
			infra_os_task_exit();
			return;
			break;
		default:
			evt_test_dtrace(EVT_TEST_L1,
				      " invalid state\n");
		}
	}
}

int evt_test_sig_set_cmd(evt_test_signaler_t *control_p,
			 evt_test_sig_task_state_t state)
{
	int		error = INFRA_NO_ERROR;

	evt_test_dtrace(EVT_TEST_L2, "\n");

	switch (state) {
	case EVT_TEST_SIG_TASK_STATE_TERM:
		evt_test_dtrace(EVT_TEST_L2, "\n");
		error = evt_test_sig_set_state(control_p, state);
		evt_test_dtrace(EVT_TEST_L2, "\n");
		if (INFRA_NO_ERROR == error)
			infra_os_wait_thread(&(control_p->thread));

		evt_test_dtrace(EVT_TEST_L2, "\n");
		evt_test_sig_term(control_p);
		break;
	case EVT_TEST_SIG_TASK_STATE_STOP:
		evt_test_dtrace(EVT_TEST_L2, "\n");
		error = evt_test_sig_set_state(control_p, state);
		evt_test_dtrace(EVT_TEST_L2, "\n");
		if (INFRA_NO_ERROR == error)
			infra_os_sema_wait(control_p->cmd_sem_p);

		evt_test_dtrace(EVT_TEST_L2, "\n");
		break;
	case EVT_TEST_SIG_TASK_STATE_INIT:
		break;
	case EVT_TEST_SIG_TASK_STATE_START:
		error = evt_test_sig_set_state(control_p, state);
		break;
	default:
		evt_test_dtrace(EVT_TEST_L1, " Invalid Command\n");
		error = -EINVAL;
		break;
	}

	evt_test_dtrace(EVT_TEST_L2, "\n");

	return error;
}

#if EVT_TEST_ENABLE_SIG_ALL
int evt_test_signaler_init_all(void)
{
	int			error = INFRA_NO_ERROR;
	evt_test_signaler_t			*control_p;

#if EVT_TEST_ENABLE_SIG_1
	control_p = (evt_test_signaler_t *) infra_os_malloc(sizeof(*control_p));
	if (NULL == control_p)
		return -ENOMEM;

	evt_test_signaler_handle_store[0] = (uint32_t) control_p;
	strncpy(control_p->dev_name,
		      evt_signaler_dev_name[0],
		      INFRA_MAX_DEV_NAME);

	evt_test_sig_init(control_p);
#endif


#if EVT_TEST_ENABLE_SIG_2
	control_p = (evt_test_signaler_t *) infra_os_malloc(sizeof(*control_p));
	if (NULL == control_p)
		return -ENOMEM;

	evt_test_signaler_handle_store[1] = (uint32_t) control_p;
	strncpy(control_p->dev_name,
		      evt_signaler_dev_name[1],
		      INFRA_MAX_DEV_NAME);

	evt_test_sig_init(control_p);
#endif

#if EVT_TEST_ENABLE_SIG_3
	control_p = (evt_test_signaler_t *) infra_os_malloc(sizeof(*control_p));
	if (NULL == control_p)
		return -ENOMEM;

	evt_test_signaler_handle_store[2] = (uint32_t) control_p;
	strncpy(signaler_3_control_p->dev_name,
		      evt_signaler_dev_name[2],
		      INFRA_MAX_DEV_NAME);

	evt_test_sig_init(control_p);
#endif

	return error;

}
#endif

static int evt_test_init_sig(void *data_p, void *usr_data)
{
	evt_test_signaler_t *control_p;
	int error = INFRA_NO_ERROR;
	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));

	if (error < 0 || arg > MAX_INSTANCES) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (evt_test_signaler_t *) infra_os_malloc(sizeof(*control_p));

	if (NULL == control_p) {
		evt_test_etrace(EVT_TEST_L1,
			      " ERROR in allocating memory for  signaller");
		return -ENOMEM;
	}

	strncpy(control_p->dev_name,
		      evt_signaler_dev_name[arg-1],
		      INFRA_MAX_DEV_NAME);
	evt_test_signaler_handle_store[arg-1] = (uint32_t) control_p;
	evt_test_sig_init(control_p);

	return INFRA_NO_ERROR;
}

int evt_test_sig_init(evt_test_signaler_t *control_p)
{

	int			error = INFRA_NO_ERROR;
	char			thr_name[EVT_TEST_NAME_SIZE];
	infra_os_task_priority_t task_priority[2] = {SCHED_RR, 52};

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");
	control_p->timeout_in_millisec = 100;
	control_p->handle = (uint32_t) control_p;
	control_p->cur_state = EVT_TEST_SIG_TASK_STATE_INIT;
	control_p->next_state = EVT_TEST_SIG_TASK_STATE_INIT;
	error = infra_os_sema_initialize(&(control_p->sem_p), 1);
	error = infra_os_sema_initialize(&(control_p->cmd_sem_p), 0);
	error |= infra_os_mutex_initialize(&(control_p->lock_p));

	sprintf(thr_name, "%s", control_p->dev_name);
	infra_os_thread_create(&(control_p->thread),
		      evt_test_signaler_task,
		      (void *) control_p,
		      thr_name,
		      task_priority);
	evt_test_dtrace(EVT_TEST_L1,
		      " %s is initialized Sucessfully\n",
		      control_p->dev_name);

	if (control_p->thread == NULL) {
		evt_test_etrace(EVT_TEST_L1,
			      "<<<   ERROR in initialization of %s <<<",
			      control_p->dev_name);
		return -ENOMEM;
	}

	return INFRA_NO_ERROR;
}

static int evt_test_term_sig(void *data_p, void *usr_data)
{
	int			error = INFRA_NO_ERROR;
	evt_test_signaler_t	*control_p;
	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));

	evt_test_etrace(EVT_TEST_L1, "\nError : %d\n ", error);

	/* To be removed.*/
	if (error < 0 || arg > MAX_INSTANCES) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (evt_test_signaler_t *) evt_test_signaler_handle_store[arg-1];

	error = evt_test_sig_set_cmd(control_p, EVT_TEST_SIG_TASK_STATE_TERM);

	infra_os_free((void *) control_p);

	return error;

}

static int evt_test_signal_sig(void *data_p, void *usr_data)
{
	evt_test_signaler_t			*control_p;
	int			error = INFRA_NO_ERROR;

	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));

	evt_test_dtrace(EVT_TEST_L1, "\nError : %d\n ", error);

	if (error < 0 || arg > MAX_INSTANCES) {

		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (evt_test_signaler_t *) evt_test_signaler_handle_store[arg-1];
	evt_test_sig_generate_evt(control_p);

	return 0;

}


static int evt_test_start_sig(void *data_p, void *usr_data)
{
	evt_test_signaler_t			*control_p;
	int			error = INFRA_NO_ERROR;

	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));

	evt_test_dtrace(EVT_TEST_L1, "\nError : %d\n ", error);

	if (error < 0 || arg > MAX_INSTANCES) {

		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (evt_test_signaler_t *) evt_test_signaler_handle_store[arg-1];
	error = evt_test_sig_set_cmd(control_p, EVT_TEST_SIG_TASK_STATE_START);

	return error;

}

static int evt_test_stop_sig(void *data_p, void *usr_data)
{
	evt_test_signaler_t			*control_p;
	int			error = INFRA_NO_ERROR;

	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));

	evt_test_dtrace(EVT_TEST_L1, "\nError : %d\n ", error);

	if (error < 0 || arg > MAX_INSTANCES) {

		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	control_p = (evt_test_signaler_t *) evt_test_signaler_handle_store[arg-1];
	error = evt_test_sig_set_cmd(control_p, EVT_TEST_SIG_TASK_STATE_STOP);

	return error;
}


int evt_test_sig_term(evt_test_signaler_t *control_p)
{
	int			error;
	error = infra_os_sema_terminate(control_p->sem_p);
	error = infra_os_sema_terminate(control_p->cmd_sem_p);
	evt_test_dtrace(EVT_TEST_L1, "\n");
	error |= infra_os_mutex_terminate(control_p->lock_p);
	evt_test_dtrace(EVT_TEST_L1, "\n");
	evt_test_dtrace(EVT_TEST_L1,
		      ">>> Successfully %s is terminated >>>",
		      control_p->dev_name);
	return INFRA_NO_ERROR;
}

#if EVT_TEST_ENABLE_SIG_ALL
static int evt_test_signaler_term_all()
{
	int			error;

	evt_test_dtrace(EVT_TEST_L1, "\n");

#if EVT_TEST_ENABLE_SIG_1
	error = evt_test_sig_set_cmd(signaler_1_control_p,
		      EVT_TEST_SIG_TASK_STATE_STOP);
	error = evt_test_sig_set_cmd(signaler_1_control_p,
		      EVT_TEST_SIG_TASK_STATE_TERM);
	infra_os_free((void *) signaler_1_control_p);
#endif

#if EVT_TEST_ENABLE_SIG_2
	error = evt_test_sig_set_cmd(signaler_2_control_p,
		      EVT_TEST_SIG_TASK_STATE_STOP);
	error = evt_test_sig_set_cmd(signaler_2_control_p,
		      EVT_TEST_SIG_TASK_STATE_TERM);
	infra_os_free((void *) signaler_2_control_p);
#endif

#if EVT_TEST_ENABLE_SIG_3
	error = evt_test_sig_set_cmd(signaler_3_control_p,
		      EVT_TEST_SIG_TASK_STATE_STOP);
	error = evt_test_sig_set_cmd(signaler_3_control_p,
		      EVT_TEST_SIG_TASK_STATE_TERM);
	infra_os_free((void *) signaler_3_control_p);
#endif

	return INFRA_NO_ERROR;
}
#endif


void evt_test_sig_generate_evt(evt_test_signaler_t *control_p)
{
	int			error;
	uint32_t				rand_num;
	stm_event_t				event;
	evt_test_sig_evt_id_string_param_t	param;

#ifdef CONFIG_ARCH_STI
	rand_num = prandom_u32();
#else
	rand_num = random32();
#endif
	rand_num = (rand_num % 10);

	event.object = (void *) control_p;
	event.event_id = (1 << rand_num);

	error = stm_event_signal(&event);

	param.evt_id = event.event_id;
	param.obj = event.object;
	error = evt_test_sig_evt_id_to_string(&param);

	if (error)
		evt_test_dtrace(EVT_TEST_L1, " ERROR(%d)\n", error);


	evt_test_dtrace(EVT_TEST_L1,
		      " generated evt:%s for Sig: 0x%x [%s]\n",
		      param.evt_name,
		      (uint32_t) control_p,
		      param.sig_name);
}


int evt_test_sig_get_object(stm_object_h *obj_p, infra_dev_name_t name)
{
	uint32_t			index = 0;
	int		error;

	*obj_p = NULL;

	for (; index < EVT_TEST_SIG_TYPE_MAX; index++) {
		if (strcmp(evt_signaler_dev_name[index], name) == 0) {
			*obj_p = (void *) evt_test_signaler_handle_store[index];
			break;
		}
	}

	error = (*obj_p == NULL) ? -EINVAL : INFRA_NO_ERROR;

	return error;
}

evt_test_signaler_type_t evt_test_sig_get_sigtype(stm_object_h obj)
{
	uint32_t			index = 0;
	evt_test_signaler_type_t	type = EVT_TEST_SIG_TYPE_MAX;

	for (; index < EVT_TEST_SIG_TYPE_MAX; index++) {
		if (evt_test_signaler_handle_store[index] == (uint32_t) obj) {
			type = index;
			break;
		}
	}

	return type;
}


int evt_test_sig_evt_id_to_string(evt_test_sig_evt_id_string_param_t *param_p)
{
	int		error = INFRA_NO_ERROR;
	evt_test_signaler_type_t	type;
	char *evt_name;
	int val;
	type = evt_test_sig_get_sigtype(param_p->obj);
	val = param_p->evt_id;
	evt_name = get_string_from_map(val);

	switch (type) {
	case EVT_TEST_SIG_TYPE_1:
		sprintf(param_p->evt_name, "%s%s",
			      evt_name,
			      "_SIG_1");
		strncpy(param_p->sig_name,
			      "SIG_TYPE_1",
			      (EVT_TEST_MAX_EVT_NAME - 1));
		break;
	case EVT_TEST_SIG_TYPE_2:
		sprintf(param_p->evt_name, "%s%s",
			      evt_name,
			      "_SIG_2");
		strncpy(param_p->sig_name,
			      "SIG_TYPE_2",
			      (EVT_TEST_MAX_EVT_NAME - 1));
		break;
	case EVT_TEST_SIG_TYPE_3:
		sprintf(param_p->evt_name, "%s%s",
			      evt_name,
			      "_SIG_3");
		strncpy(param_p->sig_name,
			      "SIG_TYPE_3",
			      (EVT_TEST_MAX_EVT_NAME - 1));
		break;
	default:
		evt_test_dtrace(EVT_TEST_L1, " Invalid Sig Type\n");
		error = -EINVAL;
		break;
	}

	return error;
}


static struct nf_hook_ops evt_test_ip_hook_ops[] = {
	{
		.hook = (nf_hookfn *) evt_test_ip_v4_nethook,
		.owner = THIS_MODULE,
		.pf = PF_INET,
		.hooknum = NF_INET_PRE_ROUTING
	}
};



static unsigned int evt_test_ip_v4_nethook(unsigned int hooknum,
					   struct sk_buff *ip_fe_buffer,
					   const struct net_device *in_p,
					   const struct net_device *out_p,
					   int (*okfn)(struct sk_buff *))
{
	evt_test_signaler_t *control_p;
	int index = 0;
	stm_event_t event;
	int ret;

	for (; index < MAX_INSTANCES; index++) {
		control_p = (evt_test_signaler_t *)
			evt_test_signaler_handle_store[index];
		if (control_p) {
			if (control_p->sig_ip_evt)
				break;
		}
		control_p = NULL;
	}

	if (control_p) {
		event.object = (void *) control_p;
		event.event_id = EVT_TEST_ID_SIG_1_IPV4_EVT;

		ret = stm_event_signal(&event);
		if (ret)
			evt_test_dtrace(EVT_TEST_L1,
				      "Event Signaling failed\n");
	}

	return NF_ACCEPT;
}

static int evt_test_sig_install_intr(void *data_p, void *usr_data)
{
	evt_test_signaler_t *control_p;
	int error = INFRA_NO_ERROR;
	int arg;
	int index = 0;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));

	if (error < 0 || arg > MAX_INSTANCES) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}

	for (; index < MAX_INSTANCES; index++) {
		control_p = (evt_test_signaler_t *)
			evt_test_signaler_handle_store[index];
		if (control_p) {
			if (control_p->sig_ip_evt) {
				evt_test_etrace(EVT_TEST_L1,
					      "IP EVT already Installed\n"
					      );
				return -1;
			}
		}
	}
	control_p = (evt_test_signaler_t *)
		evt_test_signaler_handle_store[arg-1];

	if (control_p) {
		control_p->sig_ip_evt = 0x1;
		error = nf_register_hooks(evt_test_ip_hook_ops,
			      ARRAY_SIZE(evt_test_ip_hook_ops));
		if (error)
			pr_err("!!!Error Unable to \
					register network hook: %x", error);
		else
			pr_info("Hooking EVT Test IP module...\n");
	}

	return 0;
}

static int evt_test_sig_uninstall_intr(void *data_p, void *usr_data)
{
	evt_test_signaler_t *control_p;
	int error = INFRA_NO_ERROR;
	int arg;

	evt_test_dtrace(EVT_TEST_L1, ">>>  >>>\n");

	error = get_arg(&data_p, INTEGER, (void *) &arg, sizeof(int));

	if (error < 0 || arg > MAX_INSTANCES) {
		pr_err("Error:%d Invalid Token(%d)\n",
			      error,
			      arg);
		return -EINVAL;
	}
	control_p = (evt_test_signaler_t *)
		evt_test_signaler_handle_store[arg-1];

	nf_unregister_hooks(evt_test_ip_hook_ops,
		      ARRAY_SIZE(evt_test_ip_hook_ops));
	return 0;
}



int evt_test_signaler_entry(void)
{
	infra_test_reg_param_t		cmd_param;

	intialize_map_table();

	INFRA_TEST_REG_CMD(cmd_param,
		      "SIG_INIT",
		      evt_test_init_sig,
		      "INIT SIGNALER");

	INFRA_TEST_REG_CMD(cmd_param,
		      "SIG_TERM",
		      evt_test_term_sig,
		      "TERM SIGNALER");

	INFRA_TEST_REG_CMD(cmd_param,
		      "SIG_START",
		      evt_test_start_sig,
		      "START SIGNALER");

	INFRA_TEST_REG_CMD(cmd_param,
		      "SIG_SIGNAL",
		      evt_test_signal_sig,
		      "GENERATE EVT FROM SIGNALER");

	INFRA_TEST_REG_CMD(cmd_param,
		      "SIG_STOP",
		      evt_test_stop_sig,
		      "STOP SIGNALER");

	INFRA_TEST_REG_CMD(cmd_param,
		      "SIG_INSTALL_INTR",
		      evt_test_sig_install_intr,
		      "install interrupt evt");

	INFRA_TEST_REG_CMD(cmd_param,
		      "SIG_UNINSTALL_INTR",
		      evt_test_sig_uninstall_intr,
		      "uninstall interrupt evt");

	return INFRA_NO_ERROR;
}



int evt_test_signaler_exit()
{
	infra_test_reg_param_t		cmd_param;

	evt_test_dtrace(EVT_TEST_L1, "\n");

	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_init_sig);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_term_sig);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_start_sig);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_signal_sig);
	INFRA_TEST_DEREG_CMD(cmd_param, evt_test_stop_sig);

	return INFRA_NO_ERROR;
}
