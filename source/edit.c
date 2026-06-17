#include "edit.h"

#include "maths.h"
#include "memory.h"
#include "misc.h"
#include "os.h"
#include "stb_truetype.h"
#include "draw.h"


#include <assert.h>
#include <ctype.h>

b32 IsWhiteSpace(u32 CodePoint)
{
	b32 Result = False;
	switch(CodePoint)
	{
	case '\t':
	case ' ':
	case '\r':
	case '\n':
		Result = True;
	break;
	}

	return Result;
}

b32 IsIdentifierCharacter(u32 CodePoint)
{
	b32 Result = False;
	if(isalnum(CodePoint) || CodePoint == '_')
	{
		Result = True;
	}
	return Result;
}

v2i E_GetSortedSelection(v2i Selection)
{
	v2i Result;
	Result.X = Min(Selection.X, Selection.Y);
	Result.Y = Max(Selection.X, Selection.Y);
	return Result;
}

void E_DeleteTo(edit_context *Edit, cursor *Cursor, editable_buffer *Buffer)
{
	assert(Edit->ScratchChunk);

	int ReadPos = 0;
	int WritePos = 0;

	for(cursor *C = Cursor; C; C = C->Next)
	{
		v2i Selection = E_GetSortedSelection(C->Selection);

		int DeleteStart;
		int DeleteEnd;
		if(C->Selection.X != C->Selection.Y)
		{
			DeleteStart = Selection.X;
			DeleteEnd = Selection.Y;
		}
		else
		{
			DeleteStart = Max(Selection.X - 1, 0);
			DeleteEnd = Selection.Y;
		}

		// Copy source text before this cursor's delete range
		int CopySize = DeleteStart - ReadPos;
		if(CopySize > 0)
		{
			MemoryCopy(Edit->ScratchChunk + WritePos, Buffer->Chunk + ReadPos, CopySize);
			WritePos += CopySize;
		}

		// Update cursor position
		C->Selection = v2i(WritePos, WritePos);

		// Skip over deleted range in source
		ReadPos = DeleteEnd;
	}

	// Copy remaining source text after last cursor
	int Remaining = Buffer->Length - ReadPos;
	if(Remaining > 0)
	{
		MemoryCopy(Edit->ScratchChunk + WritePos, Buffer->Chunk + ReadPos, Remaining);
		WritePos += Remaining;
	}

	// Swap temp and source
	u8 *Swap = Buffer->Chunk;
	Buffer->Chunk = Edit->ScratchChunk;
	Edit->ScratchChunk = Swap;
	Buffer->Length = WritePos;
}

void E_InsertTo(edit_context *Edit, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert)
{
	assert(Edit->ScratchChunk);

	u64 Start = OS_TimeStampNow();

	int ReadPos = 0;
	int WritePos = 0;
	int TotalBytesCopied = 0;
	int TotalBytesInserted = 0;
	int CursorCount = 0;

	for(cursor *C = Cursor; C; C = C->Next)
	{
		v2i Selection = E_GetSortedSelection(C->Selection);

		// Copy source text before this cursor
		int CopySize = Selection.X - ReadPos;
		if(CopySize > 0)
		{
			MemoryCopy(Edit->ScratchChunk + WritePos, Buffer->Chunk + ReadPos, CopySize);
			WritePos += CopySize;
			TotalBytesCopied += CopySize;
		}

		// Insert text
		MemoryCopy(Edit->ScratchChunk + WritePos, TextToInsert.Bytes, TextToInsert.Size);
		WritePos += TextToInsert.Size;
		TotalBytesInserted += TextToInsert.Size;

		// Update cursor to after inserted text
		C->Selection = v2i(WritePos, WritePos);

		// Skip over selection range in source (collapse)
		ReadPos = Selection.Y;
		CursorCount++;
	}

	// Copy remaining source text after last cursor
	int Remaining = Buffer->Length - ReadPos;
	if(Remaining > 0)
	{
		MemoryCopy(Edit->ScratchChunk + WritePos, Buffer->Chunk + ReadPos, Remaining);
		WritePos += Remaining;
		TotalBytesCopied += Remaining;
	}

	Edit->DebugBytesCopied = TotalBytesCopied;
	Edit->DebugBytesInserted = TotalBytesInserted;
	Edit->DebugCursorCount = CursorCount;
	Edit->DebugTimeMs = OS_MilliSecondsFromTimeStamp(OS_TimeStampNow() - Start);

	// Swap temp and source
	u8 *Swap = Buffer->Chunk;
	Buffer->Chunk = Edit->ScratchChunk;
	Edit->ScratchChunk = Swap;
	Buffer->Length = WritePos;
}

