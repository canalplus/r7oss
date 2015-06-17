/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

pseudo_pcmproc_tuning.c allows to request and parse firmware dedicated
for PCM processing coefficients tuning

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

This may alternatively be licensed under a proprietary license from ST.
************************************************************************/
#include <linux/firmware.h>
#include <sound/control.h>

#include "stm_se.h"
#include "pseudocard.h"

/* Firmware data Storing structure  */
struct profile_desc_s {
	char               *ProfileName;
	void               *ProfileData;
};

static int profileCountArray[PCM_PROCESSING_TYPE_LAST];
static struct profile_desc_s profileArray[PCM_PROCESSING_TYPE_LAST]
					[MAX_NUMBER_OF_PROFILE];

/*************************************************/
/*          Tuning Firmware parsing              */
/*************************************************/
static void dumpFWHeader(struct tuningFw_header_s *header, char *fwName)
{
	int i;

	printk(KERN_DEBUG "\n***audioTuningFW header***\n");
	printk(KERN_DEBUG "FirwmareName   = %s\n", fwName);
	printk(KERN_DEBUG "PcmProcessingID= %d\n", header->PcmProcessingType);
	printk(KERN_DEBUG "nb_of_profiles = %d\n", header->number_of_profiles);
	printk(KERN_DEBUG "ProfileSize    = %d\n", header->profile_struct_size);

	for (i = 0; i < header->number_of_profiles; i++) {
		printk(KERN_DEBUG "profile[%02d]    = %s\n", i,
			header->profiles_names[i]);
	}
	printk(KERN_DEBUG "\n");
}

static int checkFWsyntax(struct tuningFw_header_s *header, size_t fwSize)
{
	size_t computedSize = 0;

	/* Check if header access is valid */
	if (fwSize < sizeof(struct tuningFw_header_s)) {
		printk(KERN_ERR "checkFWsyntax ERROR: Cannot read header\n");
		return -EINVAL;
	}

	/* Check PCM PROCESSING TYPE */
	if (header->PcmProcessingType >= PCM_PROCESSING_TYPE_LAST) {
		printk(KERN_ERR "checkFWsyntax ERROR: BAD PCMPROCESSING TYPE\n");
		return -EINVAL;
	}

	/* Check if firmware size is consistent with header's information */
	computedSize  = sizeof(struct tuningFw_header_s);
	computedSize += header->number_of_profiles*header->profile_struct_size;

	if (computedSize != fwSize) {
		printk(KERN_ERR "checkFWsyntax ERROR: Unconsistent FW size:\n");
		printk(KERN_ERR "FWsize:%dbytes Header Computed size:%dbytes\n",
			fwSize, computedSize);
		return -EINVAL;
	}

	return 0;
}

static int parse_pcmproc_fw(const struct firmware *fw, char *fwName)
{
	int err, i;
	int *profile_idx;
	struct tuningFw_header_s *header = (struct tuningFw_header_s *)fw->data;
	struct profile_desc_s *profileType;
	char *pcmProcDataAddr;

	err = checkFWsyntax(header, fw->size);
	if (err)
		return err;

	dumpFWHeader(header, fwName);

	/* Store Profile name and Profile Data Address for each profile */
	profile_idx = &profileCountArray[header->PcmProcessingType];
	profileType = &profileArray[header->PcmProcessingType][0];

	for (i = 0; i < header->number_of_profiles &&
		*profile_idx < MAX_NUMBER_OF_PROFILE; i++) {
		profileType[*profile_idx].ProfileName =
						header->profiles_names[i];

		pcmProcDataAddr  = (char *)header;
		pcmProcDataAddr += sizeof(struct tuningFw_header_s);
		pcmProcDataAddr += header->profile_struct_size * i;

		profileType[*profile_idx].ProfileData = (void *)pcmProcDataAddr;

		(*profile_idx)++;
	}
	return 0;
}

