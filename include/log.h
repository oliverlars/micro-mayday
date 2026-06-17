#pragma once

#include "strings.h"
#include "memory.h"

typedef struct logger
{
	b32 Silent;
	m_arena *Arena;
	int Level;
}
logger;

logger *LoggerInitialise(m_arena *Arena);

void LogInfoF(char* FileName, char* FunctionName, u64 LineNumber, char *Format, ...);
void LogWarnF(char* FileName, char* FunctionName, u64 LineNumber, char *Format, ...);
void LogErrorF(char* FileName, char* FunctionName, u64 LineNumber, char *Format, ...);

#define LogInfo(...)  LogInfoF((char*)__FILE__,  (char*)__func__, (u64)__LINE__, __VA_ARGS__)
#define LogWarn(...)  LogWarnF((char*)__FILE__,  (char*)__func__, (u64)__LINE__, __VA_ARGS__)
#define LogError(...) LogErrorF((char*)__FILE__, (char*)__func__, (u64)__LINE__, __VA_ARGS__)
