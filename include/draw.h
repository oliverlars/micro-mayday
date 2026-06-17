#pragma once
#include "memory.h"
#include "render.h"
#include "stb_truetype.h"

typedef struct d_font
{
	stbtt_fontinfo Info;
	int Ascent;
	int Descent;
	int LineGap;
} d_font;

typedef struct d_atlas_glyph
{
	u64 Hash;
	v4i Box;
	u32 CodePoint;
	f32 AdvanceX;

	v2f *Curves;
	int CurveStart;
	int CurveCount;

	struct d_atlas_glyph *NextInHashSlot;
} d_atlas_glyph;

#define D_GLYPH_TABLE_SIZE (8192)

typedef struct draw_context
{
	m_arena *Arena;
	render_context *Render;

	v2f ViewportSize;

	d_atlas_glyph GlyphTable[D_GLYPH_TABLE_SIZE];

	int CurveCount;
} draw_context;

d_atlas_glyph *D_GlyphFromCodePoint(draw_context *D_Context,
                                    d_font Font,
                                    u32 CodePoint);

d_font D_LoadFontFromFile(draw_context *D_Context, str8 FileName);
d_font D_LoadFontFromTTF(draw_context *D_Context, u8 *Data);

typedef struct d_str8_options
{
	d_font Font;
	v3f Position;
	f32 HeightInPixels;
	v4f Colour;

	// Glyphs whose byte index falls in [HighlightStart, HighlightEnd) are drawn
	// in HighlightColour instead of Colour. Disabled when Start == End.
	v4f HighlightColour;
	s32 HighlightStart;
	s32 HighlightEnd;
}
d_str8_options;

#define D_Str8(Draw, String, ...) \
	D_BaseStr8(Draw, String, (d_str8_options) { __VA_ARGS__ } )

f32 D_Str8WidthFromHeight(d_font Font, str8 String, f32 HeightInPixels);

f32 D_Str8HeightFromHeight(d_font Font, f32 HeightInPixels);

void D_BaseStr8(draw_context *D_Context, str8 String, d_str8_options Options);

typedef enum d_align
{
	D_ALIGN_CENTRE,
	D_ALIGN_MIN,
	D_ALIGN_MAX,
} d_align;

v2f D_Str8AlignInBox(d_font Font, str8 String, f32 HeightInPixels,
                     v2f BoxPosition, v2f BoxSize, d_align AlignX, d_align AlignY);

void D_BaseStr8InBox(draw_context *D_Context, str8 String,
                     v2f BoxPosition, v2f BoxSize, d_align AlignX, d_align AlignY,
                     d_str8_options Options);

#define D_Str8InBox(Draw, String, BoxPos, BoxSize, AlignX, AlignY, ...) \
	D_BaseStr8InBox(Draw, String, BoxPos, BoxSize, AlignX, AlignY, (d_str8_options){ __VA_ARGS__ })

v4f LinearFromSRGB(v4f Colour);

typedef struct d_shape_options
{
	v4f Colour;

	v4f OutlineColour;
	f32 OutlineThickness;

	f32 Extrusion;

	v4f CornerRadii;

	v4f ShadowColour;
	v2f ShadowOffset;
	f32 ShadowBlur;
}
d_shape_options;

void D_BaseRoundedQuad(draw_context *D, v2f Position, f32 Width, f32 Height, f32 CornerRadius, d_shape_options Options);
void D_BaseCapsule    (draw_context *D, v2f Position, f32 Width, f32 Height,                   d_shape_options Options);
void D_BaseSquircle   (draw_context *D, v2f Position, f32 Width, f32 Height, f32 N,            d_shape_options Options);

#define D_RoundedQuad(D, Pos, W, H, Radius, ...) \
	D_BaseRoundedQuad(D, Pos, W, H, Radius, (d_shape_options){ __VA_ARGS__ })

// A capsule is just a rounded rectangle whose corner radius is half the short
// side, so this is a convenience wrapper around D_RoundedQuad.
#define D_Capsule(D, Pos, W, H, ...) \
	D_BaseCapsule(D, Pos, W, H, (d_shape_options){ __VA_ARGS__ })

// Apple-style squircle (superellipse). Exponent N: 2 = ellipse, 4 = classic
// squircle, ~5 matches the iOS app-icon shape, higher → more rectangular.
#define D_Squircle(D, Pos, W, H, N, ...) \
	D_BaseSquircle(D, Pos, W, H, N, (d_shape_options){ __VA_ARGS__ })

// Hard-edged rectangle. Routed through the rounded-box SDF with corner radius 0,
// so it supports the same outline / extrusion options as the rounded shapes.
void D_BaseQuad(draw_context *D, v2f Position, f32 Width, f32 Height, d_shape_options Options);
#define D_Quad(D, Pos, W, H, ...) \
	D_BaseQuad(D, Pos, W, H, (d_shape_options){ __VA_ARGS__ })

// Filled circle centred on Position (Size = diameter). A rounded box whose
// corner radius equals its half-size is a circle, so this also inherits the
// outline / extrusion options.
void D_BasePoint(draw_context *D, v2f Position, f32 Size, d_shape_options Options);
#define D_Point(D, Pos, Size, ...) \
	D_BasePoint(D, Pos, Size, (d_shape_options){ __VA_ARGS__ })

void D_Line(draw_context *D, v2f Start, v2f End, f32 Thickness, v4f Colour);

void D_Bezier(draw_context *D, v2f Start, v2f Control, v2f End, f32 Thickness, v4f Colour);

f32 D_LineHeightFromFont(d_font Font, f32 HeightInPixels);
