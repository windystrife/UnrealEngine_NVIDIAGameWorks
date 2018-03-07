// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCurveInterfaceKeyEditStructCustomization.h"

#include "GenericKeyArea.h"

#define LOCTEXT_NAMESPACE "MovieSceneCurveInterfaceKeyEditStructCustomization"

TSharedRef<IDetailCustomization> FMovieSceneCurveInterfaceKeyEditStructCustomization::MakeInstance()
{
	return MakeShared<FMovieSceneCurveInterfaceKeyEditStructCustomization>();
}

void FMovieSceneCurveInterfaceKeyEditStructCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TSharedPtr<FStructOnScope>> Structs;
	DetailBuilder.GetStructsBeingCustomized(Structs);

	if (Structs.Num() != 1)
	{
		return;
	}

	const UStruct* StructType = Structs[0]->GetStruct();
	if (!StructType || StructType != FMovieSceneCurveInterfaceKeyEditStruct::StaticStruct())
	{
		return;
	}

	auto* StructValue = (FMovieSceneCurveInterfaceKeyEditStruct*)Structs[0]->GetStructMemory();
	if (!StructValue)
	{
		return;
	}

	StructValue->EditInterface->Extend(StructValue->KeyHandle, DetailBuilder);
}

#undef LOCTEXT_NAMESPACE
