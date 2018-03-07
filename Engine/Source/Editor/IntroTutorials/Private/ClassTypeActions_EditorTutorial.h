// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ClassTypeActions_Base.h"

struct FAssetData;

class FClassTypeActions_EditorTutorial : public FClassTypeActions_Base
{
public:
	// IClassTypeActions Implementation
	virtual UClass* GetSupportedClass() const override;
	virtual TSharedPtr<class SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
};
