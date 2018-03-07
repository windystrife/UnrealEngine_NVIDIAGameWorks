// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CreateBlueprintFromActorDialog.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "SCreateAssetFromObject.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "CreateBlueprintFromActorDialog"

TWeakObjectPtr<AActor> FCreateBlueprintFromActorDialog::ActorOverride = nullptr;

void FCreateBlueprintFromActorDialog::OpenDialog(bool bInHarvest, AActor* InActorOverride )
{
	ActorOverride = InActorOverride;

	TSharedPtr<SWindow> PickBlueprintPathWidget;
	SAssignNew(PickBlueprintPathWidget, SWindow)
		.Title(LOCTEXT("SelectPath", "Select Path"))
		.ToolTipText(LOCTEXT("SelectPathTooltip", "Select the path where the Blueprint will be created at"))
		.ClientSize(FVector2D(400, 400));

	TSharedPtr<SCreateAssetFromObject> CreateBlueprintFromActorDialog;
	PickBlueprintPathWidget->SetContent
	(
		SAssignNew(CreateBlueprintFromActorDialog, SCreateAssetFromObject, PickBlueprintPathWidget)
		.AssetFilenameSuffix(TEXT("Blueprint"))
		.HeadingText(LOCTEXT("CreateBlueprintFromActor_Heading", "Blueprint Name"))
		.CreateButtonText(LOCTEXT("CreateBlueprintFromActor_ButtonLabel", "Create Blueprint"))
		.OnCreateAssetAction(FOnPathChosen::CreateStatic(FCreateBlueprintFromActorDialog::OnCreateBlueprint, bInHarvest))
	);

	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(PickBlueprintPathWidget.ToSharedRef(), RootWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(PickBlueprintPathWidget.ToSharedRef());
	}
}

void FCreateBlueprintFromActorDialog::OnCreateBlueprint(const FString& InAssetPath, bool bInHarvest)
{
	UBlueprint* Blueprint = NULL;

	if(bInHarvest)
	{
		TArray<AActor*> Actors;

		USelection* SelectedActors = GEditor->GetSelectedActors();
		for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			// We only care about actors that are referenced in the world for literals, and also in the same level as this blueprint
			AActor* Actor = Cast<AActor>(*Iter);
			if(Actor)
			{
				Actors.Add(Actor);
			}
		}
		Blueprint = FKismetEditorUtilities::HarvestBlueprintFromActors(InAssetPath, Actors, true);
	}
	else
	{
		AActor* ActorToUse = ActorOverride.Get();

		if( !ActorToUse )
		{
			TArray< UObject* > SelectedActors;
			GEditor->GetSelectedActors()->GetSelectedObjects(AActor::StaticClass(), SelectedActors);
			check(SelectedActors.Num());
			ActorToUse =  Cast<AActor>(SelectedActors[0]);
		}

		const bool bReplaceActor = true;
		Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(InAssetPath, ActorToUse, bReplaceActor);
	}

	if(Blueprint)
	{
		// Rename new instance based on the original actor label rather than the asset name
		USelection* SelectedActors = GEditor->GetSelectedActors();
		if( SelectedActors && SelectedActors->Num() == 1 )
		{
			AActor* Actor = Cast<AActor>(SelectedActors->GetSelectedObject(0));
			if(Actor)
			{
				FActorLabelUtilities::SetActorLabelUnique(Actor, FPackageName::GetShortName(InAssetPath));
			}
		}

		// Select the newly created blueprint in the content browser, but don't activate the browser
		TArray<UObject*> Objects;
		Objects.Add(Blueprint);
		GEditor->SyncBrowserToObjects( Objects, false );
	}
	else
	{
		FNotificationInfo Info( LOCTEXT("CreateBlueprintFromActorFailed", "Unable to create a blueprint from actor.") );
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if ( Notification.IsValid() )
		{
			Notification->SetCompletionState( SNotificationItem::CS_Fail );
		}
	}
}


#undef LOCTEXT_NAMESPACE
