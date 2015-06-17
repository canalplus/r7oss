/*
 * Copyright (C) 2014  STMicroelectronics
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *  	   Sudeep BIswas      <sudeep.biswas@st.com>
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/linkage.h>
#include <asm/cp15.h>

	.text
	.pushsection    .idmap.text, "aw"
ENTRY(sti_cps_exec)
	ldr	r1,=idmap_pgd		@change to id map before mmu off
	ldr	r1, [r1]
	adr	r2, VA2PA
	ldr	r2, [r2]
	sub	r1, r1, r2	@r1 now contain the physical address of idmap

	mcr	p15, 0, r0, c8, c7, 0
	mcr	p15, 0, r1, c2, c0, 0		@ set TTBR 0
	mcr	p15, 0, r1, c2, c0, 1		@ set TTBR 1
	mcr	p15, 0, r0, c8, c7, 0

	@ jump on __pa(cps_mmu_off) code
	adr	r1, cps_mmu_off		@ r1 = __va(cps_mmu_off)
	adr	r2, VA2PA
	ldr	r2, [r2]
	sub	r1, r1, r2		@ r1 = __pa(cps_mmu_off)
	mov	r0, r0
	mov	r0, r0
	mov     pc, r1

        .balign 4
cps_mmu_off:
	@ now running on identity mapping
	@ CPU can turn-off MMU
	mrc	p15, 0, r1, c1, c0, 0		@ read SCTLR
        bic	r1, r1, #CR_M			@ disable MMU
	mcr	p15, 0, r1, c1, c0, 0		@ set SCTLR

	mov 	r0, r0
	mov	r0, r0

	ldm	r0, {r4 - r7}			@ r4 = __pa(enter_table)
						@ r6 = __pa(sti_eram_code)
						@ r7 = __pa(pokeloop)


	mov	r0, r4	@ move __pa(enter_table) to be ready
			@ when pokeloop is executed


	blx	r6	@ call the sti_cps_on_eram
			@ placed in the ERAM


.balign		4
ENTRY(sti_cps_on_eram)
	/*
	 * runs the poke tables to turn-off the SOC
	 */
	blx	r7	@ call the __pokeloop on eram
1:
	wfi		@ WaitForInterrupt until
			@ the SOC is turned-off
	b 1b

.balign 4
ENTRY(sti_cps_on_eram_sz)
	.long	. - sti_cps_on_eram

/* sti_defrost_kernel is where all cpus start executing after CPS wakeup */
ENTRY(sti_defrost_kernel)
	@ the sti_defrost_kernel:
	@ - returns to the caller of sti_cps_exec_table in case of CPU-0
	@ - jumps on cps_secondary_entry in case of CPU != 0
	@ check between main and secondary core

	mrc	p15, 0, r0, c0, c0, 5
	tst	r0, #(1 << 31)		@ Register has MPCore format?
	beq	cpu_resume
	tst	r0, #(1 << 30)		@ 1 means this is a uni-processor ARM
	bne	cpu_resume
	ands	r0, #0xFF		@ Is CPU 0 (in affinity level 0)?
	beq	cpu_resume

	@ ARM core != 0 jump on stm_defrost_secondary_cpu

	b	sti_defrost_secondary_cpu

ENDPROC(sti_defrost_kernel)

#ifdef CONFIG_SMP
sti_defrost_secondary_cpu:

	@ wait the green ligh from the CPU-0
	mrc	p15, 0, r0, c0, c0, 5
	and	r0, r0, #15
	adr	r4, 1f
	ldmia	r4, {r5, r6}
	sub	r4, r4, r5
	add	r6, r6, r4

pen:    ldr	r7, [r6]
	cmp	r7, r0
	bne	pen

	b secondary_startup

	.balign 4
1:
	.long	.
	.long pen_release
#else
sti_defrost_secondary_cpu:
	@ the secondary CPUs stop here forever
1:	b	1b
#endif
	.balign 32
ENTRY(VA2PA)
	.long	0
