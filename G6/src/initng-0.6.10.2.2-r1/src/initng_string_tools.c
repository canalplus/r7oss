/*
 * Initng, a next generation sysvinit replacement.
 * Copyright (C) 2006 Jimmy Wennlund <jimmy.wennlund@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "initng.h"

#define _GNU_SOURCE
#include <fnmatch.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include "initng_string_tools.h"
#include "initng_toolbox.h"


/** Re-links a string to a argv-like string-array.
 * Example: \t  /bin/executable --ham -flt   --moohoho  lalala
 *       => \t  /bin/executable\0--ham\0-flt\0  --moo\0hoho\0 lalala
 *    argv:     ^[0]              ^[1]   ^[2]    ^[3]   ^[4]   ^[5]
 *    argc: 6
 *
 * string: String to split.
 * delim : Char that splits the strings.
 * argc  : Pointer, sets the number of splits done.
 * ofs   : Offset, start from this arg to fill.
 * @idea DEac-
 * @author TheLich
 * @got_it_working powerj
 * @improved_it ismael
 */
char **split_delim(const char *string, const char *delim, size_t * argc, int ofs)
{
	int len;
	char **array;
	size_t i = 0;

	if (!string)
		return (NULL);

	array = (char **) i_calloc(1, sizeof(char *));

	while (string[ofs] != '\0')
	{
		len = strcspn(string + ofs, delim);
		if (len != 0)
		{
			i++;
			array = (char **) i_realloc(array, sizeof(char *) * (i + 1));
			array[i - 1] = strndup(string + ofs, len);
		}
		else
			len = 1;

		ofs += len;
	}

	array[i] = NULL;

	if (argc)
		*argc = i;

	return array;
}

void split_delim_free(char **strs)
{
	int i;

	if (!strs)
		return;

	for (i = 0; strs[i] != NULL; i++)
		free(strs[i]);

	free(strs);
}

/*
 * a simple strncasecmp, which only returns TRUE or FALSE
 * it will also add the pointer of string, the value, to forward the
 * pointer to next word.
 */
