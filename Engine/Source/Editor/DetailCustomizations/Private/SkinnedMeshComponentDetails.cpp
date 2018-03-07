// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkinnedMeshComponentDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "GameFramework/Actor.h"
#include "Components/SkinnedMeshComponent.h"
#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "SkinnedMeshComponentDetails"

TSharedRef<IDetailCustomization> FSkinnedMeshComponentDetails::MakeInstance()
{
	return MakeShareable(new FSkinnedMeshComponentDetails);
}

void FSkinnedMeshComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("Mesh", FText::GetEmpty(), ECategoryPriority::Important);
	IDetailCategoryBuilder& PhysicsCategory = DetailBuilder.EditCategory("Physics", FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	IDetailCategoryBuilder& LODCategory = DetailBuilder.EditCategory("LOD", LOCTEXT("LODCategoryName", "Level of Detail"), ECategoryPriority::Default);
	
	// show extra field about actually used physics asset, but make sure to show it under physics asset override
	TSharedRef<IPropertyHandle> PhysicsAssetProperty =  DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkinnedMeshComponent, PhysicsAssetOverride));
	if (PhysicsAssetProperty->IsValidHandle())
	{
		PhysicsCategory.AddProperty(GET_MEMBER_NAME_CHECKED(USkinnedMeshComponent, PhysicsAssetOverride));
		CreateActuallyUsedPhysicsAssetWidget(PhysicsCategory.AddCustomRow( LOCTEXT("CurrentPhysicsAsset", "Currently used Physics Asset"), true), &DetailBuilder);
	}
}

void FSkinnedMeshComponentDetails::CreateActuallyUsedPhysicsAssetWidget(FDetailWidgetRow& OutWidgetRow, IDetailLayoutBuilder* DetailBuilder) const
{
	OutWidgetRow
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("CurrentPhysicsAsset", "Currently used Physics Asset"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth( 1 )
		[
			SNew(SEditableTextBox)
			.Text(this, &FSkinnedMeshComponentDetails::GetUsedPhysicsAssetAsText, DetailBuilder)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsReadOnly(true)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding( 2.0f, 1.0f )
		[
			PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FSkinnedMeshComponentDetails::BrowseUsedPhysicsAsset, DetailBuilder))
		]
	];
}

bool FSkinnedMeshComponentDetails::FindUniqueUsedPhysicsAsset(IDetailLayoutBuilder* DetailBuilder, UPhysicsAsset*& OutFoundPhysicsAsset) const
{
	int UsedPhysicsAssetCount = 0;
	OutFoundPhysicsAsset = NULL;
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjectsList = DetailBuilder->GetSelectedObjects();
	for (auto SelectionIt = SelectedObjectsList.CreateConstIterator(); SelectionIt; ++SelectionIt)
	{
		if (AActor* Actor = Cast<AActor>(SelectionIt->Get()))
		{
			TInlineComponentArray<USkinnedMeshComponent*> SkinnedMeshComponents;
			Actor->GetComponents(SkinnedMeshComponents);

			for (int32 CompIdx = 0; CompIdx < SkinnedMeshComponents.Num(); CompIdx++)
			{
				USkinnedMeshComponent* SkinnedMeshComp = SkinnedMeshComponents[CompIdx];

				// Only use registered and visible primitive components when calculating bounds
				if (UsedPhysicsAssetCount > 0)
				{
					return false;
				}
				++UsedPhysicsAssetCount;
				OutFoundPhysicsAsset = SkinnedMeshComp->GetPhysicsAsset();
			}
		}
	}
	return true;
}

FText FSkinnedMeshComponentDetails::GetUsedPhysicsAssetAsText(IDetailLayoutBuilder* DetailBuilder) const
{
	UPhysicsAsset* UsedPhysicsAsset = NULL;
	if (! FindUniqueUsedPhysicsAsset(DetailBuilder, UsedPhysicsAsset))
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}
	if (UsedPhysicsAsset)
	{
		return FText::FromString(UsedPhysicsAsset->GetName());
	}
	else
	{
		return FText::GetEmpty();
	}
}

void FSkinnedMeshComponentDetails::BrowseUsedPhysicsAsset(IDetailLayoutBuilder* DetailBuilder) const
{
	UPhysicsAsset* UsedPhysicsAsset = NULL;
	if (FindUniqueUsedPhysicsAsset(DetailBuilder, UsedPhysicsAsset) &&
		UsedPhysicsAsset != NULL)
	{
		TArray<UObject*> Objects;
		Objects.Add(UsedPhysicsAsset);
		GEditor->SyncBrowserToObjects(Objects);
	}
}

#undef LOCTEXT_NAMESPACE
