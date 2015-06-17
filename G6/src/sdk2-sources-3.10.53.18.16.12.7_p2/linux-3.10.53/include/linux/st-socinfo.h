#ifndef _STSOCINFO_H
#define _STSOCINFO_H

extern long st_socinfo_version_major;
extern long st_socinfo_version_minor;

static inline long st_socinfo_get_version_major(void){
	return st_socinfo_version_major;
}

static inline long st_socinfo_get_version_minor(void){
	return st_socinfo_version_minor;
}

#endif /* _STSOCINFO_H */
