// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComposurePostMoveSettingsPropertyTrackEditor.h"
#include "ComposurePostMoveSettingsPropertySection.h"
#include "MovieScene/MovieSceneComposurePostMoveSettingsSection.h"
#include "ComposurePostMoves.h"
#include "SComposurePostMoveSettingsImportDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ComposureEditorModule.h"

#define LOCTEXT_NAMESPACE "ComposurePostMoveSettingsPropertyTrackEditor"

TSharedRef<ISequencerTrackEditor> FComposurePostMoveSettingsPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FComposurePostMoveSettingsPropertyTrackEditor(InSequencer));
}


TSharedRef<ISequencerSection> FComposurePostMoveSettingsPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FPostMoveSettingsPropertyTrackEditor"));
	return MakeShareable(new FComposurePostMoveSettingsPropertySection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
}


void FComposurePostMoveSettingsPropertyTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	MenuBuilder.BeginSection("PostMoveSettings", NSLOCTEXT("PostMoveSettingsTrackEditor", "PostMoveSettingsMenuSection", "Post Move Settings"));
	{
		UMovieSceneComposurePostMoveSettingsTrack* PostMoveSettingsTrack = CastChecked<UMovieSceneComposurePostMoveSettingsTrack>(Track);
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("PostMoveSettingsTrackEditor", "ImportPostMoveSettings", "Import from file..."),
			NSLOCTEXT("PostMoveSettingsTrackEditor", "ImportPostMoveSettingsToolTip", "Shows a dialog used to import post move track data from an external file."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateRaw(this, &FComposurePostMoveSettingsPropertyTrackEditor::ShowImportPostMoveSettingsDialog, PostMoveSettingsTrack)));
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.AddMenuSeparator();
	FPropertyTrackEditor::BuildTrackContextMenu(MenuBuilder, Track);
}


void FComposurePostMoveSettingsPropertyTrackEditor::ShowImportPostMoveSettingsDialog(UMovieSceneComposurePostMoveSettingsTrack* PostMoveSettingsTrack)
{
	UMovieScene* ParentMovieScene = PostMoveSettingsTrack->GetTypedOuter<UMovieScene>();
	if (ParentMovieScene != nullptr)
	{
		float FrameInterval = ParentMovieScene->GetFixedFrameInterval();
		int32 StartFrame = FMath::RoundToInt(ParentMovieScene->GetPlaybackRange().GetLowerBoundValue() / FrameInterval);

		TSharedPtr<SWindow> TopLevelWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (TopLevelWindow.IsValid())
		{
			TSharedRef<SWindow> Dialog =
				SNew(SComposurePostMoveSettingsImportDialog, FrameInterval, StartFrame)
				.OnImportSelected(this, &FComposurePostMoveSettingsPropertyTrackEditor::ImportPostMoveSettings, PostMoveSettingsTrack)
				.OnImportCanceled(this, &FComposurePostMoveSettingsPropertyTrackEditor::ImportCanceled);
			FSlateApplication::Get().AddWindowAsNativeChild(Dialog, TopLevelWindow.ToSharedRef());
			ImportDialog = Dialog;
		}
	}
}


void NotifyImportFailed(FString Path, FText Message)
{
	FText FormattedMessage = FText::Format(
		LOCTEXT("NotifyImportFailedFormat", "Failed to import {0}.  Message: {1}"),
		FText::FromString(Path),
		Message);

	// Write to log.
	UE_LOG(LogComposureEditor, Warning, TEXT("%s"), *FormattedMessage.ToString());

	// Show toast.
	FNotificationInfo Info(FormattedMessage);
	Info.ExpireDuration = 5.0f;
	Info.bFireAndForget = true;
	Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
	FSlateNotificationManager::Get().AddNotification(Info);
}


