// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "BlankProgram.h"

#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlankProgram, Log, All);

IMPLEMENT_APPLICATION(BlankProgram, "BlankProgram");

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	GEngineLoop.PreInit(ArgC, ArgV);
	UE_LOG(LogBlankProgram, Display, TEXT("Hello World"));
	return 0;
}
