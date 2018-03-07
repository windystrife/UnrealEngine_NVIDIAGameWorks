// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSlateBrush;

struct FTemplateCategory
{
	static FName BlueprintCategoryName, CodeCategoryName;

	/** Localised name of this category */
	FText Name;
	
	/** A description of the templates contained within this category */
	FText Description;

	/** A thumbnail to help identify this category (on the tab) */
	const FSlateBrush* Icon;

	/** A thumbnail to help identify this category (on the screenshot)*/
	const FSlateBrush* Image;

	/** A unique name for this category */
	FName Type;
};