void E_RemoveTextFromSelection(edit_context *Edit, editable_buffer *Buffer, v2i Selection)
{
	Selection = E_GetSortedSelection(Selection);
	MemoryCopy(Edit->ScratchChunk, Buffer->Chunk, Selection.X);
	MemoryCopy(Edit->ScratchChunk + Selection.X, Buffer->Chunk + Selection.Y, Buffer->Length - Selection.Y);
	MemoryCopy(Buffer->Chunk, Edit->ScratchChunk, Buffer->Length - (Selection.Y - Selection.X));

	Buffer->Length -= Selection.Y - Selection.X;
}

void E_MoveCursors(edit_context *Edit, draw_context *Draw,
				   d_font Font, cursor *Cursor, editable_buffer *Buffer,
				   edit_movement Movement, edit_move_type MoveType)
{
	if(Movement == E_MOVE_NONE) return;
	for(;Cursor; Cursor = Cursor->Next)
	{
		v2i Selection = Cursor->Selection;
		b32 IsInSelection = Cursor->Selection.X != Cursor->Selection.Y;
		if(Movement == E_MOVE_LEFT && IsInSelection && MoveType == E_MOVE_NOT_SELECTING)
		{
			Selection = E_GetSortedSelection(Cursor->Selection);
			Selection = v2i(Selection.X, Selection.X);
			Cursor->VisualX = 0;
		}
		else if(Movement == E_MOVE_RIGHT && IsInSelection && MoveType == E_MOVE_NOT_SELECTING)
		{
			Selection = E_GetSortedSelection(Cursor->Selection);
			Selection = v2i(Selection.Y, Selection.Y);
			Cursor->VisualX = 0;
		}

		if(Movement == E_MOVE_LEFT)
		{
			Selection.X -= 1;
			Selection.Y -= 1;
			Cursor->VisualX = 0;
		}
		else if(Movement == E_MOVE_RIGHT)
		{
			Selection.X += 1;
			Selection.Y += 1;
			Cursor->VisualX = 0;
		}
		else if(Movement == E_MOVE_DOWN)
		{
			Selection = E_GetSortedSelection(Cursor->Selection);
			int Index = Selection.Y;
			int DistanceFromNextNewLine = E_DistanceFromNextNewLine(Edit, Index, Buffer);

			int LineBelowStart = Index + DistanceFromNextNewLine + 1;
			int LineBelowEnd = LineBelowStart + E_DistanceFromNextNewLine(Edit, LineBelowStart, Buffer);

			if(Cursor->VisualX == 0)
			{
				Cursor->VisualX = E_VisualXFromIndex(Edit, Draw, Font, Selection.Y, Buffer);
			}
			int IndexMaybePastLineEnd = E_IndexFromVisualXAndLineStart(Edit, Draw, Font, Buffer, Cursor->VisualX, LineBelowStart);
			Index = Min(LineBelowEnd, IndexMaybePastLineEnd);
			Selection = v2i(Index, Index);
		}
		else if(Movement == E_MOVE_UP)
		{
			Selection = E_GetSortedSelection(Cursor->Selection);
			int Index = Selection.X;
			int DistancePreviousNewLine = E_DistanceFromPreviousNewLine(Edit, Index, Buffer);

			int LineAboveEnd = Index - DistancePreviousNewLine - 1;
			int LineAboveStart = LineAboveEnd - E_DistanceFromPreviousNewLine(Edit, LineAboveEnd, Buffer);

			if(Cursor->VisualX == 0)
			{
				Cursor->VisualX = E_VisualXFromIndex(Edit, Draw, Font, Selection.X, Buffer);
			}
			int IndexMaybePastLineEnd = E_IndexFromVisualXAndLineStart(Edit, Draw, Font, Buffer, Cursor->VisualX, LineAboveStart);
			Index = Min(LineAboveEnd, IndexMaybePastLineEnd);
			Selection = v2i(Index, Index);
		}
		else if(Movement == E_MOVE_WORD_RIGHT)
		{
			if(Selection.Y < Buffer->Length && !IsIdentifierCharacter(Buffer->Chunk[Selection.Y]))
			{
				for(; Selection.Y < Buffer->Length && !IsIdentifierCharacter(Buffer->Chunk[Selection.Y]);)
				{
					if(Buffer->Chunk[Selection.Y] == '\n') break;
					Selection.Y++;
				}
			}

			for(; Selection.Y< Buffer->Length && IsIdentifierCharacter(Buffer->Chunk[Selection.Y]);)
			{
				if(Buffer->Chunk[Selection.Y] == '\n') break;
				Selection.Y++;
			}
			Selection.X = Selection.Y;
		}
		else if(Movement == E_MOVE_WORD_LEFT)
		{
			if(!IsIdentifierCharacter(Buffer->Chunk[Selection.Y - 1]))
			{
				for(; Selection.Y -1 >= 0 && !IsIdentifierCharacter(Buffer->Chunk[Selection.Y - 1]);)
				{
					if(Buffer->Chunk[Selection.Y - 1] == '\n') break;
					Selection.Y--;
				}
			}
			for(;Selection.Y - 1 >= 0 && IsIdentifierCharacter(Buffer->Chunk[Selection.Y - 1]);)
			{
				if(Buffer->Chunk[Selection.Y - 1] == '\n') break;
				Selection.Y--;
			}
			Selection.X = Selection.Y;
		}
		else if(Movement == E_MOVE_LINE_END)
		{
			int LineEnd = E_LineEndFromIndex(Edit, Selection.Y, Buffer);
			Selection.X = LineEnd;
			Selection.Y = LineEnd;
		}
		else if(Movement == E_MOVE_LINE_START)
		{
			int LineStart = E_LineStartFromIndex(Edit, Selection.Y, Buffer);
			int FirstNonWhiteSpaceChar = LineStart;

			for(;FirstNonWhiteSpaceChar < Buffer->Length &&
				IsWhiteSpace(Buffer->Chunk[FirstNonWhiteSpaceChar]);)
			{
				FirstNonWhiteSpaceChar++;
			}

			if(Selection.Y <= FirstNonWhiteSpaceChar && Selection.Y >= LineStart)
			{
				Selection.X = LineStart;
				Selection.Y = LineStart;
			}
			else
			{
				Selection.X = FirstNonWhiteSpaceChar;
				Selection.Y = FirstNonWhiteSpaceChar;
			}
		}


		if(MoveType == E_MOVE_SELECTING)
		{
			Cursor->Selection.Y = ClampS32(Selection.Y, 0, Buffer->Length);
		}
		else if(MoveType == E_MOVE_NOT_SELECTING)
		{
			Cursor->Selection.X = ClampS32(Selection.X, 0, Buffer->Length);
			Cursor->Selection.Y = ClampS32(Selection.Y, 0, Buffer->Length);
		}
	}
}

