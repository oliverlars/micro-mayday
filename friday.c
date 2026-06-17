#include "friday.h"
#include <sys/stat.h>

/*
Build system for the-thing — a minimal Metal renderer.

Usage:
	./friday          build (debug)
	./friday run      build then launch
	./friday release  optimised build (-O2)
	./friday rebuild  force full rebuild
	./friday help     print this

Bootstrap (one-time): see build.sh, or run:
	clang friday.c -O2 -o friday
*/

Entry(InputWaveInfo)
{
	wave_info *WaveInfo = (wave_info*)InputWaveInfo;
	shared_wave_info *Shared = WaveInfo->Shared;

	memory_arena Arena = {0};
	Arena.Start = MemoryReserve(GigaBytes(64));

	build_options BuildOptions = {0};
	b32 PrintHelpMenu = False;
	b32 ShouldRun = False;

	if(WaveIsFirstLane(WaveInfo))
	{
		b32 OptimisedBuild = False;
		b32 Rebuild = False;

		for(s64 ArgIndex = 1; ArgIndex < Shared->ArgCount; ArgIndex++)
		{
			str8 Arg = Str8PushF(&Arena, "%s", Shared->Args[ArgIndex]);
			if(0) {}
			else if(Str8Equal(Arg, Str8("release"))) { OptimisedBuild = True; }
			else if(Str8Equal(Arg, Str8("rebuild"))) { Rebuild = True; }
			else if(Str8Equal(Arg, Str8("run")))     { ShouldRun = True; }
			else if(Str8Equal(Arg, Str8("help")))
			{
				printf("\nfriday — build system for the-thing\n\n");
				printf("Flags:\n");
				printf("  run       build, then launch the binary\n");
				printf("  release   optimised build (-O2)\n");
				printf("  rebuild   force full rebuild (ignore object cache)\n");
				printf("  help      print this menu\n\n");
				PrintHelpMenu = True;
			}
		}

		str8_list IncludePaths = {0};
		str8_list SourceFiles = {0};
		str8_list ExtraLinkFlags = {0};
		str8_list ExtraCompileFlags = {0};

		Str8ListAppend(&Arena, &IncludePaths, Str8("include/"));
		Str8ListAppend(&Arena, &IncludePaths, Str8("external/"));
		Str8ListAppend(&Arena, &IncludePaths, Str8("build/"));

		// main.c lives at the project root; the rest live in source/.
		Str8ListAppend(&Shared->Arena, &SourceFiles, Str8("main.c"));
		Str8ListAppend(&Shared->Arena, &SourceFiles, Str8("build/embedded_fonts.c"));
		AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("source"));
		AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("source/darwin"));
		AppendSourceFilesInFolder(&Shared->Arena, &SourceFiles, Str8("external"));

		Str8ListAppend(&Shared->Arena, &ExtraLinkFlags,
			Str8("-framework Cocoa -framework Metal -framework QuartzCore "));

		BuildOptions = (build_options){
			.Compiler          = COMPILER_PLATFORM_DEFAULT,
			.OutputName        = Str8("micro-mayday"),
			.OutputType        = OUTPUT_EXECUTABLE,
			.SourceFiles       = SourceFiles,
			.IncludePaths      = IncludePaths,
			.ExtraLinkFlags    = ExtraLinkFlags,
			.ExtraCompileFlags = ExtraCompileFlags,
			.WorkingDirectory  = GetExecutableDirectory(&Arena),
			.OutputPath        = Str8("build/"),
			.Optimise          = OptimisedBuild,
			.FullRebuild       = Rebuild,
		};

		mkdir("build", 0755);

		{
			str8_list FontPaths = {0};
			Str8ListAppend(&Arena, &FontPaths, Str8("fonts/BerkeleyMono-Regular.ttf"));
			Str8ListAppend(&Arena, &FontPaths, Str8("fonts/lucide.ttf"));

			str8_list FontNames = {0};
			Str8ListAppend(&Arena, &FontNames, Str8("G_BerkeleyMono"));
			Str8ListAppend(&Arena, &FontNames, Str8("G_Lucide"));

			GenerateEmbedHeader(&Arena, Str8("build"));
			EmbedFiles(&Arena, FontPaths, Str8("build/embedded_fonts.c"), FontNames);
		}

		EmitCompileCommandsJson(&Arena, BuildOptions);
	}

	BroadcastToAllLanes(WaveInfo, BuildOptions);
	BroadcastToAllLanes(WaveInfo, PrintHelpMenu);
	BroadcastToAllLanes(WaveInfo, ShouldRun);

	b32 FailedToBuild = False;
	if(!PrintHelpMenu)
	{
		FailedToBuild = Build(WaveInfo, &Arena, BuildOptions);
	}

	if(WaveIsFirstLane(WaveInfo) && ShouldRun && !FailedToBuild)
	{
		str8 PathToExecutable = Str8PushF(&Arena, "%.*s%.*s",
			Str8ToPrintfArgs(BuildOptions.OutputPath),
			Str8ToPrintfArgs(BuildOptions.OutputName));
		printf("\nStarting the-thing...\n\n");
		b32 Outcome = False;
		ExecuteCommand(&Arena, PathToExecutable, &Outcome);
	}

	return 0;
}

int main(s32 ArgCount, char **Args)
{
	memory_arena SharedArena = {0};
	SharedArena.Start = MemoryReserve(GigaBytes(64));

	shared_wave_info *Shared = (shared_wave_info*)ArenaPush(&SharedArena, sizeof(shared_wave_info));
	Shared->ArgCount = ArgCount;
	Shared->Args = Args;
	Shared->Arena = SharedArena;
	Dispatch(Shared, GetNumberOfCPUs());
}
