/*
   toposet - update the player2 audio topology
   Copyright (C) 2003 by Takashi Iwai <tiwai@suse.de>
   Copyright (C) 2008 by STMicroelectronics (R&D) Limited <daniel.thompson@st.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <alsa/asoundlib.h>

#define SND_PSEUDO_MAX_OUTPUTS          4

enum snd_pseudo_mixer_channel_pair {                             /* pair0   pair1   pair2   pair3   pair4   */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_DEFAULT,                   /*   Y       Y       Y       Y       Y     */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_L_R = 0,                   /*   Y                               Y     */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE1 = 0,             /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSUR_RSUR = 0,             /*                   Y                     */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSURREAR_RSURREAR = 0,     /*                           Y             */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_LT_RT,                     /*   Y                               Y     */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LPLII_RPLII,               /*                                         */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTRL_CNTRR,		 /*                                         */		
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGH_RHIGH,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LWIDE_RWIDE,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LRDUALMONO,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_RESERVED1,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_0,			 /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_0_LFE1,			 /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_0_LFE2,			 /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_0,			 /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_0,		 /*           Y                             */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CSURR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CHIGH,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_TOPSUR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CHIGHREAR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CLOWFRONT,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_TOPSUR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_CHIGHREAR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_CLOWFRONT,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE2,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_LFE1,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_LFE2,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_LFE1,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_LFE2,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSIDESURR_RSIDESURR,	 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGHSIDE_RHIGHSIDE,	 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LDIRSUR_RDIRSUR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGHREAR_RHIGHREAR,	 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_0,			 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_TOPSUR_0,			 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_TOPSUR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CHIGH,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CHIGHREAR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CLOWFRONT,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_LFE1,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_LFE2,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGHREAR_0,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_DSTEREO_LsRs,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,		 /*   Y       Y       Y       Y       Y     */	
};

struct snd_pseudo_mixer_channel_assignment {
	unsigned int pair0:6; /* channels 0 and 1 */
	unsigned int pair1:6; /* channels 2 and 3 */
	unsigned int pair2:6; /* channels 4 and 5 */
	unsigned int pair3:6; /* channels 6 and 7 */
	unsigned int pair4:6; /* channels 8 and 9 */

	unsigned int reserved0:1;
	unsigned int malleable:1;
};

#define SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING 0x01
#define SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE 0x02
#define SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING 0x04

struct snd_pseudo_mixer_downstream_card {
	char name[16]; /* e.g. Analog0, HDMI */

	char alsaname[24]; /* card name (e.g. hw:0,0 or hw:ANALOG,1) */
	
	unsigned int flags;
	unsigned int max_freq; /* in hz */	
	unsigned char num_channels;
	
	char reserved[11];
	
	struct snd_pseudo_mixer_channel_assignment channel_assignment;
};

struct snd_pseudo_mixer_downstream_topology {
	struct snd_pseudo_mixer_downstream_card card[SND_PSEUDO_MAX_OUTPUTS];
};

