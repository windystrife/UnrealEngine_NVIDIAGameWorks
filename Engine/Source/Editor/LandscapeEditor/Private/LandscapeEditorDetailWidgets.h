// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

// A toolbar whose combo buttons have an additional small text label describing the current selection
class FToolSelectorBuilder : public FToolBarBuilder
{
public:
	FToolSelectorBuilder(TSharedPtr< const FUICommandList > InCommandList, FMultiBoxCustomization InCustomization, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>(), EOrientation Orientation = Orient_Horizontal);

	void AddComboButton(const FUIAction& InAction, const FOnGetContent& InMenuContentGenerator, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InSmallText, const TAttribute<FText>& InToolTipOverride, const TAttribute<FSlateIcon>& InIcon);
};

// A menu composed of toolbar-style buttons
class FToolMenuBuilder : public FMenuBuilder
{
public:
	FToolMenuBuilder(const bool bInShouldCloseWindowAfterMenuSelection, TSharedPtr< const FUICommandList > InCommandList, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>(), const bool bCloseSelfOnly = false);

	void AddToolButton(const TSharedPtr<const FUICommandInfo> InCommand, FName InExtensionHook = NAME_None, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>());
};
