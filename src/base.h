#ifndef INCLUDE_BASE_H_
#define INCLUDE_BASE_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define KB(x) ((x) * 1024LL)
#define MB(x) (KB(x) * 1024LL)
#define GB(x) (MB(x) * 1024LL)
#define TB(x) (GB(x) * 1024LL)
typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef size_t Usize;
typedef ptrdiff_t Size;
typedef uintptr_t Uptr;
typedef float F32;
typedef double F64;
typedef bool Bool;
typedef char Byte;

typedef struct {
	Byte *v;
	Size allofit;
	Size offset;
} Arena;

typedef int ArenaFlags;
enum {
	ArenaDontZero = 1 << 0,
	ArenaDontAbort = 1 << 1,
};
Arena arena_create(U64 amount);
void arena_destroy(Arena *a);
void *arena_alloc(Arena *, Size, Size, Size, ArenaFlags);
#define make(t, n, a) (t *)arena_alloc(a, sizeof(t), _Alignof(t), n, 0)
#define new(t, a) make(t, 1, a)
void arena_clear(Arena *a);

#define sllstack_push(f, n)  ((n)->next = (f), (f) = (n))

#define sllqueue_push_front(f, l, n) ((f) == 0 \
	? ((f) = (l) = (n), (n)->next = 0) \
	: ((n)->next = (f), (f) = (n)))
#define sllqueue_push(f, l, n) ((f) == 0 \
	? ((f) = (l) = (n), (n)->next = 0) \
	: ((l)->next = (n), (l) = (n), (n)->next = 0))
#define sllqueue_pop(f, l, n) ((f) ==  (l) \
	? ((f) = (l) = 0) \
	: (f) = (f)->next)

#define ll_foreach(t, x, xs) for (t *x = xs.first; x != 0; x = x->next)

typedef struct {
	char *v;
	Size len;
} Str;

typedef struct {
	Str head;
	Str tail;
	bool ok;
} Cut;

#define S(s) (Str){.v = s, .len = sizeof(s) / sizeof(*s) - 1}
#define STR(s) (Str){.v = s, .len = strlen(s)}
#define FMT(s) (int)(s).len, (s).v
#define str_new(size, arena) (Str){.v = make(char, size, arena), .len = size}
Str str_copy(Str, Arena *);
Str str_copy_cstr(char *, Arena *);
Str str_concat(Str a, Str b, Arena *perm);
Str str_append(Str s, Byte c, Arena *perm);
Str span(char *, char *);
bool str_eq(Str, Str);
Str trimleft(Str);
Str trimright(Str);
Str trim(Str s);
Str str_skip(Str, Size);
Str prefix(Str s, Size n);
Cut cut(Str, char);
Cut cut_whitespace(Str s);

bool whitespace(char c);
Str slurp_file(FILE *, Arena *);

#endif
