#include <errno.h>
#include <setjmp.h>
#include <unistd.h>

#include "base.h"
#include "common.h"
// when reading config file read static values into cfg

typedef U8 Keytag;
enum {
	KEYTAG_MENU = 1,
	KEYTAG_RUN
};

typedef struct Keyentry Keyentry;
typedef struct Menu Menu;
struct Menu {
	Str desc;
	Menu *next;
	Keyentry *v;
	Size len;
	Size cap;
};

struct Keyentry {
	Str desc;
	union {
		Menu menu;
		Str cmd;
	} u;
	Keytag tag;
	Byte key;
};

typedef struct {
	I32 lineno;
	Str fname;
	Str line;
	Arena *arena;
	jmp_buf *jmpbuf;
} Parser;

void init_cfg(void)
{
	cfg.fontsize = 50;
	cfg.line_margin = 2;
	cfg.margin_up = 0;
	cfg.margin_down = 0;
	cfg.margin_left = 0;
	cfg.margin_right = 0;
	cfg.bg_color = color_from_u32(0x24273aff);
	cfg.txt_color = color_from_u32(0xffffffff);
	cfg.separator = S(" - ");
	cfg.ctrl_caret = CTRL_CARET_PREFIX;
}

static ArrayStr error_message = {0};

void parse_error(Parser *p, char *help, char *fmt, ...)
{
	char buf[4096];
	va_list ap;
	va_start(ap, fmt);
	int n = snprintf(buf, 4096, "error: ");
	vsnprintf(buf + n, 4096, fmt, ap);
	arrstr_append(&error_message, str_copy_cstr(buf, p->arena), p->arena);
	snprintf(buf, 4096, "in file %.*s on line %d:", FMT(p->fname), p->lineno);
	arrstr_append(&error_message, str_copy_cstr(buf, p->arena), p->arena);
	snprintf(buf, 4096, "   |");
	arrstr_append(&error_message, str_copy_cstr(buf, p->arena), p->arena);
	snprintf(buf, 4096, "%3d|    %.*s", p->lineno, FMT(p->line));
	arrstr_append(&error_message, str_copy_cstr(buf, p->arena), p->arena);
	snprintf(buf, 4096, "   |");
	arrstr_append(&error_message, str_copy_cstr(buf, p->arena), p->arena);
	if (help) {
		snprintf(buf, 4096, "help: %s", help);
		arrstr_append(&error_message, str_copy_cstr(buf, p->arena), p->arena);
	}
	va_end(ap);
	for (int i = 0; i < error_message.len; i++) {
		printf("%.*s\n", FMT(error_message.v[i]));
	}
	longjmp(*p->jmpbuf, 1);
	exit(1);
}

U32 parse_u32(Parser *p, Str s)
{
	U32 ret = 0;
	for (Size i = 0; i < s.len; i++) {
		if ('0' <= s.v[i] && s.v[i] <= '9') {
			ret = 10 * ret + s.v[i] - '0';
		} else {
			parse_error(p, 0, "expected '0'..'9', got '%c'", s.v[i]);
		}
	}
	return ret;
}

Str parse_str(Parser *p, Str s)
{
	char c;
	// clang-format off
	switch (s.v[0]) {
	case '\'': c = '\''; break;
	case '"':  c = '"';  break;
	default:   return s;
	}
	// clang-format on
	if (s.v[s.len - 1] != c || s.len == 1) {
		parse_error(p, 0, "unterminated string");
	}

	return (Str){s.v + 1, s.len - 2};
}

U8 color_digit(Parser *p, char c)
{
	if ('0' <= c && c <= '9') {
		return c - '0';
	} else if ('a' <= c && c <= 'f') {
		return c - 'a' + 10;
	}

	parse_error(p, "colors can be in rgb or rgba, for example: \"#1f3d6f\",  \"#6d729daa\"", "incorrect hex digit '%c'", c);
	return 0;
}

Color parse_color(Parser *p, Str in)
{
	Color ret = {.a = 0xff};
	Str s = parse_str(p, in);
	if (*s.v == '#') s = str_skip(s, 1);
	switch (s.len) {
	case 8:
		ret.a = (color_digit(p, s.v[6]) << 4) + color_digit(p, s.v[7]);
	case 6:
		ret.r = (color_digit(p, s.v[0]) << 4) + color_digit(p, s.v[1]);
		ret.g = (color_digit(p, s.v[2]) << 4) + color_digit(p, s.v[3]);
		ret.b = (color_digit(p, s.v[4]) << 4) + color_digit(p, s.v[5]);
		break;
	default:
		parse_error(p, "colors can be in rgb or rgba, for example: \"#1f3d6f\",  \"#6d729daa\"", "incorrect color length");
	}
	return ret;
}