int st_cmp(char **string, const char *to_cmp)
{
	int chars = 0;

	assert(to_cmp);
	assert(string);

	chars = strlen(to_cmp);

	/* skip beginning first spaces */
	JUMP_SPACES(*string);
	/* this might be an "comp pare" */
	if ((*string)[0] == '"' && to_cmp[0] != '"')
		(*string)++;

	/* ok, strcasecmp this */
	if (strncasecmp((*string), to_cmp, chars) == 0)
	{
		/* increase the string pointer */
		(*string) += chars;
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Senses what in the string, that is a word, (can be a
 * line with "this is one word", it will i_strdup it, and
 * add word length on string pointer
 */
char *st_dup_next_word(const char **string)
{
	char *td = NULL;
	int i = 0;

	assert(string);

	/* skip beginning first spaces */
	JUMP_SPACES(*string);
	if (!(*string)[0] || (*string)[0] == '\n')
		return (NULL);

	/* handle '"' chars */
	if ((*string)[0] == '"')
	{
		(*string)++;
		i = strcspn(*string, "\"");
		if (i < 1)
			return (NULL);
		td = i_strndup(*string, i);
		(*string) += i;
		if (*string[0] == '"')
			(*string)++;
		return (td);
	}

	/* handle '{' '}' chars */
	if ((*string)[0] == '{')
	{
		(*string)++;
		i = strcspn(*string, "}");
		if (i < 1)
			return (NULL);
		td = i_strndup(*string, i);
		(*string) += i;
		if (*string[0] == '}')
			(*string)++;
		return (td);
	}

	/* or copy until space tab newline, ; or , */
	i = strcspn(*string, " \t\n;,");
	if (i < 1)
		return (NULL);
	/* copy string */
	td = i_strndup(*string, i);
	(*string) += i;
	return (td);
}

/*
 * This will dup, in the string, what is resolved as the line,
 * (stops with '\n', ';', '\0'), copy it and increment *string with len
 */
char *st_dup_line(char **string)
{
	char *td = NULL;
	int i = 0;

	assert(string);

	/* skip beginning first spaces */
	JUMP_SPACES(*string);
	if (!(*string)[0] || (*string)[0] == '\n')
		return (NULL);

	/* handle '"' chars */
	if ((*string)[0] == '"')
	{
		(*string)++;
		i = strcspn(*string, "\"");
		if (i < 1)
			return (NULL);
		td = i_strndup(*string, i);
		(*string) += i;
		if (*string[0] == '"')
			(*string)++;
		return (td);
	}

	/* handle '{' '}' chars */
	if ((*string)[0] == '{')
	{
		(*string)++;
		i = strcspn(*string, "}");
		if (i < 1)
			return (NULL);
		td = i_strndup(*string, i);
		(*string) += i;
		if (*string[0] == '}')
			(*string)++;
		return (td);
	}

	/* or copy until space tab newline, ; or , */
	i = strcspn(*string, "\n;");
	if (i < 1)
		return (NULL);
	/* copy string */
	td = i_strndup(*string, i);
	(*string) += i;
	return (td);
}


/* function strips "test/go/th" to "th" */
const char *st_strip_path(const char *string)
{
	char *ret = NULL;

	assert(string);

	/* get the last '/' char in string */
	if (!(ret = strrchr(string, '/')))
		return (string);

	/* skip to char after the last '/' */
	ret++;
	return (ret);
}

/* function strips "test/go/th" to "test/go" */
char *st_get_path(const char *string)
{
	char *last_slash;

	assert(string);

	/* get the last_slash position */
	if (!(last_slash = strrchr(string, '/')))
		return (i_strdup(string));

	/* ok return a copy of that string */
	return (i_strndup(string, (int) (last_slash - string)));
}

/* function strips "test/go/th" to "test/go" and then to "test" */
int st_strip_end(char **string)
{

	assert(string);
	assert(*string);

	/* go to last '/' */
	char *end = strrchr(*string, '/');

	if (end)
	{
		end[0] = '\0';
		return (TRUE);
	}
	return (FALSE);
}

/*
 * This search for a pattern.
 * If string is "net/eth0" then pattern "net/et*" and pattern "net/eth?"
 * should return true.
 * The number of '/' chars must be equal on string and pattern.
 */
 // examples:
 // string           pattern            do_match
 // net/eth0         net/et*            TRUE
 // net/eth1         net/eth?           TRUE
 // test/abc/def     test/*/*           TRUE
 // daemon/test      daemon/*/*         FALSE

int service_match(const char *string, const char *pattern)
{
	int stringslash = 0;
	int patternslash = 0;
	const char *tmp;

	assert(string);
	assert(pattern);
	D_("matching string: \"%s\", to pattern: \"%s\"\n", string, pattern);

	/* do pattern matching only if service name does not contain wildcards */
	if (strchr(string, '*') || strchr(string, '?'))
	{
		F_("The string \"%s\" contains wildcards line '*' and '?'!\n",
		   string);
		return (FALSE);
	}

#ifdef FNM_CASEFOLD
	/* check if its a match */
	if (fnmatch(pattern, string, FNM_CASEFOLD) != 0)
		return (FALSE);
#else
	/* check if its a match */
	if (fnmatch(pattern, string, 0) != 0)
		return (FALSE);
#endif

	/* count '/' in string */
	tmp = string - 1;
	while ((tmp = strchr(++tmp, '/')))
		stringslash++;
	/* count '/' in pattern */
	tmp = pattern - 1;
	while ((tmp = strchr(++tmp, '/')))
		patternslash++;

	/*D_("service_match(%s, %s): %i, %i\n", string, pattern, stringslash, patternslash); */

	/* use fnmatch that shud serve us well as these matches looks like filename matching */
	return (stringslash == patternslash);
}


/*
 * This match is simply dont that we take pattern, removes any leading
 * '*' or '?', and to a strstr on that string.
 */
int match_in_service(const char *string, const char *pattern)
{
	char *copy;
	char *tmp;

	assert(string);
	assert(pattern);

	/* if the string contains a '/' it is looking for an exact match, so no searching here */
	if (strchr(pattern, '/'))
		return (FALSE);

	/* remove starting wildcards */
	while (pattern[0] == '*' || pattern[0] == '?')
		pattern++;

	/* check if it matches */
	if (strstr(string, pattern))
		return (TRUE);

	/* if pattern wont have a '*' or '?' there is no ida to continue */
	if (!strchr(pattern, '*') && !strchr(pattern, '?'))
		return (FALSE);

	/* now copy pattern, and remove ending '*' && '?' */
	copy = i_strdup(pattern);
	assert(copy);

	/* Trunk for any '*' or '?' found */
	while ((tmp = strrchr(copy, '*')) || (tmp = strrchr(copy, '?')))
		tmp[0] = '\0';

	/* check again */
	tmp = strstr(string, copy);

	/* cleanup */
	free(copy);

	/* if tmp was set return TRUE */
	return (tmp != NULL);
}

/*
 * mprintf, a sprintf clone that automaticly mallocs the string
 * and new content to same string applys after that content.
 */
int mprintf(char **p, const char *format, ...)
{
	va_list arg;				/* used for the variable lists */

	int len = 0;				/* will contain lent for current string */
	int add_len = 0;			/* This mutch more strings are we gonna alloc for */

	/*printf("\n\nmprintf(%s);\n", format); */

	/* count old chars */
	if (*p)
		len = strlen(*p);

	/*
	 * format contains the minimum needed chars that
	 * are gonna be used, so we set that value and try
	 * with that.
	 */
	add_len = strlen(format) + 1;

	/*
	 * Now realloc the memoryspace containing the
	 * string so that the new apending string will
	 * have room.
	 * Also have a check that it succeds.
	 */
	/*printf("Changing size to %i\n", len + add_len); */
	if (!(*p = realloc(*p, ((len + add_len) * sizeof(char)))))
		return (-1);

	/* Ok, have a try until vsnprintf succeds */
	while (1)
	{
		/* start filling the newly allocaded area */
		va_start(arg, format);
		int done = vsnprintf((*p) + len, add_len, format, arg);

		va_end(arg);

		/* check if that was enouth */
		if (done > -1 && done < add_len)
		{
			/*printf("GOOD: done : %i, len: %i\n", done, add_len); */

			/* Ok return happily */
			return (done);
		}
		/*printf("BAD: done : %i, len: %i\n", done, add_len); */

		/* try increase it a bit. */
		add_len = (done < 0 ? add_len * 2 : done + 1);
		/*printf("Changing size to %i\n", len + add_len); */
		if (!(*p = realloc(*p, ((len + add_len) * sizeof(char)))))
			return (-1);

	}

	/* will never go here */
	return (-1);
}

void st_replace(char * dest, char * src, const char * n, const char * r)
{
	char *p;
	char *d = dest;
	char *last = src;
	int nlen = strlen(n);
	int rlen = strlen(r);

	while ((p = strstr(last, n)))
	{
		memmove(d, last, p - last);
		d += p - last;
		memmove(d, r, rlen);
		d += rlen;
		last = p + nlen;
	}

	if (d != last)
		memmove(d, last, strlen(last));
}
