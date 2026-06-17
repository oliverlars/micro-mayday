// micro-mayday — the absolute minimum text editor.
//
// Backend (windowing / Metal renderer / draw / text shaping) is the-thing.
// The text-editing core (buffer, cursors, undo, movement) is Mayday's edit.c.
// This file is the glue: it owns one editable buffer + one cursor, turns OS
// key events into edits via the E_* API, and renders the buffer, caret and
// selection with the draw API.

#include "misc.h"
#include "memory.h"
#include "strings.h"
#include "log.h"
#include "maths.h"
#include "os.h"
#include "render.h"
#include "draw.h"

#include "edit.h"

#include "embedded_fonts.h"

#define COLOUR_BG     v4f(0.05f, 0.05f, 0.07f, 1.0f)
#define COLOUR_TEXT   v4f(0.92f, 0.92f, 0.95f, 1.0f)
#define COLOUR_ACCENT v4f(1.0f, 0.75f, 0.1f, 1.0f)

#define DIALOG_CHUNK_SIZE MegaBytes(1)

typedef enum mm_mode
{
	MM_EDITOR,
	MM_OPEN_DIALOG,
} mm_mode;

typedef struct app_state
{
	m_arena *FrameArenas[2];
	int FrameArenaIndex;

	m_arena *Permanent;

	render_context Render;
	draw_context Draw;
	os_window *Window;
	d_font Font;

	edit_context Edit;
	editable_buffer Buffer;
	cursor Cursor;

	mm_mode Mode;
	edit_context DialogEdit;
	editable_buffer DialogBuffer;
	cursor DialogCursor;

	b32 Dragging;
}
app_state;

static u32 CharFromKeyCode(int Key, b32 Shift)
{
	switch (Key)
	{
		case OS_KEY_A: return Shift ? 'A' : 'a';
		case OS_KEY_B: return Shift ? 'B' : 'b';
		case OS_KEY_C: return Shift ? 'C' : 'c';
		case OS_KEY_D: return Shift ? 'D' : 'd';
		case OS_KEY_E: return Shift ? 'E' : 'e';
		case OS_KEY_F: return Shift ? 'F' : 'f';
		case OS_KEY_G: return Shift ? 'G' : 'g';
		case OS_KEY_H: return Shift ? 'H' : 'h';
		case OS_KEY_I: return Shift ? 'I' : 'i';
		case OS_KEY_J: return Shift ? 'J' : 'j';
		case OS_KEY_K: return Shift ? 'K' : 'k';
		case OS_KEY_L: return Shift ? 'L' : 'l';
		case OS_KEY_M: return Shift ? 'M' : 'm';
		case OS_KEY_N: return Shift ? 'N' : 'n';
		case OS_KEY_O: return Shift ? 'O' : 'o';
		case OS_KEY_P: return Shift ? 'P' : 'p';
		case OS_KEY_Q: return Shift ? 'Q' : 'q';
		case OS_KEY_R: return Shift ? 'R' : 'r';
		case OS_KEY_S: return Shift ? 'S' : 's';
		case OS_KEY_T: return Shift ? 'T' : 't';
		case OS_KEY_U: return Shift ? 'U' : 'u';
		case OS_KEY_V: return Shift ? 'V' : 'v';
		case OS_KEY_W: return Shift ? 'W' : 'w';
		case OS_KEY_X: return Shift ? 'X' : 'x';
		case OS_KEY_Y: return Shift ? 'Y' : 'y';
		case OS_KEY_Z: return Shift ? 'Z' : 'z';

		case OS_KEY_1: return Shift ? '!' : '1';
		case OS_KEY_2: return Shift ? '@' : '2';
		case OS_KEY_3: return Shift ? '#' : '3';
		case OS_KEY_4: return Shift ? '$' : '4';
		case OS_KEY_5: return Shift ? '%' : '5';
		case OS_KEY_6: return Shift ? '^' : '6';
		case OS_KEY_7: return Shift ? '&' : '7';
		case OS_KEY_8: return Shift ? '*' : '8';
		case OS_KEY_9: return Shift ? '(' : '9';
		case OS_KEY_0: return Shift ? ')' : '0';

		case OS_KEY_EQUAL:         return Shift ? '+' : '=';
		case OS_KEY_MINUS:         return Shift ? '_' : '-';
		case OS_KEY_LEFT_BRACKET:  return Shift ? '{' : '[';
		case OS_KEY_RIGHT_BRACKET: return Shift ? '}' : ']';
		case OS_KEY_BACKSLASH:     return Shift ? '|' : '\\';
		case OS_KEY_SEMICOLON:     return Shift ? ':' : ';';
		case OS_KEY_QUOTE:         return Shift ? '"' : '\'';
		case OS_KEY_COMMA:         return Shift ? '<' : ',';
		case OS_KEY_PERIOD:        return Shift ? '>' : '.';
		case OS_KEY_SLASH:         return Shift ? '?' : '/';
		case OS_KEY_GRAVE:         return Shift ? '~' : '`';

		case OS_KEY_SPACE:  return ' ';
		case OS_KEY_TAB:    return '\t';
		case OS_KEY_RETURN: return '\n';
	}
	return 0;
}

