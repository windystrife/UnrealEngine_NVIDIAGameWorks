// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include "hlslcc.h"
#include "glsl/ir_gen_glsl.h"
static bool Rebase = false;

/** Debug output. */
static void dprintf(const char* Format, ...)
{
	const int BufSize = (1 << 20);
	static char* Buf = 0;
	va_list Args;
	int Count;

	if (Buf == 0)
	{
		Buf = (char*)malloc(BufSize);
	}

	va_start(Args, Format);
	Count = vsnprintf_s(Buf, BufSize, _TRUNCATE, Format, Args);
	va_end(Args);

	if (Count < -1)
	{
		// Overflow, add a line feed and null terminate the string.
		Buf[sizeof(Buf) - 2] = '\n';
		Buf[sizeof(Buf) - 1] = 0;
	}
	else
	{
		// Make sure the string is null terminated.
		Buf[Count] = 0;
	}

	OutputDebugString(Buf);
	fprintf(stdout, "%s", Buf);
}

/** Windows does not define the va_copy macro. */
#ifndef va_copy
#ifdef __va_copy
#define va_copy(dest, src) __va_copy((dest), (src))
#else
#define va_copy(dest, src) (dest) = (src)
#endif
#endif

/**
 * Like sprintf but allocates a buffer large enough to hold the formatted string.
 * The caller is responsible for freeing the memory.
 */
static char* _asprintf(const char* fmt, ...)
{
	size_t size;
	va_list args, tmp_args;
	char* str = 0;

	va_start(args, fmt);
	va_copy(tmp_args, args);
	size = _vscprintf(fmt, tmp_args) + 1;
	va_end(tmp_args);
	if (size > 0)
	{
		str = (char*)malloc(size);
		if (vsnprintf_s(str, size, _TRUNCATE, fmt, args) < 0)
		{
			str[size-1] = 0;
		}
	}
	va_end(args);

	return str;
}

/** Directories in which to look for tests. */
const char* GTestDirectories[] = {"../tests/", "../../tests/", "../../../tests/", 0 };
const char* GTestDirectory = NULL;

/**
 * Loads a file to a string in memory. The caller is responsible for freeing the
 * memory. Returns NULL if the file cannot be opened.
 */
char* LoadFileToString(const char* Filename)
{
	char* Source = 0;
	FILE* fp = 0;
	fopen_s(&fp, Filename, "r");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		size_t FileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		Source = (char*)malloc(FileSize + 1);
		size_t NumRead = fread(Source, 1, FileSize, fp);
		Source[NumRead] = 0;
		fclose(fp);
	}
	return Source;
}

/**
 * Runs a test with the specified base filename. This will load
 * BaseFilename.hlsl from the test directory, compile it, and compare the output
 * with the contents of BaseFilename.out in the test directory. The test passes
 * the compilation output exactly matches the .out file. If the test fails, the
 * compilation output is written to BaseFilename.fail in the tests directory.
 *
 * true is returned if the test was run successfally, otherwise false is returned.
 */
