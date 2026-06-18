#pragma once
#include "misc.h"

typedef struct memory_arena
{
	u8 *Start;
	s64 Committed;
	s64 At;
}
m_arena;

typedef struct m_scratch_arena
{
	m_arena *Arena;
	s64 At;
} m_scratch_arena;

typedef struct logger logger;

typedef struct per_thread_context
{
	m_arena *LogArena;
	logger *Logger;

	m_arena *ScratchArenas[2];
	m_arena *TemporaryArena;
}
per_thread_context;

per_thread_context *GetThreadLocalContext(void);

m_scratch_arena ScratchBegin(m_arena **Arenas, int ConflictCount);
void ScratchEnd(m_scratch_arena Scratch);

m_arena *TemporaryArena(void);
void SetTemporaryArena(m_arena *Arena);

u64 AlignUp(u64 Pointer, u64 Alignment);

void MemorySet(void *Destination, int Byte, s64 NumberOfBytes);
b32 MemoryEqual(void *A, void *B, s64 NumberOfBytes);
#define MemoryCopy(Destination, Source, NumberOfBytes) __builtin_memcpy(Destination, Source, NumberOfBytes)
u8* MemoryReserve(s64 NumberOfBytes);
void MemoryCommit(void *Address, s64 NumberOfBytes);
void MemoryZero(void *Destination, s64 NumberOfBytes);

#define ArenaPushArray(Arena, Type, Count) (Type*)ArenaPushAlign(Arena, sizeof(Type)*Count, _Alignof(Type))
#define ArenaPushArrayZero(Arena, Type, Count) (Type*)ArenaPushAlignZero(Arena, sizeof(Type)*Count, _Alignof(Type))

u8* ArenaPush(m_arena *Arena, s64 Bytes);
u8* ArenaPushZero(m_arena *Arena, s64 Bytes);

u8 *ArenaPushAlignZero(m_arena *Arena, s64 Bytes, u64 Alignment);
u8 *ArenaPushAlign(m_arena *Arena, s64 Bytes, u64 Alignment);

m_arena *ArenaInitialise(s64 NumberOfBytes);

u64 GetPageSize(void);

void ArenaReset(m_arena *Arena);
