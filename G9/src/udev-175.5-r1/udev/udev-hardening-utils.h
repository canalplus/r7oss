#ifndef _UDEV_HARDENING_UTILS_
#define _UDEV_HARDENING_UTILS_

#include <string.h>

#ifndef MAX
# define MAX(x,y) (x<y)?(y):(x)
#endif /* MAX */

#ifndef MATCH
# define MATCH(x,y)(strncmp(x, y, MAX(strlen(x), strlen(y))) == 0)
#endif /* MATCH */

#ifndef FOREACH
# define FOREACH(i, l) for(i = l; *i != NULL; ++i)
#endif /* FOREACH */

#endif /*  _UDEV_HARDENING_UTILS_ */
