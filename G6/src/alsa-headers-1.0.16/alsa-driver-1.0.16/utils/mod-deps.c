/*
 *  Utility to find module dependencies from Modules.dep
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *		     Anders Semb Hermansen <ahermans@vf.telia.no>,
 *		     Martin Dahl <dahlm@vf.telia.no>,
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// Output methods
#define METHOD_ACINCLUDE	1
#define METHOD_MAKECONF		2
#define METHOD_INCLUDE		3

#define COND_AND		0
#define COND_OR			1

struct cond {
	char *name;		/* dependency name */
	struct dep *dep;	/* dependency pointer */
	int left;		/* left brackets */
	int right;		/* right brackets (after this condition element) */
	int not;
	int type;
	struct cond *next;
	struct cond *stack_prev;
};

struct sel {
	char *name;		/* dependency name */
	int hitflag;
	struct dep *dep;	/* dependency pointer */
	struct sel *next;
};

enum { TYPE_TRISTATE, TYPE_BOOL, TYPE_INT };

struct dep {
	char *name;
	// dependency part - conditions (chain)
	struct cond *cond;
	// forced selection (dependency) part
	struct sel *sel;
	// misc
	struct dep *next;
	// bool?
	int type;
	int int_val;
	// hitflag?
	int hitflag;
};

// Prototypes

static int read_file(const char *filename, struct cond **template);
static int read_file_1(const char *filename, struct cond **template);
static int include_file(char *line, struct cond **template);
static void free_cond(struct cond *cond);
static struct cond *create_cond(char *line);
static struct dep *alloc_mem_for_dep(void);
static struct dep *find_or_create_dep(char *line);
static void add_dep(struct dep * dep, char *line, struct cond *template);
static void add_select(struct dep * dep, char *line, struct cond *template);
static char *get_word(char *line, char *word);
static struct dep *find_dep(char *parent, char *depname);
static void del_all_from_list(void);
static int is_always_true(struct dep *dep);

int main(int argc, char *argv[]);
static void usage(char *programname);
static char *convert_to_config_uppercase(const char *pre, const char *line);
// static char *convert_to_escape(const char *line);
static char *get_card_name(const char *line);

// Globals
static struct dep *all_deps = NULL;
static char *basedir = "../alsa-kernel";
static char *hiddendir = "..";

static char *kernel_deps[] = {
	/* buses */
	"ISA",
	"ISA_DMA_API",
	"ISAPNP",
	"EISA",
	"PCI",
	"SBUS",
	"I2C",
	"INPUT",
	"L3",
	"USB",
	"PCMCIA",
	/* architectures */
	"ARM",
	"PARISC",
	"SPARC32",
	"SPARC64",
	"PPC",
	"PPC64",
	"PPC_PMAC",
	"X86_64",
	"X86",
	"MIPS",
	"MIPS64",
	"SUPERH",
	"SUPERH64",
	"IA32_EMULATION",
	/* architecture specific */
	"ARCH_SA1100",
	"ARCH_PXA",
	"X86_PC9800",
	"SH_DREAMCAST",
	/* other drivers */
	"RTC",
	"GAMEPORT",
	"VIDEO_DEV",
	"VIDEO_V4L1",
	"FW_LOADER",
	/* some flags/capabilities */
	"HAS_IOPORT",
	"EXPERIMENTAL",
	/* sound common */
	"AC97_BUS",
	NULL
};

/* % -> always true */
/* # -> define */
static char *no_cards[] = {
	"%#SOUND",
	"%HAS_IOMEM",
	"SOUND_PRIME",
	"%SND",
	"#SND_TIMER",
	"#SND_HWDEP",
	"#SND_RAWMIDI",
	"#SND_PCM",
	"SND_SEQUENCER",
	"SND_MIXER_OSS",
	"SND_PCM_OSS",
	"SND_PCM_OSS_PLUGINS",
	"SND_SEQUENCER_OSS",
	"SND_OSSEMUL",
	"SND_RTCTIMER",
	"SND_HPET",
	"SND_DYNAMIC_MINORS",
	"SND_DEBUG",
	"SND_DEBUG_MEMORY",
	"SND_DEBUG_DETECT",
	"SND_VERBOSE_PROCFS",
	"SND_VERBOSE_PRINTK",
	"SND_BIT32_EMUL",
	"#SND_AD1848_LIB",
	"#SND_CS4231_LIB",
	"#SND_SB_COMMON",
	"#SND_SB8_DSP",
	"#SND_SB16_DSP",
	"#SND_OPL3_LIB",
	"#SND_OPL4_LIB",
	"#SND_VX_LIB",
	"#SND_MPU401_UART",
	"#SND_GUS_SYNTH",
	"#AC97_BUS",
	"#SND_AC97_CODEC",
	"#SND_OXYGEN_LIB",
	"#SND_AT91_SOC_SSC",
	"#SND_SOC_AC97_CODEC",
	"#SND_SOC_WM8731",
	"#SND_SOC_WM8750",
	"#SND_SOC_WM8753",
	"#SND_SOC_WM9712",
	"#SND_SOC_CS4270",
	"#SND_SOC_TLV320AIC3X",
	"#SND_PXA2XX_PCM",
	"#SND_PXA2XX_AC97",
	"#SND_PXA2XX_SOC_AC97",
	"#SND_PXA2XX_SOC_I2S",
	"#SND_S3C24XX_SOC_I2S",
	"#SND_S3C2412_SOC_I2S",
	"#SND_S3C2443_SOC_AC97",
	"#SND_S3C24XX_SOC_NEO1973_WM8753",
	"#SND_S3C24XX_SOC_SMDK2443_WM9710",
	"#SND_S3C24XX_SOC_LN2440SBC_ALC650",
	"#SND_AT91_SOC_I2S",
	"#SND_SOC_SH4_HAC",
	"#SND_SOC_SH4_SSI",
	NULL
};

