#include "log.h"
#include "memory.h"
#include "strings.h"

#include <stdio.h>

logger *LoggerInitialise(m_arena *Arena)
{
	return ArenaPushArrayZero(Arena, logger, 1);
}

static void LogF(char *Tag, char *Style,
				 char *FileName, char *FunctionName, u64 LineNumber,
				 char *Format, va_list Args)
{
	per_thread_context *Ctx = GetThreadLocalContext();
	logger *Logger = Ctx->Logger;
	if (Logger && Logger->Silent) return;

	m_scratch_arena S = ScratchBegin(0, 0);
	str8 Body = Str8PushFV(S.Arena, Format, Args);
	printf("[%s%s%s] %s%s%s() %s%s%s:%llu  %.*s\n",
		Style, Tag, S_RESET,
		S_BLUE, FunctionName, S_RESET,
		S_GREEN, FileName, S_RESET,
		(unsigned long long)LineNumber,
		(int)Body.Size, Body.Bytes);
	ScratchEnd(S);
}

void LogInfoF(char *F, char *Fn, u64 L, char *Format, ...)
{
	va_list A; va_start(A, Format); LogF("INFO",  S_BOLD,           F, Fn, L, Format, A); va_end(A);
}
void LogWarnF(char *F, char *Fn, u64 L, char *Format, ...)
{
	va_list A; va_start(A, Format); LogF("WARN",  S_BOLD S_YELLOW,  F, Fn, L, Format, A); va_end(A);
}
void LogErrorF(char *F, char *Fn, u64 L, char *Format, ...)
{
	va_list A; va_start(A, Format); LogF("ERROR", S_BOLD S_RED,     F, Fn, L, Format, A); va_end(A);
}
