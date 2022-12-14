#pragma once
#include <stdint.h>
#include <stddef.h>

/*
	Runtime component of the lightir virtual machine.
	https://github.com/hsnyder/lightir-vm

	Author:   Harris Snyder
	License:  Public domain
*/

static inline int64_t 
vm_mkinstr(int64_t op, int64_t reg, int64_t arg) 
{
	int64_t sign = 0;
	if (arg < 0) {
		arg = ~arg;
		sign = 1ULL << 53;
	}
	return (op << 58) | (reg << 54) | (arg & 0x1fffffffffffffULL) | sign;
}

#ifndef LIGHTIR_NUM_REGS
#define LIGHTIR_NUM_REGS 8
#endif

_Static_assert(LIGHTIR_NUM_REGS <= 16, "Number of registers cannot exceed 16");

typedef struct {
	int64_t pc;
	int64_t r[LIGHTIR_NUM_REGS]; 
} vm_regs;

vm_regs run (int64_t mem[],  size_t sz, vm_regs s);

typedef enum {
	OP_STOP = 0,
	OP_GET,
	OP_GETP,
	OP_PUT,
	OP_DBGR,
	OP_DBGM,
	OP_LD,
	OP_SET,
	OP_CPY,
	OP_ST,
	OP_ADDM,
	OP_ADDI,
	OP_ADDR,
	OP_SUBM,
	OP_SUBI,
	OP_SUBR,
	OP_MULM,
	OP_MULI,
	OP_MULR,
	OP_DIVM,
	OP_DIVI,
	OP_DIVR,
	OP_JP,
	OP_JPZ,
	OP_JZ,
	OP_JN,
	OP_JNZ,
	OP_J,
	OP_NOP,
	OP_YIELD,

	OP_DATA, // if you add instructions, add them BEFORE this
} opcodes;

_Static_assert(OP_DATA <= (1<<6), "You added too many instructions to the VM!");

typedef enum {
	ARG_NONE=0,
	ARG_IMMEDIATE,
	ARG_MEM,
	ARG_REG,
} argtype;

typedef struct {
	int opcode;
	const char * str;
	int reg;
	argtype arg;
} op_t;

const op_t vm_ops[] = {
	[OP_STOP]  = {OP_STOP,  "stop",  0, ARG_NONE},
	[OP_GET]   = {OP_GET,   "get",   1, ARG_NONE},
	[OP_GETP]  = {OP_GETP,  "getp",  1, ARG_NONE},
	[OP_PUT]   = {OP_PUT,   "put",   1, ARG_NONE},
	[OP_DBGR]  = {OP_DBGR,  "dbgr",  0, ARG_NONE},
	[OP_DBGM]  = {OP_DBGM,  "dbgm",  1, ARG_MEM},
	[OP_LD]    = {OP_LD,    "ld",    1, ARG_MEM},
	[OP_SET]   = {OP_SET,   "set",   1, ARG_IMMEDIATE},
	[OP_CPY]   = {OP_CPY,   "cpy",   1, ARG_REG},
	[OP_ST]    = {OP_ST,    "st",    1, ARG_MEM},
	[OP_ADDM]  = {OP_ADDM,  "addm",  1, ARG_MEM},
	[OP_ADDI]  = {OP_ADDI,  "addi",  1, ARG_IMMEDIATE},
	[OP_ADDR]  = {OP_ADDR,  "add",   1, ARG_REG},
	[OP_SUBM]  = {OP_SUBM,  "subm",  1, ARG_MEM},
	[OP_SUBI]  = {OP_SUBI,  "subi",  1, ARG_IMMEDIATE},
	[OP_SUBR]  = {OP_SUBR,  "sub",   1, ARG_REG},
	[OP_MULM]  = {OP_MULM,  "mulm",  1, ARG_MEM},
	[OP_MULI]  = {OP_MULI,  "muli",  1, ARG_IMMEDIATE},
	[OP_MULR]  = {OP_MULR,  "mul",   1, ARG_REG},
	[OP_DIVM]  = {OP_DIVM,  "divm",  1, ARG_MEM},
	[OP_DIVI]  = {OP_DIVI,  "divi",  1, ARG_IMMEDIATE},
	[OP_DIVR]  = {OP_DIVR,  "div",   1, ARG_REG},
	[OP_JP]    = {OP_JP,    "jp",    1, ARG_MEM},
	[OP_JPZ]   = {OP_JPZ,   "jpz",   1, ARG_MEM},
	[OP_JZ]    = {OP_JZ,    "jz",    1, ARG_MEM},
	[OP_JN]    = {OP_JN,    "jn",    1, ARG_MEM},
	[OP_JNZ]   = {OP_JNZ,   "jnz",   1, ARG_MEM},
	[OP_J]     = {OP_J,     "j",     0, ARG_MEM},
	[OP_NOP]   = {OP_NOP,   "nop",   0, ARG_NONE},
	[OP_YIELD] = {OP_YIELD, "yield", 0, ARG_NONE},
};


