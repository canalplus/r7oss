/***************************************************************************
 *            au88x0_a3d.h
 *
 *  Fri Jul 18 14:16:03 2003
 *  Copyright  2003  mjander
 *  mjander@users.sourceforge.net
 ****************************************************************************/

typedef struct {
	ulong this00; // CAsp4hwIO
	ulong this04; 
	ulong this08; // A3D module Index
	
} CA3dSourceHw_t;


short A3dHrirZeros[0x1C] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
short A3dItdDlineZeros[0x16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
short A3dHrirImpulse[0x1c] = {7FFFh,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
