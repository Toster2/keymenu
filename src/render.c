#include "base.h"
#include "common.h"

typedef struct {
	U8 *data;
	I32 width;
	I32 height;
} Bitmap;

typedef struct {
	I32 width, height;
} WinSize;

Color color_from_u32(U32 col)
{
	Color ret;
	ret.r = (col >> 24) & 0xff;
	ret.g = (col >> 16) & 0xff;
	ret.b = (col >> 8) & 0xff;
	ret.a = (col >> 0) & 0xff;
	return ret;
}

Bitmap bitmap_create(WinSize ws, Arena *arena)
{
	Bitmap b = {0};
	b.data = (U8 *)make(U32, ws.width * ws.height, arena);
	b.width = ws.width;
	b.height = ws.height;
	return b;
}

void clear(Bitmap b, I32 w, I32 h, Color color)
{
	if (color.r == color.g && color.r == color.b && color.r == color.b) {
		memset(b.data, color.r, (U32)b.width * (U32)h * 4 * sizeof(U8));
		return;
	}
	U8 c[4] = {color.r, color.g, color.b, color.a};
	for (I32 y = 0; y < h; y++) {
		for (I32 x = 0; x < w; x++) {
			I32 idx = (y * 4 * w) + x * 4;

			memcpy(&b.data[idx], c, sizeof(U32));
		}
	}
}

Color color_alpha_blend(Color dst, Color src, U8 a)
{
	Color out = {0xff, 0xff, 0xff, 0xff};
	src.a = a;
	if (src.a == 0) {
		out = dst;
	} else if (src.a == 0xff) {
		out = src;
	} else {
		U32 alpha = (U32)src.a + 1;
		out.a = (U8)((alpha * 256 + (U32)dst.a * (256 - alpha)) >> 8);

		if (out.a) {
			out.r = (U8)((((U32)src.r * alpha * 256 + (U32)dst.r * (U32)dst.a * (256 - alpha)) / out.a) >> 8);
			out.g = (U8)((((U32)src.g * alpha * 256 + (U32)dst.g * (U32)dst.a * (256 - alpha)) / out.a) >> 8);
			out.b = (U8)((((U32)src.b * alpha * 256 + (U32)dst.b * (U32)dst.a * (256 - alpha)) / out.a) >> 8);
		}
	}

	return out;
}

void draw_bitmap(Bitmap b, FT_Bitmap *bitmap, I32 x, I32 y, I32 w, I32 h, Color src)
{
	I32 x_max = x + bitmap->width, y_max = y + bitmap->rows;
	for (I32 i = x, p = 0; i < x_max; i++, p++) {
		for (I32 j = y, q = 0; j < y_max; j++, q++) {
			if (i < 0 || j < 0 || i >= w || j >= h)
				continue;

			U8 coverage = bitmap->buffer[q * bitmap->pitch + p];
			if (coverage == 0)
				continue;
			I32 idx = 4 * (j * w + i);
			Color dst = {b.data[idx], b.data[idx + 1], b.data[idx + 2], b.data[idx + 3]};
			Color alphad = color_alpha_blend(dst, src, coverage);
			b.data[idx] = alphad.r;
			b.data[idx + 1] = alphad.g;
			b.data[idx + 2] = alphad.b;
			b.data[idx + 3] = alphad.a;
		}
	}
}

typedef struct {
	FT_Glyph *glyphs;
	FT_Vector *pos;
	Size len;
} TextRenderData;