#define READ_STATE_NONE		0
#define READ_STATE_CONFIG	1
#define READ_STATE_MENU		2
#define READ_STATE_COMMENT	3

static void nomem(void)
{
	fprintf(stderr, "No enough memory\n");
	exit(EXIT_FAILURE);
}

static int read_file(const char *filename, struct cond **template)
{
	char *fullfile;
	int err;
	
	fullfile = malloc(strlen(basedir) + strlen(hiddendir) + 1 + strlen(filename) + 1);
	sprintf(fullfile, "%s/%s", basedir, filename);
	if (access(fullfile, R_OK) == 0) {
		if ((err = read_file_1(fullfile, template)) < 0) {
			free(fullfile);
			return err;
		}
	}
	if (!strncmp(filename, "core/", 5))
		sprintf(fullfile, "%s/acore/%s", hiddendir, filename + 5);
	else
		sprintf(fullfile, "%s/%s", hiddendir, filename);
	if (access(fullfile, R_OK) == 0) {
		if ((err = read_file_1(fullfile, template)) < 0) {
			free(fullfile);
			return err;
		}
	}
	free(fullfile);
	return 0;
}

static int read_file_1(const char *filename, struct cond **template)
{
	char *buffer, *newbuf;
	FILE *file;
	int c, prev, idx, size, result = 0;
	int state = READ_STATE_NONE;
	struct dep *dep;

	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Unable to open file %s: %s\n", filename, strerror(errno));
		return -ENOENT;
	}

	size = 512;
	buffer = (char *) malloc(size);
	if (!buffer) {
		fclose(file);
		return -ENOMEM;
	}
	while (!feof(file)) {
		buffer[idx = 0] = prev = '\0';
		while (1) {
			if (idx + 1 >= size) {
				newbuf = (char *) realloc(buffer, size += 256);
				if (newbuf == NULL) {
					result = -ENOMEM;
					goto __end;
				}
				buffer = newbuf;
			}
			c = fgetc(file);
			if (c == EOF)
				break;
			if (c == '\n') {
				if (prev == '\\') {
					idx--;
					continue;
				}
				break;
			}
			buffer[idx++] = prev = c;
		}
		buffer[idx] = '\0';
		/* ignore some keywords */
		if (buffer[0] == '#')
			continue;
		if (!strncmp(buffer, "        ", 8)) {
			buffer[0] = '\t';
			for (idx = 8; idx <= strlen(buffer); idx++)
				buffer[idx-7] = buffer[idx];
		}
		if (!strncmp(buffer, "endmenu", 7)) {
			struct cond *otemplate;
		    	state = READ_STATE_NONE;
		    	if (*template == NULL) {
		    		fprintf(stderr, "Menu level error\n");
		    		exit(EXIT_FAILURE);
		    	}
		    	otemplate = *template;
		    	*template = (*template)->stack_prev;
		    	free_cond(otemplate);
			continue;
		}
		if (!strncmp(buffer, "menu", 4)) {
			struct cond *ntemplate;
			state = READ_STATE_MENU;
			strcpy(buffer, "EMPTY");
			ntemplate = create_cond(buffer);
			ntemplate->stack_prev = *template;
			*template = ntemplate;
			continue;
		}
		if (!strncmp(buffer, "config ", 7)) {
			state = READ_STATE_CONFIG;
			dep = find_or_create_dep(buffer + 7);
			if (dep == NULL) {
				result = -ENOMEM;
				goto __end;
			}
		}
		if (!strncmp(buffer, "source ", 7)) {
			state = READ_STATE_NONE;
			result = include_file(buffer + 7, template);
			if (result < 0)
				goto __end;
		}
		if (!strncmp(buffer, "comment", 7)) {
			state = READ_STATE_COMMENT;
		}
		switch (state) {
		case READ_STATE_CONFIG:
			if (!strncmp(buffer, "\ttristate", 9))
				dep->type = TYPE_TRISTATE;
			else if (!strncmp(buffer, "\tbool", 5))
				dep->type = TYPE_BOOL;
			else if (!strncmp(buffer, "\tdepends on ", 12))
				add_dep(dep, buffer + 12, *template);
			else if (!strncmp(buffer, "\tdepends ", 9))
				add_dep(dep, buffer + 9, *template);
			else if (!strncmp(buffer, "\tselect ", 8))
				add_select(dep, buffer + 8, *template);
			else if (!strncmp(buffer, "\tint ", 5))
				dep->type = TYPE_INT;
			if (!strncmp(buffer, "\tdefault ", 9)) {
				if (dep->type == TYPE_INT) {
					char *p = buffer + 9;
					for (; *p && !isdigit(*p); p++)
						;
					dep->int_val = strtol(p, NULL, 0);
				}
			}
			continue;
		case READ_STATE_MENU:
			if (!strncmp(buffer, "\tdepends ", 9)) {
				struct cond *ntemplate;
				if (strcmp((*template)->name, "EMPTY")) {
					fprintf(stderr, "Menu consistency error\n");
					exit(EXIT_FAILURE);
				}
				if (! strncmp(buffer + 9, "on ", 3))
					ntemplate = create_cond(buffer + 12);
				else
					ntemplate = create_cond(buffer + 9);
				free((*template)->name);
				(*template)->name = ntemplate->name;
				(*template)->dep = ntemplate->dep;
				(*template)->left = ntemplate->left;
				(*template)->right = ntemplate->right;
				(*template)->not = ntemplate->not;
				(*template)->type = ntemplate->type;
				(*template)->next = ntemplate->next;
				free(ntemplate);
			}
			continue;
		}
	}
      __end:
	free(buffer);
	if (file != stdin)
		fclose(file);
	return result;
}

