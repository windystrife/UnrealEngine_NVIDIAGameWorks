// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationModifierDetailCustomization.h"
#include "AnimationModifier.h"

#include "DetailLayoutBuilder.h" 
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "SButton.h"
#include "STextBlock.h"

#define LOCTEXT_NAMESPACE "FAnimationModifierDetailCustomization"

void FAnimationModifierDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	ModifierInstance = nullptr;
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	ensure(Objects.Num() == 1); // This is in here to ensure we are only showing the modifier details in the blueprint editor

	for (TWeakObjectPtr<UObject> Object : Objects)
	{ 
		if (Object->IsA<UAnimationModifier>())
		{
			ModifierInstance = Cast<UAnimationModifier>(Object.Get());
		}
	}
	
	// If we have found a valid modifier instance add a revision bump button to the details panel
	if (ModifierInstance)
	{
		IDetailCategoryBuilder& RevisionCategory = DetailBuilder.EditCategory("Revision");
		FDetailWidgetRow& UpdateRevisionRow = RevisionCategory.AddCustomRow(LOCTEXT("UpdateRevisionSearchLabel", "Update Revision"))
		.WholeRowWidget
		[
			SAssignNew(UpdateRevisionButton, SButton)
			.OnClicked(this, &FAnimationModifierDetailCustomization::OnUpdateRevisionButtonClicked)
			.ContentPadding(FMargin(2))
			.Content()
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text(LOCTEXT("UpdateRevisionText", "Update Revision"))
			]
		];
	}
}

FReply FAnimationModifierDetailCustomization::OnUpdateRevisionButtonClicked()
{
	if (ModifierInstance)
	{
		ModifierInstance->UpdateRevisionGuid(ModifierInstance->GetClass());
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE //"FAnimationModifierDetailCustomization"