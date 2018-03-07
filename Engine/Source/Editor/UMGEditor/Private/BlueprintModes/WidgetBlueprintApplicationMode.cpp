// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintModes/WidgetBlueprintApplicationMode.h"
#include "BlueprintModes/WidgetBlueprintApplicationModes.h"

/////////////////////////////////////////////////////
// FWidgetBlueprintApplicationMode

FWidgetBlueprintApplicationMode::FWidgetBlueprintApplicationMode(TSharedPtr<FWidgetBlueprintEditor> InWidgetEditor, FName InModeName)
	: FBlueprintEditorApplicationMode(InWidgetEditor, InModeName, FWidgetBlueprintApplicationModes::GetLocalizedMode, false, false)
	, MyWidgetBlueprintEditor(InWidgetEditor)
{
}

UWidgetBlueprint* FWidgetBlueprintApplicationMode::GetBlueprint() const
{
	if ( FWidgetBlueprintEditor* Editor = MyWidgetBlueprintEditor.Pin().Get() )
	{
		return Editor->GetWidgetBlueprintObj();
	}
	else
	{
		return NULL;
	}
}

TSharedPtr<FWidgetBlueprintEditor> FWidgetBlueprintApplicationMode::GetBlueprintEditor() const
{
	return MyWidgetBlueprintEditor.Pin();
}
