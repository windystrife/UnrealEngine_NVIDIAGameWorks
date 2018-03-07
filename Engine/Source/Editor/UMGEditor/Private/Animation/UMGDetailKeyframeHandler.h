// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WidgetBlueprintEditor.h"
#include "Editor/PropertyEditor/Public/IDetailKeyframeHandler.h"

class IPropertyHandle;

class FUMGDetailKeyframeHandler : public IDetailKeyframeHandler
{
public:
	FUMGDetailKeyframeHandler( TSharedPtr<class FWidgetBlueprintEditor> InBlueprintEditor );

	virtual bool IsPropertyKeyable(UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const override;

	virtual bool IsPropertyKeyingEnabled() const override;

	virtual void OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle) override;

private:
	TWeakPtr<class FWidgetBlueprintEditor> BlueprintEditor;
};