#ifdef LIGHTIR_IMPLEMENTATION

#include <assert.h>
#include <stdlib.h>

#ifndef LIGHTIR_DISABLE_STDIO
#include <stdio.h>
#include <inttypes.h>
#endif

vm_regs run (int64_t mem[], size_t sz, vm_regs s) 
{
#ifndef LIGHTIR_DISABLE_STDIO
	char buf[1024] = {};
#endif
	while (s.pc >= 0) {
		assert(s.pc <= sz);

		int64_t op   = mem[s.pc] >> 58;
		int64_t reg  = ((mem[s.pc] >> 54) & 0x0f)-1;
		int64_t arg  = mem[s.pc] & 0x1fffffffffffffULL;
		if (mem[s.pc] & 1ULL<<53) arg = ~arg;
		s.pc++;

		assert(reg < LIGHTIR_NUM_REGS);

		switch (op) {
		case OP_STOP:
			s.pc = -1;
			break;

#ifndef LIGHTIR_DISABLE_STDIO
		case OP_GETP:
			printf("enter a number: ");
		case OP_GET:
			if (fgets(buf,sizeof(buf),stdin)) 
				s.r[reg] = atoi(buf);
			else 
				s.pc = -1; 
			break;
		case OP_PUT:
			printf("%"PRIi64"\n", s.r[reg]);
			break;
		case OP_DBGR:
			printf("--- DBGR -----------------------------\n");
			printf("\tpc\t%"PRIi64"\n", s.pc-1);
			for (int i = 0; i < LIGHTIR_NUM_REGS; i++) {
				printf("\tr%i\t%"PRIi64"\n", i+1, s.r[i]);
			}
			printf("--------------------------------------\n");
			break;
		case OP_DBGM:
			printf("--- DBGM -----------------------------\n");
			for (int i = arg; i < arg+s.r[reg]; i++) {
				printf("\t%i:\t%"PRIi64"\n", i, mem[i]);
			}
			printf("--------------------------------------\n");
			break;
#endif
		case OP_LD:
			s.r[reg] = mem[arg];
			break;
		case OP_SET:
			s.r[reg] = arg;
			break;
		case OP_CPY:
			s.r[reg] = s.r[arg-1];
			break;
		case OP_ST:
			mem[arg] = s.r[reg];
			break;
		case OP_ADDM:
			s.r[reg] += mem[arg];
			break;
		case OP_ADDI:
			s.r[reg] += arg;
			break;
		case OP_ADDR:
			s.r[reg] += s.r[arg-1];
			break;
		case OP_SUBM:
			s.r[reg] -= mem[arg];
			break;
		case OP_SUBI:
			s.r[reg] -= arg;
			break;
		case OP_SUBR:
			s.r[reg] -= s.r[arg-1];
			break;
		case OP_MULM:
			s.r[reg] *= mem[arg];
			break;
		case OP_MULI:
			s.r[reg] *= arg;
			break;
		case OP_MULR:
			s.r[reg] *= s.r[arg-1];
			break;
		case OP_DIVM:
			s.r[reg] /= mem[arg];
			break;
		case OP_DIVI:
			s.r[reg] /= arg;
			break;
		case OP_DIVR:
			s.r[reg] /= s.r[arg-1];
			break;
		case OP_JP:
			if (s.r[reg] >  0) s.pc = arg;
			break;
		case OP_JPZ:
			if (s.r[reg] >= 0) s.pc = arg;
			break;
		case OP_JZ:
			if (s.r[reg] == 0) s.pc = arg;
			break;
		case OP_JN:
			if (s.r[reg] <  0) s.pc = arg;
			break;
		case OP_JNZ:
			if (s.r[reg] <= 0) s.pc = arg;
			break;
		case OP_J:
			s.pc = arg;
			break;
		case OP_NOP:
			break;
		case OP_YIELD:
			goto out;
			break;
		default:
			// illegal instruction
			abort();
		}
	}
out:
	return s;
}

#endif
