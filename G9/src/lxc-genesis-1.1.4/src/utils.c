#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

void *lxc_genesis_strdup(const char *s)
{
    char *copy;

    if (s == NULL)
        return NULL;

    copy = strdup(s);
    if (copy == NULL) {
        fprintf(stderr, "ERROR: not enough memory\n");
        exit(EXIT_FAILURE);
    }

    return copy;
}

void lxc_genesis_free(void *p)
{
    if (p != NULL)
        free(p);
}