void E_InsertTextAtIndex(edit_context *E, int Index, editable_buffer *Buffer, str8 TextToInsert)
{
	int At = Index;

	//TODO(Oliver): Not implemented yet
	assert(Buffer->Length + TextToInsert.Size < MegaBytes(1));
	assert(E->ScratchChunk);

	MemoryCopy(E->ScratchChunk, Buffer->Chunk, At);
	MemoryCopy(E->ScratchChunk + At, TextToInsert.Bytes, TextToInsert.Size);
	MemoryCopy(E->ScratchChunk + At + TextToInsert.Size, Buffer->Chunk + At, Buffer->Length - At);

	MemoryCopy(Buffer->Chunk, E->ScratchChunk, Buffer->Length + TextToInsert.Size);

	Buffer->Length = ClampS32(Buffer->Length + TextToInsert.Size, 0, MegaBytes(1));
}

v2f E_GetPositionFromEditableBuffer(edit_context *E,
									draw_context *D,
									int Index,
									editable_buffer Buffer,
									d_font Font,
									v2f Position)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer.HeightInPixels);

	f32 StartX = Position.X;
	v2f Result = Position;

	int At = Index;
	for(int I = 0; I < Buffer.Length; I++)
	{
		if(I == At)
		{
			break;
		}

		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = Buffer.Chunk[I];

		if(CodePoint == '\n')
		{
			Result.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Result.X = StartX;
			continue;
		}

		f32 TabMultiplier = 1.0f;
		if(CodePoint == '\t')
		{
			CodePoint = ' ';
			TabMultiplier *= 4.0;
		}

		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, CodePoint);

		Result.X += Glyph->AdvanceX * Scale * TabMultiplier;
	}
	Result.Y += Font.Descent*Scale;
	return Result;
}