static int CountNewLines(str8 String)
{
	int Count = 0;
	for (s64 I = 0; I < String.Size; I++)
		if (String.Bytes[I] == '\n') Count++;
	return Count;
}

static int IndexFromLine(str8 String, int Line)
{
	if (Line <= 0) return 0;
	int Count = 0;
	for (s64 I = 0; I < String.Size; I++)
		if (String.Bytes[I] == '\n' && ++Count == Line) return (int)(I + 1);
	return (int)String.Size;
}

static void DrawSelection(app_state *App, editable_buffer *Buffer, v2f Origin, v2i Range, v2i Sel)
{
	if (Sel.X == Sel.Y) return;

	d_font Font = App->Font;
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
	f32 LineHeight = (Font.Ascent - Font.Descent + Font.LineGap) * Scale;
	v4f Colour = COLOUR_TEXT; // inverted: highlight takes the text colour

	v2f Position = Origin;
	f32 StartX = Origin.X;
	f32 SelectionStartX = StartX;

	int End = Min(Range.Y, (int)Buffer->AsString.Size);
	for (int I = Range.X; I <= End; I++)
	{
		u32 CodePoint = (I < Buffer->AsString.Size) ? Buffer->AsString.Bytes[I] : 0;
		f32 TabMultiplier = 1.0f;
		if (CodePoint == '\t') { CodePoint = ' '; TabMultiplier = 4.0f; }

		if (I == Sel.X) SelectionStartX = Position.X;
		if (I == Sel.Y || (I >= Sel.X && I <= Sel.Y && CodePoint == '\n'))
		{
			if (CodePoint == '\n')
			{
				d_atlas_glyph *Space = D_GlyphFromCodePoint(&App->Draw, Font, ' ');
				Position.X += Space->AdvanceX * Scale * TabMultiplier;
			}
			D_Quad(&App->Draw,
				v2f(SelectionStartX, Position.Y + Font.Descent * Scale),
				Position.X - SelectionStartX, LineHeight, .Colour = Colour);
		}

		if (CodePoint == '\n')
		{
			Position.Y -= LineHeight;
			Position.X = StartX;
			SelectionStartX = StartX;
			continue;
		}

		if (I < Buffer->AsString.Size)
		{
			d_atlas_glyph *Glyph = D_GlyphFromCodePoint(&App->Draw, Font, CodePoint);
			Position.X += Glyph->AdvanceX * Scale * TabMultiplier;
		}
	}
}

static int IndexFromMouse(app_state *App, edit_context *Edit, editable_buffer *Buffer,
	v2f Origin, v2i Range, v2f Mouse)
{
	return E_IndexFromPosition(Edit, &App->Draw, App->Font, Buffer,
		Range, Origin, v2i((s32)Mouse.X, (s32)Mouse.Y));
}

typedef struct editor_layout
{
	int StartIndex;
	int EndIndex;
	v2i Range;
	v2f Origin;
} editor_layout;

