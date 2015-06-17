/***************************************************************************
 *            au88x0_a3d.c
 *
 *  Fri Jul 18 14:16:22 2003
 *  Copyright  2003  mjander
 *  mjander@users.sourceforge.net
 *
 * A3D. You may think i'm crazy, but this may work someday. Who knows...
 ****************************************************************************/

/* CA3dSourceHw */
void Asp4A3DTopology::MakeConnections(void) {
	if (this_34 == 0) {
		/* Connect A3DOUT to MIXIN */
		Asp4Topology::Route((int) 1, (uchar) this_0c->this_0c, 
			(uchar) this_08->this_04, (uchar) this_10 + 0x50, (uchar) this_14+0x50);
		/* Connect MIXIN's this_10 and this_14 to MIXOUT this_18 */
		(CAsp4Mix)this_18::EnableInput(int this_10);
		(CAsp4Mix)this_18::EnableInput(int this_14);
		/* Connect MIXOUT this_18 to SRCIN this_0c */
		Asp4Topology::Route((int) 1, (uchar) this_0c, 
			(uchar) this_18->this_08 + 0x30, (uchar) this_0c + 0x40);
		/* Connect SRCOUT this_0c to A3DIN this_20 */
		Asp4Topology::Route((int) 1, (uchar) 0x11, 
			(uchar) this_0c->this0c + 0x20, (uchar) this_20 + 0x70);
	} else {
		if (this_08) {
			/* Connect this_08 to SRCIN this_0c */
			Asp4Topology::Route((int) 1, (uchar) this_0c, 
				(uchar) this_08->this_04, (uchar) this_0c + 0x40);
			/* Connect SRCOUT this_0c to A3DIN this_20 */
			Asp4Topology::Route((int) 1, (uchar) 0x11, 
				(uchar) this_0c->this0c + 0x20, (uchar) this_20 + 0x70);		
		} else {
			// I'm tired. I going to sleep now .
			
		}
	}
	
}
 
void CA3dSourceHw::CA3dSourceHw(CAsp4HwIO *hwio, int a,int b) {
	CA3dSourceHw->this00 = hwio;
	CA3dSourceHw->this04 = a;
	CA3dSourceHw->this08 = b;
}

void CA3dSourceHw::Initialize(int a, int b) {
	CA3dSourceHw->this04 = a;
	CA3dSourceHw->this08 = b;
	CA3dSourceHw::ZeroState(void);
}

#define addr(a,b,c) (a + (c << 0xd) + (d*(1 + (8*4 - 3)*8))*4)

void CA3dSourceHw::SetTimeConsts(short a, short b, short c, short d) {
	hwwrite(vortex->mmio, addr(0x1837C, this04, this08),  a);
	hwwrite(vortex->mmio, addr(0x18388, this04, this08),  b);
	hwwrite(vortex->mmio, addr(0x18380, this04, this08),  c);
	hwwrite(vortex->mmio, addr(0x18384, this04, this08),  d);
}

void CA3dSourceHw::GetTimeConsts(short a, short b, short c, short d) {
	
}

/* Atmospheric absorbtion. */
void CA3dSourceHw::SetAtmosTarget(short a,short b,short c,short d,short e) {
	hwwrite(vortex->mmio, addr(0x190Ec, this04, this08), (e << 0x10) | d);
	hwwrite(vortex->mmio, addr(0x190F4, this04, this08), (b << 0x10) | a);
	hwwrite(vortex->mmio, addr(0x190FC, this04, this08), c);
}

void CA3dSourceHw::SetAtmosTarget(short *a,short *b,short *c,short *d,short *e) {
}

void CA3dSourceHw::SetAtmosCurrent(short a,short b,short c,short d,short e) {
	hwwrite(vortex->mmio, addr(0x190E8, this04, this08), (e << 0x10) | d);
	hwwrite(vortex->mmio, addr(0x190f0, this04, this08), (b << 0x10) | a);
	hwwrite(vortex->mmio, addr(0x190f8, this04, this08), c);
}

