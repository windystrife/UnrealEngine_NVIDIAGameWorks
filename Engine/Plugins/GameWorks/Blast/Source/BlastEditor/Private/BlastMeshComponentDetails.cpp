// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlastMeshComponentDetails.h"
#include "BlastMeshComponent.h"
#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "Blast"

TSharedRef<IDetailCustomization> FBlastMeshComponentDetails::MakeInstance()
{
	return MakeShareable(new FBlastMeshComponentDetails);
}

void FBlastMeshComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("Blast", FText::GetEmpty(), ECategoryPriority::Important);

	TSharedRef<IPropertyHandle> bUseBoundsFromMasterPoseComponentProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBlastMeshComponent, bUseBoundsFromMasterPoseComponent));
	DetailBuilder.HideProperty(bUseBoundsFromMasterPoseComponentProperty);
}


#undef LOCTEXT_NAMESPACE
