// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UProceduralMeshComponent;

class FProceduralMeshComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	/** Handle clicking the convert button */
	FReply ClickedOnConvertToStaticMesh();

	/** Is the convert button enabled */
	bool ConvertToStaticMeshEnabled() const;

	/** Util to get the ProcMeshComp we want to convert */
	UProceduralMeshComponent* GetFirstSelectedProcMeshComp() const;

	/** Cached array of selected objects */
	TArray< TWeakObjectPtr<UObject> > SelectedObjectsList;
};