// include a file
static int include_file(char *line, struct cond **template)
{
	char *word = NULL, *ptr;
	int result;

	word = malloc(strlen(line) + 1);
	get_word(line, word);
	ptr = word;
	if (!strncmp(ptr, "sound/", 6))
		ptr += 6;
	if (!strncmp(ptr, "oss/", 4))
		return 0;
	result = read_file(ptr, template);
	free(word);
	return result;
}

// allocate condition chain
static struct cond * create_cond(char *line)
{
	struct cond *first = NULL, *cond, *prev = NULL;
	char *word = NULL;
	int i;

	word = malloc(strlen(line) + 1);
	if (word == NULL)
		nomem();
	while (get_word(line, word)) {
		cond = calloc(sizeof(struct cond), 1);
		if (cond == NULL)
			nomem();
		if (first == NULL)
			first = cond;
		if (prev)
			prev->next = cond;
		prev = cond;
		while (word[0] == '(') {
			for (i = 1; i < strlen(word) + 1; i++)
				word[i - 1] = word[i];
			cond->left++;
			if (!word[0]) {
				if (!get_word(line, word)) {
					fprintf(stderr, "Unbalanced open-parenthesis\n");
					exit(EXIT_FAILURE);
				}
			}
		}
		/* hack for !XXX */
		if (word[0] == '!' && isascii(word[1])) {
			cond->not = 1;
			memmove(word, word + 1, strlen(word));
		}
		for (i = 0; i < strlen(word); i++) {
			if (word[i] == '!' || word[i] == '=' ||
			    word[i] == '&' || word[i] == ')') {
			    	if (word[i] == ')') {
			    		word[i] = '\0';
			    		cond->right++;
			    		continue;
			    	}
				if (!strcmp(word + i, "!=n")) {
					word[i] = '\0';
					break;
				}
				if (word[i] == '=') {
					fprintf(stderr, "can't handle word %s properly, supposing it's OK\n",
						word);
					word[i] = '\0';
					break;
				}
				fprintf(stderr, "Unknown suffix '%s'\n", word + i);
				exit(EXIT_FAILURE);
			}
		}
		cond->name = strdup(word);
		if (cond->name == NULL)
			nomem();
		if (strcmp(cond->name, "EMPTY"))
			find_or_create_dep(word);
		while (get_word(line, word)) {
			if (!strcmp(word, "&&"))
				cond->type = COND_AND;
			else if (!strcmp(word, "||"))
				cond->type = COND_OR;
			else if (!strcmp(word, "!=")) {
				get_word(line, word);
				continue;
			} else if (!strcmp(word, "!=n"))
				continue;
			else if (*word == ')') {
				for (i = 0; word[i]; i++)
					if (word[i] == ')')
						cond->right++;
			} else {
				fprintf(stderr, "Wrong condition %s\n", word);
				exit(EXIT_FAILURE);
			}
			break;
		}
	}
	free(word);
	return first;
}

// allocate a new dep structure and put it to the global list
static struct dep *alloc_mem_for_dep(void)
{
	struct dep * firstdep = all_deps, * ndep;

	ndep = (struct dep *) calloc(1, sizeof(struct dep));
	if (ndep == NULL)
		nomem();
	if (!firstdep)
		return all_deps = ndep;
	while (firstdep->next)
		firstdep = firstdep->next;
	return firstdep->next = ndep;
}

// Add a new dependency to the list
static struct dep * find_or_create_dep(char *line)
{
	struct dep *new_dep;
	char *word = NULL;

	word = malloc(strlen(line) + 1);
	if (word == NULL)
		nomem();
	get_word(line, word);
	new_dep = find_dep("<root>", word);
	if (new_dep != NULL)
		return new_dep;
	new_dep = alloc_mem_for_dep();
	new_dep->name = strdup(word);	// Fill in name of dependency
	if (new_dep->name == NULL)
		nomem();
	free(word);
	return new_dep;
}

// duplicate condition chain
static struct cond *duplicate_cond(struct cond *cond)
{
	struct cond *result = NULL, *tmp, *prev = NULL;
	int first = 1;
	
	if (cond == NULL)
		return NULL;
	if (cond->stack_prev) {
		result = prev = duplicate_cond(cond->stack_prev);
		while (prev && prev->next)
			prev = prev->next;
		if (prev)
			prev->type = COND_AND;
	}
	if (!strcmp(cond->name, "EMPTY"))
		return result;
	while (cond) {
		tmp = calloc(sizeof(struct cond), 1);
		if (tmp == NULL)
			nomem();
		*tmp = *cond;
		tmp->stack_prev = NULL;
		tmp->name = strdup(cond->name);
		if (tmp->name == NULL)
			nomem();
		tmp->next = NULL;
		if (first) {
			tmp->left++;
			first = 0;
		}
		if (result == NULL)
			result = tmp;
		if (prev)
			prev->next = tmp;
		prev = tmp;
		cond = cond->next;
	}
	if (prev && !first)
		prev->right++;
	return result;
}

// join condition chain
static struct cond *join_cond(struct cond *cond1, struct cond *cond2)
{
	struct cond *orig = cond1;

	if (cond1 == NULL)
		return cond2;
	if (cond2 == NULL)
		return cond1;
	while (cond1->next)
		cond1 = cond1->next;
	cond1->next = cond2;
	return orig;
}

