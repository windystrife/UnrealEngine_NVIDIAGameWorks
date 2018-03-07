// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Interfaces/IProjectTargetPlatformEditorModule.h"

struct FSlateBrush;
enum class ECheckBoxState : uint8;

class SProjectTargetPlatformSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProjectTargetPlatformSettings)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:
	/** Generate an entry for the given platform information */
	TSharedRef<SWidget> MakePlatformRow(const FText& DisplayName, const FName PlatformName, const FSlateBrush* Icon) const;

	/** Check to see if the "enabled" checkbox should be checked for this platform */
	ECheckBoxState HandlePlatformCheckBoxIsChecked(const FName PlatformName) const;

	/** Check to see if the "enabled" checkbox should be enabled for this platform */
	bool HandlePlatformCheckBoxIsEnabled(const FName PlatformName) const;

	/** Handle the "enabled" checkbox state being changed for this platform */
	void HandlePlatformCheckBoxStateChanged(ECheckBoxState InState, const FName PlatformName) const;

	TArray<const PlatformInfo::FPlatformInfo*> AvailablePlatforms;
};
