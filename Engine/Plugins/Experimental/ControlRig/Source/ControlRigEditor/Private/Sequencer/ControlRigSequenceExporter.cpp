// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequenceExporter.h"
#include "ControlRigSequence.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "LevelSequenceActor.h"
#include "ScopedTransaction.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "ModuleManager.h"
#include "SBox.h"
#include "SUniformGridPanel.h"
#include "SButton.h"
#include "SlateApplication.h"
#include "ControlRigSequenceExporterSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRigBindingTemplate.h"
#include "Editor.h"
#include "SNotificationList.h"
#include "AssetEditorManager.h"
#include "AssetRegistryModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "STextBlock.h"

#define LOCTEXT_NAMESPACE "ControlRigSequenceExporter"

namespace ControlRigSequenceConverter
{

class SExporterDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SExporterDialog)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SWindow>& InParentWindow)
	{
		ParentWindow = InParentWindow;

		bExport = false;

		FDetailsViewArgs Args;
		Args.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
		Args.bAllowSearch = false;
		
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(Args);
		DetailsView->SetObject(GetMutableDefault<UControlRigSequenceExporterSettings>());

		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				DetailsView
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				+SUniformGridPanel::Slot(0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
					.ForegroundColor(FLinearColor::White)
					.TextStyle(FEditorStyle::Get(), "NormalText.Important")
					.OnClicked(this, &SExporterDialog::HandleExportClicked)
					.IsEnabled(this, &SExporterDialog::IsExportEnabled)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "NormalText.Important")
						.Text(LOCTEXT("ConvertButtonLabel", "Convert"))
					]
				]
				+SUniformGridPanel::Slot(1, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton")
					.ForegroundColor(FLinearColor::White)
					.OnClicked(this, &SExporterDialog::HandleCancelClicked)
					[
						SNew(STextBlock)
						.TextStyle(FEditorStyle::Get(), "NormalText.Important")
						.Text(LOCTEXT("CancelButtonLabel", "Cancel"))
					]
				]
			]
		];
	}

	static bool ShowWindow()
	{
		const FText TitleText = LOCTEXT("ConvertWindowTitle", "Convert control rig sequence");

		// Create the window to choose our options
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(TitleText)
			.SizingRule(ESizingRule::UserSized)
			.ClientSize(FVector2D(400.0f, 400.0f))
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.SupportsMinimize(false);

		TSharedRef<SExporterDialog> DialogWidget = SNew(SExporterDialog, Window);
		Window->SetContent(DialogWidget);

		TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
		FSlateApplication::Get().AddModalWindow(Window, RootWindow);

		return DialogWidget->bExport;
	}

private:
	FReply HandleExportClicked()
	{
		bExport = true;
		ParentWindow.Pin()->RequestDestroyWindow();
		return FReply::Handled();
	}

	FReply HandleCancelClicked()
	{
		bExport = false;
		ParentWindow.Pin()->RequestDestroyWindow();
		return FReply::Handled();
	}

	bool IsExportEnabled() const
	{
		const UControlRigSequenceExporterSettings* Settings = GetDefault<UControlRigSequenceExporterSettings>();
		return Settings->Sequence != nullptr && Settings->AnimationSequence != nullptr && Settings->SkeletalMesh != nullptr && Settings->FrameRate > 0.0f;
	}

private:
	/** Parent window */
	TWeakPtr<SWindow> ParentWindow;

	/** Result flag */
	bool bExport;
};


