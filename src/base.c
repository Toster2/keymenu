#include "base.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <memoryapi.h>
#else
#include <sys/mman.h>
#endif

Arena arena_create(U64 amount)
{
	Arena a = {0};
	a.allofit = amount;
#ifdef _WIN32
	a.v = VirtualAlloc(NULL, amount, MEM_RESERVE, PAGE_READWRITE);
#else
	a.v = mmap(0, amount, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
	if (a.v == MAP_FAILED) {
		perror("mmap");
		exit(2);
	}
	return a;
}

void arena_destroy(Arena *a)
{
	munmap(a->v, a->allofit);
	a->allofit = a->offset = 0;
	a->v = 0;
}

void *arena_alloc(Arena *a, Size sz, Size align, Size count, ArenaFlags flags)
{
	Size padding = -(Uptr)(a->v + a->offset) & (align - 1);
	Size available = a->allofit - padding;
	if (available < 0 || count > available / sz) {
		if (flags & ArenaDontAbort) {
			return 0;
		}
		abort();
	}
	void *p = a->v + a->offset + padding;
	a->offset += padding + count * sz;
	return flags & ArenaDontZero ? p : memset(p, 0, count * sz);
}

void arena_clear(Arena *a)
{
	a->offset = 0;
}

Str str_copy(Str s, Arena *a)
{
	char *p = make(char, s.len, a);
	if (s.len)
		memcpy(p, s.v, s.len);
	return (Str){p, s.len};
}

Str str_copy_cstr(char *s, Arena *a)
{
	return str_copy((Str){s, strlen(s)}, a);
}

Str str_concat(Str a, Str b, Arena *perm)
{
	Str s1;
	if (perm->v + perm->offset == a.v + a.len) {
		s1 = a;
	} else {
		s1 = str_copy(a, perm);
	}
	str_copy(b, perm);
	return (Str){s1.v, a.len + b.len};
}

Str str_append(Str s, Byte c, Arena *perm)
{
	Str s1;
	if (perm->v + perm->offset == s.v + s.len) {
		s1 = s;
	} else {
		s1 = str_copy(s, perm);
	}
	*new(Byte, perm) = c;
	return (Str){s1.v, s1.len + 1};
}

Str span(char *beg, char *end)
{
	Str s = {0};
	s.v = beg;
	s.len = beg ? end - beg : 0;
	return s;
}

bool whitespace(char c)
{
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

Cut cut(Str s, char c)
{
	Cut r = {0};
	if (!s.len)
		return r;
	char *beg = s.v;
	char *end = s.v + s.len;
	char *cut = beg;
	for (; cut < end && *cut != c; cut++);
	r.ok = cut < end;
	r.head = span(beg, cut);
	r.tail = span(cut + r.ok, end);
	return r;
}

Cut cut_whitespace(Str s)
{
	Cut r = {0};
	if (!s.len)
		return r;
	char *beg = s.v;
	char *end = s.v + s.len;
	char *cut = beg;
	for (; cut < end && !whitespace(*cut); cut++);
	char *cut0 = cut;
	for (; cut < end && whitespace(*cut); cut++);
	r.ok = cut < end;
	r.head = span(beg, cut0);
	r.tail = span(cut, end);
	return r;
}

Str trimleft(Str s)
{
	for (; s.len && *s.v <= ' '; s.v++, s.len--);
	return s;
}

Str trimright(Str s)
{
	for (; s.len && s.v[s.len - 1] <= ' '; s.len--);
	return s;
}

Str trim(Str s)
{
	return trimleft(trimright(s));
}

Str str_skip(Str s, Size n)
{
	Size x = min(s.len, n);
	s.v += x;
	s.len -= x;
	return s;
}

Str prefix(Str s, Size n)
{
	s.len = n;
	return s;
}

bool str_eq(Str a, Str b)
{
	return a.len == b.len && (!a.len || !memcmp(a.v, b.v, a.len));
}

Str slurp_file(FILE *fp, Arena *perm)
{
	Str s;
	Size sz;
	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	rewind(fp);
	s = str_new(sz, perm);
	fread(s.v, sizeof(*s.v), sz, fp);
	return s;
}
