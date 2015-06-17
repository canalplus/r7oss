
#ifndef DEF_LOWPOWER_FRONTPANEL
#define DEF_LOWPOWER_FRONTPANEL


/*
 * This defines the size of the data array.
 * Be sure to keep it small enough in order to keep room in the dcache for
 * other datas.
 */
#define LPFP_DATA_SIZE		256

/*
 * The following numbers represent indexes of the data array that hold special
 * values and thus MUST NOT be used as temporary variables, i.e. not as a
 * stack.
 */
#define LPFP_INDEX_PIO3		0
#define LPFP_INDEX_PIO11	1

/* Helper macro to generate arbitrary timestamps */
#define LPLP_MK_TIME(days, hour, min, sec)   (60 * (60 * (24 * (days) + (hour)) + (min)) + (sec))

/* Special value to guarantee that all conditionnal statements will be used */
#define LPFP_SPECIAL_TIME	LPLP_MK_TIME(15000, 23, 59, 59)


/*
 * The array is used as a temporary stack by the code mentionned
 * above. This array MUST be loaded in data cache before disabling the RAM.
 */
extern unsigned long lpfp_data[LPFP_DATA_SIZE];


#endif