static editor_layout ComputeEditorLayout(editable_buffer *Buffer, v2i ViewportSize,
										 f32 Margin, f32 LineHeight)
{
	int TotalLines    = CountNewLines(Buffer->AsString) + 1;
	f32 VisibleHeight = (f32)ViewportSize.Y - 2.0f * Margin;
	f32 MaxOffsetY    = Max(0.0f, (f32)TotalLines * LineHeight - VisibleHeight);
	Buffer->Offset.Y  = ClampF32(Buffer->Offset.Y, 0.0f, MaxOffsetY);

	int FirstLine  = (int)(Buffer->Offset.Y / LineHeight);
	f32 Fraction   = Buffer->Offset.Y - (f32)FirstLine * LineHeight;
	int LineSpan   = (int)(VisibleHeight / LineHeight) + 2; // +2: partial top/bottom lines

	int LinesAbove = Min(FirstLine, 1);
	int StartLine  = FirstLine - LinesAbove;

	editor_layout Layout;
	Layout.StartIndex = IndexFromLine(Buffer->AsString, StartLine);
	Layout.EndIndex   = IndexFromLine(Buffer->AsString, FirstLine + LineSpan);
	Layout.Range      = v2i(Layout.StartIndex, Layout.EndIndex);
	Layout.Origin     = v2f(Margin,
		(f32)ViewportSize.Y - Margin - Buffer->HeightInPixels + Fraction
		+ (f32)LinesAbove * LineHeight);
	return Layout;
}

