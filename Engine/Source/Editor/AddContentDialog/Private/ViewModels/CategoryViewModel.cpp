// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/CategoryViewModel.h"
#include "IContentSource.h"
#include "AddContentDialogStyle.h"

#define LOCTEXT_NAMESPACE "ContentSourceViewModel"

FCategoryViewModel::FCategoryViewModel()
{
	Category = EContentSourceCategory::Unknown;
	Initialize();
}

FCategoryViewModel::FCategoryViewModel(EContentSourceCategory InCategory)
{
	Category = InCategory;
	Initialize();
}

FText FCategoryViewModel::GetText() const
{
	return Text;
}

const FSlateBrush* FCategoryViewModel::GetIconBrush() const
{
	return IconBrush;
}

uint32 FCategoryViewModel::GetTypeHash() const
{
	return (uint32)Category;
}

void FCategoryViewModel::Initialize()
{
	switch (Category)
	{
	case EContentSourceCategory::BlueprintFeature:
		Text = LOCTEXT("BlueprintFeature", "Blueprint Feature");
		IconBrush = FAddContentDialogStyle::Get().GetBrush("AddContentDialog.BlueprintFeatureCategory");
		SortID = 0;
		break;
	case EContentSourceCategory::CodeFeature:
		Text = LOCTEXT("CodeFeature", "C++ Feature");
		IconBrush = FAddContentDialogStyle::Get().GetBrush("AddContentDialog.CodeFeatureCategory");
		SortID = 1;
		break;
	case EContentSourceCategory::Content:
		Text = LOCTEXT("ContentPacks", "Content Packs");
		IconBrush = FAddContentDialogStyle::Get().GetBrush("AddContentDialog.ContentPackCategory");
		SortID = 2;
		break;
	default:
		Text = LOCTEXT("Miscellaneous", "Miscellaneous");
		IconBrush = FAddContentDialogStyle::Get().GetBrush("AddContentDialog.UnknownCategory");
		SortID = 3;
		break;
	}
}

#undef LOCTEXT_NAMESPACE