void CA3dSourceHw::GetAtmosCurrent(short *a,short *b,short *c,short *d,short *e) {
	*d = hwread(vortex->mmio, addr(0x180e8, this04, this08));
	*e = hwread(vortex->mmio, addr(0x190e8, this04, this08));
	*b = hwread(vortex->mmio, addr(0x180f0, this04, this08));
	*a = hwread(vortex->mmio, addr(0x190f0, this04, this08));
	*c = hwread(vortex->mmio, addr(0x180f8, this04, this08));
}

void CA3dSourceHw::SetAtmosState(short a,short b,short c,short d) {
	hwwrite(vortex->mmio, addr(0x1838C, this04, this08), a);
	hwwrite(vortex->mmio, addr(0x18390, this04, this08), b);
	hwwrite(vortex->mmio, addr(0x18394, this04, this08), c);
	hwwrite(vortex->mmio, addr(0x18398, this04, this08), d);
}

void CA3dSourceHw::GetAtmosState(short a,short b,short c,short d) {
	
}


#define addr2(a,b,c, i) (a + (((this08<<0xb)+i)<<2) + (this04*(5*9*2 - 1)*4*2))
void CA3dSourceHw::SetHrtfTarget(short const *a,short const	*b) {
	int i;
	
	for (i=0; i<0x38; i++)
		hwwrite(vortex->mmio, addr2(0x19100, this04, this08, i), (b[i]<<0x10) | a[i]);
}

void CA3dSourceHw::GetHrtfTarget(short const *a,short const	*b) {
}

void CA3dSourceHw::SetHrtfCurrent(short const *a,short const *b) {
	int i;
	
	for (i=0; i<0x38; i++)
		hwwrite(vortex->mmio, addr2(0x19000, this04, this08, i), (b[i]<<0x10) | a[i]);	
}

void CA3dSourceHw::GetHrtfCurrent(short const *a,short const *b) {
	int i;
	// FIXME: verify this!
	for (i=0; i<0x38; i++)
		a[i] = hwread(vortex->mmio, addr2(0, this04+0xc, this08, i));
	for (i=0; i<0x38; i++)
		b[i] = hwread(vortex->mmio, addr2(0x19000, this04, this08, i));	
}
void CA3dSourceHw::SetHrtfState(short const *a,short const *b) {
	int i;
	
	for (i=0; i<0x38; i++)
		hwwrite(vortex->mmio, addr2(0x191e8, this04, this08, i), (b[i]<<0x10) | a[i]);	
}

void CA3dSourceHw::GetHrtfState(short const *a,short const *b) {
	int i;
	// FIXME: verify this!
	for (i=0; i<0x38; i++)
		a[i] = hwread(vortex->mmio, addr2(0, this04+0x6a, this08, i));
	for (i=0; i<0x38; i++)
		b[i] = hwread(vortex->mmio, addr2(0x191e8, this04, this08, i));	
}

void CA3dSourceHw::GetHrtfOutput(short *left,short *right) {
	*left = hwread(vortex->mmio, addr(0x1839C, this04, this08));
	*right = hwread(vortex->mmio, addr(0x183A0, this04, this08));
}

void CA3dSourceHw::SetHrtfOutput(short left,short right) {
	hwwrite(vortex->mmio, addr(0x1839C, this04, this08), left);
	hwwrite(vortex->mmio, addr(0x183a0, this04, this08), right);
}

/* Interaural Time Difference. 
 * "The other main clue that humans use to locate sounds, is called 
 * Interaural Time Difference (ITD). The differences in distance from 
 * the sound source to a listeners ears means  that the sound will 
 * reach one ear slightly before the other...." */
void CA3dSourceHw::SetItdTarget(short litd,short ritd) {
	if (litd < 0)
		litd = 0;
	if (litd > 0x57FF)
		litd = 0x57FF;
	if (ritd < 0)
		ritd = 0;
	if (ritd > 0x57FF)
		ritd = 0x57FF;
	hwwrite(vortex->mmio, addr(0x191DF+5, this04, this08), (ritd<<0x10)|litd);
}