static void Frame(void *UserData, v2i ViewportSize)
{
	app_state *App = (app_state*)UserData;

	App->FrameArenaIndex = 1 - App->FrameArenaIndex;
	m_arena *Frame = App->FrameArenas[App->FrameArenaIndex];
	ArenaReset(Frame);
	SetTemporaryArena(Frame);

	App->Draw.ViewportSize = v2f(ViewportSize.X, ViewportSize.Y);
	App->Render.CommandsFirst = NULL;
	App->Render.CommandsLast = NULL;
	App->Render.TransformCount = 0;
	App->Render.ScissorCount = 0;

	m44f Ortho = M44FOrthographic(0, ViewportSize.X, ViewportSize.Y, 0, -1000, 1000);
	R_PushTransform(&App->Render, Ortho);
	R_PushScissor(&App->Render, v4i(0, 0, ViewportSize.X, ViewportSize.Y));
	R_BeginFrame(&App->Render, ViewportSize);

	editable_buffer *Buffer = &App->Buffer;
	cursor *Cursor = &App->Cursor;

	f32 Margin = 40.0f;
	f32 LineHeight = D_LineHeightFromFont(App->Font, Buffer->HeightInPixels);

	editor_layout Layout = ComputeEditorLayout(Buffer, ViewportSize, Margin, LineHeight);
	int StartIndex = Layout.StartIndex;
	int EndIndex   = Layout.EndIndex;
	v2i Range      = Layout.Range;
	v2f Origin     = Layout.Origin;


	editable_buffer *Dialog = &App->DialogBuffer;
	f32 DialogScale  = stbtt_ScaleForPixelHeight(&App->Font.Info, Dialog->HeightInPixels);
	f32 DialogPadding = 15.0f;
	v2f FileDialogSize = v2f(800, LineHeight + DialogPadding*2);
	v2f FileDialogPos;
	FileDialogPos.X = ViewportSize.X/2.0f - FileDialogSize.X/2.0f;
	FileDialogPos.Y = ViewportSize.Y/2.0f + FileDialogSize.Y/2.0f;

	f32 DialogTextHeight = D_Str8HeightFromHeight(App->Font, Dialog->HeightInPixels);
	f32 TextY = FileDialogPos.Y + (FileDialogSize.Y - DialogTextHeight)*0.5f
		- (f32)App->Font.Descent*DialogScale;

	f32 PromptWidth = D_Str8WidthFromHeight(App->Font, Str8("> "), Dialog->HeightInPixels);
	f32 ClipLeft  = FileDialogPos.X + DialogPadding + PromptWidth;
	f32 ClipRight = FileDialogPos.X + FileDialogSize.X - DialogPadding;
	f32 ClipWidth = ClipRight - ClipLeft;

	v2i DialogRange = v2i(0, (s32)Dialog->Length);

	v2f CaretLocal = E_GetVisualPositionFromEditableBuffer(&App->DialogEdit, &App->Draw,
		App->DialogCursor.Selection.Y, *Dialog, App->Font, DialogRange, v2f(0, 0));
	f32 DialogScroll = Max(0.0f, CaretLocal.X - ClipWidth*0.5f);
	v2f DialogOrigin = v2f(ClipLeft - DialogScroll, TextY);

	os_event Event;
	while (OS_NextEvent(App->Window, &Event))
	{
		v2f Mouse = Event.MousePosition;

		b32 InDialog = (App->Mode == MM_OPEN_DIALOG);
		edit_context    *ActiveEdit   = InDialog ? &App->DialogEdit   : &App->Edit;
		editable_buffer *ActiveBuffer = InDialog ? &App->DialogBuffer : &App->Buffer;
		cursor          *ActiveCursor = InDialog ? &App->DialogCursor : &App->Cursor;
		s64              ActiveCap    = InDialog ? DIALOG_CHUNK_SIZE   : EDIT_CHUNK_SIZE;
		v2f              ActiveOrigin = InDialog ? DialogOrigin : Origin;
		v2i              ActiveRange  = InDialog ? DialogRange  : Range;

		if (Event.Type == OS_EVENT_SCROLL)
		{
			Buffer->Offset.Y -= Event.Scroll.Y * 2.0f;
			continue;
		}

		if (Event.Type == OS_EVENT_MOUSE_DOWN && Event.KeyCode == OS_MOUSE_LEFT)
		{
			int Index = IndexFromMouse(App, ActiveEdit, ActiveBuffer, ActiveOrigin, ActiveRange, Mouse);
			if (OS_KeyDown(OS_KEY_OPTION))
			{
				cursor *Last = E_LastCursor(ActiveCursor);
				cursor NextCursor = *Last;
				NextCursor.Selection = v2i(Index, Index);
				E_AppendCursor(ActiveEdit, ActiveEdit->Arena, ActiveCursor, NextCursor);
			}
			else if (OS_KeyDown(OS_KEY_SHIFT))
			{
				ActiveCursor->Selection.Y = Index;
			}
			else
			{
				ActiveCursor->Next = Null; // collapse back to a single cursor
				ActiveCursor->Selection = v2i(Index, Index);
			}
			App->Dragging = !OS_KeyDown(OS_KEY_OPTION);
			continue;
		}
		if (Event.Type == OS_EVENT_MOUSE_MOVE && App->Dragging)
		{
			ActiveCursor->Selection.Y = IndexFromMouse(App, ActiveEdit, ActiveBuffer, ActiveOrigin, ActiveRange, Mouse);
			continue;
		}
		if (Event.Type == OS_EVENT_MOUSE_UP && Event.KeyCode == OS_MOUSE_LEFT)
		{
			App->Dragging = False;
			continue;
		}

		if (Event.Type != OS_EVENT_KEY_DOWN) continue;

		b32 Shift = OS_KeyDown(OS_KEY_SHIFT);
		b32 Cmd   = OS_KeyDown(OS_KEY_COMMAND);
		b32 Alt   = OS_KeyDown(OS_KEY_OPTION);
		edit_move_type MoveType = Shift ? E_MOVE_SELECTING : E_MOVE_NOT_SELECTING;
		b32 Ctrl  = OS_KeyDown(OS_KEY_CONTROL);
		int Key = Event.KeyCode;

		if (Cmd && Key == OS_KEY_P)
		{
			App->Mode = MM_OPEN_DIALOG;
			App->DialogBuffer.Length = 0;
			App->DialogCursor = (cursor){0};
			continue;
		}
		if (App->Mode == MM_OPEN_DIALOG && (Key == OS_KEY_ESCAPE || Key == OS_KEY_RETURN))
		{
			if(Key == OS_KEY_RETURN)
			{
				// Copy the file into the fixed-capacity Chunk rather than
				// repointing Buffer->AsString at the tight file allocation:
				// E_InsertTo ping-pongs Chunk<->ScratchChunk, so both must
				// stay EDIT_CHUNK_SIZE or a later swap leaves ScratchChunk
				// undersized and the next insert overruns it.
				str8 File = OS_LoadEntireFileIntoMemory(App->Permanent, App->DialogBuffer.AsString);
				if (File.Size < EDIT_CHUNK_SIZE)
				{
					MemoryCopy(Buffer->Chunk, File.Bytes, File.Size);
					Buffer->Length = File.Size;
					Cursor->Selection = v2i(0, 0);
					Cursor->Next = Null;
				}
				else
				{
					LogError("File too large for edit buffer: ?u64 bytes", (u64)File.Size);
				}
			}
			App->Mode = MM_EDITOR;
			continue;
		}


		if (Cmd)
		{
			switch (Key)
			{
				case OS_KEY_A: // select all
					ActiveCursor->Next = Null;
					ActiveCursor->Selection = v2i(0, ActiveBuffer->AsString.Size);
					break;
				case OS_KEY_L:
				{
					v2i Sel = E_GetSortedSelection(ActiveCursor->Selection);
					ActiveCursor->Next = Null;
					cursor *Tail = ActiveCursor;
					b32 First = True;
					for (int I = Sel.X; ; )
					{
						int LineEnd = I;
						while (LineEnd < ActiveBuffer->Length &&
							ActiveBuffer->Chunk[LineEnd] != '\n')
						{
							LineEnd++;
						}

						if (First)
						{
							ActiveCursor->Selection = v2i(LineEnd, LineEnd);
							First = False;
						}
						else
						{
							cursor NextCursor = *Tail;
							NextCursor.Selection = v2i(LineEnd, LineEnd);
							E_AppendCursor(ActiveEdit, ActiveEdit->Arena, Tail, NextCursor);
							Tail = Tail->Next;
						}

						if (LineEnd >= Sel.Y) break;
						I = LineEnd + 1;
					}
				} break;
				case OS_KEY_LEFT:
					E_MoveCursors(ActiveEdit, &App->Draw, App->Font, ActiveCursor, ActiveBuffer, E_MOVE_LINE_START, MoveType);
					break;
				case OS_KEY_RIGHT:
					E_MoveCursors(ActiveEdit, &App->Draw, App->Font, ActiveCursor, ActiveBuffer, E_MOVE_LINE_END, MoveType);
					break;
				case OS_KEY_UP:
				case OS_KEY_DOWN:
				{
					edit_movement Move = (Key == OS_KEY_UP) ? E_MOVE_UP : E_MOVE_DOWN;
					if (Alt)
					{
						cursor *Last = E_LastCursor(ActiveCursor);
						cursor NextCursor = *Last;
						E_AppendCursor(ActiveEdit, ActiveEdit->Arena, ActiveCursor, NextCursor);
						E_MoveCursors(ActiveEdit, &App->Draw, App->Font, E_LastCursor(ActiveCursor), ActiveBuffer, Move, MoveType);
					}
					else
					{
						E_MoveCursors(ActiveEdit, &App->Draw, App->Font, ActiveCursor, ActiveBuffer, Move, MoveType);
					}
				} break;
				case OS_KEY_DELETE:
					E_MoveCursors(ActiveEdit, &App->Draw, App->Font, ActiveCursor, ActiveBuffer, E_MOVE_WORD_LEFT, E_MOVE_SELECTING);
					E_DeleteTo(ActiveEdit, ActiveCursor, ActiveBuffer);
					break;
				default: break;
			}
			continue;
		}

		switch (Key)
		{
			case OS_KEY_LEFT:
				E_MoveCursors(ActiveEdit, &App->Draw, App->Font, ActiveCursor, ActiveBuffer,
					Alt ? E_MOVE_WORD_LEFT : E_MOVE_LEFT, MoveType);
				break;
			case OS_KEY_RIGHT:
				E_MoveCursors(ActiveEdit, &App->Draw, App->Font, ActiveCursor, ActiveBuffer,
					Alt ? E_MOVE_WORD_RIGHT : E_MOVE_RIGHT, MoveType);
				break;
			case OS_KEY_UP:
				E_MoveCursors(ActiveEdit, &App->Draw, App->Font, ActiveCursor, ActiveBuffer, E_MOVE_UP, MoveType);
				break;
			case OS_KEY_DOWN:
				E_MoveCursors(ActiveEdit, &App->Draw, App->Font, ActiveCursor, ActiveBuffer, E_MOVE_DOWN, MoveType);
				break;

			case OS_KEY_DELETE:
				E_DeleteTo(ActiveEdit, ActiveCursor, ActiveBuffer);
				break;

			case OS_KEY_ESCAPE:
				ActiveCursor->Next = Null;
				ActiveCursor->Selection.X = ActiveCursor->Selection.Y;
				break;

			default:
			{
				u32 Char = CharFromKeyCode(Key, Shift);
				s64 InsertCount = E_CursorCount(ActiveCursor);
				if (Char && ActiveBuffer->Length + InsertCount < ActiveCap)
				{
					u8 Bytes[1] = { (u8)Char };
					str8 Text = { 1, Bytes };
					E_InsertTo(ActiveEdit, ActiveCursor, ActiveBuffer, Text);
				}
			} break;
		}
	}

	Layout     = ComputeEditorLayout(Buffer, ViewportSize, Margin, LineHeight);
	StartIndex = Layout.StartIndex;
	EndIndex   = Layout.EndIndex;
	Range      = Layout.Range;
	Origin     = Layout.Origin;

	// --- Render ------------------------------------------------------------
	D_Quad(&App->Draw, v2f(0, 0), (f32)ViewportSize.X, (f32)ViewportSize.Y,
		.Colour = COLOUR_BG);

	for (cursor *C = Cursor; C; C = C->Next)
	{
		DrawSelection(App, Buffer, Origin, Range, E_GetSortedSelection(C->Selection));
	}

	v2i Primary = E_GetSortedSelection(Cursor->Selection);
	str8 Visible = { (s64)(EndIndex - StartIndex), Buffer->AsString.Bytes + StartIndex };
	D_Str8(&App->Draw, Visible,
		.Font = App->Font,
		.Position = v3f(Origin.X, Origin.Y, 0),
		.HeightInPixels = Buffer->HeightInPixels,
		.Colour = COLOUR_TEXT,
		.HighlightColour = COLOUR_BG,
		.HighlightStart = Primary.X - StartIndex,
		.HighlightEnd = Primary.Y - StartIndex);

	for (cursor *C = Cursor; C; C = C->Next)
	{
		if (C->Selection.Y < Range.X || C->Selection.Y > Range.Y) continue;
		v2f CaretPos = E_GetVisualPositionFromEditableBuffer(&App->Edit, &App->Draw,
			C->Selection.Y, *Buffer, App->Font, Range, Origin);
		D_Quad(&App->Draw, v2f(CaretPos.X, CaretPos.Y), 4.0f, LineHeight,
			.Colour = COLOUR_ACCENT);
	}

	if (App->Mode == MM_OPEN_DIALOG)
	{
		D_Quad(&App->Draw, FileDialogPos, FileDialogSize.X, FileDialogSize.Y,
				.Colour = COLOUR_BG,
				.OutlineColour = COLOUR_TEXT,
				.OutlineThickness = 2.0);
		D_Str8(&App->Draw, Str8(">"), .Font = App->Font,
			.Position = v3f(FileDialogPos.X + DialogPadding, TextY, 0),
			.HeightInPixels = Dialog->HeightInPixels, .Colour = COLOUR_ACCENT);

		R_PushScissor(&App->Render, v4i((s32)ClipLeft, (s32)FileDialogPos.Y,
			(s32)ClipWidth, (s32)FileDialogSize.Y));

		v2i Sel = E_GetSortedSelection(App->DialogCursor.Selection);
		DrawSelection(App, Dialog, DialogOrigin, DialogRange, Sel);
		D_Str8(&App->Draw, Dialog->AsString, .Font = App->Font,
			.Position = v3f(DialogOrigin.X, DialogOrigin.Y, 0),
			.HeightInPixels = Dialog->HeightInPixels,
			.Colour = COLOUR_TEXT,
			.HighlightColour = COLOUR_BG,
			.HighlightStart = Sel.X,
			.HighlightEnd = Sel.Y);

		v2f Caret = E_GetVisualPositionFromEditableBuffer(&App->DialogEdit, &App->Draw,
			App->DialogCursor.Selection.Y, *Dialog, App->Font, DialogRange, DialogOrigin);
		D_Quad(&App->Draw, v2f(Caret.X, Caret.Y), 4.0f, LineHeight,
			.Colour = COLOUR_ACCENT);

		R_PopScissor(&App->Render);
	}

	R_Update(&App->Render, OS_NextDrawable(App->Window));
}

