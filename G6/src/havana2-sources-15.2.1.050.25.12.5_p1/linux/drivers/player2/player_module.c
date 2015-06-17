/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/suspend.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include "player_version.h"

#include "osinline.h"
#include "report.h"
#include "stm_se.h"
#include "audio_mixer_params.h"
#include "player_threads.h"

MODULE_DESCRIPTION("Streaming Engine sw device driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_VERSION(PLAYER_VERSION);
MODULE_LICENSE("GPL");

// module init for trace groups levels
// either set a valid (positive) severity level per group, or set -1 to use default global group level
static int trace_groups_levels_init[GROUP_LAST] = {
	severity_info,  //group_api
	-1,  //group_player
	-1,  //group_havana
	-1,  //group_buffer
	-1,  //group_decodebufferm
	-1,  //group_timestamps
	-1,  //group_misc
	-1,  //group_collator_audio
	-1,  //group_collator_video
	-1,  //group_esprocessor
	-1,  //group_frameparser_audio
	-1,  //group_frameparser_video
	-1,  //group_decoder_audio
	-1,  //group_decoder_video
	-1,  //group_encoder_stream
	-1,  //group_encoder_audio_preproc
	-1,  //group_encoder_audio_coder
	-1,  //group_encoder_video_preproc
	-1,  //group_encoder_video_coder
	-1,  //group_encoder_transporter
	-1,  //group_manifestor_audio_ksound
	-1,  //group_manifestor_video_stmfb
	-1,  //group_frc
	-1,  //group_manifestor_audio_grab
	-1,  //group_manifestor_video_grab
	-1,  //group_manifestor_audio_encode
	-1,  //group_manifestor_video_encode
	-1,  //group_output_timer
	-1,  //group_avsync
	-1,  //group_audio_reader
	-1,  //group_audio_player
	-1,  //group_audio_generator
	-1,  //group_mixer
};
module_param_array(trace_groups_levels_init, int, NULL, S_IRUGO); // no sysfs entry, since updated through debugfs
MODULE_PARM_DESC(trace_groups_levels_init, "Trace group levels to be activated at the init of the module");

// init for trace global level
static int trace_global_level  = severity_info;
module_param(trace_global_level, int, S_IRUGO); // no sysfs entry, since updated through debugfs
MODULE_PARM_DESC(trace_global_level, "Trace global default level to be activated at the init of the module");

// sysfs statistics are considered as debug info:
#if defined(CONFIG_DEBUG_FS) || defined(SDK2_ENABLE_ATTRIBUTES)
#define MODPARAMVISIBILITY S_IRUGO
#else
#define MODPARAMVISIBILITY 0
#endif

extern int thpolprio_inits[][2];
module_param_array_named(thread_SE_Aud_CtoP,      thpolprio_inits[SE_TASK_AUDIO_CTOP], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Aud_PtoD,      thpolprio_inits[SE_TASK_AUDIO_PTOD], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Aud_DtoM,      thpolprio_inits[SE_TASK_AUDIO_DTOM], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Aud_MPost,     thpolprio_inits[SE_TASK_AUDIO_MPost], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Vid_CtoP,      thpolprio_inits[SE_TASK_VIDEO_CTOP], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Vid_PtoD,      thpolprio_inits[SE_TASK_VIDEO_PTOD], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Vid_DtoM,      thpolprio_inits[SE_TASK_VIDEO_DTOM], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Vid_MPost,     thpolprio_inits[SE_TASK_VIDEO_MPost], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Vid_H264Int,   thpolprio_inits[SE_TASK_VIDEO_H264INT], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Vid_HevcInt,   thpolprio_inits[SE_TASK_VIDEO_HEVCINT], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Aud_Reader,    thpolprio_inits[SE_TASK_AUDIO_READER], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Aud_StreamT,   thpolprio_inits[SE_TASK_AUDIO_STREAMT], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Aud_Mixer,     thpolprio_inits[SE_TASK_AUDIO_MIXER], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Aud_BcastMixer, thpolprio_inits[SE_TASK_AUDIO_BCAST_MIXER], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Aud_Bypass,    thpolprio_inits[SE_TASK_AUDIO_BYPASS_MIXER], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Man_Coord,     thpolprio_inits[SE_TASK_MANIF_COORD], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Man_BrSrcGrab, thpolprio_inits[SE_TASK_MANIF_BRSRCGRAB], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Man_BrCapture, thpolprio_inits[SE_TASK_MANIF_BRCAPTURE], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Man_DsVideo,   thpolprio_inits[SE_TASK_MANIF_DSVIDEO], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Playbck_Drain, thpolprio_inits[SE_TASK_PLAYBACK_DRAIN], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_EncAud_ItoP,   thpolprio_inits[SE_TASK_ENCOD_AUDITOP], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_EncAud_PtoC,   thpolprio_inits[SE_TASK_ENCOD_AUDPTOC], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_EncAud_CtoO,   thpolprio_inits[SE_TASK_ENCOD_AUDCTOO], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_EncVid_ItoP,   thpolprio_inits[SE_TASK_ENCOD_VIDITOP], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_EncVid_PtoC,   thpolprio_inits[SE_TASK_ENCOD_VIDPTOC], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_EncVid_CtoO,   thpolprio_inits[SE_TASK_ENCOD_VIDCTOO], int, NULL,
                         MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Encod_Coord,   thpolprio_inits[SE_TASK_ENCOD_COORD], int, NULL, MODPARAMVISIBILITY);
module_param_array_named(thread_SE_Other,         thpolprio_inits[SE_TASK_OTHER], int, NULL, MODPARAMVISIBILITY);
MODULE_PARM_DESC(thread_SE_xyz, "SE threads parameters: policy,priority");

extern int SysfsInit(void);
extern void SysfsDelete(void);

// SE device driver implementation
// This is a pure SW device (no physical H/W device is attached)
// Note: Used for SE power management, for HPS/CPS modes only.
//       Active Standby callbacks are implemented for HPS/CPS testing purpose only (allows SE low power APIs to be called call from user space).

// CONFIG_PM is the compilation flag used by Linux kernel to support power management
// CONFIG_PM_RUNTIME is the compilation flag used by Linux kernel to support active standby

#ifdef CONFIG_PM
// Define tuneable entry in debugfs, allowing to select the low power test mode (0 for HPS, 1 for CPS)
// This tuneable is designed for SE low power APIs unit testing purpose only.
// To be taken in account, it must be set before invoking the SE runtime_suspend PM callback,
// and must remain at the same value until the SE runtime_resume PM callback has been invoked.
#define TUNEABLE_NAME_LOW_POWER_TEST_MODE "low_power_test_mode"

static unsigned int low_power_test_mode = 0;    // default test mode is HPS

// SE suspend callback, being called on Host Passive Standby entry (HPS entry)
static int stm_pm_se_suspend(struct device *dev)
{
	pr_info("SE suspend\n");

	// Nothing to do here
	// All HPS enter actions have already been taken in PM notifier callback, on PM_SUSPEND_PREPARE event
	return 0;
}

// SE resume callback, being called on Host Passive Standby exit (HPS exit)
static int stm_pm_se_resume(struct device *dev)
{
	pr_info("SE resume\n");

	// Nothing to do here
	// All HPS exit actions will be taken afterward in PM notifier callback, on PM_POST_SUSPEND event
	return 0;
}

// SE freeze callback, being called on Controller Passive Standby entry (CPS entry)
static int stm_pm_se_freeze(struct device *dev)
{
	pr_info("SE freeze\n");

	// Nothing to do here
	// All CPS enter actions have already been taken in PM notifier callback, on PM_HIBERNATION_PREPARE event
	return 0;
}

// SE restore callback, being called on Controller Passive Standby exit (CPS exit)
static int stm_pm_se_restore(struct device *dev)
{
	pr_info("SE restore\n");

	// Nothing to do here
	// All CPS exit actions will be taken afterward in PM notifier callback, on PM_POST_HIBERNATION event
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
// SE runtime_suspend callback (global streaming engine linux device model)
// This callback is implemented for HPS/CPS testing purpose only: allows SE low power enter API to be called from user space
static int stm_pm_se_runtime_suspend(struct device *dev)
{
	int ErrorCode;
	__stm_se_low_power_mode_t LowPowerMode;

	pr_warn("SE runtime suspend - WARNING: should be used for test purpose only\n");
	pr_info("Calling SE low power enter (%s mode)..\n", (low_power_test_mode != 0) ? "CPS" : "HPS");
	LowPowerMode = (low_power_test_mode != 0) ? STM_SE_LOW_POWER_MODE_CPS : STM_SE_LOW_POWER_MODE_HPS;
	ErrorCode = __stm_se_pm_low_power_enter(LowPowerMode);
	if (ErrorCode != 0) {
		// Error returned by SE, but this should not prevent the system entering low power
		pr_err("Error: %s low power enter failed\n", __func__);
	}
	pr_info("%s ok\n", __func__);

	return 0;
}

// SE runtime_resume callback (global streaming engine linux device model)
// This callback is implemented for HPS/CPS testing purpose only: allows SE low power exit API to be called from user space
static int stm_pm_se_runtime_resume(struct device *dev)
{
	int ErrorCode;

	pr_warn("SE runtime resume - WARNING: should be used for test purpose only\n");
	pr_info("Calling SE low power exit..\n");
	ErrorCode = __stm_se_pm_low_power_exit();
	if (ErrorCode != 0) {
		// Error returned by SE, but this should not prevent the system exiting low power
		pr_err("Error: %s SE low power exit failed\n", __func__);
	}
	pr_info("=> SE runtime resume completed\n");

	return 0;
}
#endif // CONFIG_PM_RUNTIME

// PM notifier callback
// This callback must be registered to do all needed actions at SE level for HPS/CPS
// On standby entry: it is called by PM framework before call to suspend/freeze callbacks of every module
// On standby exit:  it is called by PM framework after call to resume/restore callbacks of every module
static int stm_pm_se_notifier_call(struct notifier_block *this, unsigned long event, void *ptr)
{
	int ErrorCode;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		// HPS enter: this callback is called by the PM framework before calling the suspend callback of every module
		pr_info("SE notifier: PM_SUSPEND_PREPARE\n");

		// Ask SE to enter low power state (HPS mode)
		ErrorCode = __stm_se_pm_low_power_enter(STM_SE_LOW_POWER_MODE_HPS);
		if (ErrorCode != 0) {
			// Error returned by SE, but this should not prevent the system entering low power
			pr_err("Error: %s SE low power enter failed\n", __func__);
		}
		break;

	case PM_POST_SUSPEND:
		// HPS exit: this callback is called by the PM framework after calling the resume callback of every module
		pr_info("SE notifier: PM_POST_SUSPEND\n");

		// Ask SE to exit low power state (HPS mode)
		ErrorCode = __stm_se_pm_low_power_exit();
		if (ErrorCode != 0) {
			// Error returned by SE, but this should not prevent the system exiting low power
			pr_err("Error: %s SE low power exit failed\n", __func__);
		}
		break;

	case PM_HIBERNATION_PREPARE:
		// CPS enter: this callback is called by the PM framework before calling the freeze callback of every module
		pr_info("SE notifier: PM_HIBERNATION_PREPARE\n");

		// Ask SE to enter low power state (CPS mode)
		ErrorCode = __stm_se_pm_low_power_enter(STM_SE_LOW_POWER_MODE_CPS);
		if (ErrorCode != 0) {
			// Error returned by SE, but this should not prevent the system entering low power
			pr_err("Error: %s SE low power enter failed\n", __func__);
		}
		break;

	case PM_POST_HIBERNATION:
		// CPS exit: this callback is called by the PM framework after calling the restore callback of every module
		pr_info("SE notifier: PM_POST_HIBERNATION\n");

		// Ask SE to exit low power state (CPS mode)
		ErrorCode = __stm_se_pm_low_power_exit();
		if (ErrorCode != 0) {
			// Error returned by SE, but this should not prevent the system exiting low power
			pr_err("Error: %s SE low power exit failed\n", __func__);
		}
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

// Power management callbacks
static struct notifier_block stm_pm_se_notifier = {
	.notifier_call = stm_pm_se_notifier_call,
};

static struct dev_pm_ops stm_pm_se_pm_ops = {
	.suspend = stm_pm_se_suspend,
	.resume  = stm_pm_se_resume,
	.freeze  = stm_pm_se_freeze,
	.restore = stm_pm_se_restore,
// .runtime_suspend and .runtime_resume are not needed because Active Standby is not handled at SE level
// define runtime callbacks anyway to test low power SE APIs without actual HPS/CPS request
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend  = stm_pm_se_runtime_suspend,
	.runtime_resume   = stm_pm_se_runtime_resume,
#endif // CONFIG_PM_RUNTIME
};
#endif // CONFIG_PM

static void stm_se_release(struct device *dev)
{
	// Nothing to do here, but needed to avoid WARNING at module unload;
}

// SE probe function (called on SE module load)
static int stm_se_probe(struct platform_device *pdev)
{
	int ret;
	int i;

	for (i = 0; i <= SE_TASK_OTHER; i++) {
		player_tasks_desc[i].policy   = thpolprio_inits[i][0];
		player_tasks_desc[i].priority = thpolprio_inits[i][1];
		pr_info("%s pol:%d prio:%d\n", player_tasks_desc[i].name,
		        player_tasks_desc[i].policy, player_tasks_desc[i].priority);
	}

	report_init(trace_groups_levels_init, trace_global_level);

	ret = SysfsInit();
	if (ret) {
		goto failsysfs;
	}

	// Set release function to avoid WARNING at module unload
	pdev->dev.release = stm_se_release;

	// Initialize sysfs entries for mixer transformer selection
	create_sysfs_mixer_selection(&pdev->dev);

	// Initialise the Streaming Engine through the STKPI
	ret = stm_se_init();
	if (ret) {
		goto failinit;
	}

#ifdef CONFIG_PM
	// Register tuneable allowing to select low power test mode (HPS or CPS)
	OS_RegisterTuneable(TUNEABLE_NAME_LOW_POWER_TEST_MODE, &low_power_test_mode);

	// Register with PM notifiers (for HPS/CPS support)
	register_pm_notifier(&stm_pm_se_notifier);

#ifdef CONFIG_PM_RUNTIME
	//set the device's runtime PM status to 'active'
	pm_runtime_set_active(&pdev->dev);
	//dont use any parent/children relationship for the power device
	pm_suspend_ignore_children(&pdev->dev, 1);
	//decrement the device's 'power.disable_depth' field : when '0', enables callback usage.
	pm_runtime_enable(&pdev->dev);
	//mandatory to perform a "pm_runtime_get": on the contrary, runtime_suspend will be called immediately
	pm_runtime_get(&pdev->dev);
#endif // CONFIG_PM_RUNTIME
#endif // CONFIG_PM

	OS_Dump_MemCheckCounters(__func__);
	pr_info("player2 probe done ok\n");

	return 0;

failinit:
	SysfsDelete();
failsysfs:
	report_term();
	pr_err("Error: %s player2 probe failed\n", __func__);
	return ret;
}

// SE remove function (called on SE module unload)
static int stm_se_remove(struct platform_device *pdev)
{
	OS_Dump_MemCheckCounters(__func__);

#ifdef CONFIG_PM
#ifdef CONFIG_PM_RUNTIME
	// Disable SE power management
	pm_runtime_disable(&pdev->dev);
#endif // CONFIG_PM_RUNTIME

	// Unregister with PM notifiers
	unregister_pm_notifier(&stm_pm_se_notifier);

	// Unregister tuneable allowing to select low power test mode (HPS or CPS)
	OS_UnregisterTuneable(TUNEABLE_NAME_LOW_POWER_TEST_MODE);
#endif // CONFIG_PM

	// Finally terminate the SE
	stm_se_term();

	// Destroy sysfs entries for mixer transformer selection
	remove_sysfs_mixer_selection(&pdev->dev);

	SysfsDelete();
	report_term();

	OS_Dump_MemCheckCounters(__func__);
	pr_info("player2 remove done\n");

	return 0;
}

static struct platform_driver stm_se_driver = {
	.driver = {
		.name = "stm-se",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &stm_pm_se_pm_ops,
#endif
	},
	.probe = stm_se_probe,
	.remove = stm_se_remove,
};
static struct platform_device stm_se_device = {
	.name = "stm-se",
	.id = 0,
	.dev.platform_data = NULL,
};

static __init int player_load(void)
{
	platform_device_register(&stm_se_device);
	platform_driver_register(&stm_se_driver);
	return 0;
}

static void __exit player_unload(void)
{
	platform_driver_unregister(&stm_se_driver);
	platform_device_unregister(&stm_se_device);
}

module_init(player_load);
module_exit(player_unload);
//module_platform_driver(stm_se_driver);

// Exported SE APIs

//
EXPORT_SYMBOL(stm_se_set_control);
EXPORT_SYMBOL(stm_se_get_control);
EXPORT_SYMBOL(stm_se_get_compound_control);
EXPORT_SYMBOL(stm_se_set_error_handler);
//
EXPORT_SYMBOL(stm_se_playback_new);
EXPORT_SYMBOL(stm_se_playback_delete);
EXPORT_SYMBOL(stm_se_playback_set_control);
EXPORT_SYMBOL(stm_se_playback_get_control);
EXPORT_SYMBOL(stm_se_playback_set_speed);
EXPORT_SYMBOL(stm_se_playback_get_speed);
EXPORT_SYMBOL(stm_se_playback_set_native_time);
EXPORT_SYMBOL(stm_se_playback_set_clock_data_point);
EXPORT_SYMBOL(stm_se_playback_get_clock_data_point);

//
EXPORT_SYMBOL(stm_se_play_stream_new);
EXPORT_SYMBOL(stm_se_play_stream_delete);
EXPORT_SYMBOL(stm_se_play_stream_set_control);
EXPORT_SYMBOL(stm_se_play_stream_get_control);
EXPORT_SYMBOL(stm_se_play_stream_set_enable);
EXPORT_SYMBOL(stm_se_play_stream_get_enable);
EXPORT_SYMBOL(stm_se_play_stream_get_compound_control);
EXPORT_SYMBOL(stm_se_play_stream_set_compound_control);
EXPORT_SYMBOL(stm_se_play_stream_attach);
EXPORT_SYMBOL(stm_se_play_stream_attach_to_pad);
EXPORT_SYMBOL(stm_se_play_stream_detach);
EXPORT_SYMBOL(stm_se_play_stream_inject_data);
EXPORT_SYMBOL(stm_se_play_stream_inject_discontinuity);
EXPORT_SYMBOL(stm_se_play_stream_drain);
EXPORT_SYMBOL(stm_se_play_stream_step);
EXPORT_SYMBOL(stm_se_play_stream_switch);
EXPORT_SYMBOL(stm_se_play_stream_get_info);
EXPORT_SYMBOL(stm_se_play_stream_set_interval);
EXPORT_SYMBOL(stm_se_play_stream_poll_message);
EXPORT_SYMBOL(stm_se_play_stream_get_message);
EXPORT_SYMBOL(stm_se_play_stream_subscribe);
EXPORT_SYMBOL(stm_se_play_stream_unsubscribe);
EXPORT_SYMBOL(stm_se_play_stream_register_buffer_capture_callback);
EXPORT_SYMBOL(stm_se_play_stream_set_alarm);
EXPORT_SYMBOL(stm_se_play_stream_set_discard_trigger);
EXPORT_SYMBOL(stm_se_play_stream_reset_discard_triggers);
//
EXPORT_SYMBOL(stm_se_advanced_audio_mixer_new);
EXPORT_SYMBOL(stm_se_audio_mixer_new);
EXPORT_SYMBOL(stm_se_audio_mixer_delete);
EXPORT_SYMBOL(stm_se_audio_mixer_set_control);
EXPORT_SYMBOL(stm_se_audio_mixer_get_control);
EXPORT_SYMBOL(stm_se_audio_mixer_set_compound_control);
EXPORT_SYMBOL(stm_se_audio_mixer_get_compound_control);
EXPORT_SYMBOL(stm_se_audio_mixer_attach);
EXPORT_SYMBOL(stm_se_audio_mixer_detach);
//
EXPORT_SYMBOL(stm_se_component_set_module_parameters);
//
EXPORT_SYMBOL(stm_se_audio_generator_new);
EXPORT_SYMBOL(stm_se_audio_generator_delete);
EXPORT_SYMBOL(stm_se_audio_generator_attach);
EXPORT_SYMBOL(stm_se_audio_generator_detach);
EXPORT_SYMBOL(stm_se_audio_generator_get_compound_control);
EXPORT_SYMBOL(stm_se_audio_generator_set_compound_control);
EXPORT_SYMBOL(stm_se_audio_generator_get_control);
EXPORT_SYMBOL(stm_se_audio_generator_set_control);
EXPORT_SYMBOL(stm_se_audio_generator_get_info);
EXPORT_SYMBOL(stm_se_audio_generator_commit);
EXPORT_SYMBOL(stm_se_audio_generator_start);
EXPORT_SYMBOL(stm_se_audio_generator_stop);
//
EXPORT_SYMBOL(stm_se_audio_reader_new);
EXPORT_SYMBOL(stm_se_audio_reader_delete);
EXPORT_SYMBOL(stm_se_audio_reader_attach);
EXPORT_SYMBOL(stm_se_audio_reader_detach);
EXPORT_SYMBOL(stm_se_audio_reader_get_compound_control);
EXPORT_SYMBOL(stm_se_audio_reader_set_compound_control);
EXPORT_SYMBOL(stm_se_audio_reader_get_control);
EXPORT_SYMBOL(stm_se_audio_reader_set_control);
//
EXPORT_SYMBOL(stm_se_audio_player_new);
EXPORT_SYMBOL(stm_se_audio_player_delete);
EXPORT_SYMBOL(stm_se_audio_player_set_control);
EXPORT_SYMBOL(stm_se_audio_player_get_control);
EXPORT_SYMBOL(stm_se_audio_player_set_compound_control);
EXPORT_SYMBOL(stm_se_audio_player_get_compound_control);
//
EXPORT_SYMBOL(stm_se_encode_new);
EXPORT_SYMBOL(stm_se_encode_delete);
EXPORT_SYMBOL(stm_se_encode_get_control);
EXPORT_SYMBOL(stm_se_encode_set_control);
//
EXPORT_SYMBOL(stm_se_encode_stream_new);
EXPORT_SYMBOL(stm_se_encode_stream_delete);
EXPORT_SYMBOL(stm_se_encode_stream_attach);
EXPORT_SYMBOL(stm_se_encode_stream_detach);
EXPORT_SYMBOL(stm_se_encode_stream_get_control);
EXPORT_SYMBOL(stm_se_encode_stream_set_control);
EXPORT_SYMBOL(stm_se_encode_stream_get_compound_control);
EXPORT_SYMBOL(stm_se_encode_stream_set_compound_control);
EXPORT_SYMBOL(stm_se_encode_stream_drain);
EXPORT_SYMBOL(stm_se_encode_stream_inject_frame);
EXPORT_SYMBOL(stm_se_encode_stream_inject_discontinuity);
//
EXPORT_SYMBOL(__stm_se_play_stream_get_statistics);
EXPORT_SYMBOL(__stm_se_play_stream_reset_statistics);
EXPORT_SYMBOL(__stm_se_play_stream_get_attributes);
EXPORT_SYMBOL(__stm_se_play_stream_reset_attributes);
//
EXPORT_SYMBOL(__stm_se_pm_low_power_enter);
EXPORT_SYMBOL(__stm_se_pm_low_power_exit);