void CA3dSourceHw::GetItdTarget(short *litd, short *ritd) {
	*ritd = hwread(vortex->mmio, addr(0x181E4, this04, this08));
	*litd = hwread(vortex->mmio, addr(0x191E4, this04, this08));
}

void CA3dSourceHw::SetItdCurrent(short litd, short ritd) {
		litd = 0;
	if (litd > 0x57FF)
		litd = 0x57FF;
	if (ritd < 0)
		ritd = 0;
	if (ritd > 0x57FF)
		ritd = 0x57FF;
	hwwrite(vortex->mmio, addr(0x191DF+1, this04, this08), (ritd<<0x10)|litd);
}

void CA3dSourceHw::GetItdCurrent(short *litd,short *ritd) {
	*ritd = hwread(vortex->mmio, addr(0x181E0, this04, this08));
	*litd = hwread(vortex->mmio, addr(0x191E0, this04, this08));
}

void CA3dSourceHw::SetItdDline(short const	*dline) {
	int i;
	
	for (i=0; i<0x28; i++)
		hwwrite(vortex->mmio, addr2(0x182C8, this04, this08, i), dline[i])
}

void CA3dSourceHw::GetItdDline(short *dline) {
	int i;
	
	for (i=0; i<0x28; i++)
		dline[i] = hwwrite(vortex->mmio, addr2(0x182C8, this04, this08, i));	
}

/* This is maybe used for IID Interaural Intensity Difference. */
void CA3dSourceHw::SetGainTarget(short a,short b) {
	hwwrite(vortex->mmio, addr(0x190E4, this04, this08), (b<<0x10)|a);
}

void CA3dSourceHw::GetGainTarget(short *,short *) {
	*b = hwread(vortex->mmio, addr(0x180e4, this04, this08));
	*a = hwread(vortex->mmio, addr(0x190e4, this04, this08));
}

void CA3dSourceHw::SetGainCurrent(short a,short b) {
	hwwrite(vortex->mmio, addr(0x190DC+4, this04, this08), (b<<0x10)|a);
}

void CA3dSourceHw::GetGainCurrent(short *a,short *b) {
	*b = hwread(vortex->mmio, addr(0x180e0, this04, this08));
	*a = hwread(vortex->mmio, addr(0x190e0, this04, this08));
}

/* Generic A3D stuff */
void CA3dSourceHw::SetA3DSampleRate(int sr) {
	int esp0 = 0;
	esp0 = (((esp0 & 0x7fffffff)|0xB8000000)&0x7) | ((sr&0x1f)<<3);
	hwwrite(vortex->mmio, 0x19C38 + (this08<<0xd), esp0);
}

void CA3dSourceHw::GetA3DSampleRate(int *sr) {
	*sr = ((hwread(vortex->mmio, 0x19C38 + (this08<<0xd))>>3)&0x1f);
	
}

void CA3dSourceHw::EnableA3D(void) {
	hwwrite(vortex->mmio, 19C38 + (this08<<0xd), 0xF0000001);
}

void CA3dSourceHw::DisableA3D(void) {
	hwwrite(vortex->mmio, 19C38 + (this08<<0xd), 0xF0000000);
}

void CA3dSourceHw::SetA3DControlReg(unsigned long ctrl) {
	hwwrite(vortex->mmio, 19C38 + (this08<<0xd), ctrl);
}

void CA3dSourceHw::GetA3DControlReg(unsigned long *ctrl) {
	*ctrl = hwread(vortex->mmio, 19C38 + (this08<<0xd));
}

void CA3dSourceHw::SetA3DPointerReg(unsigned long ptr) {
	hwwrite(vortex->mmio, 19c40 + (this08<<0xd), ctrl);
}

void CA3dSourceHw::GetA3DPointerReg(unsigned long *ptr) {
	*ptr = hwread(vortex->mmio, 19C40 + (this08<<0xd));
}

