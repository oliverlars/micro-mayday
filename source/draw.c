#include "log.h"
#include "maths.h"
#include "memory.h"
#include "render.h"
#include "strings.h"
#include "draw.h"

#include <assert.h>
#include <float.h>
#include <math.h>

static f32 LinearFromSRGBChannel(f32 C)
{
	return C <= 0.04045f ? C / 12.92f : powf((C + 0.055f) / 1.055f, 2.4f);
}

v4f LinearFromSRGB(v4f Colour)
{
	return v4f(LinearFromSRGBChannel(Colour.X),
	           LinearFromSRGBChannel(Colour.Y),
	           LinearFromSRGBChannel(Colour.Z),
	           Colour.W);
}

static u32 D_DecodeUTF8(str8 String, s64 *At)
{
	u8 *B = (u8*)String.Bytes;
	s64 I = *At;
	s64 Remaining = String.Size - I;
	u32 C = B[I];

	if (C < 0x80)
	{
		*At = I + 1;
		return C;
	}
	else if ((C & 0xE0) == 0xC0 && Remaining >= 2 &&
	         (B[I+1] & 0xC0) == 0x80)
	{
		*At = I + 2;
		return ((C & 0x1F) << 6) | (B[I+1] & 0x3F);
	}
	else if ((C & 0xF0) == 0xE0 && Remaining >= 3 &&
	         (B[I+1] & 0xC0) == 0x80 && (B[I+2] & 0xC0) == 0x80)
	{
		*At = I + 3;
		return ((C & 0x0F) << 12) | ((B[I+1] & 0x3F) << 6) | (B[I+2] & 0x3F);
	}
	else if ((C & 0xF8) == 0xF0 && Remaining >= 4 &&
	         (B[I+1] & 0xC0) == 0x80 && (B[I+2] & 0xC0) == 0x80 && (B[I+3] & 0xC0) == 0x80)
	{
		*At = I + 4;
		return ((C & 0x07) << 18) | ((B[I+1] & 0x3F) << 12) |
		       ((B[I+2] & 0x3F) << 6) | (B[I+3] & 0x3F);
	}

	*At = I + 1;
	return C;
}

d_font D_LoadFontFromTTF(draw_context *D, u8 *Data)
{
	d_font Font = {0};
	stbtt_fontinfo Info = {0};
	int Status = stbtt_InitFont(&Info, Data, 0);
	if (!Status)
	{
		LogError("Internal font file is invalid");
	}
	else
	{
		stbtt_GetFontVMetrics(&Info, &Font.Ascent, &Font.Descent, &Font.LineGap);
		Font.Info = Info;
	}
	return Font;
}

static void CurvesFromCodePoint(draw_context *Draw, m_arena *Arena,
                                d_atlas_glyph *Glyph, u32 CodePoint, d_font Font,
                                int *CurveCount, v2f **Curves)
{
	s32 GlyphIndex = stbtt_FindGlyphIndex(&Font.Info, CodePoint);
	stbtt_vertex *Vertices;
	s32 NumberOfVertices = stbtt_GetGlyphShape(&Font.Info, GlyphIndex, &Vertices);

	v4i Box = {0};
	stbtt_GetGlyphBox(&Font.Info, GlyphIndex, &Box.X, &Box.Y, &Box.Z, &Box.W);
	f32 Width = (f32)(Box.Z - Box.X);
	f32 Height = (f32)(Box.W - Box.Y);
	v2f Size = v2f(Width, Height);

	v2f Shift = v2f(Box.X, Box.Y);

	Curves[0] = ArenaPushArrayZero(Arena, v2f, NumberOfVertices * 3);
	CurveCount[0] = 0;

	v2f First = {0};
	for (s32 I = 0; I < NumberOfVertices; I++)
	{
		stbtt_vertex *V = Vertices + I;
		switch (V->type)
		{
		case STBTT_vmove:
			First = SubV2F(v2f(V->x, V->y), Shift);
			break;
		case STBTT_vcurve:
		{
			v2f Control = SubV2F(v2f(V->cx, V->cy), Shift);
			v2f End     = SubV2F(v2f(V->x,  V->y),  Shift);
			Curves[0][CurveCount[0]++] = End;
			Curves[0][CurveCount[0]++] = Control;
			Curves[0][CurveCount[0]++] = First;
			First = End;
		} break;
		case STBTT_vline:
		{
			v2f End = SubV2F(v2f(V->x, V->y), Shift);
			v2f Control = v2f((First.X + End.X)*0.5f, (First.Y + End.Y)*0.5f);
			Curves[0][CurveCount[0]++] = End;
			Curves[0][CurveCount[0]++] = Control;
			Curves[0][CurveCount[0]++] = First;
			First = End;
		} break;
		}
	}

	for (int I = 0; I < CurveCount[0]; I++)
	{
		Curves[0][I].X /= Size.X;
		Curves[0][I].Y = (Height - Curves[0][I].Y) / Size.Y;
	}

	stbtt_FreeShape(&Font.Info, Vertices);

	R_UpdateCurves(Draw->Render, Curves[0], CurveCount[0], Draw->CurveCount);
	Draw->CurveCount += CurveCount[0];
}

