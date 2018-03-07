// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetThumbnail.h"

class SAssetFamilyShortcutBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetFamilyShortcutBar)
	{}
		
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TSharedRef<class IAssetFamily>& InAssetFamily);

private:
	/** The thumbnail pool for displaying asset shortcuts */
	TSharedPtr<class FAssetThumbnailPool> ThumbnailPool;
};
