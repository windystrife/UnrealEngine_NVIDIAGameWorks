// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "IDetailsView.h"

class SPersonaDetails : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPersonaDetails) {}

	/** Optional content to display above the details panel */
	SLATE_ARGUMENT(TSharedPtr<SWidget>, TopContent)

	/** Optional content to display below the details panel */
	SLATE_ARGUMENT(TSharedPtr<SWidget>, BottomContent)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedPtr<class IDetailsView> DetailsView;
};
