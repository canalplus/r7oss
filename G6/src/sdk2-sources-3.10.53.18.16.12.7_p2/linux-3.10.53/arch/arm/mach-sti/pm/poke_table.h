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
#ifndef __STI_POKE_TABLE_H__
#define __STI_POKE_TABLE_H__

/* Poke table Opcode values */
#define OP_END_POKES				0
#define OP_POKE8				1
#define OP_POKE16				2
#define OP_POKE32				3
#define OP_OR8					4
#define OP_OR16					5
#define OP_OR32					6
#define OP_UPDATE8				7
#define OP_UPDATE16				8
#define OP_UPDATE32				9
#define OP_POKE_UPDATE32			10
#define OP_WHILE_NE8				11
#define OP_WHILE_NE16				12
#define OP_WHILE_NE32				13
#define OP_IF_EQ32				14
#define OP_IF_GT32				15
#define OP_ELSE					16
#define OP_DELAY				17
#define OP_IF_DEVID_GE				18
#define OP_IF_DEVID_LE				19

#ifndef __ASSEMBLER__

/* Poke table commands */
#define POKE8(A, VAL)				{OP_POKE8,  A, VAL}
#define POKE16(A, VAL)				{OP_POKE16, A, VAL}
#define POKE32(A, VAL)				{OP_POKE32, A, VAL}
#define OR8(A, VAL)				{OP_OR8, A, VAL}
#define OR16(A, VAL)				{OP_OR16, A, VAL}
#define OR32(A, VAL)				{OP_OR32, A, VAL}
#define UPDATE8(A, AND, OR)			{OP_UPDATE8, A, AND, OR}
#define UPDATE16(A, AND, OR)			{OP_UPDATE16, A, AND, OR}
#define UPDATE32(A, AND, OR)			{OP_UPDATE32, A, AND, OR}
#define POKE_UPDATE32(A1, A2, AND, SHIFT, OR)	\
		{OP_POKE_UPDATE32, A1, A2, AND, SHIFT, OR}
#define WHILE_NE8(A, AND, VAL)			{OP_WHILE_NE8, A, AND, VAL}
#define WHILE_NE16(A, AND, VAL)			{OP_WHILE_NE16, A, AND, VAL}
#define WHILE_NE32(A, AND, VAL)			{OP_WHILE_NE32, A, AND, VAL}
#define IF_EQ32(NESTLEVEL, A, AND, VAL)
#define IF_GT32(NESTLEVEL, A, AND, VAL)
#define ELSE(NESTLEVEL)
#define ENDIF(NESTLEVEL)
#define DELAY(ITERATIONS)			{OP_DELAY, ITERATIONS}
#define IF_DEVID_GE(NESTLEVEL, VAL)
#define IF_DEVID_LE(NESTLEVEL, VAL)

/* poke_operation is the structure encapsulating
 * a basic poke table entry denoting an operation.
 * Number of operands depend upon the opcode.
 * Maximum of 3 operands are there for any opcode.
 * if any of the opcode uses only 2 operands the last
 * operand would hold a 0. An opcode of 0 means end of
 * the poke table.
 */
struct poke_operation {
	long opcode;
	long operand_1;
	long operand_2;
	long operand_3;
};

/* Poke tbl utility functions, required by both HPS and CPS */
static inline
void patch_poke_table_copy(void *des_add, long const *table,
			   unsigned long ddr_addr,
			   unsigned long size)
{
	int counter = 0;
	long opcode;
	long *dest = (long *)des_add;

	while (counter < size) {
		*dest++ = opcode = table[counter++];

		if (opcode != OP_DELAY)
			*dest++ = table[counter++] + ddr_addr;
		else
			*dest++ = table[counter++];

		*dest++ = table[counter++];
		*dest++ = table[counter++];
	}
}

int sti_pokeloop(const unsigned int *poketbl);
extern unsigned long sti_pokeloop_sz;

#endif /* __ASSEMBLER__ */

#endif /* __STI_POKE_TABLE_H__ */