void compute_string_bbox(FT_BBox *abbox, TextRenderData *trd)
{
	FT_BBox bbox;
	FT_BBox glyph_bbox;
	// initialize to "empty" values
	bbox.xMin = bbox.yMin = 32000;
	bbox.xMax = bbox.yMax = -32000;
	for (I32 i = 0; i < trd->len; i++) {
		FT_Glyph_Get_CBox(trd->glyphs[i], ft_glyph_bbox_pixels, &glyph_bbox);
		glyph_bbox.xMin += trd->pos[i].x;
		glyph_bbox.xMax += trd->pos[i].x;
		glyph_bbox.yMin += trd->pos[i].y;
		glyph_bbox.yMax += trd->pos[i].y;
		bbox.xMin = min(bbox.xMin, glyph_bbox.xMin);
		bbox.yMin = min(bbox.yMin, glyph_bbox.yMin);
		bbox.xMax = max(bbox.xMax, glyph_bbox.xMax);
		bbox.yMax = max(bbox.yMax, glyph_bbox.yMax);
	}

	if (bbox.xMin > bbox.xMax) {
		bbox.xMin = 0;
		bbox.yMin = 0;
		bbox.xMax = 0;
		bbox.yMax = 0;
	}

	*abbox = bbox;
}
void get_string_render_size(Str s, hb_font_t *font, I32 *width, I32 *height, TextRenderData *trd)
{
	FT_Face face = hb_ft_font_get_ft_face(font);
	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_add_utf8(buf, s.v, s.len, 0, -1);
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
	// TODO: environment language ?
	hb_buffer_set_language(buf, hb_language_from_string("en", -1));
	hb_shape(font, buf, NULL, 0);
	U32 len = hb_buffer_get_length(buf);
	hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buf, 0);
	hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, 0);
	trd->len = 0;

	I32 pen_x = 0, pen_y = 0;
	for (U32 i = 0; i < len; i++) {
		hb_codepoint_t glyphid = infos[i].codepoint;
		trd->pos[trd->len].x = pen_x + (pos[i].x_offset >> 6);
		trd->pos[trd->len].y = pen_y + (pos[i].y_offset >> 6);
		if (FT_Load_Glyph(face, glyphid, FT_LOAD_DEFAULT)) continue;
		if (FT_Get_Glyph(face->glyph, trd->glyphs + trd->len)) continue;
		pen_x += pos[i].x_advance >> 6;
		pen_y += pos[i].y_advance >> 6;
		trd->len++;
	}

	FT_BBox string_bbox;
	compute_string_bbox(&string_bbox, trd);
	*width = string_bbox.xMax - string_bbox.xMin;
	*height = string_bbox.yMax - string_bbox.yMin;
	hb_buffer_destroy(buf);
}

void render(Bitmap bitmap, ArrayStr args, WinSize *ws, hb_font_t *font, Arena *arena)
{
	TextRenderData *render_data = make(TextRenderData, args.len, arena);
	for (I32 i = 0; i < args.len; i++) {
		I32 l = args.v[i].len;
		render_data[i].glyphs = make(FT_Glyph, l, arena);
		render_data[i].pos = make(FT_Vector, l, arena);
		render_data[i].len = 0;
	}

	I32 first_line_height = 0;
	ws->height = (args.len - 1) * cfg.fontsize;
	ws->width = 0;
	for (I32 i = 0; i < args.len; i++) {
		I32 line_width;
		I32 line_height;
		Str s = args.v[i];
		get_string_render_size(s, font, &line_width, &line_height, render_data + i);
		if (i == 0) {
			ws->height += line_height;
			first_line_height = line_height;
		}
		ws->width = max(ws->width, line_width);
	}
	ws->width += cfg.margin_left + cfg.margin_right;
	ws->height += cfg.margin_up + cfg.margin_down + cfg.line_margin * (args.len - 1);
	clear(bitmap, ws->width, ws->height, cfg.bg_color);
	I32 start_y = (ws->height - cfg.margin_up - first_line_height) * 64;
	I32 start_x = cfg.margin_left * 64;
	// TODO: dont allocate glyphs at all (?)
	for (I32 j = 0; j < args.len; j++) {
		for (I32 i = 0; i < render_data[j].len; i++) {
			FT_Glyph image = render_data[j].glyphs[i];
			FT_Vector pen;
			pen.x = start_x + render_data[j].pos[i].x * 64;
			pen.y = start_y + render_data[j].pos[i].y * 64;
			if (FT_Glyph_To_Bitmap(&image, FT_RENDER_MODE_NORMAL, &pen, 0)) {
				FT_Done_Glyph(render_data[j].glyphs[i]);
				continue;
			}
			FT_BitmapGlyph bit = (FT_BitmapGlyph)image;
			draw_bitmap(bitmap, &bit->bitmap, bit->left, ws->height - bit->top, ws->width, ws->height, cfg.txt_color);
			FT_Done_Glyph(image);
			FT_Done_Glyph(render_data[j].glyphs[i]);
		}
		start_y -= cfg.fontsize * 64 + cfg.line_margin * 64;
	}
}