d_atlas_glyph *D_GlyphFromCodePoint(draw_context *D, d_font Font, u32 CodePoint)
{
	u64 Hash = M_FNVHashBytes(M_FNV_HASH_START, (u8*)&Font, sizeof(d_font));
	Hash = M_FNVHashBytes(Hash, (u8*)&CodePoint, sizeof(u32));

	d_atlas_glyph *Glyph = D->GlyphTable + (Hash % D_GLYPH_TABLE_SIZE);
	for (; Glyph->NextInHashSlot != Null && Glyph->Hash != Hash; Glyph = Glyph->NextInHashSlot);
	d_atlas_glyph *Result = Glyph;

	if (Glyph->Hash != 0 && Glyph->Hash != Hash)
	{
		d_atlas_glyph *Next = ArenaPushArrayZero(D->Arena, d_atlas_glyph, 1);
		Glyph->NextInHashSlot = Next;
		Glyph = Next;
		Result = Next;
	}

	if (Glyph->Hash == 0)
	{
		int AdvanceX;
		int LeftSideBearing;
		stbtt_GetCodepointHMetrics(&Font.Info, CodePoint, &AdvanceX, &LeftSideBearing);
		Glyph->AdvanceX = (f32)AdvanceX;
		Glyph->Hash = Hash;
		Glyph->CodePoint = CodePoint;
		stbtt_GetCodepointBox(&Font.Info, CodePoint, &Glyph->Box.X, &Glyph->Box.Y,
		                                              &Glyph->Box.Z, &Glyph->Box.W);

		Glyph->CurveStart = D->CurveCount;
		CurvesFromCodePoint(D, D->Arena, Glyph, CodePoint, Font, &Glyph->CurveCount, &Glyph->Curves);
	}

	return Result;
}

static void PushSDFBox(draw_context *D, r_command_type Type, v2f Position,
                       f32 Width, f32 Height, f32 Param, d_shape_options Options);

void D_BaseQuad(draw_context *D, v2f Position, f32 Width, f32 Height, d_shape_options Options)
{

	PushSDFBox(D, R_COMMAND_ROUNDED_BOX, Position, Width, Height, 0.0f, Options);
}

f32 D_Str8WidthFromHeight(d_font Font, str8 String, f32 HeightInPixels)
{
	f32 Width = 0.0f;
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	for (s64 I = 0; I < String.Size; )
	{
		int CP = (int)D_DecodeUTF8(String, &I);
		s64 NextAt = I;
		int NextCP = (NextAt < String.Size) ? (int)D_DecodeUTF8(String, &NextAt) : 0;
		int GlyphIndex = stbtt_FindGlyphIndex(&Font.Info, CP);
		int Advance, LSB;
		stbtt_GetGlyphHMetrics(&Font.Info, GlyphIndex, &Advance, &LSB);
		int NextGlyphIndex = stbtt_FindGlyphIndex(&Font.Info, NextCP);
		int Kern = stbtt_GetGlyphKernAdvance(&Font.Info, GlyphIndex, NextGlyphIndex);
		Width += (Kern + Advance) * Scale;
	}
	return Width;
}

f32 D_LineHeightFromFont(d_font Font, f32 HeightInPixels)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	return (Font.Ascent - Font.Descent + Font.LineGap) * Scale;
}

f32 D_Str8HeightFromHeight(d_font Font, f32 HeightInPixels)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	return (f32)(Font.Ascent - Font.Descent) * Scale;
}