int main(int ArgCount, char **Args)
{
	(void)ArgCount; (void)Args;

	m_arena *Arena = ArenaInitialise(GigaBytes(4));
	per_thread_context *Ctx = GetThreadLocalContext();
	Ctx->ScratchArenas[0] = ArenaInitialise(GigaBytes(1));
	Ctx->ScratchArenas[1] = ArenaInitialise(GigaBytes(1));
	Ctx->LogArena = ArenaInitialise(MegaBytes(64));
	Ctx->Logger = LoggerInitialise(Ctx->LogArena);
	Ctx->Logger->Arena = Ctx->LogArena;

	app_state App = {0};
	App.Permanent = Arena;
	App.FrameArenas[0] = ArenaInitialise(MegaBytes(256));
	App.FrameArenas[1] = ArenaInitialise(MegaBytes(256));
	SetTemporaryArena(App.FrameArenas[0]);

	os_window Window = OS_MakeWindow(Str8("micro-mayday"), 1280, 800);
	App.Window = &Window;

	r_init_options Opts = {
		.Device = Window.Device,
		.Layer = Window.Layer,
		.ShaderDir = Str8("shaders"),
	};
	R_Init(&App.Render, Arena, v2i(Window.Width, Window.Height), Opts);

	App.Draw.Arena = Arena;
	App.Draw.Render = &App.Render;
	App.Font = D_LoadFontFromTTF(&App.Draw, G_BerkeleyMono.Data);

	App.Edit.Arena = Arena;
	App.Edit.Draw = &App.Draw;
	App.Edit.ScratchChunk = ArenaPushZero(Arena, EDIT_CHUNK_SIZE);
	App.Edit.Cursors = &App.Cursor;

	App.Buffer.HeightInPixels = 50.0f;
	App.Buffer.Chunk = ArenaPushZero(Arena, EDIT_CHUNK_SIZE);

	App.Mode = MM_EDITOR;
	App.DialogEdit.Arena = Arena;
	App.DialogEdit.Draw = &App.Draw;
	App.DialogEdit.ScratchChunk = ArenaPushZero(Arena, DIALOG_CHUNK_SIZE);
	App.DialogEdit.Cursors = &App.DialogCursor;

	App.DialogBuffer.HeightInPixels = 50.0f;
	App.DialogBuffer.Chunk = ArenaPushZero(Arena, DIALOG_CHUNK_SIZE);

	str8 Initial = Str8(
		"micro-mayday\n\n"
		"the absolute minimum editor:\n"
		"  the-thing backend + Mayday's edit buffer.\n\n"
		"type to insert, backspace to delete,\n"
		"arrows to move, shift+arrows to select.\n");
	MemoryCopy(App.Buffer.Chunk, Initial.Bytes, Initial.Size);
	App.Buffer.Length = Initial.Size;

	OS_Run(&Window, Frame, &App);
	return 0;
}