void Convert(UControlRigSequence* Sequence, UAnimSequence* AnimSequence, USkeletalMesh* SkeletalMesh, bool bShowDialog)
{
	UControlRigSequenceExporterSettings* Settings = GetMutableDefault<UControlRigSequenceExporterSettings>();
	Settings->Sequence = Sequence;

	if (!bShowDialog)
	{
		if (Sequence)
		{
			// First use defaults from the asset
			if (Sequence->LastExportedToAnimationSequence.IsValid())
			{
				Settings->AnimationSequence = Sequence->LastExportedToAnimationSequence.LoadSynchronous();
			}

			if (Sequence->LastExportedUsingSkeletalMesh.IsValid())
			{
				Settings->SkeletalMesh = Sequence->LastExportedUsingSkeletalMesh.LoadSynchronous();
			}
		}

		// Then override with any valid passed-in values
		if (AnimSequence)
		{
			Settings->AnimationSequence = AnimSequence;
		}

		if (SkeletalMesh)
		{
			Settings->SkeletalMesh = SkeletalMesh;
		}
	}

	if (!bShowDialog || SExporterDialog::ShowWindow())
	{
		if (bShowDialog)
		{
			Sequence = Settings->Sequence;
			AnimSequence = Settings->AnimationSequence;
			SkeletalMesh = Settings->SkeletalMesh;
		}

		if (Sequence && AnimSequence && SkeletalMesh)
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("SequenceImport", "Sequence Import"));

			AnimSequence->Modify();

			// Create a dummy actor for use with export
			UWorld* World = GEditor->GetEditorWorldContext().World();
			ASkeletalMeshActor* SkeletalMeshActor = World->SpawnActor<ASkeletalMeshActor>();
			USkeletalMeshComponent* SkeletalMeshComponent = SkeletalMeshActor->GetSkeletalMeshComponent();
			SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);

			// switch object bindings for this export (we will restore it later)
			UObject* OldObjectBinding = FControlRigBindingTemplate::GetObjectBinding();
			FControlRigBindingTemplate::SetObjectBinding(SkeletalMeshActor);

			// Create a sequence actor to run the sequence off of
			ALevelSequenceActor* LevelSequenceActor = World->SpawnActor<ALevelSequenceActor>();
			LevelSequenceActor->LevelSequence = Sequence;
			LevelSequenceActor->PlaybackSettings.bRestoreState = true;
			LevelSequenceActor->SequencePlayer = NewObject<ULevelSequencePlayer>(LevelSequenceActor, "AnimationPlayer");
			LevelSequenceActor->SequencePlayer->Initialize(Sequence, World, LevelSequenceActor->PlaybackSettings);

			// Now set up our animation sequence
			AnimSequence->RecycleAnimSequence();

			// Setup raw tracks
			USkeleton* AnimSkeleton = AnimSequence->GetSkeleton();
			for (int32 BoneIndex = 0; BoneIndex < SkeletalMeshComponent->GetComponentSpaceTransforms().Num(); ++BoneIndex)
			{
				// verify if this bone exists in skeleton
				const int32 BoneTreeIndex = AnimSkeleton->GetSkeletonBoneIndexFromMeshBoneIndex(SkeletalMesh, BoneIndex);
				if (BoneTreeIndex != INDEX_NONE)
				{
					// add tracks for the bone existing
					FName BoneTreeName = AnimSkeleton->GetReferenceSkeleton().GetBoneName(BoneTreeIndex);
					AnimSequence->AddNewRawTrack(BoneTreeName);
				}
			}

			// Setup notifies
			AnimSequence->InitializeNotifyTrack();

			// Now run our sequence
			double StartTime = Sequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();
			double SequenceLength = AnimSequence->SequenceLength = Sequence->GetMovieScene()->GetPlaybackRange().Size<float>();
			int32 FrameCount = AnimSequence->NumFrames = FMath::CeilToInt(SequenceLength * Settings->FrameRate);
			double FrameCountDouble = (double)FrameCount;
			double FrameLength = 1.0 / (double)Settings->FrameRate;

			for (int32 Frame = 0; Frame < FrameCount; ++Frame)
			{
				float CurrentTime = (float)(StartTime + FMath::Clamp(((double)Frame / FrameCountDouble) * SequenceLength, 0.0, SequenceLength));

				// Tick Sequence
				LevelSequenceActor->SequencePlayer->SetPlaybackPosition(CurrentTime);

				// Tick skeletal mesh component
				SkeletalMeshComponent->TickAnimation(FrameLength, false);
				SkeletalMeshComponent->RefreshBoneTransforms();

				// copy data to tracks
				const TArray<FTransform>& ComponentSpaceTransforms = SkeletalMeshComponent->GetComponentSpaceTransforms();
				for (int32 TrackIndex = 0; TrackIndex < AnimSequence->GetRawAnimationData().Num(); ++TrackIndex)
				{
					// verify if this bone exists in skeleton
					int32 BoneTreeIndex = AnimSequence->GetSkeletonIndexFromRawDataTrackIndex(TrackIndex);
					if (BoneTreeIndex != INDEX_NONE)
					{
						int32 BoneIndex = AnimSkeleton->GetMeshBoneIndexFromSkeletonBoneIndex(SkeletalMesh, BoneTreeIndex);
						int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
						FTransform LocalTransform = ComponentSpaceTransforms[BoneIndex];
						if (ParentIndex != INDEX_NONE)
						{
							LocalTransform.SetToRelativeTransform(ComponentSpaceTransforms[ParentIndex]);
						}

						FRawAnimSequenceTrack& RawTrack = AnimSequence->GetRawAnimationTrack(TrackIndex);
						RawTrack.PosKeys.Add(LocalTransform.GetTranslation());
						RawTrack.RotKeys.Add(LocalTransform.GetRotation());
						RawTrack.ScaleKeys.Add(LocalTransform.GetScale3D());
					}
				}
			}

			AnimSequence->PostProcessSequence();
			AnimSequence->MarkPackageDirty();

			// notify to user
			const FText NotificationText = FText::Format(LOCTEXT("ConvertAnimationNotification", "'{0}' has been successfully converted [{1} frames : {2} sec(s) @ {3} Hz]"),
				FText::FromString(AnimSequence->GetName()),
				FText::AsNumber(AnimSequence->NumFrames),
				FText::AsNumber(AnimSequence->SequenceLength),
				FText::AsNumber(Settings->FrameRate)
			);

			TWeakObjectPtr<UAnimSequence> WeakAnimSequence = AnimSequence;

			FNotificationInfo Info(NotificationText);
			Info.ExpireDuration = 8.0f;
			Info.bUseLargeFont = false;
			Info.Hyperlink = FSimpleDelegate::CreateLambda([WeakAnimSequence]()
			{
				if (WeakAnimSequence.IsValid())
				{
					TArray<UObject*> Assets;
					Assets.Add(WeakAnimSequence.Get());
					FAssetEditorManager::Get().OpenEditorForAssets(Assets);
				}
			});
			Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewAnimationHyperlink", "Open {0}"), FText::FromString(AnimSequence->GetName()));
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if (Notification.IsValid())
			{
				Notification->SetCompletionState(SNotificationItem::CS_Success);
			}

			Sequence->LastExportedUsingSkeletalMesh = SkeletalMesh;
			Sequence->LastExportedToAnimationSequence = AnimSequence;
			Sequence->LastExportedFrameRate = Settings->FrameRate;

			// inform asset registry of our asset creation
			FAssetRegistryModule::AssetCreated(AnimSequence);

			// restore object bindings
			FControlRigBindingTemplate::SetObjectBinding(OldObjectBinding);

			// Clean up the temp objects we used for export
			LevelSequenceActor->SequencePlayer->Stop();
			LevelSequenceActor->SequencePlayer->MarkPendingKill();
			LevelSequenceActor->Destroy();
			SkeletalMeshActor->Destroy();

			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}
}

}

#undef LOCTEXT_NAMESPACE