v2f E_GetVisualPositionFromEditableBuffer(edit_context *E,
									draw_context *D,
									int Index,
									editable_buffer Buffer,
									d_font Font,
									v2i Indices,
									v2f Position)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer.HeightInPixels);

	f32 StartX = Position.X;
	v2f Result = Position;

	int At = Index;
	for(int I = Indices.X; I <= Indices.Y; I++)
	{
		if(I == At)
		{
			break;
		}

		// TODO(Oliver): Needs to actually iterate utf-8
		u32 CodePoint = Buffer.Chunk[I];

		if(CodePoint == '\n')
		{
			Result.Y -= (Font.Ascent - Font.Descent + Font.LineGap)*Scale;
			Result.X = StartX;
			continue;
		}

		f32 TabMultiplier = 1.0f;
		if(CodePoint == '\t')
		{
			CodePoint = ' ';
			TabMultiplier *= 4.0;
		}

		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, CodePoint);

		Result.X += Glyph->AdvanceX * Scale * TabMultiplier;
	}
	Result.Y += Font.Descent*Scale;
	return Result;
}

int E_DistanceFromNextNewLine(edit_context *E, int Index, editable_buffer *Buffer)
{
	int At = Index;
	int I = At;
	for(;I < Buffer->Length; I++)
	{
		if(Buffer->Chunk[I] == '\n')
		{
			break;
		}
	}
	return I - At;
}

int E_DistanceFromPreviousNewLine(edit_context *E, int Index, editable_buffer *Buffer)
{
	int At = Index - 1;
	int I = At;
	for(;I >= 0; I--)
	{
		if(Buffer->Chunk[I] == '\n')
		{
			break;
		}
	}
	return At - I;
}

int E_LineEndFromIndex(edit_context *Edit, int Index, editable_buffer *Buffer)
{
	if(Index < Buffer->Length && Buffer->Chunk[Index] == '\n')
	{
		// NOTE(Oliver): We might be already on a newline, but that could be
		// apart of the line, as it's inclusive of the newline. A newline
		// only start when we are past the newline.
		Index = Max(Index - 1, 0);
	}

	for(; Index < Buffer->Length; Index++)
	{
		if(Buffer->Chunk[Index] == '\n')
		{
			break;
		}
	}
	return Index;
}


int E_LineStartFromIndex(edit_context *Edit, int Index, editable_buffer *Buffer)
{
	if(Index < Buffer->Length && Buffer->Chunk[Index] == '\n')
	{
		// NOTE(Oliver): We might be already on a newline, but that could be
		// apart of the line, as it's inclusive of the newline. A newline
		// only start when we are past the newline.
		Index = Max(Index - 1, 0);
	}

	for(; Index > 0; Index--)
	{
		if(Buffer->Chunk[Index-1] == '\n')
		{
			break;
		}
	}
	return Index;
}

int E_LineFromBufferIndex(edit_context *E_Context, int Index, editable_buffer *Buffer)
{
	int Line = 0;
	for(int I = 0; I < Index; I++)
	{
		if(Buffer->Chunk[I] == '\n') Line++;
	}

	return Line;
}

int E_GetMaxTextWidthFromRange(edit_context *E, draw_context *D,
							   d_font Font, editable_buffer *Buffer,
							   int RangeStart, int RangeEnd)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
	int MaxWidth = 0;
	int X = 0;
	for(int I = RangeStart; I < RangeEnd; I++)
	{
		u32 CodePoint =  Buffer->Chunk[I];

		if(CodePoint == '\n')
		{
			X = 0;
			continue;
		}

		f32 TabMultiplier = 1.0;
		if(CodePoint == '\t')
		{

			TabMultiplier *= 4.0;
		}
		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, CodePoint);

		X += Glyph->AdvanceX * Scale * TabMultiplier;
		MaxWidth = Max(X, MaxWidth);
	}

	return MaxWidth;
}