// Add a new dependency to the current one
static void add_dep(struct dep * dep, char *line, struct cond *template)
{
	template = duplicate_cond(template);
	if (dep->cond)
		template = join_cond(dep->cond, template);
	dep->cond = join_cond(template, create_cond(line));
}

/* Is the selected item ALSA-specific? */
static int is_alsa_item(const char *word)
{
	static const char *known_items[] = {
		"AC97_BUS",
		NULL
	};
	const char **p;

	if (!strncmp(word, "SND_", 4))
		return 1;
	for (p = known_items; *p; p++)
		if (!strcmp(word, *p))
			return 1;
	return 0;
}

// Add a new forced (selected) dependency to the current one
static void add_select(struct dep * dep, char *line, struct cond *template)
{
	char *word = NULL;
	struct sel *sel, *nsel;

	word = malloc(strlen(line) + 1);
	if (word == NULL)
		nomem();
	get_word(line, word);
	if (!is_alsa_item(word)) {
		add_dep(dep, word, template);
		free(word);
		return;
	}
	nsel = calloc(sizeof(struct sel), 1);
	if (nsel == NULL)
		nomem();
	nsel->name = strdup(word);
	if (nsel->name == NULL)
		nomem();
	nsel->dep = NULL;
	sel = dep->sel;
	if (sel == NULL)
		dep->sel = nsel;
	else {
		while (sel->next)
			sel = sel->next;
		sel->next = nsel;
	}
	free(word);
}


// Put the first word in "line" in "word". Put the rest back in "line"
static char *get_word(char *line, char *word)
{
	int i, j, c;
	char *full_line;

	if (strlen(line) == 0)
		return NULL;

	i = 0;
	while (line[i] == ' ' || line[i] == '\t')
		i++;
	c = line[i];
	if (c != '\'' && c != '"') {
		c = ' ';
	} else {
		i++;
	}

	if (strlen(line) == i)
		return NULL;

	full_line = malloc(strlen(line + i) + 1);
	if (full_line == NULL)
		nomem();
	strcpy(full_line, line + i);
	for (i = 0; i < strlen(full_line); i++) {
		if ((c != ' ' && full_line[i] != c) ||
		    (c == ' ' && full_line[i] != '\t'
		     && full_line[i] != ' '))
			word[i] = full_line[i];
		else {
			// We got the whole word
			word[i++] = '\0';
			while (full_line[i] != '\0' &&
			       (full_line[i] == ' ' || full_line[i] == '\t'))
				i++;
			for (j = 0; i < strlen(full_line); i++, j++)
				line[j] = full_line[i];
			line[j] = '\0';
			free(full_line);
			return word;
		}
	}
	// This was the last word
	word[i] = '\0';
	line[0] = '\0';
	free(full_line);
	return word;
}

// Find the dependency named "depname"
static struct dep *find_dep(char *parent, char *depname)
{
	struct dep *temp_dep = all_deps;
	int idx;

	while (temp_dep) {
		//fprintf(stderr, "depname = '%s', name = '%s'\n", depname, temp_dep->name);
		if (!strcmp(depname, temp_dep->name))
			return temp_dep;
		temp_dep = temp_dep->next;
	}
	for (idx = 0; kernel_deps[idx]; idx++) {
		//fprintf(stderr, "kdepname = '%s', name = '%s'\n", depname, kernel_deps[idx]);
		if (!strcmp(kernel_deps[idx], depname))
			return NULL;
	}
	if (strcmp(parent, "<root>"))
		fprintf(stderr, "Warning: Unsatisfied dep for %s: %s\n", parent, depname);
	return NULL;
}

// Resolve all dependencies
static void resolve_dep(struct dep * parent)
{
	struct cond *cond;

	while (parent) {
		cond = parent->cond;
		while (cond) {
			cond->dep = find_dep(parent->name, cond->name);
			cond = cond->next;
		}
		parent = parent->next;
	}
}

// Optimize all dependencies
static void optimize_dep(struct dep * parent)
{
	struct cond *cond, *prev;

	while (parent) {
	      __restart:
		cond = parent->cond;
		prev = NULL;
		while (cond) {
			int remove_flag = 0;
			if (cond->left > cond->right) {
				cond->left -= cond->right;
				cond->right = 0;
			} else {
				cond->right -= cond->left;
				cond->left = 0;
			}
			if (cond->next &&
			    !strcmp(cond->name, cond->next->name) &&
			    ((cond->left == cond->right &&
			      cond->next->left == cond->next->right) ||
			     (cond->left == cond->right + 1 &&
			      cond->next->left + 1 == cond->next->right)) &&
			      cond->left == cond->next->right)
			      	goto __remove;
			if (!is_always_true(cond->dep))
				goto __next;
			if (cond->left == cond->right &&
			    (cond->next == NULL || cond->type == COND_AND) &&
			    (prev == NULL || prev->type == COND_AND)) {
			    	remove_flag++;
			} else if (cond->left + 1 == cond->right &&
				 (prev && prev->type == COND_AND)) {
				if (prev == NULL) {
					fprintf(stderr, "optimize error (1)\n");
					exit(EXIT_FAILURE);
				}
				prev->right++;
				remove_flag++;
			} else if (cond->left == cond->right + 1 &&
				   cond->type == COND_AND) {
				if (cond->next == NULL) {
					fprintf(stderr, "optimize error (2)\n");
					exit(EXIT_FAILURE);
				}
				cond->next->left++;
 				remove_flag++;
 			}
			if (remove_flag) {
			      __remove:
			    	if (prev == NULL) {
			    		parent->cond = cond->next;
			    	} else {
			    		prev->next = cond->next;
			    	}
			    	cond->next = NULL;
			    	free_cond(cond);
			    	goto __restart;
			}
		      __next:
			prev = cond;
			cond = cond->next;
		}
		parent = parent->next;
	}
}

