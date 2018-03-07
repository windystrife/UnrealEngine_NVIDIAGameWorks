//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononSceneDetails.h"
#include "PhononScene.h"
#include "PhononCommon.h"
#include "TickableNotification.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"
#include "IDetailCustomization.h"
#include "Async/Async.h"
#include "LevelEditorViewport.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Editor.h"

namespace SteamAudio
{
	static TSharedPtr<SteamAudio::FTickableNotification> GExportSceneTickable = MakeShareable(new FTickableNotification());

	TSharedRef<IDetailCustomization> FPhononSceneDetails::MakeInstance()
	{
		return MakeShareable(new FPhononSceneDetails);
	}

	void FPhononSceneDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();

		for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
		{
			const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
			if (CurrentObject.IsValid())
			{
				APhononScene* CurrentCaptureActor = Cast<APhononScene>(CurrentObject.Get());
				if (CurrentCaptureActor)
				{
					PhononSceneActor = CurrentCaptureActor;
					break;
				}
			}
		}

		DetailLayout.EditCategory("Scene Export").AddCustomRow(NSLOCTEXT("PhononSceneDetails", "Scene Export", "Scene Export"))
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
					.OnClicked(this, &FPhononSceneDetails::OnExportScene)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("PhononSceneDetails", "Export Scene", "Export Scene"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			];

		DetailLayout.EditCategory("Scene Statistics").AddCustomRow(NSLOCTEXT("PhononSceneDetails", "Scene Data Size", "Scene Data Size"))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("PhononSceneDetails", "Scene Data Size", "Scene Data Size"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(this, &FPhononSceneDetails::GetSceneDataSizeText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
	}

	FReply FPhononSceneDetails::OnExportScene()
	{
		// Display editor notification
		GExportSceneTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "Exporting scene...", "Exporting scene..."));
		GExportSceneTickable->CreateNotification();

		// Grab a copy of the scene ptr as it will be destroyed if user clicks off of volume in GUI
		auto PhononSceneActorHandle = PhononSceneActor.Get();

		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			// Export the scene
			UWorld* World = GEditor->LevelViewportClients[0]->GetWorld();
			IPLhandle PhononScene = nullptr;
			TArray<IPLhandle> PhononStaticMeshes;
			SteamAudio::LoadScene(World, &PhononScene, &PhononStaticMeshes);

			// Save data in PhononScene actor
			auto SceneSize = iplSaveFinalizedScene(PhononScene, nullptr);
			PhononSceneActor->SceneData.SetNum(SceneSize);
			iplSaveFinalizedScene(PhononScene, PhononSceneActor->SceneData.GetData());

			// Clean up Phonon structures
			for (IPLhandle PhononStaticMesh : PhononStaticMeshes)
			{
				iplDestroyStaticMesh(&PhononStaticMesh);
			}
			iplDestroyScene(&PhononScene);

			// Notify UI that we're done
			GExportSceneTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
				DisplayText = NSLOCTEXT("SteamAudio", "Export scene complete.", "Export scene complete.");
			}, SNotificationItem::CS_Success, true));
		});

		return FReply::Handled();
	}

	FText FPhononSceneDetails::GetSceneDataSizeText() const
	{
		return GetKBTextFromByte(PhononSceneActor->SceneData.Num());
	}
}