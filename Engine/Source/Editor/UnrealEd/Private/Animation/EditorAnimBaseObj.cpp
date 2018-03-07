// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimMontage.cpp: Montage classes that contains slots
=============================================================================*/ 

#include "Animation/EditorAnimBaseObj.h"
#include "Animation/AnimSequenceBase.h"

#define LOCTEXT_NAMESPACE "SSkeletonTree"

UEditorAnimBaseObj::UEditorAnimBaseObj(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UEditorAnimBaseObj::InitFromAnim(UAnimSequenceBase* AnimObjectIn, FOnAnimObjectChange OnChangeIn)
{
	AnimObject = AnimObjectIn;
	OnChange = OnChangeIn;
}
bool UEditorAnimBaseObj::ApplyChangesToMontage()
{
	return false;
}

void UEditorAnimBaseObj::PreEditChange( class FEditPropertyChain& PropertyAboutToChange )
{
	//Handle undo from the details panel
	AnimObject->Modify();
}

void UEditorAnimBaseObj::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(OnChange.IsBound())
	{
		if (ApplyChangesToMontage())
		{
			OnChange.Execute(this, PropertyChangeRequiresRebuild(PropertyChangedEvent));
		}
	}
}

/*
void UEditorAnimBaseObj::NotifyUser()
{
	FNotificationInfo Info( FString::Printf( *LOCTEXT( "InvalidAnimSeg", "Invalid Animation is selected" ).ToString() ) );

	Info.bUseLargeFont = false;
	Info.ExpireDuration = 5.0f;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification( Info );
}*/

#undef LOCTEXT_NAMESPACE
