// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SFacialAnimationBulkImporter : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFacialAnimationBulkImporter)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	bool IsImportButtonEnabled() const;

	FReply HandleImportClicked();
};
