#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include "stm_ci.h"
#include "ci_internal.h"
#include "ci_utils.h"
#include "ci_hal.h"

/***********************************************************/
#define	CI_DEVICE_MAX 2
/*Need to be dynamic as per platform through platform_data*/
/**********************************************************/

/**************************************LOCAL GLOBALS***/
extern struct ci_hw_data_s ci_data[];
static stm_ci_t	*g_ci_h_arr[CI_DEVICE_MAX];
static struct rt_mutex *g_lock;
static struct rt_mutex ci_global_lock;

/******************************************************************************/
/* Private types */
/******************************************************************************/
ca_error_code_t ci_validate_init_param(uint32_t dev_id, stm_ci_h *device)
{
	if (ci_data[dev_id].init == false) {
		ci_error_trace(CI_API_RUNTIME, "CI not supported\n");
		return -EPERM;
	}

	if (device == NULL) {
		ci_error_trace(CI_API_RUNTIME,
				"Invalid Params\n");
		return -EINVAL;
	}
	if ((g_ci_h_arr[dev_id]) != NULL) {
		ci_error_trace(CI_API_RUNTIME,
				"CI Instance(%d) already initialized\n",
				dev_id);
		return -EBUSY;
	}
	if (dev_id >= CI_DEVICE_MAX) {
		ci_error_trace(CI_API_RUNTIME,
				"Invalid Device Id\n");
		return -ENODEV;
	}
	return 0;
}
ca_error_code_t ci_validate_del_param(stm_ci_h device)
{
	uint8_t	dev_id;
	if (device == NULL) {
		ci_error_trace(CI_API_RUNTIME,
			"INVALID PARAM[handle=%p]\n",
			device);
		return -EINVAL;
	}
	if ((CI_VALIDATE_HANDLE(device->magic_num)) != 0) {
		ci_error_trace(CI_API_RUNTIME,
			"INVALID HANDLE[handle=%p ]\n",
			device);
		return -ENODEV;
	}

	dev_id = device->magic_num & 0xF;
	if (dev_id >= CI_DEVICE_MAX)
		return -EINVAL;
	if ((g_ci_h_arr[dev_id]) == NULL) {
		ci_error_trace(CI_API_RUNTIME,
			"CI Instance(%d) already deleted\n",
			dev_id);
		return -EBUSY;
	}
	return 0;
}

void ci_enter_critical_section(void)
{
	if (g_lock != NULL)
		ca_os_mutex_lock(g_lock);
}

void ci_exit_critical_section(void)
{
	if (g_lock != NULL)
		ca_os_mutex_unlock(g_lock);
}

ca_error_code_t ci_alloc_global_param(void)
{
	g_lock = &ci_global_lock;
	rt_mutex_init(g_lock);
	return CA_NO_ERROR;
}

ca_error_code_t ci_dealloc_global_param(void)
{
	return CA_NO_ERROR;
}

ca_error_code_t ci_alloc_control_param(uint32_t dev_id, stm_ci_h *device)
{
	stm_ci_t		*ci_p;
	ca_error_code_t	error = CA_NO_ERROR;

	ci_p = ca_os_malloc(sizeof(stm_ci_t));
	if (ci_p == NULL) {
		ci_error_trace(CI_API_RUNTIME,
				"CI control block alloc	failed\n");
		return -ENOMEM;
	}
	g_ci_h_arr[dev_id] = ci_p;

	*device	= ci_p;

	ci_p->device.cis_buf_p = (uint8_t *)ca_os_malloc(\
			MAX_NO_OF_TUPLES*MAX_TUPLE_BUFFER);
	if (ci_p->device.cis_buf_p == NULL) {
		ci_error_trace(CI_API_RUNTIME,
				"CI cis	buffer alloc failed\n");
		return -ENOMEM;
	}
	error =	ca_os_sema_initialize(&(ci_p->ctrl_sem_p), 0);
	error |= ca_os_mutex_initialize(&(ci_p->lock_ci_param_p));
	spin_lock_init(&ci_p->lock);

	return error;
}

ca_error_code_t ci_dealloc_control_param(stm_ci_t *ci_p)
{
	ca_error_code_t	error =	CA_NO_ERROR;
	uint8_t			dev_id;

	if (NULL == ci_p)
		return -1;

	if (ci_p->lock_ci_param_p)
		error |= ca_os_mutex_terminate(ci_p->lock_ci_param_p);
	if (ci_p->ctrl_sem_p)
		error =	ca_os_sema_terminate(ci_p->ctrl_sem_p);

	dev_id = ci_p->magic_num & 0xF;
	g_ci_h_arr[dev_id] = NULL;

	ci_p->magic_num = 0;

	if (ci_p->device.cis_buf_p != NULL)
		ca_os_free(ci_p->device.cis_buf_p);

	ca_os_free(ci_p);

	return error;
}

