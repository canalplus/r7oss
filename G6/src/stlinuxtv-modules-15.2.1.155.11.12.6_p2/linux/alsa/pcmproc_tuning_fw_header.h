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

#define PROFILE_NAME_MAX_LENGTH 32
#define MAX_NUMBER_OF_PROFILE 16

enum PcmProcessingType_e {
	PCM_PROCESSING_TYPE_VOLUME_MANAGERS,
	PCM_PROCESSING_TYPE_VIRTUALIZERS,
	PCM_PROCESSING_TYPE_UPMIXERS,
	PCM_PROCESSING_TYPE_DIALOG_ENHANCER,

	PCM_PROCESSING_TYPE_LAST
};

struct tuningFw_header_s {
	enum PcmProcessingType_e PcmProcessingType;
	int                      number_of_profiles;
	int                      profile_struct_size;
	char                     profiles_names[MAX_NUMBER_OF_PROFILE]
						[PROFILE_NAME_MAX_LENGTH];
};