struct channel_pair_lookup_table {
	enum snd_pseudo_mixer_channel_pair channel_pair;
	const char *str;
} channel_pair_lookup[] = {
#define L(x) { SND_PSEUDO_MIXER_CHANNEL_PAIR_ ## x, #x }
	L(L_R),
	L(CNTR_LFE1),
	L(LSUR_RSUR),
	L(LSURREAR_RSURREAR),
	L(DEFAULT), /* this is deliberately out of order to allow this table to be indexed by possition */

	L(LT_RT),
	L(LPLII_RPLII),
	L(CNTRL_CNTRR),
	L(LHIGH_RHIGH),
	L(LWIDE_RWIDE),
	L(LRDUALMONO),
	L(RESERVED1),

	L(CNTR_0),
	L(0_LFE1),
	L(0_LFE2),
	L(CHIGH_0),
	L(CLOWFRONT_0),

	L(CNTR_CSURR),
        L(CNTR_CHIGH),
	L(CNTR_TOPSUR),
	L(CNTR_CHIGHREAR),
	L(CNTR_CLOWFRONT),

	L(CHIGH_TOPSUR),
	L(CHIGH_CHIGHREAR),
	L(CHIGH_CLOWFRONT),

	L(CNTR_LFE2),
	L(CHIGH_LFE1),
	L(CHIGH_LFE2),
	L(CLOWFRONT_LFE1),
	L(CLOWFRONT_LFE2),

	L(LSIDESURR_RSIDESURR),
	L(LHIGHSIDE_RHIGHSIDE),
	L(LDIRSUR_RDIRSUR),
	L(LHIGHREAR_RHIGHREAR),

	L(CSURR_0),
	L(TOPSUR_0),
	L(CSURR_TOPSUR),
	L(CSURR_CHIGH),
	L(CSURR_CHIGHREAR),
	L(CSURR_CLOWFRONT),
	L(CSURR_LFE1),
	L(CSURR_LFE2),
	L(CHIGHREAR_0),
	L(DSTEREO_LsRs),

	L(NOT_CONNECTED),
#undef L
	{ 0 }
};

enum snd_pseudo_mixer_channel_pair string_to_channel_pair(const char *str)
{
	int i;

	for (i=0; channel_pair_lookup[i].str; i++)
		if (0 == strcmp(str, channel_pair_lookup[i].str))
			return channel_pair_lookup[i].channel_pair;

	return SND_PSEUDO_MIXER_CHANNEL_PAIR_DEFAULT;
}

const char *channel_pair_to_string(enum snd_pseudo_mixer_channel_pair channel_pair, int pos)
{
	int i;

	if (SND_PSEUDO_MIXER_CHANNEL_PAIR_DEFAULT == channel_pair) // special case
		return channel_pair_lookup[pos].str;

	for (i=0; channel_pair_lookup[i].str; i++)
		if (channel_pair == channel_pair_lookup[i].channel_pair)
			return channel_pair_lookup[i].str;

	return "INVALID";
}

static int get_bool(const char *str)
{
	if (strncmp(str, "yes", 3) == 0 ||
	    strncmp(str, "YES", 3) == 0 ||
	    strncmp(str, "on", 2) == 0 ||
	    strncmp(str, "ON", 2) == 0 ||
	    strncmp(str, "true", 4) == 0 ||
	    strncmp(str, "TRUE", 4) == 0 ||
	    *str == '1')
		return 1;
	return 0;
}

#define IDX(item, bit) ((offsetof(struct snd_pseudo_mixer_downstream_topology, item)*8) + bit)

struct cmdtbl {
	const char *name;
	int idx;
	const char *desc;
};

#define FATPIPE_INVALID_STREAM (1<<4)
#define FATPIPE_LEVEL_SHIFT_VALUE (31)

