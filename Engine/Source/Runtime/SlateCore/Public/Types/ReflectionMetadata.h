// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Types/ISlateMetaData.h"
#include "Widgets/SWidget.h"

/**
 * Reflection meta-data that can be used by the widget reflector to determine
 * additional information about slate widgets that are constructed by UObject
 * classes for UMG.
 */
class FReflectionMetaData : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FReflectionMetaData, ISlateMetaData)

	FReflectionMetaData(FName InName, UClass* InClass, UObject* InSourceObject, const UObject* InAsset)
		: Name(InName)
		, Class(InClass)
		, SourceObject(InSourceObject)
		, Asset(InAsset)
	{
	}

	/** The name of the widget in the hierarchy */
	FName Name;
	
	/** The class the constructed the slate widget. */
	TWeakObjectPtr<UClass> Class;

	/** The UObject wrapper that creates the widget, this is expected to be a UWidget. */
	TWeakObjectPtr<UObject> SourceObject;

	/** The asset that owns the widget and is responsible for its specific existence. */
	TWeakObjectPtr<const UObject> Asset;

public:

	static FString GetWidgetDebugInfo(const SWidget* InWidget)
	{
		// UMG widgets have meta-data to help track them
		TSharedPtr<FReflectionMetaData> MetaData = InWidget->GetMetaData<FReflectionMetaData>();
		if ( MetaData.IsValid() && MetaData->Asset.Get() != nullptr )
		{
			const FName AssetName = MetaData->Asset->GetFName();
			const FName WidgetName = MetaData->Name;

			return FString::Printf(TEXT("%s [%s]"), *AssetName.ToString(), *WidgetName.ToString());
		}
		else
		{
			return InWidget->GetReadableLocation();
		}
	}
};