// Resolve fixed (selected) dependecies
static void resolve_sel(struct dep * parent)
{
	struct sel *sel;

	while (parent) {
		sel = parent->sel;
		while (sel) {
			sel->dep = find_dep(parent->name, sel->name);
			sel = sel->next;
		}
		parent = parent->next;
	}
}

// free condition chain
static void free_cond(struct cond *first)
{
	struct cond *next;
	
	while (first) {
		next = first->next;
		free(first->name);
		free(first);
		first = next;
	}
}

// free selection chain
static void free_sel(struct sel *first)
{
	struct sel *next;
	
	while (first) {
		next = first->next;
		free(first);
		first = next;
	}
}

// Free memory for all deps in Toplevel and Deps
static void del_all_from_list(void)
{
	struct dep *list = all_deps, *next;

	while (list) {
		next = list->next;
		free_cond(list->cond);
		free_sel(list->sel);
		if (list->name)
			free(list->name);
		free(list);
		list = next;
	}
}

// is toplevel module
static int is_toplevel(struct dep *dep)
{
	int idx;
	char *str;
	
	if (dep == NULL)
		return 0;
	for (idx = 0; kernel_deps[idx]; idx++) {
		if (!strcmp(kernel_deps[idx], dep->name))
			return 0;
	}
	for (idx = 0; no_cards[idx]; idx++) {
		str = no_cards[idx];
		if (*str == '%' || *str == '#')
			str++;
		if (!strcmp(str, dep->name))
			return 0;
	}
	return 1;
}

// whether to output AC_DEFINE() for this variable
static int output_ac_define(struct dep *dep)
{
	int idx;
	char *str;
	
	if (dep == NULL)
		return 0;
	for (idx = 0; kernel_deps[idx]; idx++) {
		if (!strcmp(kernel_deps[idx], dep->name))
			return 0;
	}
	for (idx = 0; no_cards[idx]; idx++) {
		int def = 0;
		str = no_cards[idx];
		if (*str == '%')
			str++;
		if (*str == '#') {
			def = 1;
			str++;
		}
		if (!strcmp(str, dep->name))
			return def;
	}
	return 1;
}

// is CONFIG_ variable is always true
static int is_always_true(struct dep *dep)
{
	int idx;
	char *str;

	if (dep == NULL)
		return 0;
	for (idx = 0; no_cards[idx]; idx++) {
		str = no_cards[idx];
		if (*str != '%')
			continue;
		str++;
		if (!strcmp(str, dep->name))
			return 1;
	}
	return 0;
}

// Print out ALL deps for firstdep (Cards, Deps)
static void output_card_list(struct dep *firstdep, int space, int size, int type)
{
	struct dep *temp_dep=firstdep;
	char *card_name;
	int tmp_size = 0, first = 1, idx;

	printf("  [");
	for (idx = 0; idx < space; idx++)
		printf(" ");
	while(temp_dep) {
		if (!is_toplevel(temp_dep))
			goto __skip;
		if (temp_dep->type != type)
			goto __skip;
		card_name=get_card_name(temp_dep->name);
		if (card_name) {
			if (!first) {
				printf(", ");
				tmp_size += 2;
			} else {
				first = 0;
			}
			if (tmp_size + strlen(card_name) + 2 > size) {
				printf("]\n  [");
				for (idx = 0; idx < space; idx++)
					printf(" ");
				tmp_size = 0;
			}
			printf(card_name);
			tmp_size += strlen(card_name);
			free(card_name);
		}
	      __skip:
		temp_dep=temp_dep->next;
	}
}

// acinclude.m4 helpers
static void sel_remove_hitflags(void)
{
	struct dep * dep;
	struct sel * nsel;
	
	for (dep = all_deps; dep; dep = dep->next) {
		dep->hitflag = 0;
		for (nsel = dep->sel; nsel; nsel = nsel->next)
			nsel->hitflag = 0;
	}
}

struct kconfig_verdep {
	const char *name;
	const char *version;
	struct kconfig_verdep *next;
};

static struct kconfig_verdep *version_deps;

static char *get_token(char **pp, int need_space)
{
	char *token;
	char *p = *pp;

	for (; isspace(*p); p++)
		if (!*p)
			return NULL;
	if (*p == '#' || *p == '%')
		return NULL;
	token = p;
	for (; !isspace(*p); p++)
		if (!*p) {
			if (need_space)
				return NULL;
			*pp = p;
			return token;
		}
	*p++ = 0;
	*pp = p;
	return token;
}

static int read_version_deps(const char *fname)
{
	FILE *fp;
	char buf[128], *p, *name, *val;
	struct kconfig_verdep *ver;

	fp = fopen(fname, "r");
	if (!fp) {
		fprintf(stderr, "cannot open %s\n", fname);
		return -ENODEV;
	}
	while (fgets(buf, sizeof(buf), fp)) {
		p = buf;
		name = get_token(&p, 1);
		if (!name)
			continue;
		val = get_token(&p, 0);
		if (!val)
			continue;
		ver = malloc(sizeof(*ver));
		if (!ver)
			return -ENOMEM;
		ver->name = strdup(name);
		ver->version = strdup(val);
		ver->next = version_deps;
		version_deps = ver;
	}
	fclose(fp);
	return 0;
}

static const char *get_version_dep(const char *name)
{
	const struct kconfig_verdep *p;
	for (p = version_deps; p; p = p->next)
		if (! strcmp(name, p->name))
			return p->version;
	return NULL;
}

