#pragma once

#include "memory.h"
#include "misc.h"
#include "maths.h"
#include "draw.h"

#define EDIT_CHUNK_SIZE MegaBytes(32)

typedef struct cursor
{
	v2f Position;
	v2f TargetPosition;

	v2i Selection;

	int VisualX;
	
} cursor;

typedef enum undo_type
{
	UNDO_NONE,
	UNDO_REMOVED,
	UNDO_INSERTED,
} undo_type;

typedef struct undo_stack
{
	undo_type Type;
	int Index;
	str8 String;
	struct undo_stack *Previous;
	struct undo_stack *Next;
} undo_stack;

typedef struct editable_buffer
{
	union
	{
		struct
		{
			s64 Length;
			u8 *Chunk;
		};
		str8 AsString;
	};

	f32 HeightInPixels;

	v2f Offset;

	b32 EditedSinceLastSave;

	u64 LastModifiedTime;

	b32 Search;

	struct editable_buffer *NextInFreeList;
} editable_buffer;

typedef struct edit_context
{
	m_arena *Arena;

	draw_context *Draw;

	editable_buffer Buffer;
	editable_buffer SearchBox;

	undo_stack *NewestAction;
	undo_stack *OldestAction;


	cursor *Cursors;
	int     CursorCount;

	u8 *ScratchChunk;

	// Debug stats for insert/delete
	int DebugBytesCopied;
	int DebugBytesInserted;
	int DebugCursorCount;
	f32 DebugTimeMs;
} edit_context;

typedef enum edit_movement
{
	E_MOVE_NONE,
	E_MOVE_LINE_START,
	E_MOVE_LINE_END,
	E_MOVE_LEFT,
	E_MOVE_RIGHT,
	E_MOVE_UP,
	E_MOVE_DOWN,
	E_MOVE_WORD_LEFT,
	E_MOVE_WORD_RIGHT,

} edit_movement;

typedef enum edit_move_type
{
	E_MOVE_NOT_SELECTING,
	E_MOVE_SELECTING,
} edit_move_type;

b32 IsWhiteSpace(u32 CodePoint);
b32 IsIdentifierCharacter(u32 CodePoint);

v2i E_GetSortedSelection(v2i Selection);

void E_DeleteTo(edit_context *Edit, cursor *Cursor, editable_buffer *Buffer);

void E_InsertTo(edit_context *Edit, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert);

void E_RemoveTextFromSelection(edit_context *Edit, editable_buffer *Buffer, v2i Selection);
void E_InsertTextAtIndex(edit_context *E, int Index, editable_buffer *Buffer, str8 TextToInsert);

void E_MoveCursors(edit_context *Edit, draw_context *Draw,
				   d_font Font, cursor *Cursor, editable_buffer *Buffer,
				   edit_movement Movement, edit_move_type MoveType);

void E_DeleteAtLeastOneCharacterInRange(edit_context *Edit,
										edit_movement Movement,
										cursor *Cursors);

void E_InsertTextAtCursor(edit_context *E, cursor *Cursor, editable_buffer *Buffer, str8 TextToInsert);

v2f E_GetPositionFromEditableBuffer(edit_context *E,
									draw_context *D,
									int Index,
									editable_buffer Buffer,
									d_font Font,
									v2f Position);

v2f E_GetVisualPositionFromEditableBuffer(edit_context *E,
									draw_context *D,
									int Index,
									editable_buffer Buffer,
									d_font Font,
									v2i Indices,
									v2f Position);

int E_DistanceFromPreviousNewLine(edit_context *E, int Index, editable_buffer *Buffer);
int E_DistanceFromNextNewLine(edit_context *E, int Index, editable_buffer *Buffer);
int E_LineStartFromIndex(edit_context *Edit, int Index, editable_buffer *Buffer);
int E_LineEndFromIndex(edit_context *Edit, int Index, editable_buffer *Buffer);
int E_IndexFromPosition(edit_context *Edit, draw_context *Draw, d_font Font,
						editable_buffer *Buffer, v2i Indices,
						v2f BufferPosition, v2i Position);


void E_RemoveSelectedTextAtCursors(edit_context *E_Context, cursor *Cursor, editable_buffer *Buffer);

int E_LineFromBufferIndex(edit_context *E_Context, int Index, editable_buffer *Buffer);

int E_VisualXFromIndex(edit_context *Edit, draw_context *Draw,
					   d_font Font, int Index, editable_buffer *Buffer);

int E_GetMaxTextWidthFromRange(edit_context *E, draw_context *D,
							   d_font Font, editable_buffer *Buffer,
							   int RangeStart, int RangeEnd);

int E_IndexFromVisualXAndLineStart(edit_context *E, draw_context *D, d_font Font,
								   editable_buffer *Buffer, int VisualX, int LineStart);

cursor *E_LastCursor(edit_context *Edit);
void E_AppendCursor(edit_context *Edit, m_arena *Arena, cursor Cursor);

void PushUndoAction(edit_context *E,
					m_arena *Arena,
					cursor *Cursor,
					undo_type Type,
					str8 InsertedOrRemovedText);
undo_stack *PopOffUndoStack(edit_context *E);