void FComposurePostMoveSettingsPropertyTrackEditor::ImportPostMoveSettings(FString ImportFilePath, float FrameInverval, int32 StartFrame, UMovieSceneComposurePostMoveSettingsTrack* PostMoveSettingsTrack)
{
	TSharedPtr<SWindow> DialogPinned = ImportDialog.Pin();
	if (DialogPinned.IsValid())
	{
		FSlateApplication::Get().RequestDestroyWindow(DialogPinned.ToSharedRef());
	}

	FString ImportFileContents;
	FFileHelper::LoadFileToString(ImportFileContents, *ImportFilePath);
	if (ImportFileContents.Len() == 0)
	{
		NotifyImportFailed(ImportFilePath, LOCTEXT("EmptyImportFileMessgae", "File was empty."));
		return;
	}

	UMovieSceneComposurePostMoveSettingsSection* PostMoveSettingsSection = CastChecked<UMovieSceneComposurePostMoveSettingsSection>(PostMoveSettingsTrack->CreateNewSection());
	PostMoveSettingsSection->SetIsInfinite(true);

	FRichCurve& PivotXCurve = PostMoveSettingsSection->GetCurve(EComposurePostMoveSettingsChannel::Pivot, EComposurePostMoveSettingsAxis::X);
	FRichCurve& PivotYCurve = PostMoveSettingsSection->GetCurve(EComposurePostMoveSettingsChannel::Pivot, EComposurePostMoveSettingsAxis::Y);

	FRichCurve& TranslationXCurve = PostMoveSettingsSection->GetCurve(EComposurePostMoveSettingsChannel::Translation, EComposurePostMoveSettingsAxis::X);
	FRichCurve& TranslationYCurve = PostMoveSettingsSection->GetCurve(EComposurePostMoveSettingsChannel::Translation, EComposurePostMoveSettingsAxis::Y);

	FRichCurve& RotationAngleCurve = PostMoveSettingsSection->GetCurve(EComposurePostMoveSettingsChannel::RotationAngle, EComposurePostMoveSettingsAxis::None);

	FRichCurve& ScaleCurve = PostMoveSettingsSection->GetCurve(EComposurePostMoveSettingsChannel::Scale, EComposurePostMoveSettingsAxis::None);

	TArray<FString> ImportFileLines;
	ImportFileContents.ParseIntoArray(ImportFileLines, TEXT("\n"), true);
	float StartTime = UMovieScene::CalculateFixedFrameTime(StartFrame * FrameInverval, FrameInverval);
	float Time = StartTime;
	float EndTime = StartTime;
	int32 LineNumber = 1;
	for (const FString& ImportFileLine : ImportFileLines)
	{
		FString ImportFileLineClean = ImportFileLine.Replace(TEXT("\r"), TEXT(""));

		TArray<FString> ImportFileLineValues;
		ImportFileLineClean.ParseIntoArray(ImportFileLineValues, TEXT(" "), true);

		if (ImportFileLineValues.Num() > 0)
		{
			if (ImportFileLineValues.Num() != 6)
			{
				NotifyImportFailed(ImportFilePath, FText::Format(LOCTEXT("ParseFailedFormat", "Parse failed on line {0}."), LineNumber));
				return;
			}

			float FixedIntervalTime = UMovieScene::CalculateFixedFrameTime(Time, FrameInverval);

			float PivotX = FCString::Atof(*ImportFileLineValues[0]);
			float PivotY = FCString::Atof(*ImportFileLineValues[1]);
			PivotXCurve.AddKey(FixedIntervalTime, PivotX);
			PivotYCurve.AddKey(FixedIntervalTime, PivotY);

			float TranslationX = FCString::Atof(*ImportFileLineValues[2]);
			float TranslationY = FCString::Atof(*ImportFileLineValues[3]);
			TranslationXCurve.AddKey(FixedIntervalTime, TranslationX);
			TranslationYCurve.AddKey(FixedIntervalTime, TranslationY);

			float Rotation = FCString::Atof(*ImportFileLineValues[4]);
			RotationAngleCurve.AddKey(FixedIntervalTime, Rotation);

			float Scale = FCString::Atof(*ImportFileLineValues[5]);
			ScaleCurve.AddKey(FixedIntervalTime, Scale);

			EndTime = FixedIntervalTime;
			Time += FrameInverval;
		}
		LineNumber++;
	}
	PostMoveSettingsSection->SetStartTime(StartTime);
	PostMoveSettingsSection->SetEndTime(EndTime);

	FScopedTransaction ImportPostMoveSettingsTransaction(NSLOCTEXT("PostMoveSettingsPropertyTrackEditor", "ImportTransaction", "Import post move settings from file"));
	PostMoveSettingsTrack->Modify();
	PostMoveSettingsTrack->RemoveAllAnimationData();
	PostMoveSettingsTrack->AddSection(*PostMoveSettingsSection);
	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}