static void print_version_dep(const char *ver)
{
	const char *p;
	printf("test \"$kversion.$kpatchlevel\" = \"");
	for (p = ver; *p; p++) {
		putchar(*p);
		if (*p == '.') {
			p++;
			break;
		}
	}
	for (; *p && *p != '.'; p++)
		putchar(*p);
	putchar('"');
	if (*p) {
		printf(" -a $ksublevel -ge ");
		for (p++; *p && *p != '.'; p++)
			putchar(*p);
	}
}

static void sel_print_acinclude(struct sel *sel)
{
	struct dep * dep;
	struct sel * nsel;
	const char *ver;

	dep = sel->dep;
	if (dep == NULL)
		return;
	if (dep->hitflag)
		return;
	for (nsel = dep->sel; nsel; nsel = nsel->next) {
		if (nsel->hitflag)
			continue;
		nsel->hitflag = 1;
		sel_print_acinclude(nsel);
		if (!nsel->dep->hitflag) {
			nsel->dep->hitflag = 1;
			printf("      ");
			ver = get_version_dep(nsel->name);
			if (ver) {
				print_version_dep(ver);
				printf(" && ");
			}
			printf("CONFIG_%s=\"%c\"\n", nsel->name,
				(nsel->dep && nsel->dep->type == TYPE_BOOL) ? 'y' : 'm');
		}
	}
}