void CA3dSourceHw::ZeroSliceIO(void) {
	int i;
	
	for (i=0; i<8; i++)
		hwwrite(vortex->mmio, 0x19C00 + (((this08<<0xb)+i)*4), 0);
	for (i=0; i<4; i++)
		hwwrite(vortex->mmio, 0x19C20 + (((this08<<0xb)+i)*4), 0);	
}

void CA3dSourceHw::ZeroState(void) {
	int i;
	
	CA3dSourceHw::SetAtmosState(0,0,0,0);
	CA3dSourceHw::SetHrtfState(A3dHrirZeros, A3dHrirZeros);
	CA3dSourceHw::SetItdDline(A3dItdDlineZeros, A3dItdDlineZeros);
	CA3dSourceHw::SetHrtfOutput(0, 0);
	CA3dSourceHw::SetTimeConsts(0,0,0,0);
	CA3dSourceHw::SetAtmosCurrent(0,0,0,0,0);
	CA3dSourceHw::SetAtmosTarget(0,0,0,0,0);
	CA3dSourceHw::SetItdCurrent(0,0);
	CA3dSourceHw::SetItdTarget(0,0);
	CA3dSourceHw::SetGainCurrent(0,0);
	/* The person who originally wrote this surely was smoking crack ...
	/* (or his/her compiler) */
	CA3dIO::WriteReg(190E4 + (this08<<0xd) + (this04*5*9*2 - this04)*8,0,0);
	
	CA3dSourceHw::SetHrtfCurrent(A3dHrirZeros, A3dHrirZeros);
	CA3dSourceHw::SetHrtfTarget(A3dHrirZeros, A3dHrirZeros)
}

void CA3dSourceHw::ZeroStateA3D(void) {
	int i, ii, var, var2;
	
	CA3dSourceHw::SetA3DControlReg(0);
	CA3dSourceHw::SetA3DPointerReg(0);
	var = this08;
	var2 = this04; // ??
	for (ii=0; ii<4; ii++) {
		this08 = ii;
		
		CA3dSourceHw::ZeroSliceIO(void);
		CA3dSourceHw::ZeroState(void);	
	}
	this04 = var2;
	this08 = var;
}

/* Program A3D block as pass through */
void CA3dSourceHw::ProgramPipe(void) {
	CA3dSourceHw::SetTimeConsts(0,0,0,0);
	CA3dSourceHw::SetAtmosCurrent(0, 0x4000, 0,0,0);
	CA3dSourceHw::SetAtmosTarget(0x4000,0,0,0,0);
	CA3dSourceHw::SetItdCurrent(0,0);
	CA3dSourceHw::SetItdTarget(0,0);
	CA3dSourceHw::SetGainCurrent(0x7fff,0x7fff);
	CA3dSourceHw::SetGainTarget(0x7fff,0x7fff);
	CA3dSourceHw::SetHrtfCurrent(A3dHrirImpulse, A3dHrirImpulse);
	CA3dSourceHw::SetHrtfTarget(A3dHrirImpulse, A3dHrirImpulse);
}
/* VDB = Vortex audio Dataflow Bus */
void CA3dSourceHw::ClearVDBData(unsigned long a) {
	hwwrite(vortex->mmio, 0x19c00 + (((a>>2)*255*4)+a)*8, 0);
	hwwrite(vortex->mmio, 0x19c04 + (((a>>2)*255*4)+a)*8, 0);
}
/*CA3dIO*/

void CA3dIO::WriteReg(unsigned long addr, short a, short b) {
	hwwrite(vortex->mmio, addr, (a<<0x10)|b);
}

/* CHrtf */
void CHrtf::CHrtf(void) {
	this_00 = 0;
	this_04 = 0;
	this_08 = 0;
}
void CHrtf::Initialize(int arg_0 ,int arg_4 ,short *arg_8) {
	this_04 = arg_0;
	this_00 = arg_4;
	this_08 = *arg_8;
}
void CHrtf::InterpolateHrtf(short *arg_0, short *arg_4, short *arg_8) {
	
	
}
/* CVort3dRend */

void CVort3DRend::CVort3DRend(class CAsp4Core *core, class CAsp4HwIO *hwio) {
	this00 = core;
	this04 = hwio;
	this08 = 0;
}

