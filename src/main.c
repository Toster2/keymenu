#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"
Arena g_arena;
#define RGFW_ALLOC(size) arena_alloc(&g_arena, size, alignof(max_align_t), 1, ArenaDontAbort)
#define RGFW_FREE(size)
#define RGFW_IMPLEMENTATION
#include "RGFW.h"

#include "base.c"
#include "parse.c"
#include "render.c"

static Menu *menu = 0;
static hb_font_t *font;
static Bitmap bitmap;

void *ft_alloc(FT_Memory mem, long size)
{
	Arena *a = (Arena *)mem->user;
	return arena_alloc(a, size, alignof(max_align_t), 1, ArenaDontAbort | ArenaDontZero);
}

void ft_free(FT_Memory mem, void *block)
{
	(void)mem;
	(void)block;
}

void *ft_realloc(FT_Memory mem, long cur_size, long new_size, void *block)
{
	Arena *a = (Arena *)mem->user;
	if ((Byte *)block + cur_size == a->v + a->offset) {
		a->offset += new_size - cur_size;
		return block;
	}
	void *p = arena_alloc(a, new_size, alignof(max_align_t), 1, ArenaDontAbort | ArenaDontZero);
	return memcpy(p, block, cur_size);
}

void arrstr_append(ArrayStr *xs, Str s, Arena *a)
{
	if (xs->len == xs->cap) {
		size_t new_cap = xs->cap ? xs->cap * 2 : 4;
		size_t old_size = xs->cap * sizeof(Keyentry);
		size_t new_size = new_cap * sizeof(Keyentry);

		Byte *end = (Byte *)xs->v + old_size;

		if ((Byte *)a->v + a->offset == end) {
			a->offset += (new_size - old_size);
		} else {
			Str *p = make(Str, new_cap, a);
			if (xs->v) {
				memcpy(p, xs->v, xs->len * sizeof(Str));
			}
			xs->v = p;
		}

		xs->cap = new_cap;
	}

	xs->v[xs->len++] = s;
}

Str str_null_terminate(Str s, Arena *perm)
{
	return str_concat(s, (Str){"\0", 1}, perm);
}

void init_font(Arena *a)
{
	FT_Library ft;
	FT_Face face;
	FT_Memory ft_allocator = new(struct FT_MemoryRec_, a);
	ft_allocator->user = a;
	ft_allocator->alloc = ft_alloc;
	ft_allocator->free = ft_free;
	ft_allocator->realloc = ft_realloc;
	if (FT_New_Library(ft_allocator, &ft)) exit(1);
	FT_Add_Default_Modules(ft);
	if (FT_New_Face(ft, str_null_terminate(cfg.font, &g_arena).v, 0, &face)) {
		printf("error: cant load font file '%.*s'\n", FMT(cfg.font));
		exit(1);
	}
	if (FT_Set_Pixel_Sizes(face, 0, cfg.fontsize)) exit(1);
	font = hb_ft_font_create_referenced(face);
	hb_ft_font_set_funcs(font);
}

Str str_from_keyentry(Keyentry k, Arena *a)
{
	Str s = {0};
	if (cfg.ctrl_caret == CTRL_CARET_SUFFIX) {
		s = str_append(s, k.key & ~CTRL_BIT, a);
	}
	if (k.key & CTRL_BIT) {
		s = str_append(s, '^', a);
	} else {
		s = str_append(s, ' ', a);
	}
	if (cfg.ctrl_caret == CTRL_CARET_PREFIX) {
		s = str_append(s, k.key & ~CTRL_BIT, a);
	}
	s = str_concat(s, cfg.separator, a);
	return str_concat(s, k.desc, a);
}

void blit(RGFW_window *win, WinSize ws)
{
	RGFW_window_center(win);
	RGFW_surface *surface = RGFW_createSurface(bitmap.data, ws.width, ws.height, RGFW_formatRGBA8);
	RGFW_window_blitSurface(win, surface);
	RGFW_surface_free(surface);
}

Byte char_from_key(RGFW_key key, RGFW_keymod keymod)
{
	Byte ch = key;
	if (keymod & RGFW_modShift) {
		if ('a' <= ch && ch <= 'z') {
			ch += 'A' - 'a';
		}
	}
	if (keymod & RGFW_modControl) {
		ch |= CTRL_BIT;
	}
	return ch;
}

void set_menu_and_render(Menu *m, RGFW_window *win, Arena *arena)
{
	ArrayStr xs = {0};
	WinSize ws = {0};
	menu = m;
	for (I32 i = 0; i < menu->len; i++) {
		arrstr_append(&xs, str_from_keyentry(menu->v[i], arena), arena);
	}
	render(bitmap, xs, &ws, font, arena);
	RGFW_window_resize(win, ws.width, ws.height);
	blit(win, ws);
}