// Output in acinlude.m4
static void output_acinclude(void)
{
	struct dep *tempdep;
	char *text;
	struct cond *cond, *cond_prev;
	struct sel *sel;
	const char *ver;
	int j;
	
	printf("dnl ALSA soundcard configuration\n");
	printf("dnl Find out which cards to compile driver for\n");
	printf("dnl Copyright (c) by Jaroslav Kysela <perex@perex.cz>,\n");
	printf("dnl                  Anders Semb Hermansen <ahermans@vf.telia.no>\n\n");

	printf("AC_DEFUN([ALSA_TOPLEVEL_INIT], [\n");
	for (tempdep = all_deps; tempdep; tempdep = tempdep->next) {
		text = convert_to_config_uppercase("CONFIG_", tempdep->name);
		printf("\t%s=\"\"\n", text);
		free(text);
	}
	printf("])\n\n");

	printf("AC_DEFUN([ALSA_TOPLEVEL_SELECT], [\n");
	printf("dnl Check for which cards to compile driver for...\n");
	printf("AC_MSG_CHECKING(for which soundcards to compile driver for)\n");
	printf("AC_ARG_WITH(cards,\n"
	       "  [  --with-cards=<list>     compile driver for cards in <list>; ]\n"
	       "  [                        cards may be separated with commas; ]\n"
	       "  [                        'all' compiles all drivers; ]\n"
	       "  [                        Possible cards are: ]\n");
	output_card_list(all_deps, 26, 50, TYPE_TRISTATE);
	printf(" ],\n");
	printf("  cards=\"$withval\", cards=\"all\")\n");
	printf("SELECTED_CARDS=`echo $cards | sed 's/,/ /g'`\n");
	/* check cards */
	printf("for card in $SELECTED_CARDS; do\n"
	       "  probed=\n");
	for (tempdep = all_deps; tempdep; tempdep = tempdep->next) {
		int put_if;
		if (!is_toplevel(tempdep) || tempdep->type != TYPE_TRISTATE)
			continue;
		text = get_card_name(tempdep->name);
		if (! text)
			continue;
		printf("  if test \"$card\" = \"all\" -o \"$card\" = \"%s\"; then\n", text);
		free(text);
		put_if = 0;
		for (cond = tempdep->cond, cond_prev = NULL; cond; cond = cond->next) {
			if (!put_if)
				printf("    if ");
			else {
				printf(cond_prev->type == COND_AND ? " &&" : " ||");
				printf("\n      ");
			}
			if (cond->not)
				printf(" ! ");
			for (j = 0; j < cond->left; j++)
				printf("( ");
			printf("( test \"$CONFIG_%s\" = \"y\" -o \"$CONFIG_%s\" = \"m\" )", cond->name, cond->name);
			for (j = 0; j < cond->right; j++)
				printf(" )");
			put_if = 1;
			cond_prev = cond;
		}
		text = convert_to_config_uppercase("", tempdep->name);
		ver = get_version_dep(text);
		if (ver) {
			if (!put_if)
				printf("    if ");
			else {
				printf(cond_prev->type == COND_AND ? " &&" : " ||");
				printf("\n      ");
			}
			printf("( ");
			print_version_dep(ver);
			printf(" )");
			put_if = 1;
		}
		free(text);
		if (put_if)
			printf("; then\n");

		sel_remove_hitflags();
		for (sel = tempdep->sel; sel; sel = sel->next)
			sel_print_acinclude(sel);			
		for (sel = tempdep->sel; sel; sel = sel->next) {
			if (is_always_true(sel->dep))
				continue;
			printf("      ");
			ver = get_version_dep(sel->name);
			if (ver) {
				print_version_dep(ver);
				printf(" && ");
			}
			printf("CONFIG_%s=\"%c\"\n", sel->name,
			       (sel->dep && sel->dep->type == TYPE_BOOL) ? 'y' : 'm');
		}
		text = convert_to_config_uppercase("CONFIG_", tempdep->name);
		if (tempdep->type == TYPE_INT)
			printf("      %s=\"%d\"\n", text, tempdep->int_val);
		else
			printf("      %s=\"%c\"\n", text, tempdep->type == TYPE_BOOL ? 'y' : 'm');
		free(text);
		printf("      probed=1\n");
		if (put_if)
			printf("    elif test -z \"$probed\"; then\n"
			       "      probed=0\n"
			       "    fi\n");
		printf("  fi\n");
	}
	printf("  if test -z \"$probed\"; then\n"
	       "    AC_MSG_ERROR(Unknown soundcard $card)\n"
	       "  elif test \"$probed\" = \"0\"; then\n"
	       "    AC_MSG_ERROR(Unsupported soundcard $card)\n"
	       "  fi\n"
	       "done\n\n");
	/* options */
	printf("AC_ARG_WITH(card_options,\n"
	       "  [  --with-card-options=<list> enable driver options in <list>; ]\n"
	       "  [                        options may be separated with commas; ]\n"
	       "  [                        'all' enables all options; ]\n"
	       "  [                        Possible options are: ]\n");
	output_card_list(all_deps, 26, 50, TYPE_BOOL);
	printf(" ],\n");
	printf("  cards=\"$withval\", cards=\"all\")\n");
	printf("SELECTED_OPTIONS=`echo $cards | sed 's/,/ /g'`\n");
	/* check cards */
	printf("for card in $SELECTED_OPTIONS; do\n"
	       "  probed=\n");
	for (tempdep = all_deps; tempdep; tempdep = tempdep->next) {
		int put_if;
		if (!is_toplevel(tempdep) || tempdep->type == TYPE_TRISTATE)
			continue;
		text = get_card_name(tempdep->name);
		if (! text)
			continue;
		printf("  if test \"$card\" = \"all\" -o \"$card\" = \"%s\"; then\n", text);
		free(text);
		put_if = 0;
		for (cond = tempdep->cond, cond_prev = NULL; cond; cond = cond->next) {
			if (!put_if)
				printf("    if ");
			else {
				printf(cond_prev->type == COND_AND ? " &&" : " ||");
				printf("\n      ");
			}
			if (cond->not)
				printf(" ! ");
			for (j = 0; j < cond->left; j++)
				printf("( ");
			printf("( test \"$CONFIG_%s\" = \"y\" -o \"$CONFIG_%s\" = \"m\" )", cond->name, cond->name);
			for (j = 0; j < cond->right; j++)
				printf(" )");
			put_if = 1;
			cond_prev = cond;
		}
		if (put_if)
			printf("; then\n");

		sel_remove_hitflags();
		for (sel = tempdep->sel; sel; sel = sel->next)
			sel_print_acinclude(sel);			
		for (sel = tempdep->sel; sel; sel = sel->next) {
			if (sel->dep->type == TYPE_INT)
				continue;
			if (is_always_true(sel->dep))
				continue;
			printf("      ");
			ver = get_version_dep(sel->name);
			if (ver) {
				print_version_dep(ver);
				printf(" && ");
			}
			printf("CONFIG_%s=\"%c\"\n", sel->name,
			       (sel->dep && sel->dep->type == TYPE_BOOL) ? 'y' : 'm');
		}
		text = convert_to_config_uppercase("CONFIG_", tempdep->name);
		if (tempdep->type == TYPE_INT)
			printf("      %s=\"%d\"\n", text, tempdep->int_val);
		else
			printf("      %s=\"y\"\n", text);
		free(text);
		printf("      probed=1\n");
		if (put_if)
			printf("    elif test -z \"$probed\"; then\n"
			       "      probed=0\n"
			       "    fi\n");
		printf("  fi\n");
	}
	printf("  if test -z \"$probed\"; then\n"
	       "    AC_MSG_ERROR(Unknown option $card)\n"
	       "  elif test \"$probed\" = \"0\" -a \"$card\" != \"all\"; then\n"
	       "    AC_MSG_ERROR(Unsupported option $card)\n"
	       "  fi\n"
	       "done\n\n");

	printf("AC_MSG_RESULT($SELECTED_CARDS)\n\n");
	printf("CONFIG_SND=\"m\"\n");
	printf("])\n\n");

	printf("AC_DEFUN([ALSA_TOPLEVEL_DEFINES], [\n");
	for (tempdep = all_deps; tempdep; tempdep = tempdep->next) {
		if (!output_ac_define(tempdep))
			continue;
		text = convert_to_config_uppercase("CONFIG_", tempdep->name);
		printf("if test -n \"$%s\"; then\n", text);
		if (tempdep->type == TYPE_INT) {
			printf("  AC_DEFINE_UNQUOTED([%s], [%d])\n",
			       text, tempdep->int_val);
		} else if (tempdep->type == TYPE_BOOL)
			printf("  AC_DEFINE(%s)\n", text);
		else
			printf("  AC_DEFINE(%s_MODULE)\n", text);
		printf("fi\n");
		free(text);
	}

	printf("])\n\n");
	printf("AC_DEFUN([ALSA_TOPLEVEL_OUTPUT], [\n");
	printf("dnl output all subst\n");
	for (tempdep = all_deps; tempdep; tempdep = tempdep->next) {
		text = convert_to_config_uppercase("CONFIG_", tempdep->name);
		printf("AC_SUBST(%s)\n", text);
		free(text);
	}
	printf("])\n\n");
}

// Output in toplevel.conf
static void output_makeconf(void)
{
	struct dep *tempdep;
	char *text;
	
	printf("# Soundcard configuration for ALSA driver\n");
	printf("# Copyright (c) by Jaroslav Kysela <perex@perex.cz>,\n");
	printf("#                  Anders Semb Hermansen <ahermans@vf.telia.no>\n\n");
	for (tempdep = all_deps; tempdep; tempdep = tempdep->next) {
		text = convert_to_config_uppercase("CONFIG_", tempdep->name);
		printf("%s=@%s@\n", text, text);
		free(text);
	}
}

