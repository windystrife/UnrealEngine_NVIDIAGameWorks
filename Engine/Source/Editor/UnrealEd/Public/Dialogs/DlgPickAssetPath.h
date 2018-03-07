// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "DlgPickAssetPath"

class SDlgPickAssetPath : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SDlgPickAssetPath)
	{
	}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, DefaultAssetPath)
	SLATE_END_ARGS()

	UNREALED_API SDlgPickAssetPath()
	:	UserResponse(EAppReturnType::Cancel)
	{
	}

	UNREALED_API void Construct(const FArguments& InArgs);

	/** Displays the dialog in a blocking fashion */
	UNREALED_API EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	UNREALED_API const FText& GetAssetPath();

	/** Gets the resulting asset name */
	UNREALED_API const FText& GetAssetName();

	/** Gets the resulting full asset path (path+'/'+name) */
	UNREALED_API FText GetFullAssetPath();

protected:
	void OnPathChange(const FString& NewPath);
	void OnNameChange(const FText& NewName, ETextCommit::Type CommitInfo);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);

	bool ValidatePackage();

	EAppReturnType::Type UserResponse;
	FText AssetPath;
	FText AssetName;
};

#undef LOCTEXT_NAMESPACE
