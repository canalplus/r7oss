#ifndef __SOUND_FIRMWARE_COMPAT_H
#define __SOUND_FIRMWARE_COMPAT_H

#define FIRMWARE_NAME_MAX 30

struct firmware {
	size_t size;
	u8 *data;
};

int snd_compat_request_firmware(const struct firmware **fw, const char *name);
void snd_compat_release_firmware(const struct firmware *fw);

#define request_firmware(fw, name, device) snd_compat_request_firmware(fw, name)
#define release_firmware(fw) snd_compat_release_firmware(fw)

#define NEEDS_COMPAT_FW_LOADER

#endif /* __SOUND_FIRMWARE_COMPAT_H */