static void register_pcmproc_fw(struct device *dev,
				struct pcmproc_FW_s *pcmproc_fw_ctx,
				char *fwName)
{
	int err;
	const struct firmware **fw;

	/* Check firmware handle storage availability */
	if (pcmproc_fw_ctx->nb_firmware >= MAX_NUMBER_OF_PCMPROC_FW) {
		printk(KERN_ERR
			"Max number of PCMPROC Firmwares(%d) exceeded\n",
			MAX_NUMBER_OF_PCMPROC_FW);
		return;
	}

	/* Request the firmware */
	fw = &pcmproc_fw_ctx->fwArray[pcmproc_fw_ctx->nb_firmware];
	err = request_firmware(fw, fwName, dev);
	if (err) {
		printk(KERN_ERR "register_pcmproc_fw: Cannot request %s(%d)\n",
			fwName, err);
		return;
	}

	pcmproc_fw_ctx->nb_firmware++;

	/* Parse the firmware */
	parse_pcmproc_fw(*fw, fwName);
}

/*************************************************/
/*     ALSA controls for PCM Processings         */
/*************************************************/
static int snd_tuning_ctrl_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	int ctrl = kcontrol->private_value;
	struct profile_desc_s *profile;

	if (ctrl >= PCM_PROCESSING_TYPE_LAST)
		return -EINVAL;

	uinfo->type  = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = profileCountArray[ctrl];
	if (uinfo->value.enumerated.item > profileCountArray[ctrl] - 1)
		uinfo->value.enumerated.item = profileCountArray[ctrl] - 1;

	profile = &profileArray[ctrl][uinfo->value.enumerated.item];
	strlcpy(uinfo->value.enumerated.name, profile->ProfileName,
		sizeof(uinfo->value.enumerated.name));

	return 0;
}

static int snd_get_ctrl_from_tuning_type(int ctrl, stm_se_ctrl_t *se_ctrl)
{
	if (ctrl >= PCM_PROCESSING_TYPE_LAST)
		return -EINVAL;

	switch (ctrl) {
	case PCM_PROCESSING_TYPE_VOLUME_MANAGERS:
		*se_ctrl = STM_SE_CTRL_VOLUME_MANAGER;
		break;
	case PCM_PROCESSING_TYPE_VIRTUALIZERS:
		*se_ctrl = STM_SE_CTRL_VIRTUALIZER;
		break;
	case PCM_PROCESSING_TYPE_UPMIXERS:
		*se_ctrl = STM_SE_CTRL_UPMIXER;
		break;
	case PCM_PROCESSING_TYPE_DIALOG_ENHANCER:
		*se_ctrl = STM_SE_CTRL_DIALOG_ENHANCER;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int snd_mixer_tuning_ctrl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;

	ucontrol->value.enumerated.item[0] =
			pseudo->mixer_tuning_kcontrols_values[ctrl];
	return 0;
}

static int snd_mixer_tuning_ctrl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int err;
	stm_se_ctrl_t se_ctrl;
	struct profile_desc_s *profile;

	err = snd_get_ctrl_from_tuning_type(ctrl, &se_ctrl);
	if (err)
		return err;

	if (ucontrol->value.enumerated.item[0] >= profileCountArray[ctrl])
		return -EINVAL;

	profile = &profileArray[ctrl][ucontrol->value.enumerated.item[0]];

	err = stm_se_audio_mixer_set_compound_control(pseudo->backend_mixer,
						se_ctrl, profile->ProfileData);

	if (!err) {
		pseudo->mixer_tuning_kcontrols_values[ctrl] =
					ucontrol->value.enumerated.item[0];
	} else {
		printk(KERN_ERR "Failed to set profile[%d]:%s on Mixer\n",
				ucontrol->value.enumerated.item[0],
				profile->ProfileName);
	}

	return 0;
}

#define MIXER_TUNING_CTRL(xProcType, xName)                        \
	{                                                          \
		.iface         = SNDRV_CTL_ELEM_IFACE_MIXER,       \
		.name          = xName,                            \
		.info          = snd_tuning_ctrl_info,             \
		.get           = snd_mixer_tuning_ctrl_get,        \
		.put           = snd_mixer_tuning_ctrl_put,        \
		.private_value = xProcType,                        \
	}

