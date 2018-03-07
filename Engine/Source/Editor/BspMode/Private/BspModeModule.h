// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"
#include "IBspModeModule.h"

struct FSlateBrush;

struct FBspBuilderType
{
	FBspBuilderType(class UClass* InBuilderClass, const FText& InText, const FText& InToolTipText, const FSlateBrush* InIcon)
		: BuilderClass(InBuilderClass)
		, Text(InText)
		, ToolTipText(InToolTipText)
		, Icon(InIcon)
	{
	}

	/** The class of the builder brush */
	TWeakObjectPtr<UClass> BuilderClass;

	/** The name to be displayed for this builder */
	FText Text;

	/** The name to be displayed for this builder */
	FText ToolTipText;

	/** The icon to be displayed for this builder */
	const FSlateBrush* Icon;
};

class FBspModeModule : public IBspModeModule
{
public:
	
	/** IModuleInterface interface */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IBspModeModule interface */
	virtual void RegisterBspBuilderType( class UClass* InBuilderClass, const FText& InBuilderName, const FText& InBuilderTooltip, const FSlateBrush* InBuilderIcon ) override;
	virtual void UnregisterBspBuilderType( class UClass* InBuilderClass ) override;

	const TArray< TSharedPtr<FBspBuilderType> >& GetBspBuilderTypes();
	TSharedPtr<FBspBuilderType> FindBspBuilderType(UClass* InBuilderClass) const;

private:

	TArray< TSharedPtr<FBspBuilderType> > BspBuilderTypes;
};
