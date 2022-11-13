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
	return (op << 58) | (reg << 54) | (arg & 0x3fffffffffffffull);
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
	OP_PUT,
	OP_LD,
	OP_LDI,
	OP_ST,
	OP_ADD,
	OP_ADDI,
	OP_SUB,
	OP_SUBI,
	OP_MUL,
	OP_MULI,
	OP_DIV,
	OP_DIVI,
	OP_JP,
	OP_JPZ,
	OP_JZ,
	OP_JN,
	OP_JNZ,
	OP_J,
	OP_NOP,
	OP_YIELD,
	OP_DATA,
} opcodes;

typedef enum {
	ARG_NONE=0,
	ARG_IMMEDIATE,
	ARG_MEM,
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
	[OP_PUT]   = {OP_PUT,   "put",   1, ARG_NONE},
	[OP_LD]    = {OP_LD,    "ld",    1, ARG_MEM},
	[OP_LDI]   = {OP_LDI,   "ldi",   1, ARG_IMMEDIATE},
	[OP_ST]    = {OP_ST,    "st",    1, ARG_MEM},
	[OP_ADD]   = {OP_ADD,   "add",   1, ARG_MEM},
	[OP_ADDI]  = {OP_ADDI,  "addi",  1, ARG_IMMEDIATE},
	[OP_SUB]   = {OP_SUB,   "sub",   1, ARG_MEM},
	[OP_SUBI]  = {OP_SUBI,  "subi",  1, ARG_IMMEDIATE},
	[OP_MUL]   = {OP_MUL,   "mul",   1, ARG_MEM},
	[OP_MULI]  = {OP_MULI,  "muli",  1, ARG_IMMEDIATE},
	[OP_DIV]   = {OP_DIV,   "div",   1, ARG_MEM},
	[OP_DIVI]  = {OP_DIVI,  "divi",  1, ARG_IMMEDIATE},
	[OP_JP]    = {OP_JP,    "jp",    1, ARG_MEM},
	[OP_JPZ]   = {OP_JPZ,   "jpz",   1, ARG_MEM},
	[OP_JZ]    = {OP_JZ,    "jz",    1, ARG_MEM},
	[OP_JN]    = {OP_JN,    "jn",    1, ARG_MEM},
	[OP_JNZ]   = {OP_JNZ,   "jnz",   1, ARG_MEM},
	[OP_J]     = {OP_J,     "j",     1, ARG_MEM},
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
		int64_t arg  = mem[s.pc] & 0x3fffffffffffffull;
		s.pc++;

		assert(reg < LIGHTIR_NUM_REGS);

		switch (op) {
		case OP_STOP:
			s.pc = -1;
			break;

#ifndef LIGHTIR_DISABLE_STDIO
		case OP_GET:
			printf("enter a number: ");
			if (fgets(buf,sizeof(buf),stdin)) 
				s.r[reg] = atoi(buf);
			else 
				s.pc = -1; 
			break;
		case OP_PUT:
			printf("%"PRIi64"\n", s.r[reg]);
			break;
#endif

		case OP_LD:
			s.r[reg] = mem[arg];
			break;
		case OP_LDI:
			s.r[reg] = arg;
			break;
		case OP_ST:
			mem[arg] = s.r[reg];
			break;
		case OP_ADD:
			s.r[reg] += mem[arg];
			break;
		case OP_ADDI:
			s.r[reg] += arg;
			break;
		case OP_SUB:
			s.r[reg] -= mem[arg];
			break;
		case OP_SUBI:
			s.r[reg] -= arg;
			break;
		case OP_MUL:
			s.r[reg] *= mem[arg];
			break;
		case OP_MULI:
			s.r[reg] *= arg;
			break;
		case OP_DIV:
			s.r[reg] /= mem[arg];
			break;
		case OP_DIVI:
			s.r[reg] /= arg;
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