static struct snd_kcontrol_new mixer_tuning_controls[] = {
[PCM_PROCESSING_TYPE_VOLUME_MANAGERS] = MIXER_TUNING_CTRL
	(PCM_PROCESSING_TYPE_VOLUME_MANAGERS, "Volume Manager Profile"),

[PCM_PROCESSING_TYPE_VIRTUALIZERS] = MIXER_TUNING_CTRL
	(PCM_PROCESSING_TYPE_VIRTUALIZERS, "Virtualizer Profile"),

[PCM_PROCESSING_TYPE_UPMIXERS] = MIXER_TUNING_CTRL
	(PCM_PROCESSING_TYPE_UPMIXERS, "Upmixer Profile"),

[PCM_PROCESSING_TYPE_DIALOG_ENHANCER] = MIXER_TUNING_CTRL
	(PCM_PROCESSING_TYPE_DIALOG_ENHANCER, "Dialog Enhancer Profile"),
};

static int snd_player_tuning_ctrl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo_hw_player_entity *player =
						snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;

	ucontrol->value.enumerated.item[0] =
				player->tuning_kcontrols_values[ctrl];

	return 0;
}

static int snd_player_tuning_ctrl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo_hw_player_entity *player =
						snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int err;
	stm_se_ctrl_t se_ctrl;
	struct profile_desc_s *profile;

	err = snd_get_ctrl_from_tuning_type(ctrl, &se_ctrl);
	if (err)
		return err;

	if (ucontrol->value.enumerated.item[0] >= profileCountArray[ctrl])
		return -EINVAL;

	profile = &profileArray[ctrl][ucontrol->value.enumerated.item[0]];

	err = stm_se_audio_player_set_compound_control(player->backend_player,
						se_ctrl, profile->ProfileData);
	if (!err) {
		player->tuning_kcontrols_values[ctrl] =
					ucontrol->value.enumerated.item[0];
	} else {
		printk(KERN_ERR "Failed to set profile[%d]:%s on Player\n",
				ucontrol->value.enumerated.item[0],
				profile->ProfileName);
	}

	return 0;
}

#define PLAYER_TUNING_CTRL(xProcType, xName)                       \
	{                                                          \
		.iface         = SNDRV_CTL_ELEM_IFACE_MIXER,       \
		.name          = xName,                            \
		.info          = snd_tuning_ctrl_info,             \
		.get           = snd_player_tuning_ctrl_get,       \
		.put           = snd_player_tuning_ctrl_put,       \
		.private_value = xProcType,                        \
	}

static struct snd_kcontrol_new player_tuning_controls[] = {
[PCM_PROCESSING_TYPE_VOLUME_MANAGERS] = PLAYER_TUNING_CTRL
	(PCM_PROCESSING_TYPE_VOLUME_MANAGERS, "Volume Manager Profile"),

[PCM_PROCESSING_TYPE_VIRTUALIZERS] = PLAYER_TUNING_CTRL
	(PCM_PROCESSING_TYPE_VIRTUALIZERS, "Virtualizer Profile"),

[PCM_PROCESSING_TYPE_UPMIXERS] = PLAYER_TUNING_CTRL
	(PCM_PROCESSING_TYPE_UPMIXERS, "Upmixer Profile"),

[PCM_PROCESSING_TYPE_DIALOG_ENHANCER] = PLAYER_TUNING_CTRL
	(PCM_PROCESSING_TYPE_DIALOG_ENHANCER, "Dialog Enhancer Profile"),
};

/* Tuning Amount control definition */

