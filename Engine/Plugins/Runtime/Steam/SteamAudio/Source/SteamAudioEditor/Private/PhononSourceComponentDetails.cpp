//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononSourceComponentDetails.h"

#include "PhononCommon.h"
#include "PhononSourceComponent.h"
#include "SteamAudioEditorModule.h"
#include "IndirectBaker.h"

#include "Components/AudioComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "IDetailCustomization.h"
#include "IDetailChildrenBuilder.h"
#include "LevelEditorViewport.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"

namespace SteamAudio
{
	//==============================================================================================================================================
	// FPhononSourceComponentDetails
	//==============================================================================================================================================

	TSharedRef<IDetailCustomization> FPhononSourceComponentDetails::MakeInstance()
	{
		return MakeShareable(new FPhononSourceComponentDetails());
	}

	void FPhononSourceComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();

		for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
		{
			const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
			if (CurrentObject.IsValid())
			{
				auto SelectedPhononSourceComponent = Cast<UPhononSourceComponent>(CurrentObject.Get());
				if (SelectedPhononSourceComponent)
				{
					PhononSourceComponent = SelectedPhononSourceComponent;
					break;
				}
			}
		}

		DetailLayout.EditCategory("Baking").AddProperty(GET_MEMBER_NAME_CHECKED(UPhononSourceComponent, UniqueIdentifier));
		DetailLayout.EditCategory("Baking").AddProperty(GET_MEMBER_NAME_CHECKED(UPhononSourceComponent, BakingRadius));
		DetailLayout.EditCategory("Baking").AddCustomRow(NSLOCTEXT("PhononSourceComponentDetails", "Bake Propagation", "Bake Propagation"))
			.NameContent()
			[
				SNullWidget::NullWidget
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(2)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled(this, &FPhononSourceComponentDetails::IsBakeEnabled)
					.OnClicked(this, &FPhononSourceComponentDetails::OnBakePropagation)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("PhononSourceComponentDetails", "Bake Propagation", "Bake Propagation"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			];
	}

	FReply FPhononSourceComponentDetails::OnBakePropagation()
	{
		TArray<UPhononSourceComponent*> SourceComponents;
		SourceComponents.Add(PhononSourceComponent.Get());

		Bake(SourceComponents, false, nullptr);
		
		return FReply::Handled();
	}

	bool FPhononSourceComponentDetails::IsBakeEnabled() const
	{
		return !(PhononSourceComponent->UniqueIdentifier.IsNone() || GIsBaking.load());
	}
}