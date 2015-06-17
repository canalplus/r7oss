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

#if __KERNEL__
#include "osdev_sched.h"
#else
#define SCHED_RR 0
#endif

#include "player_threads.h"

// tasks descriptions table: name, policy, prio
// keep order in sync with tasks_desc_e indexes
OS_TaskDesc_t player_tasks_desc[] = {
	{ "SE-Aud-CtoP"  , -1 , -1 },
	{ "SE-Aud-PtoD"  , -1 , -1  },
	{ "SE-Aud-DtoM"  , -1 , -1  },
	{ "SE-Aud-MPost" , -1 , -1  },

	{ "SE-Vid-CtoP"  , -1 , -1  },
	{ "SE-Vid-PtoD"  , -1 , -1  },
	{ "SE-Vid-DtoM"  , -1 , -1  },
	{ "SE-Vid-MPost" , -1 , -1  },

	{ "SE-Vid-H264Int"  , -1 , -1 },
	{ "SE-Vid-HevcInt"  , -1 , -1 },

	{ "SE-Aud-Reader"   , -1 , -1 },
	{ "SE-Aud-StreamT"  , -1 , -1 },
	{ "SE-Aud-Mixer"    , -1 , -1 },
	{ "SE-Aud-BcstMix"  , -1 , -1 },
	{ "SE-Aud-Bypass"   , -1 , -1 },

	{ "SE-Man-Coord"      , -1 , -1 },
	{ "SE-Man-BrSrcGrab"  , -1 , -1 },
	{ "SE-Man-BrCapture"  , -1 , -1 },
	{ "SE-Man-DsVideo"    , -1 , -1 },

	{ "SE-Playbck-Drain" , -1 , -1 },

	{ "SE-EncAud-ItoP", -1 , -1 },
	{ "SE-EncAud-PtoC", -1 , -1 },
	{ "SE-EncAud-CtoO", -1 , -1 },

	{ "SE-EncVid-ItoP", -1 , -1 },
	{ "SE-EncVid-PtoC", -1 , -1 },
	{ "SE-EncVid-CtoO", -1 , -1 },

	{ "SE-Encod-Coord", -1 , -1 },

	{ "SE-Other", -1 , -1 },
};

// init table for thread policy and prio => used for module parameters
// keep order in sync with tasks_desc_e indexes
int thpolprio_inits[][2] = {
	/* SE-Aud-CtoP" */       { SCHED_RR, OS_MID_PRIORITY + 8  },
	/* SE-Aud-PtoD" */       { SCHED_RR, OS_MID_PRIORITY + 8  },
	/* SE-Aud-DtoM" */       { SCHED_RR, OS_MID_PRIORITY + 10 },
	/* SE-Aud-MPost" */      { SCHED_RR, OS_MID_PRIORITY + 12 },

	/* SE-Vid-CtoP" */       { SCHED_RR, OS_MID_PRIORITY + 8  },
	/* SE-Vid-PtoD" */       { SCHED_RR, OS_MID_PRIORITY + 8  },
	/* SE-Vid-DtoM" */       { SCHED_RR, OS_MID_PRIORITY + 10 },
	/* SE-Vid-MPost" */      { SCHED_RR, OS_MID_PRIORITY + 12 },

	/* SE-Vid-H264Int" */    { SCHED_RR, OS_MID_PRIORITY + 9  },
	/* SE-Vid-HevcInt" */    { SCHED_RR, OS_MID_PRIORITY + 9  },

	/* SE-Aud-Reader" */     { SCHED_RR, OS_MID_PRIORITY + 14 },
	/* SE-Aud-StreamT" */    { SCHED_RR, OS_MID_PRIORITY + 9  },
	/* SE-Aud-Mixer" */      { SCHED_RR, OS_MID_PRIORITY + 14 },
	/* SE-Aud-BcastMixer"*/  { SCHED_RR, OS_MID_PRIORITY + 14 },
	/* SE-Aud-Bypass" */     { SCHED_RR, OS_MID_PRIORITY + 14 },

	/* SE-Man-Coord" */      { SCHED_RR, OS_MID_PRIORITY + 9  },
	/* SE-Man-BrSrcGrab" */  { SCHED_RR, OS_MID_PRIORITY      },
	/* SE-Man-BrCapture" */  { SCHED_RR, OS_MID_PRIORITY      },
	/* SE-Man-DsVideo" */    { SCHED_RR, (OS_HIGHEST_PRIORITY + OS_MID_PRIORITY) / 2 },

	/* SE-Playbck-Drain" */  { SCHED_RR, OS_MID_PRIORITY + 14 },

	/* SE-EncAud-ItoP" */    { SCHED_RR, OS_MID_PRIORITY + 11 },
	/* SE-EncAud-PtoC" */    { SCHED_RR, OS_MID_PRIORITY + 8  },
	/* SE-EncAud-CtoO" */    { SCHED_RR, OS_MID_PRIORITY + 9  },

	/* SE-EncVid-ItoP" */    { SCHED_RR, OS_MID_PRIORITY + 11 },
	/* SE-EncVid-PtoC" */    { SCHED_RR, OS_MID_PRIORITY + 8  },
	/* SE-EncVid-CtoO" */    { SCHED_RR, OS_MID_PRIORITY + 9  },

	/* SE-Encod-Coord" */    { SCHED_RR, OS_MID_PRIORITY + 11 },

	/* SE-Other" */          { SCHED_RR, OS_MID_PRIORITY  },
};

