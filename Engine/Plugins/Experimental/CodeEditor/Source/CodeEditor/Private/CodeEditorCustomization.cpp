// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CodeEditorCustomization.h"

UCodeEditorCustomization::UCodeEditorCustomization(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FCodeEditorControlCustomization& UCodeEditorCustomization::GetControl(const FName& ControlCustomizationName)
{
	static FCodeEditorControlCustomization Default;

	return Default;
}

const FCodeEditorTextCustomization& UCodeEditorCustomization::GetText(const FName& TextCustomizationName)
{
	static FCodeEditorTextCustomization Default;

	return Default;
}
