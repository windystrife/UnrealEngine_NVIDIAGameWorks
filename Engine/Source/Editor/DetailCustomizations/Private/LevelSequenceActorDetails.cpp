// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceActorDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "LevelSequenceActor.h"

#define LOCTEXT_NAMESPACE "LevelSequenceActorDetails"


TSharedRef<IDetailCustomization> FLevelSequenceActorDetails::MakeInstance()
{
	return MakeShareable( new FLevelSequenceActorDetails );
}

void FLevelSequenceActorDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetSelectedObjects();

	for( int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex )
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if ( CurrentObject.IsValid() )
		{
			ALevelSequenceActor* CurrentLevelSequenceActor = Cast<ALevelSequenceActor>(CurrentObject.Get());
			if (CurrentLevelSequenceActor != NULL)
			{
				LevelSequenceActor = CurrentLevelSequenceActor;
				break;
			}
		}
	}
	
	//DetailLayout.EditCategory( "LevelSequenceActor", NSLOCTEXT("LevelSequenceActorDetails", "LevelSequenceActor", "Level Sequence Actor"), ECategoryPriority::Important )
	DetailLayout.EditCategory( "General", NSLOCTEXT("GeneralDetails", "General", "General"), ECategoryPriority::Important )
	.AddCustomRow( NSLOCTEXT("LevelSequenceActorDetails", "OpenLevelSequence", "Open Level Sequence") )
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0, 5, 10, 5)
		[
			SNew(SButton)
			.ContentPadding(3)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.IsEnabled( this, &FLevelSequenceActorDetails::CanOpenLevelSequenceForActor )
			.OnClicked( this, &FLevelSequenceActorDetails::OnOpenLevelSequenceForActor )
			.Text( NSLOCTEXT("LevelSequenceActorDetails", "OpenLevelSequence", "Open Level Sequence") )
		]
	];
}


bool FLevelSequenceActorDetails::CanOpenLevelSequenceForActor() const
{
	if( LevelSequenceActor.IsValid() )
	{
		return LevelSequenceActor.Get()->LevelSequence.IsValid();
	}
	return false;
}

FReply FLevelSequenceActorDetails::OnOpenLevelSequenceForActor()
{
	if( LevelSequenceActor.IsValid() )
	{
		UObject* LoadedObject = LevelSequenceActor.Get()->LevelSequence.TryLoad();
		if (LoadedObject != nullptr)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(LoadedObject);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
