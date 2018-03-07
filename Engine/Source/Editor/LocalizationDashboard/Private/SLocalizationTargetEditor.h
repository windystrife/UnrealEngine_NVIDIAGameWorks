// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "PropertyEditorDelegates.h"

class ULocalizationTarget;
class ULocalizationTargetSet;

class SLocalizationTargetEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLocalizationTargetEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, ULocalizationTargetSet* const InProjectSettings, ULocalizationTarget* const InTarget, const FIsPropertyEditingEnabled& IsPropertyEditingEnabled);
};