static int snd_get_amount_ctrl_from_tuning_type(int ctrl, stm_se_ctrl_t *se_ctrl)
{
	if (ctrl >= PCM_PROCESSING_TYPE_LAST)
		return -EINVAL;

	switch (ctrl) {
	case PCM_PROCESSING_TYPE_VOLUME_MANAGERS:
		*se_ctrl = STM_SE_CTRL_VOLUME_MANAGER_AMOUNT;
		break;
	case PCM_PROCESSING_TYPE_VIRTUALIZERS:
		*se_ctrl = STM_SE_CTRL_VIRTUALIZER_AMOUNT;
		break;
	case PCM_PROCESSING_TYPE_UPMIXERS:
		*se_ctrl = STM_SE_CTRL_UPMIXER_AMOUNT;
		break;
	case PCM_PROCESSING_TYPE_DIALOG_ENHANCER:
		*se_ctrl = STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int snd_tuning_amount_ctrl_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 100;
	return 0;
}

static int snd_mixer_tuning_amount_ctrl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int err, val;
	stm_se_ctrl_t se_ctrl;

	err = snd_get_amount_ctrl_from_tuning_type(ctrl, &se_ctrl);
	if (err)
		return err;

	err = stm_se_audio_mixer_get_control(pseudo->backend_mixer, se_ctrl, &val);

	if (!err)
		ucontrol->value.integer.value[0] = val;
	else
		printk(KERN_ERR "Failed to get tuning amount value on player\n");

	return 0;
}

static int snd_mixer_tuning_amount_ctrl_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo *pseudo = snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int err;
	stm_se_ctrl_t se_ctrl;

	err = snd_get_amount_ctrl_from_tuning_type(ctrl, &se_ctrl);
	if (err)
		return err;

	err = stm_se_audio_mixer_set_control(pseudo->backend_mixer, se_ctrl,
				ucontrol->value.integer.value[0]);

	if (err)
		printk(KERN_ERR "Failed to set tuning amount=%ld on mixer\n",
				ucontrol->value.integer.value[0]);

	return 0;
}

#define MIXER_AMOUNT_CTRL(xProcType, xName)                          \
	{                                                            \
		.iface         = SNDRV_CTL_ELEM_IFACE_MIXER,         \
		.name          = xName,                              \
		.info          = snd_tuning_amount_ctrl_info,        \
		.get           = snd_mixer_tuning_amount_ctrl_get,   \
		.put           = snd_mixer_tuning_amount_ctrl_put,   \
		.private_value = xProcType,                          \
	}

static struct snd_kcontrol_new mixer_amount_controls[] = {
[PCM_PROCESSING_TYPE_VOLUME_MANAGERS] = MIXER_AMOUNT_CTRL
	(PCM_PROCESSING_TYPE_VOLUME_MANAGERS, "Volume Manager Amount"),

[PCM_PROCESSING_TYPE_VIRTUALIZERS] = MIXER_AMOUNT_CTRL
	(PCM_PROCESSING_TYPE_VIRTUALIZERS, "Virtualizer Amount"),

[PCM_PROCESSING_TYPE_UPMIXERS] = MIXER_AMOUNT_CTRL
	(PCM_PROCESSING_TYPE_UPMIXERS, "Upmixer Amount"),

[PCM_PROCESSING_TYPE_DIALOG_ENHANCER] = MIXER_AMOUNT_CTRL
	(PCM_PROCESSING_TYPE_DIALOG_ENHANCER, "Dialog Enhancer Amount"),
};

static int snd_player_tuning_amount_ctrl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo_hw_player_entity *player =
						snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int err, val;
	stm_se_ctrl_t se_ctrl;

	err = snd_get_amount_ctrl_from_tuning_type(ctrl, &se_ctrl);
	if (err)
		return err;

	err = stm_se_audio_player_get_control(player->backend_player, se_ctrl, &val);

	if (!err)
		ucontrol->value.integer.value[0] = val;
	else
		printk(KERN_ERR "Failed to get tuning amount value on player\n");

	return 0;
}