int E_IndexFromPosition(edit_context *Edit, draw_context *Draw, d_font Font,
						editable_buffer *Buffer, v2i Indices,
						v2f BufferPosition, v2i Position)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
	f32 LineHeight = D_LineHeightFromFont(Font, Buffer->HeightInPixels);
	int Index = Indices.X;
	f32 BufferXStart = BufferPosition.X;

	int Result = Index;
	for(;Index <= Indices.Y; Index++)
	{
		// Indices.Y is the inclusive slice end and can equal Buffer->Length
		// (caret past the last byte). Read a sentinel there rather than the
		// stale byte left past the end by the edit buffer swap.
		u32 CodePoint = (Index < Buffer->Length) ? Buffer->Chunk[Index] : 0;

		f32 TabMultiplier = 1.0;
		if(CodePoint == '\t')
		{

			TabMultiplier *= 4.0;
		}

		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(Draw, Font, CodePoint);

		if(Position.Y > (BufferPosition.Y + LineHeight))
		{
			break;
		}
		else if( BufferPosition.X < Position.X)
		{
			Result = Index;
		}

		if(CodePoint == '\n')
		{
			BufferPosition.X = BufferXStart;
			BufferPosition.Y -= LineHeight;
			continue;
		}

		BufferPosition.X += Glyph->AdvanceX * Scale * TabMultiplier;
	}

	return Result;
}

int E_IndexFromVisualXAndLineStart(edit_context *E, draw_context *D, d_font Font,
								   editable_buffer *Buffer, int VisualX, int LineStart)
{
	f32 Scale = stbtt_ScaleForPixelHeight(&Font.Info, Buffer->HeightInPixels);
	int X = 0;
	int Index = LineStart;
	for(;Index < Buffer->Length; Index++)
	{
		u32 CodePoint =  Buffer->Chunk[Index];

		if(CodePoint == '\n' || X >= VisualX)
		{
			break;
		}

		f32 TabMultiplier = 1.0;
		if(CodePoint == '\t')
		{

			TabMultiplier *= 4.0;
		}
		d_atlas_glyph *Glyph  = D_GlyphFromCodePoint(D, Font, CodePoint);

		X += Glyph->AdvanceX * Scale * TabMultiplier;
	}

	return Index;
}

int E_VisualXFromIndex(edit_context *Edit, draw_context *Draw,
					   d_font Font, int Index, editable_buffer *Buffer)
{
	int DistanceFromPreviousNewLine = E_DistanceFromPreviousNewLine(Edit, Index, Buffer);
	int LineStart = Index - DistanceFromPreviousNewLine;
	return E_GetMaxTextWidthFromRange(Edit, Draw, Font,  Buffer, LineStart, Index);
}

cursor *E_LastCursor(cursor *Cursor)
{
	for(;Cursor->Next; Cursor = Cursor->Next);
	return Cursor;
}

int E_CursorCount(cursor *Cursor)
{
	int Result = 0;
	for(; Cursor; Cursor = Cursor->Next)
	{
		Result++;
	}
	return Result;
}

void E_AppendCursor(edit_context *Edit, m_arena *Arena, cursor *Cursor, cursor NextCursor)
{
	assert(Cursor);
	Cursor = E_LastCursor(Cursor);

	Cursor->Next = ArenaPushArrayZero(Arena, cursor, 1);
	NextCursor.Previous = Cursor;
	Cursor->Next[0] = NextCursor;
}

void PushUndoAction(edit_context *E, m_arena *Arena, cursor *Cursor, undo_type Type, str8 InsertedOrRemovedText)
{
	undo_stack *Action = ArenaPushArrayZero(Arena, undo_stack, 1);
	Action->Index = Cursor->Selection.X;
	Action->String = Str8Copy(Arena, InsertedOrRemovedText);
	Action->Type = Type;
	if(!E->NewestAction)
	{
		E->NewestAction = Action;
		E->OldestAction = Action;
	}
	else
	{
		E->NewestAction->Next = Action;
		Action->Previous = E->NewestAction;
		E->NewestAction = Action;
	}
}

undo_stack *PopOffUndoStack(edit_context *E)
{
	assert(E->NewestAction);
	undo_stack *Result = E->NewestAction;
	if(E->NewestAction->Previous)
	{
		E->NewestAction = E->NewestAction->Previous;
	}
	else
	{
		E->NewestAction = Null;
		E->OldestAction = Null;
	}
	return Result;
}