static struct cmdtbl cmds[] = {
	{ "name[0]", IDX(card[0].name, 0),
	  "name[0]\n\tName of the zeroth card (string) or \\\"\\\" to disable the card" },
	{ "alsaname[0]", IDX(card[0].alsaname, 0),
	  "alsaname[0]\n\tALSA device used by the zeroth card (string)" },
	{ "enable_spdif_formating[0]", IDX(card[0].flags, 0),
	  "enable_spdif_formating[0]\n\tfalse = PCM, true = IEC60958" },
	{ "fatpipe[0]", IDX(card[0].flags, 1),
	  "fatpipe[0]\n\tfalse = permit fatpipe, true = prohibit fatpipe" },
	{ "enable_hdmi_formating[0]", IDX(card[0].flags, 2),
	  "enable_hdmi_formating[0]\n\tfalse = PCM, true = IEC60958" },
	{ "max_freq[0]", IDX(card[0].max_freq, 0),
          "max_freq[0]\n\tMaximum support sampling frequency (in hz)" },
  	{ "num_channels[0]", IDX(card[0].num_channels, 0),
          "num_channels[0]\n\tNumber of channels" },
        { "channel_assignment[0]", IDX(card[0].channel_assignment, 0),
          "channel_assignment[0]\n\tChannels assignment within the output" },
  	{ "name[1]", IDX(card[1].name, 0),
  	  "name[1]\n\tName of the first card (string) or \\\"\\\" to disable the card" },
  	{ "alsaname[1]", IDX(card[1].alsaname, 0),
  	  "alsaname[1]\n\tALSA device used by the first card (string)" },
  	{ "enable_spdif_formating[1]", IDX(card[1].flags, 0),
  	  "enable_spdif_formating[1]\n\tfalse = PCM, true = IEC60958" },
  	{ "fatpipe[1]", IDX(card[1].flags, 1),
  	  "fatpipe[1]\n\tfalse = permit fatpipe, true = prohibit fatpipe" },
  	{ "enable_hdmi_formating[1]", IDX(card[1].flags, 2),
  	  "enable_hdmi_formating[1]\n\tfalse = PCM, true = IEC60958" },
  	{ "max_freq[1]", IDX(card[1].max_freq, 0),
          "max_freq[1]\n\tMaximum support sampling frequency (in hz)" },
    	{ "num_channels[1]", IDX(card[1].num_channels, 0),
          "num_channels[1]\n\tNumber of channels" },
        { "channel_assignment[1]", IDX(card[1].channel_assignment, 0),
          "channel_assignment[1]\n\tChannels assignment within the output" },
    	{ "name[2]", IDX(card[2].name, 0),
    	  "name[2]\n\tName of the second card (string) or \\\"\\\" to disable the card" },
    	{ "alsaname[2]", IDX(card[2].alsaname, 0),
    	  "alsaname[2]\n\tALSA device used by the second card (string)" },
    	{ "enable_spdif_formating[2]", IDX(card[2].flags, 0),
    	  "enable_spdif_formating[2]\n\tfalse = PCM, true = IEC60958" },
    	{ "fatpipe[2]", IDX(card[2].flags, 1),
    	  "fatpipe[2]\n\tfalse = permit fatpipe, true = prohibit fatpipe" },
    	{ "enable_hdmi_formating[2]", IDX(card[2].flags, 2),
    	  "enable_hdmi_formating[2]\n\tfalse = PCM, true = IEC60958" },
    	{ "max_freq[2]", IDX(card[2].max_freq, 0),
          "max_freq[2]\n\tMaximum support sampling frequency (in hz)" },
      	{ "num_channels[2]", IDX(card[2].num_channels, 0),
          "num_channels[2]\n\tNumber of channels" },
        { "channel_assignment[2]", IDX(card[2].channel_assignment, 0),
          "channel_assignment[2]\n\tChannels assignment within the output" },
      	{ "name[3]", IDX(card[3].name, 0),
      	  "name[3]\n\tName of the third card (string) or \\\"\\\" to disable the card" },
      	{ "alsaname[3]", IDX(card[3].alsaname, 0),
      	  "alsaname[3]\n\tALSA device used by the third card (string)" },
      	{ "enable_spdif_formating[3]", IDX(card[3].flags, 0),
      	  "enable_spdif_formating[3]\n\tfalse = PCM, true = IEC60958" },
      	{ "fatpipe[3]", IDX(card[3].flags, 1),
      	  "fatpipe[3]\n\tfalse = permit fatpipe, true = prohibit fatpipe" },
      	{ "enable_hdmi_formating[3]", IDX(card[3].flags, 2),
      	  "enable_hdmi_formating[3]\n\tfalse = PCM, true = IEC60958" },
      	{ "max_freq[3]", IDX(card[3].max_freq, 0),
          "max_freq[3]\n\tMaximum support sampling frequency (in hz)" },
        { "num_channels[3]", IDX(card[3].num_channels, 0),
          "num_channels[3]\n\tNumber of channels" },  
        { "channel_assignment[3]", IDX(card[3].channel_assignment, 0),
          "channel_assignment[3]\n\tChannels assignment within the output" },
};

