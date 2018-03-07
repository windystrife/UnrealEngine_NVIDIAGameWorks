// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CodeProjectFactory.h"
#include "CodeProject.h"


#define LOCTEXT_NAMESPACE "CodeEditor"


UCodeProjectFactory::UCodeProjectFactory(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UCodeProject::StaticClass();
}


UObject* UCodeProjectFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UCodeProject* NewCodeProject = NewObject<UCodeProject>(InParent, Class, Name, Flags);
	return NewCodeProject;
}


#undef LOCTEXT_NAMESPACE