void CVort3DRend::~CVort3DRend(void) {
	if (this08)
		operator delete(this08);
}

int CVort3DRend::Initialize(unsigned short a) {
	if (this00 == 0)
		return -1;
	this14 = a;
	CXtalkHw = operator new(4);
	CXtalkHw::CXtalkHw(this04);

	CXtalkHw::SetGains(XtalkGainsAllChan);
	if (this14) {
		if (this08==0xffff) {
			/* Speaker type 1 ? */
			CXtalkHw::ProgramXtalkNarrow(void);
		} else{
			/* Speaker type 2 ? */
			CXtalkHw::ProgramXtalkWide(void)
		}
	} else {
		/* Headphones ? */
		CXtalkHw::ProgramPipe(void);
	}
	CXtalkHw::SetSampleRate(0x11);
	CXtalkHw::Enable(void);
}

int CVort3DRend::SetGlobalControl(struct _XTALKctrl *xtctrl) {
	if (xtctrl==0)
		return 0;
	
	this0c->this04 = xtctrl->this04;
	this0c->this08 = xtctrl->this08;
	this0c->this0c = xtctrl->this0c;
	
	this0c = xtctrl->this00;
	CXtalkHw = this08; 
	if (this14) {
		if (this08==0xffff)
			CXtalkHw::ProgramXtalkNarrow(void);
		else
			CXtalkHw::ProgramXtalkWide(void)
	} else {
		CXtalkHw::ProgramPipe(void);
	}
}

int  CVort3DRend::GetGlobalControl(struct _XTALKctrl *xtctrl) {
}
int CVort3DRend::AddBuffer(class CVort3dWave *a) {
	return 0;
}
int CVort3DRend::RemoveBuffer(class CVort3dWave *a) {
	return 0;
}
/* CVort3dWave */
void CVort3dWave::CVort3dWave(class CAsp4Core *,class CAsp4HwIO	*,class	CHrtfMgr *) {
	
}
//174743
void CVort3dWave::Initialize(struct _ASPWAVEFORMAT * arg_0, void (__stdcall *)(class CAuWave *,char *,unsigned long,int,unsigned long,unsigned long),unsigned long arg_4 ,void *arg_c) {
	a3dsource_t a3d;
	CWave::Initialize(_ASPWAVEFORMAT *arg_0 ,ASPDIRECTION 0,void (*)(CAuWave *,char *,ulong ,int,ulong,ulong) arg_4,void *arg_c, ulong 1);
	
	this_320 = 0;
	this_324 = 0;
	this_334 = (short)0xffff;
	this_338 = (short)0x8000;
	this_328 = 0;
	this_77c = 0;
	this_790 = 0;
	this_794 = 0;
	this_7a4 = 0;
	this_7c8 = 0; /* struct ? */
	this_79c = 0;
	this_7a0 = 0;
	this_7b4 = 0;
	this_7c0 = 0;
	this_7b0 = 0;
	this_7bc = 0;
	this_788 = 0;
	this_7cc = 0;
		
	if ((a3d = CAdbTopology::GetA3DSource(void)) < 0) {
		(CAsp4Core)this_318::GetProperty((COREPROPERTY) 4, void *arg_c, (ulong) 4);
		if (arg_c == 1)
			return 8007000E;
	} else {
		if (this_340 == 0) {
			operator new((uint) 0xc, (_POOL_TYPE) 0, (ulong) 44334143);
		
		}
	}
}
	
void CVort3DWave::SetLRGains(void) {
	
	
}

int  CVort3dWave::Set3dParms(unsigned long offset, unsigned long arg4, void * arg8) {
	
	if (offset)
		return 80070057;
	if (arg4 == 0xf6) {
		CVort3dWave::Anzio3dToSuperCtrl((Anzio3d *) arg8, (A3DCTRL_SRC_SUPER *) this_370);
		CVort3dWave::Set3dParms((A3DCTRL_SRC_SUPER *) this_370);
		return 0;
	}
	if (arg4 == 0x40) {
		CVort3dWave::Set3dParms((A3DCTRL_SRC_SUPER *) arg8)
	}
}

