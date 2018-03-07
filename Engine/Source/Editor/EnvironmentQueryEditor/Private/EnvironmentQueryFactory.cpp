// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryFactory.h"
#include "Misc/ConfigCacheIni.h"
#include "Settings/EditorExperimentalSettings.h"
#include "EnvironmentQuery/EnvQuery.h"

UEnvironmentQueryFactory::UEnvironmentQueryFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UEnvQuery::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UEnvironmentQueryFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UEnvQuery::StaticClass()));
	return NewObject<UEnvQuery>(InParent, Class, Name, Flags);
}

bool UEnvironmentQueryFactory::CanCreateNew() const
{
	if (GetDefault<UEditorExperimentalSettings>()->bEQSEditor)
	{
		return true;
	}

	// Check ini to see if we should enable creation
	bool bEnableEnvironmentQueryEd = false;
	GConfig->GetBool(TEXT("EnvironmentQueryEd"), TEXT("EnableEnvironmentQueryEd"), bEnableEnvironmentQueryEd, GEngineIni);
	return bEnableEnvironmentQueryEd;
}
