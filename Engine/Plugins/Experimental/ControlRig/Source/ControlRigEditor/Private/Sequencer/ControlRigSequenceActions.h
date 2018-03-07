// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

/**
 * Implements actions for UControlRigSequence assets.
 */
class FControlRigSequenceActions
	: public FAssetTypeActions_Base
{
public:

	/**
	 * Creates and initializes a new instance.
	 */
	FControlRigSequenceActions();

public:
	
	// IAssetTypeActions interface

	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual bool ShouldForceWorldCentric() override;
	virtual bool CanLocalize() const override { return false; }
};