static f32 D_AlignFraction(d_align Align)
{
	switch (Align)
	{
		case D_ALIGN_CENTRE: return 0.5f;
		case D_ALIGN_MAX:    return 1.0f;
		case D_ALIGN_MIN:    return 0.0f;
		default:             return 0.0f;
	}
}

v2f D_Str8AlignInBox(d_font Font, str8 String, f32 HeightInPixels,
                     v2f BoxPosition, v2f BoxSize, d_align AlignX, d_align AlignY)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, HeightInPixels);
	f32 TextW = D_Str8WidthFromHeight(Font, String, HeightInPixels);
	f32 TextH = (f32)(Font.Ascent - Font.Descent) * Scale;

	f32 PenX = BoxPosition.X + (BoxSize.X - TextW) * D_AlignFraction(AlignX);

	f32 BoxBottomY = BoxPosition.Y + (BoxSize.Y - TextH) * D_AlignFraction(AlignY);
	f32 PenY = BoxBottomY - (f32)Font.Descent * Scale;

	return v2f(PenX, PenY);
}

void D_BaseStr8InBox(draw_context *D, str8 String,
                     v2f BoxPosition, v2f BoxSize, d_align AlignX, d_align AlignY,
                     d_str8_options Options)
{
	v2f Pen = D_Str8AlignInBox(Options.Font, String, Options.HeightInPixels,
	                           BoxPosition, BoxSize, AlignX, AlignY);
	Options.Position.X = Pen.X;
	Options.Position.Y = Pen.Y;
	D_BaseStr8(D, String, Options);
}

void D_BaseStr8(draw_context *D, str8 String, d_str8_options Options)
{
	m_arena *Arena = TemporaryArena();
	int TotalTriangleCount = (int)String.Size * 6;
	r_command Command = {0};
	Command.Vertices = (r_vertex*)ArenaPushZero(Arena, TotalTriangleCount * sizeof(r_vertex));
	Command.VerticesCount = 0;

	f32 StartX = Options.Position.X;
	r_vertex *Out = Command.Vertices;
	Options.Colour = LinearFromSRGB(Options.Colour);

	v4f HighlightColour = LinearFromSRGB(Options.HighlightColour);
	b32 HasHighlight = (Options.HighlightStart != Options.HighlightEnd);

	assert(Options.HeightInPixels > 0.0f);
	f32 Scale = stbtt_ScaleForPixelHeight(&Options.Font.Info, Options.HeightInPixels);

	for (s64 I = 0; I < String.Size; )
	{
		s64 GlyphStart = I;
		u32 CodePoint = D_DecodeUTF8(String, &I);
		s64 NextAt = I;
		u32 NextCodePoint = (NextAt < String.Size) ? D_DecodeUTF8(String, &NextAt) : 0;

		if (CodePoint == '\r') continue;
		f32 TabMultiplier = 1.0f;
		if (CodePoint == '\t') { CodePoint = ' '; TabMultiplier = 4.0f; }

		if (CodePoint == '\n')
		{
			Options.Position.Y -= (Options.Font.Ascent - Options.Font.Descent + Options.Font.LineGap) * Scale;
			Options.Position.X = StartX;
			continue;
		}

		d_atlas_glyph *Glyph = D_GlyphFromCodePoint(D, Options.Font, CodePoint);
		int Kern = stbtt_GetCodepointKernAdvance(&Options.Font.Info, CodePoint, NextCodePoint);
		f32 AdvanceX = (Kern + Glyph->AdvanceX) * Scale * TabMultiplier;

		f32 GlyphWidth  = ((f32)(Glyph->Box.Z - Glyph->Box.X) * Scale);
		f32 GlyphHeight = ((f32)(Glyph->Box.W - Glyph->Box.Y) * Scale);

		v2f V[6] =
		{
			v2f(Options.Position.X,              Options.Position.Y),
			v2f(Options.Position.X + GlyphWidth, Options.Position.Y),
			v2f(Options.Position.X + GlyphWidth, Options.Position.Y + GlyphHeight),

			v2f(Options.Position.X,              Options.Position.Y),
			v2f(Options.Position.X,              Options.Position.Y + GlyphHeight),
			v2f(Options.Position.X + GlyphWidth, Options.Position.Y + GlyphHeight),
		};
		v2f UV[6] =
		{
			v2f(0,1), v2f(1,1), v2f(1,0),
			v2f(0,1), v2f(0,0), v2f(1,0)
		};

		v4f GlyphColour = Options.Colour;
		if (HasHighlight && GlyphStart >= Options.HighlightStart && GlyphStart < Options.HighlightEnd)
		{
			GlyphColour = HighlightColour;
		}

		if (CodePoint != ' ')
		{
			for (int J = 0; J < 6; J++)
			{
				r_vertex Vx = {
					.Position   = v4f(V[J].X + Glyph->Box.X * Scale, V[J].Y + Glyph->Box.Y * Scale, Options.Position.Z, 1),
					.UV         = UV[J],
					.Colour     = GlyphColour,
					.CurveStart = Glyph->CurveStart,
					.CurveCount = Glyph->CurveCount,
				};
				*Out++ = Vx;
				Command.VerticesCount++;
			}
		}

		Options.Position.X += AdvanceX;
	}

	if (Command.VerticesCount > 0)
	{
		Command.Type = R_COMMAND_GLYPH;
		Command.Attributes = R_AttributesFromCommandType(Command.Type);
		R_PushCommand(D->Render, Command);
	}
}

