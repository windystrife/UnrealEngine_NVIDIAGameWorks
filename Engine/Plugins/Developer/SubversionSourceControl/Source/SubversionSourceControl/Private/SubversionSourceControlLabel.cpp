// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlLabel.h"
#include "Modules/ModuleManager.h"
#include "SubversionSourceControlState.h"
#include "SubversionSourceControlModule.h"
#include "XmlFile.h"
#include "SubversionSourceControlUtils.h"

const FString& FSubversionSourceControlLabel::GetName() const
{
	return Name;
}

bool FSubversionSourceControlLabel::GetFileRevisions( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> >& OutRevisions ) const
{
	SubversionSourceControlUtils::CheckFilenames(InFiles);

	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();

	bool bCommandOK = true;
	for(auto Iter(InFiles.CreateConstIterator()); Iter; Iter++)
	{
		TArray<FString> Files;
		Files.Add(*Iter);

		TArray<FXmlFile> ResultsXml;
		TArray<FString> Parameters;
		TArray<FString> ErrorMessages;
		SubversionSourceControlUtils::FHistoryOutput History;

		//limit to last change
		Parameters.Add(TEXT("--limit 1"));
		// output all properties
		Parameters.Add(TEXT("--with-all-revprops"));
		// we want to view over merge boundaries
		Parameters.Add(TEXT("--use-merge-history"));
		// we want all the output!
		Parameters.Add(TEXT("--verbose"));
		// limit the range of revisions up to the one the tag specifies
		Parameters.Add(FString::Printf(TEXT("--revision %i:0"), Revision));
		
		bCommandOK &= SubversionSourceControlUtils::RunCommand(TEXT("log"), Files, Parameters, ResultsXml, ErrorMessages, Provider.GetUserName());
		SubversionSourceControlUtils::ParseLogResults(*Iter, ResultsXml, Provider.GetUserName(), History);

		// add the first (should be only) result
		if(History.Num() > 0)
		{
			check(History.Num() == 1);
			for(auto RevisionIter(History.CreateIterator().Value().CreateIterator()); RevisionIter; RevisionIter++)
			{
				OutRevisions.Add(*RevisionIter);
			}
		}
	}

	return bCommandOK;
}

bool FSubversionSourceControlLabel::Sync( const TArray<FString>& InFilenames ) const
{
	SubversionSourceControlUtils::CheckFilenames(InFilenames);

	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();

	TArray<FString> Results;
	TArray<FString> Parameters;
	TArray<FString> ErrorMessages;
	Parameters.Add(FString::Printf(TEXT("--revision %i"), Revision));

	bool bCommandOK = SubversionSourceControlUtils::RunCommand(TEXT("update"), InFilenames, Parameters, Results, ErrorMessages, Provider.GetUserName());

	// also update cached state
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		TArray<FSubversionSourceControlState> OutStates;
		StatusParameters.Add(TEXT("--verbose"));
		StatusParameters.Add(TEXT("--show-updates"));

		bCommandOK &= SubversionSourceControlUtils::RunCommand(TEXT("status"), InFilenames, StatusParameters, ResultsXml, ErrorMessages, Provider.GetUserName());
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, ErrorMessages, Provider.GetUserName(), Provider.GetWorkingCopyRoot(), OutStates);
		SubversionSourceControlUtils::UpdateCachedStates(OutStates);
	}

	return bCommandOK;
}