bool RunTest(const char* BaseFilename)
{
	char Filename[MAX_PATH];
	EHlslShaderFrequency ShaderFrequency = HSF_InvalidFrequency;
	char* HlslSource = NULL;
	char* CompiledGlsl = NULL;
	char* ErrorLog = NULL;
	bool bPass = true;

	dprintf("Running %s...", BaseFilename);
	if (BaseFilename[0] == 'v' && BaseFilename[1] == 's' && BaseFilename[2] == '_')
	{
		ShaderFrequency = HSF_VertexShader;
	}
	else if (BaseFilename[0] == 'p' && BaseFilename[1] == 's' && BaseFilename[2] == '_')
	{
		ShaderFrequency = HSF_PixelShader;
	}
	else if (BaseFilename[0] == 'g' && BaseFilename[1] == 's' && BaseFilename[2] == '_')
	{
		ShaderFrequency = HSF_GeometryShader;
	}
	else if (BaseFilename[0] == 'h' && BaseFilename[1] == 's' && BaseFilename[2] == '_')
	{
		ShaderFrequency = HSF_HullShader;
	}
	else if (BaseFilename[0] == 'd' && BaseFilename[1] == 's' && BaseFilename[2] == '_')
	{
		ShaderFrequency = HSF_DomainShader;
	}
	else if (BaseFilename[0] == 'c' && BaseFilename[1] == 's' && BaseFilename[2] == '_')
	{
		ShaderFrequency = HSF_ComputeShader;
	}

	if (ShaderFrequency == HSF_InvalidFrequency)
	{
		dprintf("test must start with vs_, ps_, gs_, or cs\n");
		return false;
	}

	_snprintf_s(Filename, MAX_PATH, "%s%s.hlsl", GTestDirectory, BaseFilename);
	HlslSource = LoadFileToString(Filename);
	if (HlslSource == NULL)
	{
		dprintf("can't open HLSL source '%s'\n", Filename);
		return false;
	}

	const int testConfigs = 2;
	struct TestConfig
	{
		EHlslCompileTarget target;
		const char* extension;
		const char* label;
	};
	TestConfig targets[testConfigs] = 
	{
		{ HCT_FeatureLevelSM4, "", "GLSL 1.50" },
		{ HCT_FeatureLevelSM5, "_gl4", "GLSL 4.30" }
	};

	dprintf( "\n");

	for ( int i = 0; i < testConfigs; i++ )
	{
		char OutFilename[MAX_PATH];
		dprintf( "    %s...", targets[i].label);
		
		unsigned int CCFlags = HLSLCC_PackUniforms | HLSLCC_DX11ClipSpace;
		FGlslCodeBackend GlslBackEnd(CCFlags);
		//@todo-rco: Fix me!
		FGlslLanguageSpec GlslLanguageSpec(false);
		
		HlslCrossCompile(
			Filename,
			HlslSource,
			"TestMain",
			ShaderFrequency,
			&GlslBackEnd,
			&GlslLanguageSpec,
			CCFlags,
			targets[i].target,
			&CompiledGlsl,
			&ErrorLog
			);

		char* TestOutput = _asprintf(
			"----------------------------------------------------------------------\n"
			"%s\n----------------------------------------------------------------------\n%s",
			ErrorLog ? ErrorLog : "no errors",
			CompiledGlsl ? CompiledGlsl : "no compiler output"
			);

		bool bTargetPass = false;
		bool bRebase = Rebase;
		if (TestOutput)
		{
			_snprintf_s(OutFilename, MAX_PATH, "%s%s%s.out", GTestDirectory, BaseFilename, targets[i].extension);
			char* ExpectedOutput = LoadFileToString(OutFilename);
			if (ExpectedOutput)
			{
				if (strcmp(TestOutput, ExpectedOutput) == 0)
				{
					dprintf("succeeded\n");
					bTargetPass = true;
				}
				else
				{
					// check to see if it differs by just the version
					const char* TrimmedTestOutput;
					const char* TrimmedExpectedOutput;
					TrimmedTestOutput = strstr( TestOutput, "// Compiled by HLSLCC");
					TrimmedExpectedOutput = strstr( ExpectedOutput, "// Compiled by HLSLCC");
					TrimmedTestOutput = TrimmedTestOutput ? strstr( TrimmedTestOutput, "\n") : NULL;
					TrimmedExpectedOutput = TrimmedExpectedOutput ? strstr( TrimmedExpectedOutput, "\n") : NULL;

					if ( TrimmedTestOutput && TrimmedExpectedOutput &&
						strcmp(TrimmedTestOutput, TrimmedExpectedOutput) == 0 )
					{
						dprintf("conditional success, update version numbers\n");
						bTargetPass = true;
						bRebase = bRebase & true;
					}
					else
					{
						dprintf("failed\n", Filename);
					}
				}
				free(ExpectedOutput);
			}
			else
			{
				dprintf("can't open expected output '%s'\n", OutFilename);
				// don't rebase files that don't have a former version
				bRebase = false;
			}
			if (!bTargetPass || bRebase)
			{
				FILE* fp = NULL;
				_snprintf_s(OutFilename, MAX_PATH, "%s%s%s.%s", GTestDirectory, BaseFilename, targets[i].extension, bRebase ? "out" : "fail");
				fopen_s(&fp, OutFilename, "w");
				if (fp)
				{
					fprintf(fp, "%s", TestOutput);
					fclose(fp);
					dprintf("\toutput written to '%s'\n", OutFilename);
				}
				else
				{
					dprintf("\toutput couldn't be written to '%s':\n%s\n", OutFilename, TestOutput);
				}
			}
			free(TestOutput);
		}
		if (CompiledGlsl)
		{
			free(CompiledGlsl);
		}
		if (ErrorLog)
		{
			free(ErrorLog);
		}

		//pass means all targets passed
		bPass &= bTargetPass;
	}

	return bPass;
}

static BOOL DirectoryExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

int main( int argc, char **argv)
{
	WIN32_FIND_DATA FindData;
	HANDLE FindHandle = INVALID_HANDLE_VALUE;
	char Buf[MAX_PATH];
	int NumTests = 0;
	int NumPassed = 0;

	if ( argc > 1 && strcmp( argv[1], "-rebase") == 0)
	{
		Rebase = true;
	}

	for( int i = 0; GTestDirectories[i] != 0; ++i )
	{
		if( DirectoryExists( GTestDirectories[i] ) )
		{
			GTestDirectory = GTestDirectories[i];
			break;
		}
	}

	if( !GTestDirectory )
	{
		dprintf("tests directory not found\n");
		return -1;
	}

	_snprintf_s(Buf, MAX_PATH, "%s*", GTestDirectory);
	FindHandle = FindFirstFile(Buf, &FindData);
	if (FindHandle == INVALID_HANDLE_VALUE)
	{
		dprintf("tests directory not found\n");
		return -1;
	}

	do 
	{
		if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			char* Extension = FindData.cFileName;
			do
			{
				Extension = strstr(Extension, ".hlsl");
			}
			while (Extension && Extension[5] != 0);

			if (Extension && Extension - FindData.cFileName < MAX_PATH - 1)
			{
				strncpy_s(Buf, FindData.cFileName, Extension - FindData.cFileName);
				Buf[Extension - FindData.cFileName] = 0;
				if (RunTest(Buf))
				{
					NumPassed++;
				}
				NumTests++;
			}
		}
	} while (FindNextFile(FindHandle, &FindData) != 0);

	dprintf("%d of %d tests passed\n", NumPassed, NumTests);

	return (NumPassed == NumTests) ? 0 : -1;
}
