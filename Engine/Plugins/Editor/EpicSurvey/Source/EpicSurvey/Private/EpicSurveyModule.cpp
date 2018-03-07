// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IEpicSurveyModule.h"
#include "EpicSurvey.h"
#include "Interfaces/IMainFrameModule.h"

class FEpicSurveyModule : public IEpicSurveyModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool PromptSurvey( const FGuid& SurveyIdentifier ) override;


private:

	void Initialize( TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow );

private:
	TSharedPtr< FEpicSurvey > EpicSurvey;
};

IMPLEMENT_MODULE(FEpicSurveyModule, EpicSurvey)

void FEpicSurveyModule::StartupModule()
{
	if ( !IsRunningCommandlet() )
	{
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.OnMainFrameCreationFinished().AddRaw( this, &FEpicSurveyModule::Initialize );
	}
}

void FEpicSurveyModule::Initialize( TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow )
{
	if( !bIsNewProjectWindow )
	{
		EpicSurvey = FEpicSurvey::Create();

		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.OnMainFrameCreationFinished().RemoveAll(this);
	}
}

void FEpicSurveyModule::ShutdownModule()
{
	EpicSurvey.Reset();

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.OnMainFrameCreationFinished().RemoveAll(this);
	}
}

bool FEpicSurveyModule::PromptSurvey( const FGuid& SurveyIdentifier )
{
	if ( EpicSurvey.IsValid() )
	{
		return EpicSurvey->PromptSurvey( SurveyIdentifier );
	}

	return false;
}