void ci_fill_control_param(uint32_t dev_id , stm_ci_t *ci_p)
{
	uint32_t i = 0;

	ci_signal_map	*sigmap	= ci_data[dev_id].platform_data->sigmap;
	ci_p->ci_hw_data_p	= &ci_data[dev_id];

	ci_p->currstate	= CI_STATE_OPEN;
	ci_p->fr_timeout = CI_CAM_FREE_TIMEOUT;
	ci_p->da_timeout = CI_DATA_AVAILABLE_TIMEOUT;
	ci_p->ciplus_in_polling = false;
	ci_p->currstate	= CI_STATE_OPEN;
	ci_p->device.cam_present = false;
	ci_p->device.link_initialized =	false;
	ci_p->device.ci_caps.cam_type =	STM_CI_CAM_NONE;

	memset(&ci_p->device.ci_caps, 0x00, sizeof(stm_ci_capabilities_t));
	memset(&ci_p->ca, 0, sizeof(struct ci_dvb_ca_en50221));

	for (i = 0; i < (CI_SIGNAL_MAX + 1); i++) {
		/* set to zero for interface data*/
		memset(&ci_p->ci_info[i], 0, sizeof(ci_interface_t));
		ci_p->sig_map[i] = *(sigmap);
		sigmap++;
		ci_debug_trace(CI_UTILS_RUNTIME,
			"%d sig_type %d interface type %d State %d \
			bit mask 0x%x IntNum %d\n",
			i,
			ci_p->sig_map[i].sig_type,
			ci_p->sig_map[i] .interface_type,
			ci_p->sig_map[i] .active_high,
			ci_p->sig_map[i] .bit_mask,
			ci_p->sig_map[i] .interrupt_num);
		ci_debug_trace(CI_UTILS_RUNTIME,
				"****************************************\n");
	}
}

ca_error_code_t ci_set_vcc_type(struct stm_ci_platform_data *platform_data_p,
					stm_ci_t *ci_p)
{
	ca_error_code_t error = CA_NO_ERROR;
	ci_vcc_type_t type = CI_VCC_NONE;

	ci_signal_map vcc3EN = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc3EN];
	ci_signal_map vcc5EN = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_Vcc5EN];
	ci_signal_map EN0 = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN0];
	ci_signal_map EN1 = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN1];
	ci_signal_map vccEN = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	ci_signal_map vcc_3_5EN = ci_p->sig_map[CI_SIGNAL_VCC_3_5EN];

	if ((vcc3EN.interface_type != CI_UNKNOWN_INTERFACE) && \
		(vcc5EN.interface_type != CI_UNKNOWN_INTERFACE)	&& \
		(EN0.interface_type != CI_UNKNOWN_INTERFACE) && \
		(EN1.interface_type != CI_UNKNOWN_INTERFACE)) {
		type = CI_VCC_MICREL;
	} else if ((vcc5EN.interface_type != CI_UNKNOWN_INTERFACE) && \
			(vcc3EN.interface_type != CI_UNKNOWN_INTERFACE)) {
		type = CI_VCC_ALT_VOLT_EN;
	} else if ((vccEN.interface_type != CI_UNKNOWN_INTERFACE) && \
			(vcc_3_5EN.interface_type != CI_UNKNOWN_INTERFACE)) {
		type = CI_VCC;
	} else if (vccEN.interface_type != CI_UNKNOWN_INTERFACE) {
		type = CI_VCC_DIRECT;
	} else {
		error = -EINVAL;
	}
	ci_p->vcc_type = type;
	return error;
}

/* setting function pointers */
static void ci_register_slot_status(stm_ci_t *ci_p)
{
	/* CD1 is mandatory:assuming that CD1 and CD2 are mapped
	** to same interface which is always the case */
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];

	if (cd1.interface_type == CI_EPLD)
		ci_p->ca.slot_status = ci_slot_status_epld;
	else if	(cd1.interface_type == CI_GPIO)
		ci_p->ca.slot_status = ci_slot_status_pio;
	else
		ci_p->ca.slot_status = NULL;

#ifdef CONFIG_MACH_STM_B2000
	/* CD1 interrupt mapped to PIO but status comes from epld register*/
	if (cd1.interface_type == CI_GPIO)
		ci_p->ca.slot_status = ci_slot_status_epld;