int  CVort3dWave::Set3dParms(struct A3DCTRL_SRC_SUPER *) {
	int var14;
	struct var0 {
		int a;
		short b;
		char c;
	};

	CAsp4Core = this_318;
	CAsp4Core::GetProperty((COREPROPERTY) 4, (void *) &var14, (ulong) 4);
	CAsp4Core::GetProperty((COREPROPERTY) 1, (void *) &var0+0x10, (ulong) 0x10);
	if ((var0.b != this_354)||(var14 != this_350)) {
		this_354 = var0.b;
		this_350 = var14;
		CHrtfMgr = this_344;
		if (var14 == 1) {
			if (var0.b)
				CHrtfMgr::Initialize((ulong) 0xBB80, (ulong) 0x3a, (ulong) 0x102)
			else
				CHrtfMgr::Initialize((ulong) 0xBB80, (ulong) 2, (ulong) 0x101);
		} else
			CHrtfMgr::Initialize((ulong) 0xBB80, (ulong) 0x38, (ulong) 0);
	}
	if (var14 == 1) {
		if (var0.b == 0)
			CVort3dWave::RenderQuadPan((A3DCTRL_SRC_SUPER *) arg0);
		else
			CVort3dWave::RenderSuperQuad((A3DCTRL_SRC_SUPER *) arg0);
		return 0;
	}
	eax = CAdbTopology::GetA3DSource(void);
	if (eax >= 0) 
		CVort3dWave::Render3d((A3DCTRL_SRC_SUPER *) arg0);
	else
		CVort3dWave::Render2d((A3DCTRL_SRC_SUPER *) arg0);
	return 0;
}

void CVort3dWave::Render2d(struct A3DCTRL_SRC_SUPER *) {
	
	CVort3dWave::Calc2d(A3DCTRL_EAR *,A3DCTRL_EAR	*,float,ulong *,_ASPVOLUME *);
	
	CVort3dWave::SetLRGains(void);
	CAdbTopology::SetFilter(ulong);
	
}
void CVort3dWave::Render3d(struct A3DCTRL_SRC_SUPER *) {
	
	CA3dSourceHw::SetAtmosTarget(short,short,short,short,short);
	CA3dSourceHw::SetAtmosCurrent(short,short,short,short,short);
	CVort3dWave::SetLRGains(void);
	CA3dSourceHw::SetHrtfTarget(short const *,short const *);
	CA3dSourceHw::SetHrtfCurrent(short const *,short const *)
	CA3dSourceHw::SetItdTarget(short,short);
	
	
}

void CVort3dWave::RenderQuadPan(struct A3DCTRL_SRC_SUPER *) {
	
	
}

void CAdbTopology::SetFilter(unsigned long a) {
	int eax;
	
	if (this_1E0 == 0)
		return;  // "CAdbTopology::SetFilter - m_pWtDma not available\n"
	if (a) {
		if (a & 0xFFFF0000)
			eax = 0xcf;
		else {
			a <<= 0x10;
			eax = 0xc;
			while (((a & 0x80000000) == 0)&&(eax > 0)) {
				a <<= 1;
				eax--;
			}
			if (eax) 
				a << 1;
			eax <<= 4;
			a >>= 0x1c;
			eax |= a;
		}
	} else
		eax = 0;
	this_1E4 = (this_1E4 & 0xFFFFFF) | (eax << 0x18);
	this_1E8 = this_1E4 | 1;
	AuWt::WriteReg(0x204 + (this_1DC << 4), this_1E8);
	AuWt::WriteReg(0x200 + (this_1DC << 4), this_1E4);
}

void AuWt::WriteReg(unsigned long addr, unsigned long data) {
	hwwrite(vortex->mmio, (this_04 << 0xf) + addr, data);
}

int  AuWt::ReadReg(unsigned long addr) {
	return hwread(vortex->mmio, (this_04 << 0xf) + addr);
}
