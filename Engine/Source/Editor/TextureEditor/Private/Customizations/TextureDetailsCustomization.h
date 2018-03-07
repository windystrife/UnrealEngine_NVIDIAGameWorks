// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/WeakObjectPtr.h"
#include "Types/SlateEnums.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;

class FTextureDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	FTextureDetails()
		: bIsUsingSlider(false)
	{}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	TOptional<int32> OnGetMaxTextureSize() const;
	void OnMaxTextureSizeChanged(int32 NewValue);
	void OnMaxTextureSizeCommitted(int32 NewValue, ETextCommit::Type CommitInfo);
	void OnBeginSliderMovement();
	void OnEndSliderMovement(int32 NewValue);
	bool CanEditMaxTextureSize() const;
	void CreateMaxTextureSizeMessage() const;
	void OnPowerOfTwoModeChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	bool CanEditPowerOfTwoMode(int32 NewPowerOfTwoMode) const;
	void CreatePowerOfTwoModeMessage() const;

	TSharedPtr<IPropertyHandle> MaxTextureSizePropertyHandle;
	TSharedPtr<IPropertyHandle> PowerOfTwoModePropertyHandle;
	TArray<TSharedPtr<FString>> PowerOfTwoModeComboBoxList;
	TWeakObjectPtr<UObject> TextureBeingCustomized;
	TSharedPtr<STextComboBox> TextComboBox;
	bool bIsUsingSlider;
};

