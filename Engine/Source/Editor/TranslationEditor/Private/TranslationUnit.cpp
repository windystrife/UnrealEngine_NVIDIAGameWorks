// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "TranslationUnit.h"
#include "UObject/UnrealType.h"

UTranslationUnit::UTranslationUnit( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{

}

void UTranslationUnit::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	TranslationUnitPropertyChangedEvent.Broadcast(Name);
}
