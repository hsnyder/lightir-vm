#define LIGHTIR_IMPLEMENTATION
#include "virtualmachine.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include <errno.h>
#include <stdarg.h>
static _Noreturn void 
#if defined(__clang__) || defined(__GNUC__)
__attribute__ ((format (printf, 1, 2)))
#endif
die(char *fmt, ...)
{
	int e = errno;
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	if (e!= 0) fprintf(stderr, " (errno %d: %s)", e, strerror(e));
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

static void 
usage (char * progname) 
{
	fprintf(stderr, "Usage: %s command file\n", progname);
	fprintf(stderr, "where,\n");
	fprintf(stderr, "\tcommand   is 'as' (to assemble a source code file), 'disas' (to disassemble a bytecode file), or 'run' (to interpret bytecode)\n");
	fprintf(stderr, "\tfile      is the filename to operate on\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "When assembling, bytecode is written to stdout, so piping to a file is advised.\n");
	fprintf(stderr, "\n");
	exit (EXIT_FAILURE);
}

static void assemble    (char    file[], size_t sz);
static void disassemble (int64_t file[], size_t sz);

int main (int argc, char ** argv)
{
#ifdef _WIN32
	// fix binary stream output on windows
	int _setmode(int, int);
	_setmode(0, 0x8000);
	_setmode(1, 0x8000);
#endif
	if (argc < 3) usage(argv[0]);

	const char * action = argv[1];
	const char * file   = argv[2];

	static int64_t inputfile[1<<22] = {0};

	if (!strcmp(action, "as")) {

		FILE * f = fopen(file, "rb");
		if (!f) die("Couldn't open %s", file);
		size_t n = fread(inputfile, 1, sizeof(inputfile)/sizeof(inputfile[0]), f);
		fclose(f);
		
		if(n == sizeof(inputfile)) die("input file too big");

		assemble((char*)inputfile, 1+strlen((char*)inputfile));

		exit (EXIT_SUCCESS);
	}

	if (!strcmp(action, "run")) {

		FILE * f = fopen(file, "rb");
		if (!f) die("Couldn't open %s", file);
		size_t n = fread(inputfile, 1, sizeof(inputfile)/sizeof(inputfile[0]), f);
		fclose(f);
		
		if(n == sizeof(inputfile)) die("input file too big");
	
		vm_regs s = {0};
		run(inputfile, sizeof(inputfile), s);

		exit (EXIT_SUCCESS);
	}

	if (!strcmp(action, "disas")) {

		FILE * f = fopen(file, "rb");
		if (!f) die("Couldn't open %s", file);
		size_t n = fread(inputfile, 1, sizeof(inputfile)/sizeof(inputfile[0]), f);
		fclose(f);
		
		if(n == sizeof(inputfile)) die("input file too big");

		disassemble(inputfile, n/sizeof(inputfile[0]));

		exit (EXIT_SUCCESS);
	}
	

	usage(argv[0]);
}

typedef enum {
	END = 0,
	ID,
	COLON,
	COMMA,
	NUMBER,
	NEWLINE,
} token_e;

#define MAXID 32

typedef struct
{
	token_e tok;
	union {
		int64_t num;
		char id[MAXID];
	};
} token_t;


static token_t
lex_eat(char **t) {

	while (isspace(**t) && **t != '\n') (*t)++;

	if (**t == '#') {
		// skip comments
		while (**t != '\n' && **t) (*t)++;
	}

	if (**t == ':') {
		(*t)++;
		return (token_t) { .tok = COLON };
	}

	if (**t == '\n') {
		(*t)++;
		return (token_t) { .tok = NEWLINE };
	}

	if (**t == ',') {
		(*t)++;
		return (token_t) { .tok = COMMA };
	}

	if (
		( (**t == '+' || **t == '-') && isdigit(*t[1]) ) 
		|| isdigit(**t)
		
	   ) {
	
		int sign = 1;
		if (**t == '-') { sign = -1; (*t)++; }
		if (**t == '+') { sign =  1; (*t)++; }		

		uint64_t val = 0;
		while (isdigit(**t)) {
			val = val * 10 + ((**t)-'0');
			(*t)++;
		}

		val *= sign;

		return (token_t) { .tok = NUMBER, .num = val };
	}

	if (isalnum(**t) || **t == '_') {

		token_t dummy = {.tok = ID};
		char * q = *t;
		for (unsigned i = 0; i < sizeof(dummy.id)-1; i++) {

			if (isalnum(**t) || **t == '_') {
				dummy.id[i] = **t;
				(*t)++;
			}
			else return dummy;
		}

		die("identifier too long: %.20s", q);
	}


	if (**t == 0) return (token_t) { .tok = END };

	die ("invalid token: %.20s", *t);

}

static token_t 
lex_peek (char **t) {
	char * u = *t;
	token_t tok = lex_eat(&u);
	return tok;
}


typedef struct {
	int64_t addr;
	char    id[MAXID];
} sym;


static struct {
	int64_t mem[1<<20];
	sym symtab[1<<20];
	int nsym;
	int nextmem;
} state = {};


static void 
expect_newline (char **t)
{
	char *u = *t;
	token_t tok = lex_eat(t);
	if (tok.tok != NEWLINE)
		die("newline expected: %.40s", u);
}

static int 
parse_label(char **t, int pass) {
	assert (pass == 1 || pass == 2);

	char *u = *t;
	token_t tok1 = lex_eat(&u);
	token_t tok2 = lex_eat(&u);

	if (tok1.tok == ID && tok2.tok == COLON) {
		*t = u;

		if (pass == 1) {
			memcpy(state.symtab[state.nsym].id, tok1.id, sizeof(tok1.id));
			state.symtab[state.nsym++].addr = state.nextmem;
		} 

		return 1;
	}
	return 0;

}

static token_t 
expect_id (char **t) 
{
	char *u = *t;
	token_t tok = lex_eat(t);
	if (tok.tok != ID)
		die("identifier expected: %.40s", u);
	return tok;
}

static token_t 
expect_comma (char **t) 
{
	char *u = *t;
	token_t tok = lex_eat(t);
	if (tok.tok != COMMA)
		die("comma expected: %.40s", u);
	return tok;
}

static token_t 
expect_number (char **t) 
{
	char *u = *t;
	token_t tok = lex_eat(t);
	if (tok.tok != NUMBER)
		die("number expected: %.40s", u);
	return tok;
}

static int64_t 
symtab_lookup(char * id) 
{
	for (unsigned u = 0; u < state.nsym; u++) {
		if (!strcmp(id, state.symtab[u].id)) {
			return state.symtab[u].addr;
		}
	}
	die("symbol '%s' not defined", id);
}

int valid_register(const char * t) {
	if (t[0] != 'r') 
		return 0;
	const char * end = t;
	for(;*end;end++);
	char * pend = 0;
	long val = strtol(t+1, &pend, 10);
	if (pend != end) 
		return 0;
	if (val <= 0) 
		return 0;
	if (val >= LIGHTIR_NUM_REGS)
		return 0;
	return val;
}

static int
parse_instruction(char **t, int pass) 
{
	assert (pass == 1 || pass == 2);

	token_t tok = lex_peek(t);
	if (tok.tok != ID) return 0;

	for (unsigned u = 0; u < sizeof(vm_ops)/sizeof(vm_ops[0]); u++) {

		if (!strcmp(tok.id, vm_ops[u].str)) {
			lex_eat(t);

			if (pass == 1) {
				state.nextmem++;

				if (vm_ops[u].reg) {
					token_t tok = expect_id(t);
					if(!valid_register(tok.id)) 
						die("invalid register: %s", tok.id);
				}

				if (vm_ops[u].reg && vm_ops[u].arg) 
					expect_comma(t);

				if (vm_ops[u].arg == ARG_REG) {
					token_t tok = expect_id(t);
					if(!valid_register(tok.id)) 
						die("invalid register: %s", tok.id);
				}
				else if (vm_ops[u].arg == ARG_MEM) 
					expect_id(t);
				else if (vm_ops[u].arg == ARG_IMMEDIATE)
				       	expect_number(t);

			} else if (pass == 2) {

				int64_t arg = 0;
				int64_t reg = 0;

				if (vm_ops[u].reg) 
					reg = valid_register(expect_id(t).id);

				if (vm_ops[u].reg && vm_ops[u].arg) 
					expect_comma(t);

				if (vm_ops[u].arg == ARG_REG) {

					arg = valid_register(expect_id(t).id);

				} else if (vm_ops[u].arg == ARG_MEM) {

					arg = symtab_lookup(expect_id(t).id);
					if(arg & 0xff00000000000000LL) 
						die("program too large"); 

				} else if (vm_ops[u].arg == ARG_IMMEDIATE) {

					arg = expect_number(t).num;
					if(arg & 0xff00000000000000LL) 
						die("immediate value negative or too large"); 
				}	

				state.mem[state.nextmem++] = vm_mkinstr(vm_ops[u].opcode, reg, arg);
			}
			return 1;
		}
	}
	return 0;
}

static int
parse_data (char **t, int pass) 
{
	assert (pass == 1 || pass == 2);
	char *u = *t;

	token_t tok = lex_peek(t);
	if (tok.tok != ID) return 0;
	if (strcmp(tok.id, "data")) return 0; 
	lex_eat(t);
	token_t arg = lex_eat(t);

	if (arg.tok == NUMBER) {
		if (pass == 1) {
			state.nextmem++;
		} else if (pass == 2) {
			state.mem[state.nextmem++] = arg.num;
		} 
		return 1;
	}

	die ("expected a number: %.40s", u);
}

static int 
parse_line(char **t, int pass)
{

	if (parse_label(t, pass)) {
		// optional instruction following label
		parse_instruction(t, pass) || 
		parse_data(t, pass);
		expect_newline(t);
		return 1;
	}

	if (parse_instruction(t, pass)) {
		expect_newline(t);
		return 1;
	}

	if (parse_data(t, pass)) {
		expect_newline(t);
		return 1;
	}

	char *u = *t;
	token_t tok = lex_eat(&u);
	if (tok.tok == NEWLINE || tok.tok == END) {
		*t = u;
		return 1;	
	}

	return 0;
}

static void 
assemble (char file[], size_t sz)
{
	// while ((tok = lex(&t)).tok != END) dbg_print_token(tok);

	// Assembler pass 1

	char * t = file;
	while (*t && (file+sz > t)) {
		char *u = t;
		if(!parse_line(&t, 1))
			die("pass 1, invalid line: %.40s", u);
	}

	// Assembler pass 2
	state.nextmem = 0;

	t = file;
	while (*t && (file+sz > t)) {
		char *u = t;
		if(!parse_line(&t, 2))
			die("pass 2, invalid line: %.40s", u);
	}

	fwrite(state.mem, 1, sizeof(state.mem[0])*state.nextmem, stdout);
}


static void
disassemble (int64_t file[], size_t sz)
{
	const size_t N_vm_ops = sizeof(vm_ops)/sizeof(vm_ops[0]);

	for (unsigned i = 0; i < sz; i++)
	{

		int64_t file_op   = file[i] >> 58;
		int64_t file_reg  = ((file[i] >> 54) & 0x0f)-1;
		int64_t file_arg  = file[i] & 0x3fffffffffffffull;

		int op;
		for (op = 0; op < N_vm_ops; op++) {
			if (file_op == vm_ops[op].opcode) 
				break; 
		}

		if (op == N_vm_ops) {
			printf("%8u:    ?? <%"PRIi64">\n", i, file[i]);
		} else {
			if (vm_ops[op].reg && vm_ops[op].arg == ARG_REG) 
				printf("%8u:    %-8s  r%"PRIi64", r%"PRIi64"\n", i, vm_ops[op].str, file_reg+1, file_arg);
			else if (vm_ops[op].reg && vm_ops[op].arg) 
				printf("%8u:    %-8s  r%"PRIi64", %"PRIi64"\n", i, vm_ops[op].str, file_reg+1, file_arg);
			else if (vm_ops[op].reg)
				printf("%8u:    %-8s  r%"PRIi64"\n", i, vm_ops[op].str, file_reg+1);
			else if (vm_ops[op].arg)
				printf("%8u:    %-8s  %"PRIi64"\n", i, vm_ops[op].str, file_arg);
			else 
				printf("%8u:    %-8s\n", i, vm_ops[op].str);
		}
	}
}