static void PushQuad6(r_vertex *Out, v2f Min, v2f Max, r_vertex Template)
{
	v2f V[6] = {
		v2f(Min.X, Min.Y), v2f(Max.X, Min.Y), v2f(Max.X, Max.Y),
		v2f(Min.X, Min.Y), v2f(Min.X, Max.Y), v2f(Max.X, Max.Y),
	};
	for (int I = 0; I < 6; I++)
	{
		Out[I] = Template;
		Out[I].Position = v4f(V[I].X, V[I].Y, 0, 1);
	}
}

void D_Line(draw_context *D, v2f Start, v2f End, f32 Thickness, v4f Colour)
{
	m_arena *Arena = TemporaryArena();
	r_command Command = {0};
	Command.Vertices = ArenaPushArrayZero(Arena, r_vertex, 6);
	Command.VerticesCount = 6;

	f32 Pad = Thickness * 0.5f + 1.0f;
	v2f Min = v2f(fminf(Start.X, End.X) - Pad, fminf(Start.Y, End.Y) - Pad);
	v2f Max = v2f(fmaxf(Start.X, End.X) + Pad, fmaxf(Start.Y, End.Y) + Pad);

	r_vertex Tmpl = {
		.Colour = LinearFromSRGB(Colour),
		.Start = Start,
		.End = End,
		.Thickness = Thickness,
	};
	PushQuad6(Command.Vertices, Min, Max, Tmpl);

	Command.Type = R_COMMAND_LINES;
	Command.Attributes = R_AttributesFromCommandType(Command.Type);
	R_PushCommand(D->Render, Command);
}

void D_Bezier(draw_context *D, v2f Start, v2f Control, v2f End, f32 Thickness, v4f Colour)
{
	m_arena *Arena = TemporaryArena();
	r_command Command = {0};
	Command.Vertices = ArenaPushArrayZero(Arena, r_vertex, 6);
	Command.VerticesCount = 6;

	f32 Pad = Thickness * 0.5f + 1.0f;
	f32 MinX = fminf(Start.X, fminf(Control.X, End.X)) - Pad;
	f32 MinY = fminf(Start.Y, fminf(Control.Y, End.Y)) - Pad;
	f32 MaxX = fmaxf(Start.X, fmaxf(Control.X, End.X)) + Pad;
	f32 MaxY = fmaxf(Start.Y, fmaxf(Control.Y, End.Y)) + Pad;

	r_vertex Tmpl = {
		.Colour = LinearFromSRGB(Colour),
		.Start = Start,
		.Control = Control,
		.End = End,
		.Thickness = Thickness,
	};
	PushQuad6(Command.Vertices, v2f(MinX, MinY), v2f(MaxX, MaxY), Tmpl);

	Command.Type = R_COMMAND_BEZIER;
	Command.Attributes = R_AttributesFromCommandType(Command.Type);
	R_PushCommand(D->Render, Command);
}

