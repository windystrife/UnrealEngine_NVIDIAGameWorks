// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SAnimationEditorViewportTabBody;
struct FTabId;

// Called when a blend profile is selected in the blend profile tab
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectBlendProfile, class UBlendProfile*);

// Called when the preview viewport is created
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCreateViewport, TWeakPtr<class SAnimationEditorViewportTabBody>);

// Called to invoke a specified tab
DECLARE_DELEGATE_OneParam(FOnInvokeTab, const FTabId&);

// Called to display a selected object in a details panel
DECLARE_DELEGATE_OneParam(FOnObjectsSelected, const TArray<UObject*>& /*InObjects*/);

// Called to display a selected object in a details panel
DECLARE_DELEGATE_OneParam(FOnObjectSelected, UObject* /*InObject*/);

// Called to replace the currently edited asset or open it (depending on context)
DECLARE_DELEGATE_OneParam(FOnOpenNewAsset, UObject* /* InAsset */);

// Called to get an object (used by the asset details panel)
DECLARE_DELEGATE_RetVal(UObject*, FOnGetAsset);
