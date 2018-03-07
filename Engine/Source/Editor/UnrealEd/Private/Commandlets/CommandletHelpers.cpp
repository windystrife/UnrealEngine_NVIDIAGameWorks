// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/CommandletHelpers.h"

namespace CommandletHelpers
{
	FString BuildCommandletProcessArguments(const TCHAR* const CommandletName, const TCHAR* const ProjectPath, const TCHAR* const AdditionalArguments)
	{
		check(CommandletName && FCString::Strlen(CommandletName) != 0);

		FString Arguments;

		if (ProjectPath && FCString::Strlen(ProjectPath) != 0)
		{
			Arguments += FString(Arguments.IsEmpty() ? TEXT("") : TEXT(" ")) + ProjectPath;
		}

		const FString RunCommand = FString::Printf( TEXT("-run=%s"), CommandletName );
		Arguments += (Arguments.IsEmpty() ? "" : " ") + RunCommand;

		if (AdditionalArguments && FCString::Strlen(AdditionalArguments) != 0)
		{
			Arguments += FString(Arguments.IsEmpty() ? TEXT("") : TEXT(" ")) + AdditionalArguments;
		}

		return Arguments;
	}
}
