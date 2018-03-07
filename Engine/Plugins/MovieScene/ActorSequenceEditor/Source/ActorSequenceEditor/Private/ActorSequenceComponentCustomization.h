// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Widgets/Layout/SBox.h"
#include "Input/Reply.h"
#include "UObject/WeakObjectPtr.h"
#include "Framework/Docking/TabManager.h"

class IDetailsView;
class UActorSequence;
class UActorSequenceComponent;
class ISequencer;
class FSCSEditorTreeNode;
class IPropertyUtilities;

class FActorSequenceComponentCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	FReply InvokeSequencer();
	UActorSequence* GetActorSequence() const;

	TWeakObjectPtr<UActorSequenceComponent> WeakSequenceComponent;
	TWeakPtr<FTabManager> WeakTabManager;
	TSharedPtr<SBox> InlineSequencer;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};