// Output in config.h
static void output_include(void)
{
	struct dep *tempdep;
	char *text;
	
	printf("/* Soundcard configuration for ALSA driver */\n");
	printf("/* Copyright (c) by Jaroslav Kysela <perex@perex.cz>, */\n");
	printf("/*                  Anders Semb Hermansen <ahermans@vf.telia.no> */\n\n");
	for (tempdep = all_deps; tempdep; tempdep = tempdep->next) {
		text = convert_to_config_uppercase("CONFIG_", tempdep->name);
		printf("#undef %s%s\n", text,
		       tempdep->type == TYPE_TRISTATE ? "_MODULE" : "");
		free(text);
	}
}

// example: sb16 -> CONFIG_SND_SB16
static char *convert_to_config_uppercase(const char *pre, const char *line)
{
	char *holder, *p;
	int i;

	holder = malloc(strlen(line) * 2 + strlen(pre) + 1);
	if (holder == NULL)
		nomem();
	p = strcpy(holder, pre) + strlen(pre);
	for (i = 0; i < strlen(line); i++)
		switch (line[i]) {
		case '-':
			*p++ = '_';
			break;
		default:
			*p++ = toupper(line[i]);
			break;
		}

	*p++ = '\0';

	return holder;
}

#if 0
// example: a'b -> a\'b
static char *convert_to_escape(const char *line)
{
	char *holder, *p;
	int i;

	holder = malloc(strlen(line) + 1);
	if (holder == NULL)
		nomem();
	p = holder;
	for (i = 0; i < strlen(line); i++)
		switch (line[i]) {
		case '\'':
			*p++ = '`';
			break;
		default:
			*p++ = line[i];
			break;
		}

	*p++ = '\0';

	return holder;
}
#endif

// example: snd-sb16 -> sb16
static char *remove_word(const char *remove, const char *line)
{
	char *holder;
	int i;

	holder=malloc(strlen(line)-strlen(remove)+1);
	if(holder==NULL)
	{
		fprintf(stderr, "Not enough memory\n");
		exit(EXIT_FAILURE);
	}

	for(i=strlen(remove);i<strlen(line);i++)
		holder[i-strlen(remove)]=line[i];

	holder[i-strlen(remove)]='\0';

	return holder;
}

// example: SND_ABCD_DEF -> abcd-def
static char *get_card_name(const char *line)
{
	char *result, *tmp = malloc(strlen(line) + 16);
	int i;

	if (tmp == NULL)
		nomem();
	for (i = 0; i < strlen(line); i++) {
		if (line[i] == '_')
			tmp[i] = '-';
		else
			tmp[i] = tolower(line[i]);
	}
	tmp[i] = '\0';
	if (strncmp(tmp, "snd-", 4))
		return NULL;
	result = remove_word("snd-", tmp);
	free(tmp);
	return result;
}

// Main function
int main(int argc, char *argv[])
{
	int method = METHOD_ACINCLUDE;
	int argidx = 1;
	char *filename;
	struct cond *template = NULL;

	// Find out which method to use
	if (argc < 2)
		usage(argv[0]);
	while (1) {
		if (argc <= argidx + 1)
			break;
		if (strcmp(argv[argidx], "--basedir") == 0) {
			basedir = strdup(argv[argidx + 1]);
			if (basedir == NULL)
				nomem();
			argidx += 2;
			continue;
		}
		if (strcmp(argv[argidx], "--hiddendir") == 0) {
			hiddendir = strdup(argv[argidx + 1]);
			if (hiddendir == NULL)
				nomem();
			argidx += 2;
			continue;
		}
		if (strcmp(argv[argidx], "--versiondep") == 0) {
			if (read_version_deps(argv[argidx + 1]) < 0)
				exit(EXIT_FAILURE);
			argidx += 2;
			continue;
		}
		break;
	}
	if (strcmp(argv[argidx], "--acinclude") == 0)
		method = METHOD_ACINCLUDE;
	else if (strcmp(argv[argidx], "--makeconf") == 0)
		method = METHOD_MAKECONF;
	else if (strcmp(argv[argidx], "--include") == 0)
		method = METHOD_INCLUDE;
	else
		usage(argv[0]);
	argidx++;
	
	// Check the filename
	if (argc > argidx)
		filename = argv[argidx++];
	else
		filename = "Kconfig";

	// Read the file into memory
	if (read_file(filename, &template) < 0) {
		fprintf(stderr, "Error reading %s: %s\n",
			filename ? filename : "stdin", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (template)
		free_cond(template);
	// Resolve dependencies
	resolve_dep(all_deps);
	optimize_dep(all_deps);
	resolve_sel(all_deps);

	// Use method
	switch (method) {
	case METHOD_ACINCLUDE:
		output_acinclude();
		break;
	case METHOD_MAKECONF:
		output_makeconf();
		break;
	case METHOD_INCLUDE:
		output_include();
		break;
	default:
		fprintf(stderr, "This should not happen!\n");
		usage(argv[0]);
		break;
	}

	// Free some memory
	del_all_from_list();

	exit(EXIT_SUCCESS);
}

// Print out syntax
static void usage(char *programname)
{
	fprintf(stderr, "Usage: %s --acinclude [<cfgfile>]\n", programname);
	fprintf(stderr, "       %s --makeconf [<cfgfile>]\n", programname);
	fprintf(stderr, "       %s --include [<cfgfile>]\n", programname);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "       --basedir <basedir>\n");
	fprintf(stderr, "       --hiddendir <hiddendir>\n");
	exit(EXIT_FAILURE);
}
