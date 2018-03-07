// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/BaseToolkit.h"

class SFoliageEdit;

/**
 * Public interface to Foliage Edit mode.
 */
class FFoliageEdModeToolkit : public FModeToolkit
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Initializes the foliage mode toolkit */
	virtual void Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;

	void RefreshFullList();
	void NotifyFoliageTypeMeshChanged(class UFoliageType* FoliageType);

private:
	TSharedPtr< class SFoliageEdit > FoliageEdWidget;
};
