#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include "strings.h"
#include "memory.h"
#include "maths.h"

b32 Str8Equal(str8 A, str8 B)
{
	if (A.Size != B.Size) return False;
	for (s64 I = 0; I < A.Size; I++) if (A.Bytes[I] != B.Bytes[I]) return False;
	return True;
}

str8 Str8Copy(m_arena *Arena, str8 String)
{
	str8 R = {0};
	R.Size = String.Size;
	R.Bytes = ArenaPush(Arena, String.Size + 1);
	MemoryCopy(R.Bytes, String.Bytes, String.Size);
	R.Bytes[R.Size] = 0;
	return R;
}

char* CStrFromStr8(m_arena *Arena, str8 String)
{
	char* R = (char*)ArenaPushArrayZero(Arena, char, String.Size + 1);
	MemoryCopy(R, String.Bytes, String.Size);
	R[String.Size] = 0;
	return R;
}

void Str8ListAppend(m_arena *Arena, str8_list *List, str8 String)
{
	if (String.Size <= 0) return;
	str8_list_node *N = ArenaPushArrayZero(Arena, str8_list_node, 1);
	N->String = Str8Copy(Arena, String);
	if (!List->First) { List->First = N; List->Last = N; }
	else { List->Last->Next = N; N->Previous = List->Last; List->Last = N; }
}

str8 Str8ListJoin(m_arena *Arena, str8_list *List)
{
	u64 Total = 0;
	for (str8_list_node *N = List->First; N; N = N->Next) Total += N->String.Size;
	u8 *Bytes = ArenaPush(Arena, Total);
	u64 At = 0;
	for (str8_list_node *N = List->First; N; N = N->Next)
	{
		MemoryCopy(Bytes + At, N->String.Bytes, N->String.Size);
		At += N->String.Size;
	}
	return (str8){ Total, Bytes };
}

str8 Str8PushFV(m_arena *Arena, char *Format, va_list InputArgList)
{
	va_list ArgList;
	va_copy(ArgList, InputArgList);
	str8_list Builder = {0};
	char *Iter = Format;
	char *Start = Format;

	while (Iter && Iter[0])
	{
		if (Iter[0] && *Iter++ == '?')
		{
			str8 UpTo = {Iter - Start - 1, (u8*)Start};
			Str8ListAppend(Arena, &Builder, UpTo);

			char Buf[1024];
			int Len = 0;
			int Skip = 0;

			if (Str8Equal((str8){1, (u8*)Iter}, Str8("S")))
			{
				str8 S = va_arg(ArgList, str8);
				str8 V = Str8Copy(Arena, S);
				Str8ListAppend(Arena, &Builder, V);
				Skip = 1;
			}
			else if (Str8Equal((str8){4, (u8*)Iter}, Str8("cstr")))
			{
				char *S = va_arg(ArgList, char*);
				Len = snprintf(Buf, sizeof(Buf), "%s", S);
				Skip = 4;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("s32")))
			{
				int V = va_arg(ArgList, int);
				Len = snprintf(Buf, sizeof(Buf), "%d", V);
				Skip = 3;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("u32")))
			{
				unsigned V = va_arg(ArgList, unsigned);
				Len = snprintf(Buf, sizeof(Buf), "%u", V);
				Skip = 3;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("u64")))
			{
				unsigned long long V = va_arg(ArgList, unsigned long long);
				Len = snprintf(Buf, sizeof(Buf), "%llu", V);
				Skip = 3;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("f32")))
			{
				double V = va_arg(ArgList, double);
				Len = snprintf(Buf, sizeof(Buf), "%f", V);
				Skip = 3;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("v2f")))
			{
				v2f V = va_arg(ArgList, v2f);
				Len = snprintf(Buf, sizeof(Buf), "(%f, %f)", V.X, V.Y);
				Skip = 3;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("v2i")))
			{
				v2i V = va_arg(ArgList, v2i);
				Len = snprintf(Buf, sizeof(Buf), "(%d, %d)", V.X, V.Y);
				Skip = 3;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("v3f")))
			{
				v3f V = va_arg(ArgList, v3f);
				Len = snprintf(Buf, sizeof(Buf), "(%f, %f, %f)", V.X, V.Y, V.Z);
				Skip = 3;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("v4f")))
			{
				v4f V = va_arg(ArgList, v4f);
				Len = snprintf(Buf, sizeof(Buf), "(%f, %f, %f, %f)", V.X, V.Y, V.Z, V.W);
				Skip = 3;
			}
			else if (Str8Equal((str8){3, (u8*)Iter}, Str8("v4i")))
			{
				v4i V = va_arg(ArgList, v4i);
				Len = snprintf(Buf, sizeof(Buf), "(%d, %d, %d, %d)", V.X, V.Y, V.Z, V.W);
				Skip = 3;
			}
			else if (Str8Equal((str8){4, (u8*)Iter}, Str8("m44f")))
			{
				m44f M = va_arg(ArgList, m44f);
				Len = snprintf(Buf, sizeof(Buf),
					"[%f %f %f %f | %f %f %f %f | %f %f %f %f | %f %f %f %f]",
					M.E[0][0], M.E[0][1], M.E[0][2], M.E[0][3],
					M.E[1][0], M.E[1][1], M.E[1][2], M.E[1][3],
					M.E[2][0], M.E[2][1], M.E[2][2], M.E[2][3],
					M.E[3][0], M.E[3][1], M.E[3][2], M.E[3][3]);
				Skip = 4;
			}

			if (Skip)
			{
				if (Len > 0)
				{
					str8 V = { Len, ArenaPush(Arena, Len) };
					MemoryCopy(V.Bytes, Buf, Len);
					Str8ListAppend(Arena, &Builder, V);
				}
				Iter += Skip;
				Start = Iter;
			}
		}
	}
	va_end(ArgList);

	str8 Tail = {Iter - Start, (u8*)Start};
	Str8ListAppend(Arena, &Builder, Tail);
	return Str8ListJoin(Arena, &Builder);
}

str8 Str8PushF(m_arena *Arena, char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	str8 R = Str8PushFV(Arena, Format, Args);
	va_end(Args);
	return R;
}

void Print(char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	m_scratch_arena S = ScratchBegin(0, 0);
	str8 R = Str8PushFV(S.Arena, Format, Args);
	va_end(Args);
	printf("%.*s", (int)R.Size, R.Bytes);
	ScratchEnd(S);
}

void PrintLn(char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	m_scratch_arena S = ScratchBegin(0, 0);
	str8 R = Str8PushFV(S.Arena, Format, Args);
	va_end(Args);
	printf("%.*s\n", (int)R.Size, R.Bytes);
	ScratchEnd(S);
}