static void error(const char *s, int err)
{
	fprintf(stderr, "%s: %s\n", s, snd_strerror(err));
}

static void usage(void)
{
	int i;

	printf("Usage: toposet [options]\n");
	printf("Options:\n");
	printf("    -D device   specifies the control device to use\n");
	printf("    -c card     specifies the card number to use (equiv. with -Dhw:#)\n");
	printf("    -p          dump topology in parseable form\n");
	printf("    -z          zero everything before populating from commands\n");
	printf("Commands:\n");
	for (i = 0; i < (int)(sizeof(cmds)/sizeof(cmds[0])); i++) {
		printf("    %s\n", cmds[i].desc);
	}
}

static void update_fields(struct snd_pseudo_mixer_downstream_topology *update,
		          struct snd_pseudo_mixer_downstream_topology *mask,
		          unsigned int offset, int sz)
{
	unsigned char *pupdate = (unsigned char *) update;
	unsigned char *pmask = (unsigned char *) mask;
	
	offset /= 8;
	
	memset(pupdate + offset, 0, sz);
	memset(pmask + offset, 0xff, sz);
}

/*
 * parse toposet commands
 */

static struct snd_pseudo_mixer_channel_assignment parse_channel_assignment(const char *ca)
{
	struct snd_pseudo_mixer_channel_assignment channel_assignment = { 0 };
	char *p, *q;

	q = strdup(ca);
	p = strtok(q, " ,:");
	if (!p)	goto finished;
	channel_assignment.pair0 = string_to_channel_pair(p);

	p = strtok(NULL, " ,:");
	if (!p)	goto finished;
	channel_assignment.pair1 = string_to_channel_pair(p);

	p = strtok(NULL, " ,:");
	if (!p) goto finished;
	channel_assignment.pair2 = string_to_channel_pair(p);

	p = strtok(NULL, " ,:");
 	if (!p) goto finished;
	channel_assignment.pair3 = string_to_channel_pair(p);

	p = strtok(NULL, " ,:");
	if (!p) goto finished;
	channel_assignment.pair4 = string_to_channel_pair(p);
	
 finished:
	free(q);
	return channel_assignment;
}

static void parse_command(struct snd_pseudo_mixer_downstream_topology *update,
	                  struct snd_pseudo_mixer_downstream_topology *mask,
	                  const char *c, const char *arg)
{
	int i;

	for (i = 0; i < (int)(sizeof(cmds)/sizeof(cmds[0])); i++) {
		if (strncmp(c, cmds[i].name, strlen(cmds[i].name)) == 0) {
			int idx = cmds[i].idx;
			int card;
			char *p;
			
			/* strip any double quoting from the argument */
			if ('"' == arg[0] && '"' == *(p = arg + strlen(arg) - 1))
				arg++, *p = '\0';

			/* the index is composed of three fields:
			 * 
			 *   card select (highest bits)
			 *   byte offset (middle bits)
			 *   bit offset (lowest bits)
			 * 
			 * we have a macro, IDX to handle the byte and bit offsets
			 * but to save yet more lookup table code we chop of the card
			 * select before handing things on
			 */
			
			for (card = 0; idx >= IDX(card[1].name, 0); card++)
				idx -= IDX(card[1].name, 0);
			
			switch (idx) {
			case IDX(card[0].name, 0):

				update_fields(update, mask, cmds[i].idx, sizeof(update->card[0].name));
				strncpy(update->card[card].name, arg, sizeof(update->card[0].name));
				update->card[card].name[sizeof(update->card[0].name)-1] = '\0';
				return;
				
			case IDX(card[0].alsaname, 0):
				update_fields(update, mask, cmds[i].idx, sizeof(update->card[0].alsaname));
				strncpy(update->card[card].alsaname, arg, sizeof(update->card[0].alsaname));
				update->card[card].alsaname[sizeof(update->card[0].alsaname)-1] = '\0';
				return;
				
			case IDX(card[0].flags, 0):
				mask->card[card].flags |= SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING;
				update->card[card].flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING;
				if (get_bool(arg))
					update->card[card].flags |=
							SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING;
				return;

			case IDX(card[0].flags, 1):
				mask->card[card].flags |= SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE;
				update->card[card].flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE;
				if (get_bool(arg))
					update->card[card].flags |= SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE;
				return;
				
			case IDX(card[0].flags, 2):
				mask->card[card].flags |= SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING;
				update->card[card].flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING;
				if (get_bool(arg))
					update->card[card].flags |=
							SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING;
				return;
			case IDX(card[0].max_freq, 0):
				update_fields(update, mask, cmds[i].idx, sizeof(update->card[0].max_freq));
				update->card[card].max_freq = (int)strtol(arg, NULL, 0);
				return;
				
			case IDX(card[0].num_channels, 0):
				update_fields(update, mask, cmds[i].idx, sizeof(update->card[0].num_channels));
				update->card[card].num_channels = (int)strtol(arg, NULL, 0);
				return;

			case IDX(card[0].channel_assignment, 0):
				update_fields(update, mask, cmds[i].idx, sizeof(update->card[0].channel_assignment));
				update->card[card].channel_assignment = parse_channel_assignment(arg);
				return;

			default:
				fprintf(stderr, "Internal error - unknown index while handling: %s %s\n", c, arg);
				return;
			}

			return;
		}
	}
	
	fprintf(stderr, "Unknown command: %s %s\n", c, arg);
}