#define CFG_SET(fn, x) do { if (str_eq(option, S(#x))) { cfg.x = parse_##fn(p, value); return;} } while(0)
void assign_cfg_option(Parser *p, Str option, Str value)
{
	CFG_SET(u32, fontsize);
	CFG_SET(u32, line_margin);
	CFG_SET(u32, margin_up);
	CFG_SET(u32, margin_down);
	CFG_SET(u32, margin_left);
	CFG_SET(u32, margin_right);
	CFG_SET(color, bg_color);
	CFG_SET(color, txt_color);
	CFG_SET(str, separator);
	CFG_SET(str, font);
	if (str_eq(option, S("ctrl_caret"))) {
		if (str_eq(value, S("prefix"))) {
			cfg.ctrl_caret = CTRL_CARET_PREFIX;
		} else if (str_eq(value, S("suffix"))) {
			cfg.ctrl_caret = CTRL_CARET_SUFFIX;
		} else {
			parse_error(p, "try ctrl_caret = prefix", "expected either 'prefix' or 'suffix', got '%.*s'", FMT(value));
		}
		return;
	}

	parse_error(p, 0, "expected menu or config option, found '%.*s'", FMT(option));
}
#undef CFG_SET

// TODO: handle "stuff like \"this\"" and newlines
Cut cut_quoted_str(Str s)
{
	switch (s.v[0]) {
	case '"':
		s = str_skip(s, 1);
		return cut(s, '"');
	case '\'':
		s = str_skip(s, 1);
		return cut(s, '\'');
	default:
		return cut_whitespace(s);
	}
}

I32 get_indent(Str s)
{
	I32 indent = 0;
	for (int i = 0; i < s.len; i++) {
		if (s.v[i] == '\t') {
			// haskell does the same so...
			indent += 8;
		} else if (s.v[i] != '\n' && whitespace(s.v[i])) {
			indent += 1;
		} else {
			break;
		}
	}
	return indent;
}

Str eat_eq_sign(Parser *p, Str s, char *help)
{
	Cut c = cut_whitespace(trimleft(s));
	if (!str_eq(c.head, S("="))) {
		parse_error(p, help, "missing '='");
	}
	return c.tail;
}

void karr_append(Menu *m, Keyentry k, Arena *a)
{
	if (m->len == m->cap) {
		size_t new_cap = m->cap ? m->cap * 2 : 4;
		size_t old_size = m->cap * sizeof(Keyentry);
		size_t new_size = new_cap * sizeof(Keyentry);

		Byte *end = (Byte *)m->v + old_size;

		// Can we grow in-place?
		if ((Byte *)a->v + a->offset == end) {
			a->offset += (new_size - old_size);
		} else {
			Keyentry *p = make(Keyentry, new_cap, a);
			if (m->v) {
				memcpy(p, m->v, m->len * sizeof(Keyentry));
			}
			m->v = p;
		}

		m->cap = new_cap;
	}

	m->v[m->len++] = k;
}

Byte parse_key(Parser *p, char a, char b)
{
	if (a != '^' && b != '^') {
		parse_error(p, "valid keys are for example 'a', '(', '^n'", "expected '^<key>' or '<key>^' (Ctrl), got '%c%c'", a, b);
	} else if (a == '^') {
		return b | 0x80;
	} else if (b == '^') {
		return a | 0x80;
	}
	parse_error(p, 0, "unreachable");
	return 0;
}

Menu *parse_menu(Str s, I32 indent, Parser *p, Str *therest)
{
	Str tailing = {0};
	Cut c = {.tail = s};
	Menu *menu = new(Menu, p->arena);
	while (c.tail.len) {
		p->lineno++;
		tailing = c.tail;
		c = cut(c.tail, '\n');
		Str line = p->line = c.head;
		I32 line_indent = get_indent(line);
		Str trimmed = trimleft(line);
		if (trimmed.len == 0 || trimmed.v[0] == '#') continue;
		if (line_indent <= indent) {
			break;
		}
		Cut lcut = cut_whitespace(trimmed);
		Str key = lcut.head;
		Byte ch = 0;
		switch (key.len) {
		case 2:
			ch = parse_key(p, key.v[0], key.v[1]);
			break;
		case 1:
			ch = key.v[0];
			break;
		default:
			parse_error(p, "valid keys are for example 'a', '(', '^n'", "expected key, got '%c'", key.v[0]);
		}
		lcut = cut_quoted_str(eat_eq_sign(p, lcut.tail, "the syntax for menu items is <key> = [run|menu] ..."));
		Str desc = lcut.head;
		lcut = cut_whitespace(trimleft(lcut.tail));
		Str action = lcut.head;
		Keyentry k = {.key = ch, .desc = desc};
		if (str_eq(action, S("run"))) {
			k.tag = KEYTAG_RUN;
			k.u.cmd = trim(lcut.tail);
		} else if (str_eq(action, S("menu"))) {
			k.tag = KEYTAG_MENU;
			k.u.menu = *parse_menu(c.tail, line_indent, p, &c.tail);
			k.u.menu.desc = desc;
		} else {
			parse_error(p, 0, "expected 'menu' or 'run', got '%.*s'", FMT(action));
		}
		karr_append(menu, k, p->arena);
	}
	therest->v = tailing.v;
	therest->len = tailing.len;
	return menu;
}

