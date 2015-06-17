#ifdef HQVDPLITE_API_FOR_STAPI_SANITY_CHECK
#include <stdlib.h>
#include <stdio.h>

#define EXPECTED_CMD_SIZE (2800)

#include "HQVDP_api.h"

void esw_sync() {}

int main () {

  HQVDPLITE_CMD_t mycmd;
  U8 shiftyh,shiftch;
  U8 shiftyv,shiftcv;
  int sum,i;
  int sizemismatch;
  sizemismatch=0;

  printf("api compilation ok\n");
  printf("API VERSION %s\n", HQVDP_API_VERSION);
  printf("FW VERSION %s\n", HQVDP_FW_VERSION);
  printf("rdplug  base addr = 0x%.8x\n", HQVDP_STR64_RDPLUG_OFFSET );
  printf("wrplug  base addr = 0x%.8x\n", HQVDP_STR64_WRPLUG_OFFSET );
  printf("mailbox base addr = 0x%.8x\n", HQVDP_MBX_OFFSET);
  printf("pmem base addr = 0x%.8x\n", HQVDP_PMEM_OFFSET);
  printf("dmem base addr = 0x%.8x\n", HQVDP_DMEM_OFFSET);
  printf("pmem size = 0x%.8x\n", HQVDPLITE_PMEM_SIZE);
  printf("dmem size = 0x%.8x\n", HQVDPLITE_DMEM_SIZE);

  if ( sizeof(U8) == 1 ) {
    printf("U8 size ok (%d)\n",sizeof(U8));
  } else {
    printf("U8 size ko (%d)\n",sizeof(U8));
    sizemismatch++;
  }

  if ( sizeof(S8) == 1 ) {
    printf("S8 size ok (%d)\n",sizeof(S8));
  } else {
    printf("S8 size ko (%d)\n",sizeof(S8));
    sizemismatch++;
  }

  if ( sizeof(U16) == 2 ) {
    printf("U16 size ok (%d)\n",sizeof(U16));
  } else {
    printf("U16 size ko (%d)\n",sizeof(U16));
    sizemismatch++;
  }

  if ( sizeof(S16) == 2 ) {
    printf("S16 size ok (%d)\n",sizeof(S16));
  } else {
    printf("S16 size ko (%d)\n",sizeof(S16));
    sizemismatch++;
  }

  if ( sizeof(U32) == 4 ) {
    printf("U32 size ok (%d)\n",sizeof(U32));
  } else {
    printf("U32 size ko (%d)\n",sizeof(U32));
    sizemismatch++;
  }

  if ( sizeof(S32) == 4 ) {
    printf("S32 size ok (%d)\n",sizeof(S32));
  } else {
    printf("S32 size ko (%d)\n",sizeof(S32));
    sizemismatch++;
  }

  if ( sizeof(BOOL) == 1 ) {
    printf("BOOL size ok (%d)\n",sizeof(BOOL));
  } else {
    printf("BOOL size ko (%d)\n",sizeof(BOOL));
    sizemismatch++;
  }

  if ( sizeof(HQVDPLITE_CMD_t) == EXPECTED_CMD_SIZE) {
    printf("cmd size ok (%d)\n",sizeof(HQVDPLITE_CMD_t));
  } else {
    printf("cmd size ko, should be %d and not %d\n",EXPECTED_CMD_SIZE,sizeof(HQVDPLITE_CMD_t));
    sizemismatch++;
  }
  if (sizemismatch != 0) {
    exit(1);
  }

  HVSRC_getFilterCoefHV(10,
                        1720,
                        1400,
                        1080,
                        600,
                        HVSRC_LUT_Y_SHARP,
                        HVSRC_LUT_C_OPT,
                        HVSRC_LUT_Y_MEDIUM,
                        HVSRC_LUT_C_LEGACY,
                        0,
                        1,
                        0,
                        0,
                        mycmd.Hvsrc.YhCoef,
                        mycmd.Hvsrc.ChCoef,
                        mycmd.Hvsrc.YvCoef,
                        mycmd.Hvsrc.CvCoef,
                        &shiftyh,
                        &shiftch,
                        &shiftyv,
                        &shiftcv);

  printf("H shift Y = %d shift C = %d\n",shiftyh,shiftch);
  printf("V shift Y = %d shift C = %d\n",shiftyv,shiftcv);

  if ( shiftyh != 8 || shiftyv != 9 || shiftch != 8 || shiftcv != 9 ) {
    printf("bad Lut shift selection\n");
    exit(1);
  } else {
    printf("lut selection ok\n");
  }

  sum = 0;
  for (i=0; i<HQVDPLITE_DMEM_SIZE; i++ ) {
    sum = (sum +  HQVDPLITE_dmem[i]) % 65536;
  }
  if ( sum != HQVDPLITE_DMEM_SUM ) {
    printf("bad DMEM sum\n");
    exit(1);
  } else {
    printf("DMEM code ok\n");
  }

  sum = 0;
  for (i=0; i<HQVDPLITE_PMEM_SIZE; i++ ) {
    sum = (sum +  HQVDPLITE_pmem[i]) % 65536;
  }
  if ( sum != HQVDPLITE_PMEM_SUM ) {
    printf("bad PMEM sum\n");
    exit(1);
  } else {
    printf("PMEM code ok\n");
  }

  exit(0);
}
#endif