static char *skipspace(char *line)
{
	char *p;
	for (p = line; *p && isspace(*p); p++)
		;
	return p;
}

/*
 * parse toposet commands from the file
 */
static void parse_file(struct snd_pseudo_mixer_downstream_topology *update,
	               struct snd_pseudo_mixer_downstream_topology *mask,
	               FILE *fp)
{
	char line[1024], *cmd, *arg;
	while (fgets(line, sizeof(line), fp) != NULL) {
		if ('\n' == *(cmd = line + strlen(line) - 1))
			*cmd = '\0';
		cmd = skipspace(line);
		if (*cmd == '#' || ! *cmd)
			continue;
		for (arg = cmd; *arg && !isspace(*arg); arg++)
			;
		if (! *arg)
			continue;
		*arg++ = 0;
		arg = skipspace(arg);
		if (! *arg)
			continue;
		parse_command(update, mask, cmd, arg);
	}
}

/* update fatpipe status values
 * return non-zero if the values are modified
 */
static int update_topology(struct snd_pseudo_mixer_downstream_topology *topo,
		           struct snd_pseudo_mixer_downstream_topology *update,
		           struct snd_pseudo_mixer_downstream_topology *mask)
{
	struct snd_pseudo_mixer_downstream_topology original = *topo;
	unsigned char *ptopo = (unsigned char *) topo;
	unsigned char *pupdate = (unsigned char *) update;
	unsigned char *pmask = (unsigned char *) mask;
	int i;
	
	for (i=0; i<sizeof(*topo); i++)
		ptopo[i] = (ptopo[i] & ~pmask[i]) | pupdate[i];
	
	return memcmp(topo, &original, sizeof(*topo));
}

const char *dump_channel_assignment(struct snd_pseudo_mixer_channel_assignment channel_assignment, int num_channels)
{
	static char str[80];
	int num_pairs = num_channels / 2;
	int i;
	char *p;

	if (num_pairs <= 0 || num_pairs > 5)
		num_pairs = 5;

	p = str;
	for (i=0; i<num_pairs; i++) {
		const char *name = channel_pair_to_string( (i == 0 ? channel_assignment.pair0 :
							    i == 1 ? channel_assignment.pair1 :
							    i == 2 ? channel_assignment.pair2 :
							    i == 3 ? channel_assignment.pair3 :
							    channel_assignment.pair4),
							   i);
		int len = strlen(name);
		*p++ = ':';
		memcpy(p, name, len+1);
		p += len;
	}
		
	return str + 1;
}