static int snd_player_tuning_amount_ctrl_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pseudo_hw_player_entity *player =
						snd_kcontrol_chip(kcontrol);
	int ctrl = kcontrol->private_value;
	int err;
	stm_se_ctrl_t se_ctrl;

	err = snd_get_amount_ctrl_from_tuning_type(ctrl, &se_ctrl);
	if (err)
		return err;

	err = stm_se_audio_player_set_control(player->backend_player, se_ctrl,
					ucontrol->value.integer.value[0]);

	if (err)
		printk(KERN_ERR "Failed to set tuning amount=%ld on player\n",
				ucontrol->value.integer.value[0]);

	return 0;
}
#define PLAYER_AMOUNT_CTRL(xProcType, xName)                         \
	{                                                            \
		.iface         = SNDRV_CTL_ELEM_IFACE_MIXER,         \
		.name          = xName,                              \
		.info          = snd_tuning_amount_ctrl_info,        \
		.get           = snd_player_tuning_amount_ctrl_get,  \
		.put           = snd_player_tuning_amount_ctrl_put,  \
		.private_value = xProcType,                          \
	}

static struct snd_kcontrol_new player_amount_controls[] = {
[PCM_PROCESSING_TYPE_VOLUME_MANAGERS] = PLAYER_AMOUNT_CTRL
	(PCM_PROCESSING_TYPE_VOLUME_MANAGERS, "Volume Manager Amount"),

[PCM_PROCESSING_TYPE_VIRTUALIZERS] = PLAYER_AMOUNT_CTRL
	(PCM_PROCESSING_TYPE_VIRTUALIZERS, "Virtualizer Amount"),

[PCM_PROCESSING_TYPE_UPMIXERS] = PLAYER_AMOUNT_CTRL
	(PCM_PROCESSING_TYPE_UPMIXERS, "Upmixer Amount"),

[PCM_PROCESSING_TYPE_DIALOG_ENHANCER] = PLAYER_AMOUNT_CTRL
	(PCM_PROCESSING_TYPE_DIALOG_ENHANCER, "Dialog Enhancer Amount"),
};

static int add_tuning_kcontrol(struct snd_kcontrol **kctlPtr,
				struct snd_kcontrol_new *kctrlNew,
				char *namePrefix,
				struct snd_card *card,
				void *private_data)
{
	int err;
	struct snd_kcontrol *kctl;

	if (!kctrlNew) {
		printk(KERN_ERR "add_tuning_kcontrol: Bad snd_kcontrol_new\n");
		return -EINVAL;
	}

	kctl = snd_ctl_new1(kctrlNew, private_data);
	if (!kctl) {
		printk(KERN_ERR "add_tuning_kcontrol: snd_ctl_new1 FAILED\n");
		return -EINVAL;
	}

	if (namePrefix)
		snprintf(kctl->id.name, sizeof(kctl->id.name),
			 "%s %s", namePrefix, kctrlNew->name);

	err = snd_ctl_add(card, kctl);
	if (err < 0) {
		printk(KERN_ERR
			"add_tuning_kcontrol: snd_ctl_add FAILED (err:%d)\n",
			err);
		return err;
	}

	*kctlPtr = kctl;

	return 0;
}

/*************************************************/
/*             Non-static functions              */
/*************************************************/
void request_pcmproc_fw(struct device *dev,
			struct pcmproc_FW_s *pcmproc_fw_ctx,
			int tuningFwNumber,
			char **fwNames)
{
	int i;
	printk(KERN_DEBUG "request_pcmproc_fw: FW ctx users:%d\n",
		pcmproc_fw_ctx->users);

	/* Request firmwares only for the first caller */
	if (pcmproc_fw_ctx->users++ == 0) {
		/* Request all firwmares and parse data */
		for (i = 0; i < tuningFwNumber; i++)
			register_pcmproc_fw(dev, pcmproc_fw_ctx, fwNames[i]);
	}
}

void release_pcmproc_fw(struct pcmproc_FW_s *pcmproc_fw_ctx,
			struct snd_pseudo *pseudo)
{
	int i;

	printk(KERN_DEBUG "release_pcmproc_fw: FW ctx users:%d\n",
		pcmproc_fw_ctx->users);

	/* Release firmwares only for the last caller */
	if (--pcmproc_fw_ctx->users > 0)
		return;

	printk(KERN_DEBUG "release_pcmproc_fw: RELEASE FW\n");
	for (i = 0; i < pcmproc_fw_ctx->nb_firmware; i++) {
		release_firmware(pcmproc_fw_ctx->fwArray[i]);
		pcmproc_fw_ctx->fwArray[i] = NULL;
	}
	pcmproc_fw_ctx->nb_firmware = 0;
}