void keyfunc(RGFW_window *win, RGFW_key key, RGFW_keymod keymod, RGFW_bool repeat, RGFW_bool pressed)
{
	Arena *arena = &g_arena;
	int i;
	if (!pressed || key > 127 || menu == 0) return;
	if (key == RGFW_backSpace) {
		set_menu_and_render(menu->parent, win, arena);
		return;
	}
	Byte ch = char_from_key(key, keymod);
	for (i = 0; i < menu->len; i++) {
		if (menu->v[i].key == ch) {
			goto dothething;
		}
	}
	return;
dothething:
	switch (menu->v[i].tag) {
	case KEYTAG_RUN: {
		char *arg3 = make(char, menu->v[i].u.cmd.len + 1, arena);
		char *argv[4] = {"sh", "-c", arg3, 0};
		memcpy(arg3, menu->v[i].u.cmd.v, menu->v[i].u.cmd.len);
		arg3[menu->v[i].u.cmd.len] = '\0';
		RGFW_window_close(win);
		execvp(argv[0], argv);
		perror("execvp");
		_exit(127);
	} break;
	case KEYTAG_MENU:
		set_menu_and_render(&menu->v[i].u.menu, win, arena);
		break;
	case KEYTAG_UP:
		// TODO: maybe error when up is on top level menu ?
		// if (menu->parent == menu) ...
		set_menu_and_render(menu->parent, win, arena);
		break;
	}
}

Arena *init(void)
{
	Arena *a = &g_arena;
	g_arena = arena_create(GB(1));
	init_cfg();
	bitmap = bitmap_create((WinSize){1980, 1080}, a);
	return a;
}

void help(char *argv0)
{
	printf("nvim-which-menu for the window manager\n\n"
	       "usage: %s [OPTIONS] <which_menu>\n\n"
	       "options:\n"
	       "  -c, --config <FILE> : use a different config file, default is '$HOME/.config/keymenu/keymenu.conf'\n"
	       "  -l, --list          : list all available menus\n"
	       "  -h, --help          : print this help and exit\n"
	       "  -V, --version       : print version information and exit\n",
	       argv0);
}

void version(void)
{
	printf("keymenu-0.1\n");
}

bool gui_errors = true;

int main(int argc, char **argv)
{
	Arena *arena = init();
	ArrayStr args = {0};
	WinSize ws = {0};
	bool lflag = false;
	int option = 0;
	Str config_fname = {0};
	jmp_buf jmpbuf = {0};
	struct optparse options;
	struct optparse_long longopts[] = {
	    {"config", 'c', OPTPARSE_REQUIRED},
	    {"no-gui-errors", 'g', OPTPARSE_NONE},
	    {"list", 'l', OPTPARSE_NONE},
	    {"help", 'h', OPTPARSE_NONE},
	    {"version", 'v', OPTPARSE_NONE},
	    {0, 0}};
	optparse_init(&options, argv);
	if (setjmp(jmpbuf) == 1) {
		args = error_message;
		if (!gui_errors) return 1;
		goto drawtime;
	}

	while ((option = optparse_long(&options, longopts, NULL)) != -1) {
		switch (option) {
		case 'c':
			config_fname = str_copy_cstr(options.optarg, arena);
			config_fname.v = str_append(config_fname, '\0', arena).v;
			break;
		case 'l':
			lflag = true;
			break;
		case 'g':
			gui_errors = false;
			break;
		case 'h':
			help(*argv);
			return 0;
		case 'v':
			version();
			return 0;
		case '?':
			fprintf(stderr, "error: %s\n", options.errmsg);
			return 1;
		}
	}
	if (lflag) {
		menu = parse_config(config_fname, &jmpbuf, arena);
		for (; menu != 0; menu = menu->next) {
			printf("%.*s\n", FMT(menu->desc));
		}
		return 0;
	}

	if (argc < 2) {
		help(*argv);
		return 1;
	}
	menu = parse_config(config_fname, &jmpbuf, arena);
	Str which_menu = STR(argv[1]);
	for (; menu != 0; menu = menu->next) {
		if (str_eq(menu->desc, which_menu)) {
			break;
		}
	}
	if (menu == 0) {
		fprintf(stderr, "no such menu as '%.*s' exists\n", FMT(which_menu));
		return 1;
	}
	for (I32 i = 0; i < menu->len; i++) {
		arrstr_append(&args, str_from_keyentry(menu->v[i], arena), arena);
	}
drawtime:
	init_font(arena);
	render(bitmap, args, &ws, font, arena);
	RGFW_setClassName("keymenu");
	RGFW_window *win = RGFW_createWindow("keymenu", 0, 0, ws.width, ws.height, RGFW_windowNoResize | RGFW_windowTransparent);
	RGFW_setKeyCallback(keyfunc);
	RGFW_window_setExitKey(win, RGFW_escape);
	blit(win, ws);
	while (!RGFW_window_shouldClose(win)) {
		RGFW_pollEvents();
	}

	RGFW_window_close(win);
	hb_font_destroy(font);
	arena_destroy(arena);
	return 0;
}
