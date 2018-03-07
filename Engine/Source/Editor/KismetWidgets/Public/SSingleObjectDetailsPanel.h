// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/Commands/UICommandList.h"

class IDetailsView;

/////////////////////////////////////////////////////
// SSingleObjectDetailsPanel

class KISMETWIDGETS_API SSingleObjectDetailsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSingleObjectDetailsPanel) {}
		SLATE_ARGUMENT(TSharedPtr<class FUICommandList>, HostCommandList)
		SLATE_ARGUMENT(TSharedPtr<class FTabManager>, HostTabManager)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, bool bAutomaticallyObserveViaGetObjectToObserve = true, bool bAllowSearch = false);

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

protected:
	// Should be implemented by derived classes to provide the object being observed
	virtual UObject* GetObjectToObserve() const;

	// 
	void SetPropertyWindowContents(TArray<UObject*> Objects);

	//
	virtual TSharedRef<SWidget> PopulateSlot(TSharedRef<SWidget> PropertyEditorWidget);

	// Property viewing widget
	TSharedPtr<class IDetailsView> PropertyView;

private:
	// Cached object view
	TWeakObjectPtr<UObject> LastObservedObject;

	// If true, GetObjectToObserve will be called every frame to update the object being displayed in the details panel
	// Otherwise, users must call SetPropertyWindowContents manually
	bool bAutoObserveObject;
};