void add_mixer_tuning_controls(struct snd_pseudo *pseudo)
{
	int i, ret;
	struct snd_kcontrol *kctrl;

	/* For each PCM processing type found, create associated control */
	for (i = 0; i < PCM_PROCESSING_TYPE_LAST; i++) {
		if (profileCountArray[i] > 0) {
			ret = add_tuning_kcontrol(&kctrl,
					    &mixer_tuning_controls[i],
					    NULL,
					    pseudo->card,
					    (void *) pseudo);

			if (!ret)
				pseudo->mixer_tuning_kcontrols[i] = kctrl;
			else
				pseudo->mixer_tuning_kcontrols[i] = NULL;

			ret = add_tuning_kcontrol(&kctrl,
					    &mixer_amount_controls[i],
					    NULL,
					    pseudo->card,
					    (void *) pseudo);
			if (!ret)
				pseudo->mixer_tuning_amount_kcontrols[i] = kctrl;
			else
				pseudo->mixer_tuning_amount_kcontrols[i] = NULL;

		} else {
			pseudo->mixer_tuning_kcontrols[i] = NULL;
			pseudo->mixer_tuning_amount_kcontrols[i] = NULL;
		}
	}
}

void remove_mixer_tuning_controls(struct snd_pseudo *pseudo)
{
	int i;

	/* Delete controls associated to this mixer object */
	for (i = 0; i < PCM_PROCESSING_TYPE_LAST; i++) {
		if (pseudo->mixer_tuning_kcontrols[i] != NULL) {
			snd_ctl_remove(pseudo->card,
					pseudo->mixer_tuning_kcontrols[i]);
			pseudo->mixer_tuning_kcontrols[i] = NULL;
		}
		if (pseudo->mixer_tuning_amount_kcontrols[i] != NULL) {
			snd_ctl_remove(pseudo->card,
				pseudo->mixer_tuning_amount_kcontrols[i]);
			pseudo->mixer_tuning_amount_kcontrols[i] = NULL;
		}
	}
}

void add_player_tuning_controls(struct snd_pseudo *pseudo,
				struct snd_pseudo_hw_player_entity *player)
{
	int i, ret;
	struct snd_kcontrol *kctrl;

	/* For each PCM processing type found, create associated control */
	for (i = 0; i < PCM_PROCESSING_TYPE_LAST; i++) {
		if (profileCountArray[i] > 0) {
			ret = add_tuning_kcontrol(&kctrl,
					&player_tuning_controls[i],
					player->card->name, pseudo->card,
					(void *) player);
			if (!ret)
				player->tuning_kcontrols[i] = kctrl;
			else
				player->tuning_kcontrols[i] = NULL;

			ret = add_tuning_kcontrol(&kctrl,
					&player_amount_controls[i],
					player->card->name, pseudo->card,
					(void *) player);
			if (!ret)
				player->tuning_amount_kcontrols[i] = kctrl;
			else
				player->tuning_amount_kcontrols[i] = NULL;

		} else {
			player->tuning_kcontrols[i] = NULL;
			player->tuning_amount_kcontrols[i] = NULL;
		}
	}
}

void remove_player_tuning_controls(struct snd_pseudo *pseudo,
				struct snd_pseudo_hw_player_entity *player)
{
	int i;

	/* Delete controls associated to this player object */
	for (i = 0; i < PCM_PROCESSING_TYPE_LAST; i++) {
		if (player->tuning_kcontrols[i] != NULL) {
			snd_ctl_remove(pseudo->card,
					player->tuning_kcontrols[i]);
			player->tuning_kcontrols[i] = NULL;
		}
		if (player->tuning_amount_kcontrols[i] != NULL) {
			snd_ctl_remove(pseudo->card,
					player->tuning_amount_kcontrols[i]);
			player->tuning_amount_kcontrols[i] = NULL;
		}
	}
}
