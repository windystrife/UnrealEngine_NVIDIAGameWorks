// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"

#define IOS_MAX_PATH 1024
#define CMD_LINE_MAX 16384

extern FString GSavedCommandLine;

class FIOSCommandLineHelper
{
	public:
		/**
		 * Merge the given commandline with GSavedCommandLinePortion, which may start with ?
		 * options that need to come after first token
		 */

		static void MergeCommandlineWithSaved(TCHAR CommandLine[16384])
		{
			// the saved commandline may be in the format ?opt?opt -opt -opt, so we need to insert it 
			// after the first token on the commandline unless the first token starts with a -, in which 
			// case use it at the start of the command line
			if (CommandLine[0] == '-' || CommandLine[0] == 0)
			{
				// handle the easy - case, just use the saved command line part as the start, in case it
				// started with a ?
				FString CombinedCommandLine = GSavedCommandLine + CommandLine;
				FCString::Strcpy(CommandLine, CMD_LINE_MAX, *CombinedCommandLine);
			}
			else
			{
				// otherwise, we need to get the first token from the command line and insert after
				TCHAR* Space = FCString::Strchr(CommandLine, ' ');
				if (Space == NULL)
				{
					// if there is only one token (no spaces), just append us after it
					FCString::Strcat(CommandLine, CMD_LINE_MAX, *GSavedCommandLine);
				}
				else
				{
					// save off what's after the space (include the space for pasting later)
					FString AfterSpace(Space);
					// copy the save part where the space was
					FCString::Strcpy(Space, CMD_LINE_MAX - (Space - CommandLine), *GSavedCommandLine);
					// now put back the 2nd and so on token
					FCString::Strcat(CommandLine, CMD_LINE_MAX, *AfterSpace);
				}
			}
		}

		static void InitCommandArgs(FString AdditionalCommandArgs)
		{
			// initialize the commandline
			FCommandLine::Set(TEXT(""));

			FString CommandLineFilePath = FString([[NSBundle mainBundle] bundlePath]) + TEXT("/ue4commandline.txt");

			// read in the command line text file (coming from UnrealFrontend) if it exists
			FILE* CommandLineFile = fopen(TCHAR_TO_UTF8(*CommandLineFilePath), "r");
			if(CommandLineFile)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Found ue4commandline.txt file") LINE_TERMINATOR);

				char CommandLine[CMD_LINE_MAX];
				fgets(CommandLine, ARRAY_COUNT(CommandLine) - 1, CommandLineFile);

				// chop off trailing spaces
				while (*CommandLine && isspace(CommandLine[strlen(CommandLine) - 1]))
				{
					CommandLine[strlen(CommandLine) - 1] = 0;
				}

				FCommandLine::Append(UTF8_TO_TCHAR(CommandLine));
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("No ue4commandline.txt [%s] found") LINE_TERMINATOR, *CommandLineFilePath);
			}

			FCommandLine::Append(*AdditionalCommandArgs);

			// now merge the GSavedCommandLine with the rest
			FCommandLine::Append(*GSavedCommandLine);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Combined iOS Commandline: %s") LINE_TERMINATOR, FCommandLine::Get());
		}

};

