/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *	   Sudeep Biswas	  <sudeep.biswas@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include "poke_table.h"

/*
 * The poke table is a series of long words, in the format:
 *
 *	opcode, operand, operand (, operand)*
 *
 * An opcode of 0 marks the table end.
 * In this case it is assumed that an opcode will be followed
 * by 3 operands. So basically the format is:
 *	opcode, operand_1, operand_2, operand_3
 * Inside the poke table each instruction is stored in the above
 * mentioned format. If any opcode has fewer operands then
 * the remaining operands will be zero and pokeloop will simply
 * bypass them before reading/executing the next opcode.
 */

	.global		sti_pokeloop

	.balign	32
sti_pokeloop:

do_poking:
	ldr	r3, [r0], #4	/* opcode */
	ldr	r2, [r0], #4	/* operand 1 */

/* END_MARKER/RETURN(x) */
	cmp	r3, #OP_END_POKES
	bne	do_poke32
	mov	r0, r2		/* Exit code */
	bx	lr		/* Return point */


/* POKE32(A, VAL) */
do_poke32:
	cmp	r3, #OP_POKE32
	bne	do_or32
	ldr	r3, [r0], #4	/* read VAL */
	str	r3, [r2]	/* *A = VAL */
	add	r0, r0, #4	/* increment to next opcode*/
	b	do_poking

/* OR32(A, VAL) */
do_or32:
	cmp	r3, #OP_OR32
	bne	do_update32
	ldr	r3, [r0], #4	/* VAL */
	ldr	r12, [r2]	/* *A */
	orr	r3, r3, r12	/* *A | OR */
	str	r3, [r2]	/* *A |= OR */
	add	r0, r0, #4	/* increment to next opcode*/
	b	do_poking

/* UPDATE32(A, and, OR) */
do_update32:
	cmp	r3, #OP_UPDATE32
	bne	do_while_ne32
	ldr	r3, [r0], #4	/* read and */
	ldr	r12, [r2]	/* *A */
	and	r12, r12, r3	/* *A & AND */
	ldr	r3, [r0], #4	/* read OR */
	orr	r12, r12, r3	/* (*A & and) | OR */
	str	r12, [r2]	/* *A = ((*A & and) | OR) */
	b	do_poking

/* WHILE_NE32(A, and, VAL) */
do_while_ne32:
	cmp	r3, #OP_WHILE_NE32
	bne	do_delay
1:	ldr	r3, [r0]	/* read and */
	ldr	r12, [r2]	/* *A */
	and	r3, r3, r12	/* *A & AND */
	ldr	r12, [r0, #4]	/* read VAL */
	cmp	r3, r12		/* if ((*A & and) == VAL) */
	bne	1b		/* loop if false */
	add	r0, r0, #8	/* increment to next opcode*/
	b	do_poking

/* DELAY(ITERATIONS) */
do_delay:
1:	subs	r2, r2, #1	/* if (--OPERATIONS == 0) */
	beq	1b		/* subs should have set Z bit if equal */
	add	r0, r0, #8
	b	do_poking

	.balign 4
.global sti_pokeloop_sz
sti_pokeloop_sz:
	.long . - sti_pokeloop