void FComposurePostMoveSettingsPropertyTrackEditor::ImportCanceled()
{
	TSharedPtr<SWindow> DialogPinned = ImportDialog.Pin();
	if (DialogPinned.IsValid())
	{
		FSlateApplication::Get().RequestDestroyWindow(DialogPinned.ToSharedRef());
	}
}


void FComposurePostMoveSettingsPropertyTrackEditor::GenerateKeysFromPropertyChanged(const FPropertyChangedParams& PropertyChangedParams, TArray<FComposurePostMoveSettingsKey>& NewGeneratedKeys, TArray<FComposurePostMoveSettingsKey>& DefaultGeneratedKeys)
{
	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;
	FComposurePostMoveSettings PostMoveSettings = PropertyChangedParams.GetPropertyValue<FComposurePostMoveSettings>();

	TArray<FComposurePostMoveSettingsKey>& PivotKeys = ChannelName == NAME_None || ChannelName == GET_MEMBER_NAME_CHECKED(FComposurePostMoveSettings, Pivot) ? NewGeneratedKeys : DefaultGeneratedKeys;
	PivotKeys.Add(FComposurePostMoveSettingsKey(EComposurePostMoveSettingsChannel::Pivot, EComposurePostMoveSettingsAxis::X, PostMoveSettings.Pivot.X));
	PivotKeys.Add(FComposurePostMoveSettingsKey(EComposurePostMoveSettingsChannel::Pivot, EComposurePostMoveSettingsAxis::Y, PostMoveSettings.Pivot.Y));

	TArray<FComposurePostMoveSettingsKey>& TranslateKeys = ChannelName == NAME_None || ChannelName == GET_MEMBER_NAME_CHECKED(FComposurePostMoveSettings, Translation) ? NewGeneratedKeys : DefaultGeneratedKeys;
	TranslateKeys.Add(FComposurePostMoveSettingsKey(EComposurePostMoveSettingsChannel::Translation, EComposurePostMoveSettingsAxis::X, PostMoveSettings.Translation.X));
	TranslateKeys.Add(FComposurePostMoveSettingsKey(EComposurePostMoveSettingsChannel::Translation, EComposurePostMoveSettingsAxis::Y, PostMoveSettings.Translation.Y));

	TArray<FComposurePostMoveSettingsKey>& RotationKeys = ChannelName == NAME_None || ChannelName == GET_MEMBER_NAME_CHECKED(FComposurePostMoveSettings, RotationAngle) ? NewGeneratedKeys : DefaultGeneratedKeys;
	RotationKeys.Add(FComposurePostMoveSettingsKey(EComposurePostMoveSettingsChannel::RotationAngle, EComposurePostMoveSettingsAxis::None, PostMoveSettings.RotationAngle));

	TArray<FComposurePostMoveSettingsKey>& ScaleKeys = ChannelName == NAME_None || ChannelName == GET_MEMBER_NAME_CHECKED(FComposurePostMoveSettings, Scale) ? NewGeneratedKeys : DefaultGeneratedKeys;
	ScaleKeys.Add(FComposurePostMoveSettingsKey(EComposurePostMoveSettingsChannel::Scale, EComposurePostMoveSettingsAxis::None, PostMoveSettings.Scale));
}

#undef LOCTEXT_NAMESPACE