#endif
}

static void ci_register_slot_reset(stm_ci_t *ci_p)
{
	ci_signal_map rst = ci_p->sig_map[CI_SIGNAL_RST];

	if (rst.interface_type == CI_EPLD)
		ci_p->ca.slot_reset = ci_slot_reset_epld;
	else if (rst.interface_type == CI_GPIO)
		ci_p->ca.slot_reset = ci_slot_reset_pio;
	else
		ci_p->ca.slot_reset = NULL;
}
static void ci_register_slot_power(stm_ci_t *ci_p)
{
	/* CD1 is mandatory:assuming that CD1 and CD2 are mapped to
	*same interface which is always the case	*/
	ci_signal_map vcc = ci_p->sig_map[CI_SIGNAL_VCC_EN];

	if (ci_p->vcc_type == CI_VCC_MICREL)
		vcc = ci_p->sig_map[CI_SIGNAL_VCC_MICREL_EN0];
	else if ((ci_p->vcc_type == CI_VCC) || \
			(ci_p->vcc_type	== CI_VCC_DIRECT))
		vcc = ci_p->sig_map[CI_SIGNAL_VCC_EN];
	else if (ci_p->vcc_type == CI_VCC_ALT_VOLT_EN)
		vcc = ci_p->sig_map[CI_SIGNAL_VCC_EN];

	if (vcc.interface_type == CI_EPLD) {
		ci_p->ca.slot_power_enable = ci_slot_power_enable_epld;
		ci_p->ca.slot_power_disable = ci_slot_power_disable_epld;
	} else if (vcc.interface_type == CI_GPIO) {
		ci_p->ca.slot_power_enable = ci_slot_power_enable_pio;
		ci_p->ca.slot_power_disable = ci_slot_power_disable_pio;
	} else {
		ci_p->ca.slot_power_enable = NULL;
		ci_p->ca.slot_power_disable = NULL;
	}
}

static void ci_register_switch_access_mode(stm_ci_t *ci_p)
{
	ci_signal_map iois16 = ci_p->sig_map[CI_SIGNAL_IOIS16];

	if (iois16.interface_type == CI_EPLD)
		ci_p->ca.switch_access_mode = ci_switch_access_mode_epld;
	else if (iois16.interface_type == CI_GPIO)
		ci_p->ca.switch_access_mode = ci_switch_access_mode_pio;
	else
		ci_p->ca.switch_access_mode = NULL;
}

ca_error_code_t ci_slot_init(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = 0;
	/* Initialize all signal interfaces */
	error =	ci_slot_init_pio(ca);
	if (error == 0)
		ci_slot_init_epld(ca);
	return error;
}

ca_error_code_t ci_slot_shutdown(struct ci_dvb_ca_en50221 *ca)
{
	ca_error_code_t error = 0;
	error =	ci_slot_shutdown_pio(ca);
	if (error == 0)
		ci_slot_shutdown_epld(ca);

	return error;
}
static void ci_register_init(stm_ci_t	*ci_p)
{
	ci_p->ca.slot_init = ci_slot_init;
}

static void ci_register_shutdown(stm_ci_t *ci_p)
{
	ci_p->ca.slot_shutdown = ci_slot_shutdown;
}

static void ci_register_slot_enable(stm_ci_t *ci_p)
{
	ci_signal_map sel = ci_p->sig_map[CI_SIGNAL_SEL];

	if (sel.interface_type == CI_GPIO)
		ci_p->ca.slot_enable = ci_slot_enable_pio;
	else if (sel.interface_type == CI_EPLD)
		ci_p->ca.slot_enable = ci_slot_enable_epld;
	else
		ci_p->ca.slot_enable = NULL;
}

/* Open	the pio	in case	of CD1 on PIO */
static void ci_register_slot_detect_irq_install(stm_ci_t *ci_p)
{
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];
	if (cd1.interface_type == CI_EPLD)
		ci_p->ca.slot_detect_irq_install = \
			ci_slot_detect_irq_install_epld;
	else if (cd1.interface_type == CI_GPIO)
		ci_p->ca.slot_detect_irq_install = \
			ci_slot_detect_irq_install_pio;
	else
		ci_p->ca.slot_detect_irq_install = NULL;
}

static void ci_register_slot_detect_irq_uninstall(stm_ci_t *ci_p)
{
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];

	if (cd1.interface_type == CI_EPLD)
		ci_p->ca.slot_detect_irq_uninstall = \
				ci_slot_detect_irq_uninstall_epld;
	else if (cd1.interface_type == CI_GPIO)
		ci_p->ca.slot_detect_irq_uninstall = \
				ci_slot_detect_irq_uninstall_pio;
	else
		ci_p->ca.slot_detect_irq_uninstall = NULL;
}

