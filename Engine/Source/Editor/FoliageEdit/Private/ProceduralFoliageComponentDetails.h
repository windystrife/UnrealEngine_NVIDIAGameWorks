// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UProceduralFoliageComponent;

class FProceduralFoliageComponentDetails : public IDetailCustomization
{
public:
	virtual ~FProceduralFoliageComponentDetails(){};

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder );
private:
	FReply OnResimulateClicked();
	bool IsResimulateEnabled() const;
	FText GetResimulateTooltipText() const;
private:
	TArray< TWeakObjectPtr<class UProceduralFoliageComponent> > SelectedComponents;

};
