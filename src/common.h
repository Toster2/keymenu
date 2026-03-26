#ifndef INCLUDE_COMMON_H_
#define INCLUDE_COMMON_H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_SYSTEM_H
#include FT_MODULE_H
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb.h>

#include "base.h"

typedef struct {
	U8 r, g, b, a;
} Color;

typedef bool CtrlCaret;
enum { CTRL_CARET_PREFIX = false,
	   CTRL_CARET_SUFFIX = true };
typedef struct {
	U32 fontsize;
	U32 line_margin;
	U32 margin_up;
	U32 margin_down;
	U32 margin_left;
	U32 margin_right;
	Color bg_color;
	Color txt_color;
	Str separator;
	Str font;
	CtrlCaret ctrl_caret;
} GlobalConfig;

GlobalConfig cfg;

typedef struct {
	Str *v;
	Size len;
	Size cap;
} ArrayStr;

Color color_from_u32(U32 col);
void arrstr_append(ArrayStr *xs, Str s, Arena *a);

#endif