static void ci_register_slot_detect_irq_enable(stm_ci_t *ci_p)
{
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];

	if (cd1.interface_type == CI_EPLD)
		ci_p->ca.slot_detect_irq_enable	= \
			ci_slot_detect_irq_enable_epld;
	else if (cd1.interface_type == CI_GPIO)
		ci_p->ca.slot_detect_irq_enable	= \
			ci_slot_detect_irq_enable_pio;
	else
		ci_p->ca.slot_detect_irq_enable	= NULL;
}

static void ci_register_slot_detect_irq_disable(stm_ci_t *ci_p)
{
	ci_signal_map cd1 = ci_p->sig_map[CI_SIGNAL_CD1];

	if (cd1.interface_type == CI_EPLD)
		ci_p->ca.slot_detect_irq_disable = \
					ci_slot_detect_irq_disable_epld;
	else if (cd1.interface_type == CI_GPIO)
		ci_p->ca.slot_detect_irq_disable = \
					ci_slot_detect_irq_disable_pio;
	else
		ci_p->ca.slot_detect_irq_disable = NULL;
}

/* data	interrupt settings */
/* Claim the pad in case of IREQ on gpio */
static void ci_register_slot_data_irq_install(stm_ci_t *ci_p)
{
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];

	if (ireq.interface_type == CI_EPLD)
		ci_p->ca.slot_data_irq_install = ci_slot_data_irq_install_epld;
	else if (ireq.interface_type == CI_GPIO)
		ci_p->ca.slot_data_irq_install = ci_slot_data_irq_install_pio;
	else
		ci_p->ca.slot_data_irq_install = NULL;
}

static void ci_register_slot_data_irq_uninstall(stm_ci_t *ci_p)
{
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];
	if (ireq.interface_type == CI_EPLD)
		ci_p->ca.slot_data_irq_uninstall = \
			ci_slot_data_irq_uninstall_epld;
	else if (ireq.interface_type == CI_GPIO)
		ci_p->ca.slot_data_irq_uninstall = \
			ci_slot_data_irq_uninstall_pio;
	else
		ci_p->ca.slot_data_irq_uninstall = NULL;
}

static void ci_register_slot_data_irq_enable(stm_ci_t	*ci_p)
{
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];

	if (ireq.interface_type == CI_EPLD)
		ci_p->ca.slot_data_irq_enable = ci_slot_data_irq_enable_epld;
	else if (ireq.interface_type == CI_GPIO)
		ci_p->ca.slot_data_irq_enable = ci_slot_data_irq_enable_pio;
	else
		ci_p->ca.slot_data_irq_enable = NULL;
}

static void ci_register_slot_data_irq_disable(stm_ci_t *ci_p)
{
	ci_signal_map ireq = ci_p->sig_map[CI_SIGNAL_IREQ];

	if (ireq.interface_type == CI_EPLD)
		ci_p->ca.slot_data_irq_disable = ci_slot_data_irq_disable_epld;
	else if (ireq.interface_type == CI_GPIO)
		ci_p->ca.slot_data_irq_disable = ci_slot_data_irq_disable_pio;
	else
		ci_p->ca.slot_data_irq_disable = NULL;
}

static void ci_register_interrupt_fptr(stm_ci_t *ci_p)
{
	ci_register_slot_detect_irq_install(ci_p);
	ci_register_slot_detect_irq_uninstall(ci_p);
	ci_register_slot_detect_irq_enable(ci_p);
	ci_register_slot_detect_irq_disable(ci_p);

	ci_register_slot_data_irq_install(ci_p);
	ci_register_slot_data_irq_uninstall(ci_p);
	ci_register_slot_data_irq_enable(ci_p);
	ci_register_slot_data_irq_disable(ci_p);
}

void ci_register_interface_fptr(stm_ci_t *ci_p)
{
	/* store the data */
	ci_p->ca.data = (stm_ci_t *)ci_p;

	/* Card Detect */
	ci_register_slot_status(ci_p);

	/* Module Reset */
	ci_register_slot_reset(ci_p);

	ci_register_slot_power(ci_p);

	ci_register_switch_access_mode(ci_p);

	ci_register_init(ci_p);

	ci_register_shutdown(ci_p);
	ci_register_slot_enable(ci_p);

	ci_register_interrupt_fptr(ci_p);
}
