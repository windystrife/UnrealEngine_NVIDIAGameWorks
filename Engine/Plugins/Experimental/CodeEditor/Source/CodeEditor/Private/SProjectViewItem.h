// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SProjectViewItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProjectViewItem) {}

	SLATE_ARGUMENT(FName, IconName)

	SLATE_ARGUMENT(FText, Text)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
