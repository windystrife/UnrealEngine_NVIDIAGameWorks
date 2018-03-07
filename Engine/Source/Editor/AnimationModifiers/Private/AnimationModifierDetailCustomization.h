// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Reply.h"
class SButton;
class UAnimationModifier;

/** Animation modifier detail customization */
class FAnimationModifierDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShareable(new FAnimationModifierDetailCustomization());  }

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
protected:
	FReply OnUpdateRevisionButtonClicked();
protected:
	TSharedPtr<SButton> UpdateRevisionButton;
	UAnimationModifier* ModifierInstance;
};