void dump_topology(struct snd_pseudo_mixer_downstream_topology *topo, int force)
{
	int i, j;
	unsigned int flags;
	
	for (i=0; i<SND_PSEUDO_MAX_OUTPUTS; i++) {
		struct snd_pseudo_mixer_downstream_card *card = topo->card + i;
		
		if ('\0' == card->name[0] && !force)
			break;
		
		printf("Card %d:\n", i);
		printf("\tname:               [%-15s]\n", card->name);
		printf("\talsaname:           [%-23s]\n", card->alsaname);
		printf("\tflags:             ");
		flags = card->flags;
		if (flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING)
			printf(" enable_spdif_formating");
		flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING;
		if (flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING)
			printf(" enable_hdmi_formating");
		flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING;
		if (flags & SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE)
			printf(" fatpipe");
		flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE;
		if (flags)
			printf(" [unexpected bits set, 0x%08x]", flags);
		else if (flags == card->flags)
			printf(" None");
		printf("\n");
		printf("\tmax_freq:           %d\n", card->max_freq);
		printf("\tnum_channels:       %d\n", card->num_channels);
		printf("\tchannel_assignment: %s\n", dump_channel_assignment(card->channel_assignment, card->num_channels));
		
		
		for (j=0; j<sizeof(card->reserved); j++) {
			if (card->reserved[j])
				printf("\treserved[%2d]: [unexpected non-zero value, 0x%2x]\n",
				       j, card->reserved[j]);		
		}
	}
	
	if (0 == i)
		printf("No cards enabled\n");
}

void dump_parseable_topology(struct snd_pseudo_mixer_downstream_topology *topo, int force)
{
	int i, j;
	unsigned int flags;

	printf("#!/usr/bin/toposet -zf\n");

	for (i=0; i<SND_PSEUDO_MAX_OUTPUTS; i++) {
		struct snd_pseudo_mixer_downstream_card *card = topo->card + i;
		
		if ('\0' == card->name[0] && !force)
			break;
		
		printf("\n");
		printf("name[%d]                   %s\n", i, card->name);
		printf("alsaname[%d]               %s\n", i, card->alsaname);
		flags = card->flags;
		if (flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING)
			printf("enable_spdif_formating[%d] 1\n", i);
		flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING;
		if (flags & SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING)
			printf("enable_hdmi_formating[%d]  1\n", i);
		flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING;
		if (flags & SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE)
			printf("fatpipe[i]                 1\n", i);
		flags &= ~SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE;
		if (flags)
 			printf("#[unexpected bits set, 0x%08x]\n", flags);
		else if (flags == card->flags)
			printf("#Flags                    None\n");
		printf("max_freq[%d]               %d\n", i, card->max_freq);
		printf("num_channels[%d]           %d\n", i, card->num_channels);
		
		for (j=0; j<sizeof(card->reserved); j++) {
			if (card->reserved[j])
				printf("# reserved[%2d]: [unexpected non-zero value, 0x%2x]\n",
				       j, card->reserved[j]);		
		}

		printf("channel_assignment[%d]     %s\n", i, dump_channel_assignment(card->channel_assignment, card->num_channels));
	}
	
	if (0 == i)
		printf("\n# No cards enabled\n");
}