void p_space(int n)
{
	for (int i = 0; i < n; i++) {
		putchar(' ');
	}
}

void p_menu(Menu menu, int n)
{
	printf("menu '%.*s'\n", FMT(menu.desc));
	n += 4;
	p_space(n);
	printf("len: '%ld'\n", menu.len);
	for (int i = 0; i < menu.len; i++) {
		p_space(n);
		if (menu.v[i].key & 0x80) putchar('^');
		printf("%c = ", menu.v[i].key & ~0x80);
		switch (menu.v[i].tag) {
		case KEYTAG_RUN:
			printf("run '%.*s' '%.*s'\n", FMT(menu.v[i].desc), FMT(menu.v[i].u.cmd));
			break;
		case KEYTAG_MENU:
			p_menu(menu.v[i].u.menu, n);
			break;
		}
	}
}

Str path_concat(Str a, Str b, Arena *perm)
{
	if (str_eq(a, S(".")))
		return b;
	if (str_eq(b, S(".")))
		return a;
	Str s = a.v[a.len - 1] == '/' ? a : str_concat(a, S("/"), perm);
	return str_concat(s, b, perm);
}

Str config_fname(Arena *perm)
{
	Str home = str_copy_cstr(getenv("HOME"), perm);
	if (!home.v) {
		fprintf(stderr, "error: HOME environment variable not set\n");
		exit(1);
	}
	Str fname = path_concat(home, S(".config/keymenu/keymenu.conf"), perm);
	fname.v = str_append(fname, '\0', perm).v;
	if (access(fname.v, F_OK)) {
		fprintf(stderr, "error: config file '%s' doesnt exist\n", fname.v);
		exit(1);
	}
	return fname;
}

Menu *parse_config(Str fname, jmp_buf *jmpbuf, Arena *arena)
{
	Menu *menu = 0;
	Parser p = {.lineno = 0, .arena = arena, .jmpbuf = jmpbuf};
	p.fname = fname.v ? fname : config_fname(arena);
	FILE *fp = fopen(p.fname.v, "r");
	if (!fp) {
		if (errno == ENOENT) {
			fprintf(stderr, "error: file '%.*s' doesnt exist\n", FMT(fname));
		} else {
			perror("fopen");
		}
		exit(1);
	}
	Str full = slurp_file(fp, p.arena);
	fclose(fp);
	Cut c = {0};
	c.tail = full;
	while (c.tail.len) {
		p.lineno++;
		c = cut(c.tail, '\n');
		Str line = p.line = c.head;
		if (trimleft(line).len == 0 || trimleft(line).v[0] == '#') {
			continue;
		}
		Cut lcut = cut_whitespace(line);
		Str option = lcut.head;
		if (whitespace(option.v[0])) {
			parse_error(&p, "remove the extraneous whitespace", "whitespace before option");
		} else if (str_eq(option, S("menu"))) {
			lcut = cut_quoted_str(trimleft(lcut.tail));
			if (trim(lcut.tail).len) parse_error(&p, "the syntax for declaring menus is menu <description>", "too many arguments to menu");
			if (lcut.head.len == 0) parse_error(&p, "the syntax for declaring menus is menu <description>", "missing menu description");
			Menu *n = parse_menu(c.tail, 0, &p, &c.tail);
			n->desc = lcut.head;
			n->next = menu;
			menu = n;
			continue;
		}
		Str value = trim(eat_eq_sign(&p, lcut.tail, "the syntax for setting options is <option> = <value>"));
		if (value.len == 0) parse_error(&p, 0, "missing value");
		assign_cfg_option(&p, option, value);
	}

	// p_menu(*menu->next, 0);

	return menu;
}