static void EmitSDFQuad(draw_context *D, r_command_type Type, v2f Min, v2f Max, r_vertex Tmpl)
{
	m_arena *Arena = TemporaryArena();
	r_command Command = {0};
	Command.Vertices = ArenaPushArrayZero(Arena, r_vertex, 6);
	Command.VerticesCount = 6;
	PushQuad6(Command.Vertices, Min, Max, Tmpl);
	Command.Type = Type;
	Command.Attributes = R_AttributesFromCommandType(Type);
	R_PushCommand(D->Render, Command);
}

static void PushSDFBox(draw_context *D, r_command_type Type, v2f Position,
                       f32 Width, f32 Height, f32 Param, d_shape_options Options)
{
	v2f HalfSize = v2f(Width * 0.5f, Height * 0.5f);
	v2f Centre   = v2f(Position.X + HalfSize.X, Position.Y + HalfSize.Y);

	v4f CornerRadii = Options.CornerRadii;
	if (CornerRadii.X == 0.0f && CornerRadii.Y == 0.0f &&
	    CornerRadii.Z == 0.0f && CornerRadii.W == 0.0f)
	{
		CornerRadii = v4f(Param, Param, Param, Param);
	}

	if (Options.ShadowBlur > 0.0f)
	{
		v4f ShadowColour = Options.ShadowColour;
		if (ShadowColour.X == 0.0f && ShadowColour.Y == 0.0f &&
		    ShadowColour.Z == 0.0f && ShadowColour.W == 0.0f)
		{
			ShadowColour = v4f(0.0f, 0.0f, 0.0f, 0.35f);
		}

		v2f SPos = AddV2F(Position, Options.ShadowOffset);
		f32 SPad = 1.0f + Options.ShadowBlur;
		v2f SMin = v2f(SPos.X - SPad,         SPos.Y - SPad);
		v2f SMax = v2f(SPos.X + Width + SPad, SPos.Y + Height + SPad);

		r_vertex ShadowTmpl = {
			.Colour      = LinearFromSRGB(ShadowColour),
			.Centre      = v2f(SPos.X + HalfSize.X, SPos.Y + HalfSize.Y),
			.HalfSize    = HalfSize,
			.Thickness   = Param,
			.CornerRadii = CornerRadii,
			.ShadowBlur  = Options.ShadowBlur,
		};
		EmitSDFQuad(D, Type, SMin, SMax, ShadowTmpl);
	}

	f32 Pad = 1.0f + fmaxf(0.0f, Options.OutlineThickness);
	v2f Min = v2f(Position.X - Pad,         Position.Y - Pad);
	v2f Max = v2f(Position.X + Width + Pad, Position.Y + Height + Pad);

	r_vertex Tmpl = {
		.Colour           = LinearFromSRGB(Options.Colour),
		.Centre           = Centre,
		.HalfSize         = HalfSize,
		.Thickness        = Param,
		.OutlineColour    = LinearFromSRGB(Options.OutlineColour),
		.OutlineThickness = Options.OutlineThickness,
		.Extrusion        = Options.Extrusion,
		.CornerRadii      = CornerRadii,
		.ShadowBlur       = 0.0f,
	};
	EmitSDFQuad(D, Type, Min, Max, Tmpl);
}

void D_BaseRoundedQuad(draw_context *D, v2f Position, f32 Width, f32 Height, f32 CornerRadius, d_shape_options Options)
{
	PushSDFBox(D, R_COMMAND_ROUNDED_BOX, Position, Width, Height, CornerRadius, Options);
}

void D_BaseCapsule(draw_context *D, v2f Position, f32 Width, f32 Height, d_shape_options Options)
{
	f32 Radius = 0.5f * fminf(Width, Height);
	PushSDFBox(D, R_COMMAND_ROUNDED_BOX, Position, Width, Height, Radius, Options);
}

void D_BaseSquircle(draw_context *D, v2f Position, f32 Width, f32 Height, f32 N, d_shape_options Options)
{
	PushSDFBox(D, R_COMMAND_SQUIRCLE, Position, Width, Height, N, Options);
}

void D_BasePoint(draw_context *D, v2f Position, f32 Size, d_shape_options Options)
{
	f32 Half = Size * 0.5f;
	v2f Corner = v2f(Position.X - Half, Position.Y - Half);
	PushSDFBox(D, R_COMMAND_ROUNDED_BOX, Corner, Size, Size, Half, Options);
}