int main(int argc, char **argv)
{
	const char *dev = "hw:MIXER0";
	const char *topo_str = "Downstream Topology Playback Default";
	int topo_index = -1;
	snd_ctl_t *ctl;
	snd_ctl_elem_list_t *clist;
	snd_ctl_elem_id_t *cid;
	snd_ctl_elem_value_t *cval;
	struct snd_pseudo_mixer_downstream_topology topo, update = {{{{0}}}}, mask = {{{{0}}}};
	unsigned char *ptopo = (unsigned char *) &topo;
	int force = 0;
	int from_stdin = 0;
	int parseable = 0;
	const char *from_file = NULL;
	int i, c, err;
	unsigned int controls, cidx;
	char tmpname[32];
	
	while ((c = getopt(argc, argv, "D:Fc:f:n:hpz")) != -1) {
			switch (c) {
			case 'D':
				dev = optarg;
				break;
			case 'F':
				force = 1;
				break;
			case 'c':
				i = atoi(optarg);
				if (i < 0 || i >= 32) {
					fprintf(stderr, "invalid card index %d\n", i);
					return 1;
				}
				sprintf(tmpname, "hw:%d", i);
				dev = tmpname;
				break;
			case 'f':
				from_file = optarg;
				break;
			case 'n':
				topo_index = atoi(optarg);
				break;
			case 'i':
				from_stdin = 1;
				break;
			case 'p':
				parseable = 1;
				break;
			case 'z':
				memset(&mask, 0xff, sizeof(mask));
				break;
			default:
				usage();
				return 1;
			}
		}

	
	if ((err = snd_ctl_open(&ctl, dev, 0)) < 0) {
		error("snd_ctl_open", err);
		return 1;
	}

	snd_ctl_elem_list_alloca(&clist);
	if ((err = snd_ctl_elem_list(ctl, clist)) < 0) {
		error("snd_ctl_elem_list", err);
		return 1;
	}
	
	if ((err = snd_ctl_elem_list_alloc_space(clist, snd_ctl_elem_list_get_count(clist))) < 0) {
		error("snd_ctl_elem_list_alloc_space", err);
		return 1;
	}
	
	if ((err = snd_ctl_elem_list(ctl, clist)) < 0) {
		error("snd_ctl_elem_list", err);
		return 1;
	}

	controls = snd_ctl_elem_list_get_used(clist);
	for (cidx = 0; cidx < controls; cidx++) {
		if (!strcmp(snd_ctl_elem_list_get_name(clist, cidx), topo_str))
			if (topo_index < 0 ||
			    snd_ctl_elem_list_get_index(clist, cidx) == topo_index)
				break;
	}
	if (cidx >= controls) {
		fprintf(stderr, "control \"%s\" (index %d) not found\n",
			topo_str, topo_index);
		return 1;
	}
	
	snd_ctl_elem_id_alloca(&cid);
	snd_ctl_elem_list_get_id(clist, cidx, cid);
	snd_ctl_elem_value_alloca(&cval);
	snd_ctl_elem_value_set_id(cval, cid);
	if ((err = snd_ctl_elem_read(ctl, cval)) < 0) {
		error("snd_ctl_elem_read", err);
		return 1;
	}

	for (i=0; i<sizeof(topo); i++)
		ptopo[i] = snd_ctl_elem_value_get_byte(cval, i);	

	/* parse from stdin */
	if (from_stdin)
		parse_file(&update, &mask, stdin);

	/* parse from file */
	if (from_file) {
		FILE *f = fopen(from_file, "r");
		if (f) {
			parse_file(&update, &mask, f);
			fclose(f);
		} else
			fprintf(stderr, "Cannot open file: %s", from_file);
	}

	/* parse commands */
	for (c = optind; c < argc - 1; c += 2)
		parse_command(&update, &mask, argv[c], argv[c + 1]);

	if (update_topology(&topo, &update, &mask)) {
		/* store the values */
	        for (i=0; i<sizeof(topo); i++){
			snd_ctl_elem_value_set_byte(cval, i, ptopo[i]);
			//printf("%d ",i);
		}
		if ((err = snd_ctl_elem_write(ctl, cval)) < 0) {
			error("snd_ctl_elem_write", err);
			return 1;
		}
		if ((err = snd_ctl_elem_read(ctl, cval)) < 0) {
			error("snd_ctl_elem_write", err);
			return 1;
		}
		for (i=0; i<sizeof(topo); i++)
			ptopo[i] = snd_ctl_elem_value_get_byte(cval, i);	
	}

	if (parseable)
		dump_parseable_topology(&topo, force);
	else
		dump_topology(&topo, force);

	snd_ctl_close(ctl);
	return 0;
}
