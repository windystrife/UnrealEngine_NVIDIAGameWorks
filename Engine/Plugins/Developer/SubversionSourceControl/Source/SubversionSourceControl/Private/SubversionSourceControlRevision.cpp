// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlRevision.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SubversionSourceControlModule.h"
#include "SubversionSourceControlUtils.h"
#include "Logging/MessageLog.h"

#define LOCTEXT_NAMESPACE "SubversionSourceControl"

bool FSubversionSourceControlRevision::Get( FString& InOutFilename ) const
{
	SubversionSourceControlUtils::CheckFilename(Filename);

	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();

	TArray<FString> Results;
	TArray<FString> Parameters;
	TArray<FString> ErrorMessages;

	// Make temp filename to export to
	FString RevString = (RevisionNumber < 0) ? TEXT("HEAD") : FString::Printf(TEXT("%d"), RevisionNumber);
	FString AbsoluteFileName;
	if(InOutFilename.Len() > 0)
	{
		AbsoluteFileName = InOutFilename;
	}
	else
	{
		// create the diff dir if we don't already have it (SVN wont)
		IFileManager::Get().MakeDirectory(*FPaths::DiffDir(), true);

		static int32 TempFileCount = 0;
		FString TempFileName = FString::Printf(TEXT("%sTemp-%d-Rev-%s-%s"), *FPaths::DiffDir(), TempFileCount++, *RevString, *FPaths::GetCleanFilename(Filename));
		AbsoluteFileName = FPaths::ConvertRelativePathToFull(TempFileName);
	}

	Parameters.Add(FString(TEXT("--revision ")) + RevString);
	Parameters.Add(TEXT("--force"));

	TArray<FString> Files;
	Files.Add(Filename);
	Files.Add(AbsoluteFileName);

	if(SubversionSourceControlUtils::RunCommand(TEXT("export"), Files, Parameters, Results, ErrorMessages, Provider.GetUserName()))
	{
		InOutFilename = AbsoluteFileName;
		return true;
	}
	else
	{
		for(auto Iter(ErrorMessages.CreateConstIterator()); Iter; Iter++)
		{
			FMessageLog("SourceControl").Error(FText::FromString(*Iter));
		}
	}
	return false;
}

static bool IsWhiteSpace(TCHAR InChar)
{
	return InChar == TCHAR(' ') || InChar == TCHAR('\t');
}

static FString NextToken( const FString& InString, int32& InIndex, bool bIncludeWhiteSpace )
{
	FString Result;

	// find first non-whitespace char
	while(!bIncludeWhiteSpace && IsWhiteSpace(InString[InIndex]) && InIndex < InString.Len())
	{
		InIndex++;
	}

	// copy non-whitespace chars
	while(((!IsWhiteSpace(InString[InIndex]) || bIncludeWhiteSpace) && InIndex < InString.Len()))
	{
		Result += InString[InIndex];
		InIndex++;
	}

	return Result;
}

static void ParseBlameResults( const TArray<FString>& InResults, TArray<FAnnotationLine>& OutLines )
{
	// each line is revision number <whitespace> username <whitespace> change
	for(int32 ResultIndex = 0; ResultIndex < InResults.Num(); ResultIndex++)
	{
		const FString& Result = InResults[ResultIndex];
		
		int32 Index = 0;
		FString RevisionString = NextToken(Result, Index, false);
		FString UserString = NextToken(Result, Index, false);

		// start at Index + 1 here so we don't include an extra space form the SVN output
		FString TextString = NextToken(Result, ++Index, true);

		OutLines.Add(FAnnotationLine(FCString::Atoi(*RevisionString), UserString, TextString));
	}
}

bool FSubversionSourceControlRevision::GetAnnotated( TArray<FAnnotationLine>& OutLines ) const
{
	SubversionSourceControlUtils::CheckFilename(Filename);

	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();

	TArray<FString> Results;
	TArray<FString> Parameters;
	TArray<FString> ErrorMessages;

	// Make temp filename to export to
	FString RevString = (RevisionNumber < 0) ? TEXT("HEAD") : FString::Printf(TEXT("%d"), RevisionNumber);
	Parameters.Add(FString(TEXT("--revision ")) + RevString);
	Parameters.Add(TEXT("--use-merge-history"));

	TArray<FString> Files;
	Files.Add(Filename);

	if(SubversionSourceControlUtils::RunCommand(TEXT("blame"), Files, Parameters, Results, ErrorMessages, Provider.GetUserName()))
	{
		ParseBlameResults(Results, OutLines);
		return true;
	}
	else
	{
		for(auto Iter(ErrorMessages.CreateConstIterator()); Iter; Iter++)
		{
			FMessageLog("SourceControl").Error(FText::FromString(*Iter));
		}
	}

	return false;
}

bool FSubversionSourceControlRevision::GetAnnotated( FString& InOutFilename ) const
{
	TArray<FAnnotationLine> Lines;
	if(GetAnnotated(Lines))
	{
		FString FileBuffer;
		for(auto Iter(Lines.CreateConstIterator()); Iter; Iter++)
		{
			FileBuffer += FString::Printf( TEXT( "%8d %20s:\t%s\r\n" ), Iter->ChangeNumber, *Iter->UserName, *Iter->Line );
		}

		// Make temp filename to output to, or used a pass in one if there is one
		FString AbsoluteFileName;
		if(InOutFilename.Len() > 0)
		{
			AbsoluteFileName = InOutFilename;
		}
		else
		{
			FString RevString = (RevisionNumber < 0) ? TEXT("head") : FString::Printf(TEXT("%d"), RevisionNumber);
			static int32 TempFileCount = 0;
			FString TempFileName = FString::Printf(TEXT("%sAnnotated-%d-Rev-%s-%s"), *FPaths::DiffDir(), TempFileCount++, *RevString, *FPaths::GetCleanFilename(Filename));
			AbsoluteFileName = FPaths::ConvertRelativePathToFull(TempFileName);
		}

		if(!FFileHelper::SaveStringToFile(FileBuffer, *AbsoluteFileName, FFileHelper::EEncodingOptions::ForceAnsi))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add( TEXT("Filename"), FText::FromString(AbsoluteFileName) );
			FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("FailedToWrite", "Failed to write to file: {Filename}"), Arguments));
			return false;
		}

		InOutFilename = AbsoluteFileName;
		return true;
	}

	return false;
}

const FString& FSubversionSourceControlRevision::GetFilename() const
{
	return Filename;
}

int32 FSubversionSourceControlRevision::GetRevisionNumber() const
{
	return RevisionNumber;
}

const FString& FSubversionSourceControlRevision::GetRevision() const
{
	return Revision;
}

const FString& FSubversionSourceControlRevision::GetDescription() const
{
	return Description;
}

const FString& FSubversionSourceControlRevision::GetUserName() const
{
	return UserName;
}

const FString& FSubversionSourceControlRevision::GetClientSpec() const
{
	static FString EmptyString(TEXT(""));
	return EmptyString;
}

const FString& FSubversionSourceControlRevision::GetAction() const
{
	return Action;
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FSubversionSourceControlRevision::GetBranchSource() const
{
	return BranchSource;
}

const FDateTime& FSubversionSourceControlRevision::GetDate() const
{
	return Date;
}

int32 FSubversionSourceControlRevision::GetCheckInIdentifier() const
{
	// in SVN, revisions apply to the whole repository so (in Perforce terms) the revision *is* the changelist
	return RevisionNumber;
}

int32 FSubversionSourceControlRevision::GetFileSize() const
{
	return 0;
}

#undef LOCTEXT_NAMESPACE
