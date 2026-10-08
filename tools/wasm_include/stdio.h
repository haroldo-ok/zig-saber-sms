#ifndef WASM_STDIO_H
#define WASM_STDIO_H
#include <stddef.h>
typedef void FILE;
extern FILE *stderr;
int printf(const char *,...);
int fprintf(FILE *,const char *,...);
#endif
