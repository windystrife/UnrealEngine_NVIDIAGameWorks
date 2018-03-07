// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolHelpers.h"
#include "MovieScene.h"
#include "Layout/Margin.h"
#include "Misc/Paths.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "AssetData.h"
#include "Containers/ArrayView.h"
#include "ISequencer.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SComboBox.h"
#include "ScopedTransaction.h"
#include "EditorStyleSet.h"
#include "EditorDirectories.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "LevelSequence.h"
#include "AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "MovieSceneCaptureHelpers.h"
#include "FbxImporter.h"
#include "MatineeImportTools.h"
#include "MovieSceneToolsProjectSettings.h"
#include "MovieSceneToolsUserSettings.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Math/UnitConversion.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"
#include "LevelEditorViewport.h"

/* MovieSceneToolHelpers
 *****************************************************************************/

void MovieSceneToolHelpers::TrimSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time, bool bTrimLeft)
{
	for (auto Section : Sections)
	{
		if (Section.IsValid())
		{
			Section->TrimSection(Time, bTrimLeft);
		}
	}
}


void MovieSceneToolHelpers::SplitSection(const TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections, float Time)
{
	for (auto Section : Sections)
	{
		if (Section.IsValid())
		{
			Section->SplitSection(Time);
		}
	}
}

bool MovieSceneToolHelpers::ParseShotName(const FString& ShotName, FString& ShotPrefix, uint32& ShotNumber, uint32& TakeNumber)
{
	// Parse a shot name
	// 
	// sht010:
	//  ShotPrefix = sht
	//  ShotNumber = 10
	//  TakeNumber = 1 (default)
	// 
	// sp020_002
	//  ShotPrefix = sp
	//  ShotNumber = 20
	//  TakeNumber = 2
	//
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	uint32 FirstShotNumberIndex = INDEX_NONE;
	uint32 LastShotNumberIndex = INDEX_NONE;
	bool bInShotNumber = false;

	uint32 FirstTakeNumberIndex = INDEX_NONE;
	uint32 LastTakeNumberIndex = INDEX_NONE;
	bool bInTakeNumber = false;

	bool bFoundTakeSeparator = false;
	TakeNumber = ProjectSettings->FirstTakeNumber;

	for (int32 CharIndex = 0; CharIndex < ShotName.Len(); ++CharIndex)
	{
		if (FChar::IsDigit(ShotName[CharIndex]))
		{
			// Find shot number indices
			if (FirstShotNumberIndex == INDEX_NONE)
			{
				bInShotNumber = true;
				FirstShotNumberIndex = CharIndex;
			}
			if (bInShotNumber)
			{
				LastShotNumberIndex = CharIndex;
			}

			if (FirstShotNumberIndex != INDEX_NONE && LastShotNumberIndex != INDEX_NONE)
			{
				if (bFoundTakeSeparator)
				{
					// Find take number indices
					if (FirstTakeNumberIndex == INDEX_NONE)
					{
						bInTakeNumber = true;
						FirstTakeNumberIndex = CharIndex;
					}
					if (bInTakeNumber)
					{
						LastTakeNumberIndex = CharIndex;
					}
				}
			}
		}

		if (FirstShotNumberIndex != INDEX_NONE && LastShotNumberIndex != INDEX_NONE)
		{
			if (ShotName[CharIndex] == ProjectSettings->TakeSeparator[0])
			{
				bFoundTakeSeparator = true;
			}
		}
	}

	if (FirstShotNumberIndex != INDEX_NONE)
	{
		ShotPrefix = ShotName.Left(FirstShotNumberIndex);
		ShotNumber = FCString::Atoi(*ShotName.Mid(FirstShotNumberIndex, LastShotNumberIndex-FirstShotNumberIndex+1));
	}

	if (FirstTakeNumberIndex != INDEX_NONE)
	{
		TakeNumber = FCString::Atoi(*ShotName.Mid(FirstTakeNumberIndex, LastTakeNumberIndex-FirstTakeNumberIndex+1));
	}

	return FirstShotNumberIndex != INDEX_NONE;
}


FString MovieSceneToolHelpers::ComposeShotName(const FString& ShotPrefix, uint32 ShotNumber, uint32 TakeNumber)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	FString ShotName = ShotPrefix;

	FString ShotFormat = TEXT("%0") + FString::Printf(TEXT("%d"), ProjectSettings->ShotNumDigits) + TEXT("d");
	FString TakeFormat = TEXT("%0") + FString::Printf(TEXT("%d"), ProjectSettings->TakeNumDigits) + TEXT("d");

	ShotName += FString::Printf(*ShotFormat, ShotNumber);
	if (TakeNumber != INDEX_NONE)
	{
		ShotName += ProjectSettings->TakeSeparator;
		ShotName += FString::Printf(*TakeFormat, TakeNumber);
	}
	return ShotName;
}

bool IsPackageNameUnique(const TArray<FAssetData>& ObjectList, FString& NewPackageName)
{
	for (auto AssetObject : ObjectList)
	{
		if (AssetObject.PackageName.ToString() == NewPackageName)
		{
			return false;
		}
	}
	return true;
}

FString MovieSceneToolHelpers::GenerateNewShotPath(UMovieScene* SequenceMovieScene, FString& NewShotName)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> ObjectList;
	AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

	UObject* SequenceAsset = SequenceMovieScene->GetOuter();
	UPackage* SequencePackage = SequenceAsset->GetOutermost();
	FString SequencePackageName = SequencePackage->GetName(); // ie. /Game/cine/max/master
	int LastSlashPos = SequencePackageName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	FString SequencePath = SequencePackageName.Left(LastSlashPos);

	FString NewShotPrefix;
	uint32 NewShotNumber = INDEX_NONE;
	uint32 NewTakeNumber = INDEX_NONE;
	ParseShotName(NewShotName, NewShotPrefix, NewShotNumber, NewTakeNumber);

	FString NewShotDirectory = ComposeShotName(NewShotPrefix, NewShotNumber, INDEX_NONE);
	FString NewShotPath = SequencePath;

	FString ShotDirectory = ProjectSettings->ShotDirectory;
	if (!ShotDirectory.IsEmpty())
	{
		NewShotPath /= ShotDirectory;
	}
	NewShotPath /= NewShotDirectory; // put this in the shot directory, ie. /Game/cine/max/shots/shot0010

	// Make sure this shot path is unique
	FString NewPackageName = NewShotPath;
	NewPackageName /= NewShotName; // ie. /Game/cine/max/shots/shot0010/shot0010_001
	if (!IsPackageNameUnique(ObjectList, NewPackageName))
	{
		while (1)
		{
			NewShotNumber += ProjectSettings->ShotIncrement;
			NewShotName = ComposeShotName(NewShotPrefix, NewShotNumber, NewTakeNumber);
			NewShotDirectory = ComposeShotName(NewShotPrefix, NewShotNumber, INDEX_NONE);
			NewShotPath = SequencePath;
			if (!ShotDirectory.IsEmpty())
			{
				NewShotPath /= ShotDirectory;
			}
			NewShotPath /= NewShotDirectory;

			NewPackageName = NewShotPath;
			NewPackageName /= NewShotName;
			if (IsPackageNameUnique(ObjectList, NewPackageName))
			{
				break;
			}
		}
	}

	return NewShotPath;
}


FString MovieSceneToolHelpers::GenerateNewShotName(const TArray<UMovieSceneSection*>& AllSections, float Time)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	UMovieSceneCinematicShotSection* BeforeShot = nullptr;
	UMovieSceneCinematicShotSection* NextShot = nullptr;

	float MinEndDiff = FLT_MAX;
	float MinStartDiff = FLT_MAX;
	for (auto Section : AllSections)
	{
		if (Section->GetEndTime() >= Time)
		{
			float EndDiff = Section->GetEndTime() - Time;
			if (MinEndDiff > EndDiff)
			{
				MinEndDiff = EndDiff;
				BeforeShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}
		if (Section->GetStartTime() <= Time)
		{
			float StartDiff = Time - Section->GetStartTime();
			if (MinStartDiff > StartDiff)
			{
				MinStartDiff = StartDiff;
				NextShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}
	}
	
	// There aren't any shots, let's create the first shot name
	if (BeforeShot == nullptr || NextShot == nullptr)
	{
		// Default case
	}
	// This is the last shot
	else if (BeforeShot == NextShot)
	{
		FString NextShotPrefix = ProjectSettings->ShotPrefix;
		uint32 NextShotNumber = ProjectSettings->FirstShotNumber;
		uint32 NextTakeNumber = ProjectSettings->FirstTakeNumber;

		if (ParseShotName(NextShot->GetShotDisplayName().ToString(), NextShotPrefix, NextShotNumber, NextTakeNumber))
		{
			uint32 NewShotNumber = NextShotNumber + ProjectSettings->ShotIncrement;
			return ComposeShotName(NextShotPrefix, NewShotNumber, ProjectSettings->FirstTakeNumber);
		}
	}
	// This is in between two shots
	else 
	{
		FString BeforeShotPrefix = ProjectSettings->ShotPrefix;
		uint32 BeforeShotNumber = ProjectSettings->FirstShotNumber;
		uint32 BeforeTakeNumber = ProjectSettings->FirstTakeNumber;

		FString NextShotPrefix = ProjectSettings->ShotPrefix;
		uint32 NextShotNumber = ProjectSettings->FirstShotNumber;
		uint32 NextTakeNumber = ProjectSettings->FirstTakeNumber;

		if (ParseShotName(BeforeShot->GetShotDisplayName().ToString(), BeforeShotPrefix, BeforeShotNumber, BeforeTakeNumber) &&
			ParseShotName(NextShot->GetShotDisplayName().ToString(), NextShotPrefix, NextShotNumber, NextTakeNumber))
		{
			if (BeforeShotNumber < NextShotNumber)
			{
				uint32 NewShotNumber = BeforeShotNumber + ( (NextShotNumber - BeforeShotNumber) / 2); // what if we can't find one? or conflicts with another?
				return ComposeShotName(BeforeShotPrefix, NewShotNumber, ProjectSettings->FirstTakeNumber);
			}
		}
	}

	// Default case
	return ComposeShotName(ProjectSettings->ShotPrefix, ProjectSettings->FirstShotNumber, ProjectSettings->FirstTakeNumber);
}

void MovieSceneToolHelpers::GatherTakes(const UMovieSceneSection* Section, TArray<uint32>& TakeNumbers, uint32& CurrentTakeNumber)
{
	const UMovieSceneCinematicShotSection* Shot = Cast<const UMovieSceneCinematicShotSection>(Section);
	
	if (Shot->GetSequence() == nullptr)
	{
		return;
	}

	FAssetData ShotData(Shot->GetSequence()->GetOuter());

	FString ShotPackagePath = ShotData.PackagePath.ToString();

	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	CurrentTakeNumber = INDEX_NONE;

	if (ParseShotName(Shot->GetShotDisplayName().ToString(), ShotPrefix, ShotNumber, CurrentTakeNumber))
	{
		// Gather up all level sequence assets
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ObjectList;
		AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

		for (auto AssetObject : ObjectList)
		{
			FString AssetPackagePath = AssetObject.PackagePath.ToString();

			if (AssetPackagePath == ShotPackagePath)
			{
				FString AssetShotPrefix;
				uint32 AssetShotNumber = INDEX_NONE;
				uint32 AssetTakeNumber = INDEX_NONE;

				if (ParseShotName(AssetObject.AssetName.ToString(), AssetShotPrefix, AssetShotNumber, AssetTakeNumber))
				{
					if (AssetShotPrefix == ShotPrefix && AssetShotNumber == ShotNumber)
					{
						TakeNumbers.Add(AssetTakeNumber);
					}
				}
			}
		}
	}

	TakeNumbers.Sort();
}

UObject* MovieSceneToolHelpers::GetTake(const UMovieSceneSection* Section, uint32 TakeNumber)
{
	const UMovieSceneCinematicShotSection* Shot = Cast<const UMovieSceneCinematicShotSection>(Section);

	FAssetData ShotData(Shot->GetSequence()->GetOuter());

	FString ShotPackagePath = ShotData.PackagePath.ToString();
	int32 ShotLastSlashPos = INDEX_NONE;
	ShotPackagePath.FindLastChar(TCHAR('/'), ShotLastSlashPos);
	ShotPackagePath = ShotPackagePath.Left(ShotLastSlashPos);

	FString ShotPrefix;
	uint32 ShotNumber = INDEX_NONE;
	uint32 TakeNumberDummy = INDEX_NONE;

	if (ParseShotName(Shot->GetShotDisplayName().ToString(), ShotPrefix, ShotNumber, TakeNumberDummy))
	{
		// Gather up all level sequence assets
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ObjectList;
		AssetRegistryModule.Get().GetAssetsByClass(ULevelSequence::StaticClass()->GetFName(), ObjectList);

		for (auto AssetObject : ObjectList)
		{
			FString AssetPackagePath = AssetObject.PackagePath.ToString();
			int32 AssetLastSlashPos = INDEX_NONE;
			AssetPackagePath.FindLastChar(TCHAR('/'), AssetLastSlashPos);
			AssetPackagePath = AssetPackagePath.Left(AssetLastSlashPos);

			if (AssetPackagePath == ShotPackagePath)
			{
				FString AssetShotPrefix;
				uint32 AssetShotNumber = INDEX_NONE;
				uint32 AssetTakeNumber = INDEX_NONE;

				if (ParseShotName(AssetObject.AssetName.ToString(), AssetShotPrefix, AssetShotNumber, AssetTakeNumber))
				{
					if (AssetShotPrefix == ShotPrefix && AssetShotNumber == ShotNumber && TakeNumber == AssetTakeNumber)
					{
						return AssetObject.GetAsset();
					}
				}
			}
		}
	}

	return nullptr;
}

class SEnumCombobox : public SComboBox<TSharedPtr<int32>>
{
public:
	SLATE_BEGIN_ARGS(SEnumCombobox) {}

	SLATE_ATTRIBUTE(int32, CurrentValue)
	SLATE_ARGUMENT(FOnEnumSelectionChanged, OnEnumSelectionChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UEnum* InEnum)
	{
		Enum = InEnum;
		CurrentValue = InArgs._CurrentValue;
		check(CurrentValue.IsBound());
		OnEnumSelectionChangedDelegate = InArgs._OnEnumSelectionChanged;

		bUpdatingSelectionInternally = false;

		for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
		{
			if (Enum->HasMetaData( TEXT("Hidden"), i ) == false)
			{
				VisibleEnumNameIndices.Add(MakeShareable(new int32(i)));
			}
		}

		SComboBox::Construct(SComboBox<TSharedPtr<int32>>::FArguments()
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Light")
			.OptionsSource(&VisibleEnumNameIndices)
			.OnGenerateWidget_Lambda([this](TSharedPtr<int32> InItem)
			{
				return SNew(STextBlock)
					.Text(Enum->GetDisplayNameTextByIndex(*InItem));
			})
			.OnSelectionChanged(this, &SEnumCombobox::OnComboSelectionChanged)
			.OnComboBoxOpening(this, &SEnumCombobox::OnComboMenuOpening)
			.ContentPadding(FMargin(2, 0))
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
				.Text(this, &SEnumCombobox::GetCurrentValue)
			]);
	}

private:
	FText GetCurrentValue() const
	{
		int32 CurrentNameIndex = Enum->GetIndexByValue(CurrentValue.Get());
		return Enum->GetDisplayNameTextByIndex(CurrentNameIndex);
	}

	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<int32> InItem)
	{
		return SNew(STextBlock)
			.Text(Enum->GetDisplayNameTextByIndex(*InItem));
	}

	void OnComboSelectionChanged(TSharedPtr<int32> InSelectedItem, ESelectInfo::Type SelectInfo)
	{
		if (bUpdatingSelectionInternally == false)
		{
			OnEnumSelectionChangedDelegate.ExecuteIfBound(*InSelectedItem, SelectInfo);
		}
	}

	void OnComboMenuOpening()
	{
		int32 CurrentNameIndex = Enum->GetIndexByValue(CurrentValue.Get());
		TSharedPtr<int32> FoundNameIndexItem;
		for ( int32 i = 0; i < VisibleEnumNameIndices.Num(); i++ )
		{
			if ( *VisibleEnumNameIndices[i] == CurrentNameIndex )
			{
				FoundNameIndexItem = VisibleEnumNameIndices[i];
				break;
			}
		}
		if ( FoundNameIndexItem.IsValid() )
		{
			bUpdatingSelectionInternally = true;
			SetSelectedItem(FoundNameIndexItem);
			bUpdatingSelectionInternally = false;
		}
	}	

private:
	const UEnum* Enum;

	TAttribute<int32> CurrentValue;

	TArray<TSharedPtr<int32>> VisibleEnumNameIndices;

	bool bUpdatingSelectionInternally;

	FOnEnumSelectionChanged OnEnumSelectionChangedDelegate;
};

TSharedRef<SWidget> MovieSceneToolHelpers::MakeEnumComboBox(const UEnum* InEnum, TAttribute<int32> InCurrentValue, FOnEnumSelectionChanged InOnSelectionChanged)
{
	return SNew(SEnumCombobox, InEnum)
		.CurrentValue(InCurrentValue)
		.OnEnumSelectionChanged(InOnSelectionChanged);
}

bool MovieSceneToolHelpers::ShowImportEDLDialog(UMovieScene* InMovieScene, float InFrameRate, FString InOpenDirectory)
{
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("CMX 3600 EDL (*.edl)|*.edl|");

		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("MovieSceneToolHelpers", "ImportEDL", "Import EDL from...").ToString(), 
			InOpenDirectory,
			TEXT(""), 
			*ExtensionStr,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}
	if (!bOpen)
	{
		return false;
	}

	if (!OpenFilenames.Num())
	{
		return false;
	}

	const FScopedTransaction Transaction( NSLOCTEXT( "MovieSceneTools", "ImportEDLTransaction", "Import EDL" ) );

	return MovieSceneCaptureHelpers::ImportEDL(InMovieScene, InFrameRate, OpenFilenames[0]);
}

bool MovieSceneToolHelpers::ShowExportEDLDialog(const UMovieScene* InMovieScene, float InFrameRate, FString InSaveDirectory, int32 InHandleFrames)
{
	TArray<FString> SaveFilenames;
	FString SequenceName = InMovieScene->GetOuter()->GetName();

	// Pop open a dialog to request the location of the edl
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bSave = false;
	if (DesktopPlatform)
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("CMX 3600 EDL (*.edl)|*.edl|");
		ExtensionStr += TEXT("RV (*.rv)|*.rv|");

		bSave = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("MovieSceneTools", "ExportEDL", "Export EDL to...").ToString(), 
			InSaveDirectory,
			SequenceName + TEXT(".edl"), 
			*ExtensionStr,
			EFileDialogFlags::None,
			SaveFilenames
			);
	}
	if (!bSave)
	{
		return false;
	}

	if (!SaveFilenames.Num())
	{
		return false;
	}

	if (MovieSceneCaptureHelpers::ExportEDL(InMovieScene, InFrameRate, SaveFilenames[0], InHandleFrames))
	{
		const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(SaveFilenames[0]);
		const FString SaveDirectory = FPaths::GetPath(AbsoluteFilename);

		FNotificationInfo NotificationInfo(NSLOCTEXT("MovieSceneTools", "EDLExportFinished", "EDL Export finished"));
		NotificationInfo.ExpireDuration = 5.f;
		NotificationInfo.Hyperlink = FSimpleDelegate::CreateStatic( [](FString InDirectory) { FPlatformProcess::ExploreFolder( *InDirectory ); }, SaveDirectory);
		NotificationInfo.HyperlinkText = NSLOCTEXT("MovieSceneTools", "OpenEDLExportFolder", "Open EDL Export Folder...");
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		
		return true;
	}

	return false;
}

bool ImportFBXProperty(FString NodeName, FString AnimatedPropertyName, FGuid ObjectBinding, UnFbx::FFbxCurvesAPI& CurveAPI, UMovieScene* InMovieScene, ISequencer& InSequencer)
{
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	TArrayView<TWeakObjectPtr<>> BoundObjects = InSequencer.FindBoundObjects(ObjectBinding, InSequencer.GetFocusedTemplateID());

	for (auto FbxSetting : ProjectSettings->FbxSettings)
	{
		if (FCString::Strcmp(*FbxSetting.FbxPropertyName.ToUpper(), *AnimatedPropertyName.ToUpper()) != 0)
		{
			continue;
		}

		for (TWeakObjectPtr<>& WeakObject : BoundObjects)
		{
			UObject* FoundObject = WeakObject.Get();

			if (!FoundObject)
			{
				continue;
			}
			
			UObject* PropertyOwner = FoundObject;
			if (!FbxSetting.PropertyPath.ComponentName.IsEmpty())
			{
				PropertyOwner = FindObjectFast<UObject>(FoundObject, *FbxSetting.PropertyPath.ComponentName);
			}

			if (!PropertyOwner)
			{
				continue;
			}
		
			FGuid PropertyOwnerGuid = InSequencer.GetHandleToObject(PropertyOwner);
			if (!PropertyOwnerGuid.IsValid())
			{
				continue;
			}

			UMovieSceneFloatTrack* FloatTrack = InMovieScene->FindTrack<UMovieSceneFloatTrack>(PropertyOwnerGuid, *FbxSetting.PropertyPath.PropertyName);
			if (!FloatTrack)
			{
				FString PropertyPath = FbxSetting.PropertyPath.ComponentName + TEXT(".") + FbxSetting.PropertyPath.PropertyName;
				InMovieScene->Modify();
				FloatTrack = InMovieScene->AddTrack<UMovieSceneFloatTrack>(PropertyOwnerGuid);
				FloatTrack->SetPropertyNameAndPath(*FbxSetting.PropertyPath.PropertyName, PropertyPath);
			}

			if (FloatTrack)
			{
				bool bSectionAdded = false;
				UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->FindOrAddSection(0.f, bSectionAdded));
				if (!FloatSection)
				{
					continue;
				}

				if (bSectionAdded)
				{
					FloatSection->SetIsInfinite(true);
				}

				float MinTime = FLT_MAX;
				float MaxTime = -FLT_MAX;

				const int32 ChannelIndex = 0;
				const int32 CompositeIndex = 0;
				FInterpCurveFloat CurveHandle;
				const bool bNegative = false;
				CurveAPI.GetCurveData(NodeName, AnimatedPropertyName, ChannelIndex, CompositeIndex, CurveHandle, bNegative);

				FRichCurve& FloatCurve = FloatSection->GetFloatCurve();
				FloatCurve.Reset();
				for (int32 KeyIndex = 0; KeyIndex < CurveHandle.Points.Num(); ++KeyIndex)
				{
					MinTime = FMath::Min(MinTime, CurveHandle.Points[KeyIndex].InVal);
					MaxTime = FMath::Max(MaxTime, CurveHandle.Points[KeyIndex].InVal);

					float ArriveTangent = CurveHandle.Points[KeyIndex].ArriveTangent;
					if (KeyIndex > 0)
					{
						ArriveTangent = ArriveTangent / (CurveHandle.Points[KeyIndex].InVal - CurveHandle.Points[KeyIndex-1].InVal);
					}
					
					float LeaveTangent = CurveHandle.Points[KeyIndex].LeaveTangent;
					if (KeyIndex < CurveHandle.Points.Num() - 1)
					{
						LeaveTangent = LeaveTangent / (CurveHandle.Points[KeyIndex+1].InVal - CurveHandle.Points[KeyIndex].InVal);
					}

					FMatineeImportTools::SetOrAddKey(FloatCurve, CurveHandle.Points[KeyIndex].InVal, CurveHandle.Points[KeyIndex].OutVal, ArriveTangent, LeaveTangent, CurveHandle.Points[KeyIndex].InterpMode);
				}

				FloatCurve.RemoveRedundantKeys(KINDA_SMALL_NUMBER);
				FloatCurve.AutoSetTangents();

				FloatSection->SetStartTime(MinTime);
				FloatSection->SetEndTime(MaxTime);
				return true;
			}
		}
	}
	return false;
}

bool ImportFBXTransform(FString NodeName, FGuid ObjectBinding, UnFbx::FFbxCurvesAPI& CurveAPI, UMovieScene* InMovieScene)
{
	// Look for transforms explicitly
	FInterpCurveFloat Translation[3];
	FInterpCurveFloat EulerRotation[3];
	FInterpCurveFloat Scale[3];
	FTransform DefaultTransform;
	CurveAPI.GetConvertedTransformCurveData(NodeName, Translation[0], Translation[1], Translation[2], EulerRotation[0], EulerRotation[1], EulerRotation[2], Scale[0], Scale[1], Scale[2], DefaultTransform);

	UMovieScene3DTransformTrack* TransformTrack = InMovieScene->FindTrack<UMovieScene3DTransformTrack>(ObjectBinding); 
	if (!TransformTrack)
	{
		InMovieScene->Modify();
		TransformTrack = InMovieScene->AddTrack<UMovieScene3DTransformTrack>(ObjectBinding);
	}

	bool bSectionAdded = false;
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(TransformTrack->FindOrAddSection(0.f, bSectionAdded));
	if (!TransformSection)
	{
		return false;
	}

	if (bSectionAdded)
	{
		TransformSection->SetIsInfinite(true);
	}

	for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
	{
		EAxis::Type ChannelAxis = EAxis::X;
		if (ChannelIndex == 1)
		{
			ChannelAxis = EAxis::Y;
		}
		else if (ChannelIndex == 2)
		{
			ChannelAxis = EAxis::Z;
		}
		TransformSection->GetTranslationCurve(ChannelAxis).SetDefaultValue(DefaultTransform.GetLocation()[ChannelIndex]);
		TransformSection->GetRotationCurve(ChannelAxis).SetDefaultValue(DefaultTransform.GetRotation().Euler()[ChannelIndex]);
		TransformSection->GetScaleCurve(ChannelAxis).SetDefaultValue(DefaultTransform.GetScale3D()[ChannelIndex]);
	}

	float MinTime = FLT_MAX;
	float MaxTime = -FLT_MAX;

	const int NumCurves = 3; // Trans, Rot, Scale
	for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
	{
		for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
		{
			EAxis::Type ChannelAxis = EAxis::X;
			if (ChannelIndex == 1)
			{
				ChannelAxis = EAxis::Y;
			}
			else if (ChannelIndex == 2)
			{
				ChannelAxis = EAxis::Z;
			}
	
			FInterpCurveFloat* CurveFloat = nullptr;
			FRichCurve* ChannelCurve = nullptr;
			bool bNegative = false;

			if (CurveIndex == 0)
			{
				CurveFloat = &Translation[ChannelIndex];
				ChannelCurve = &TransformSection->GetTranslationCurve(ChannelAxis);
				if (ChannelIndex == 1)
				{
					bNegative = true;
				}
			}
			else if (CurveIndex == 1)
			{
				CurveFloat = &EulerRotation[ChannelIndex];
				ChannelCurve = &TransformSection->GetRotationCurve(ChannelAxis);
				if (ChannelIndex == 1 || ChannelIndex == 2)
				{
					bNegative = true;
				}
			}
			else if (CurveIndex == 2)
			{
				CurveFloat = &Scale[ChannelIndex];
				ChannelCurve = &TransformSection->GetScaleCurve(ChannelAxis);
			}

			if (ChannelCurve != nullptr && CurveFloat != nullptr)
			{
				ChannelCurve->Reset();
				
				for (int32 KeyIndex = 0; KeyIndex < CurveFloat->Points.Num(); ++KeyIndex)
				{
					MinTime = FMath::Min(MinTime, CurveFloat->Points[KeyIndex].InVal);
					MaxTime = FMath::Max(MaxTime, CurveFloat->Points[KeyIndex].InVal);
					
					float ArriveTangent = CurveFloat->Points[KeyIndex].ArriveTangent;
					if (KeyIndex > 0)
					{
						ArriveTangent = ArriveTangent / (CurveFloat->Points[KeyIndex].InVal - CurveFloat->Points[KeyIndex-1].InVal);
					}
					
					float LeaveTangent = CurveFloat->Points[KeyIndex].LeaveTangent;
					if (KeyIndex < CurveFloat->Points.Num() - 1)
					{
						LeaveTangent = LeaveTangent / (CurveFloat->Points[KeyIndex+1].InVal - CurveFloat->Points[KeyIndex].InVal);
					}

					if (bNegative)
					{
						ArriveTangent = -ArriveTangent;
						LeaveTangent = -LeaveTangent;
					}

					FMatineeImportTools::SetOrAddKey(*ChannelCurve, CurveFloat->Points[KeyIndex].InVal, CurveFloat->Points[KeyIndex].OutVal, ArriveTangent, LeaveTangent, CurveFloat->Points[KeyIndex].InterpMode);
				}

				ChannelCurve->RemoveRedundantKeys(KINDA_SMALL_NUMBER);
				ChannelCurve->AutoSetTangents();
			}
		}
	}
		
	TransformSection->SetStartTime(MinTime);
	TransformSection->SetEndTime(MaxTime);
	return true;
}

bool ImportFBXNode(FString NodeName, UnFbx::FFbxCurvesAPI& CurveAPI, UMovieScene* InMovieScene, ISequencer& InSequencer, const TMap<FGuid, FString>& InObjectBindingMap)
{
	// Find the matching object binding to apply this animation to. Defaults to the first.
	FGuid ObjectBinding;
	for (auto It = InObjectBindingMap.CreateConstIterator(); It; ++It)
	{
		if (InObjectBindingMap.Num() == 1 || FCString::Strcmp(*It.Value().ToUpper(), *NodeName.ToUpper()) == 0)
		{
			ObjectBinding = It.Key();
			break;
		}
	}

	if (!ObjectBinding.IsValid())
	{
		//@todo output warning
		return false;
	}

	// Look for animated float properties
	TArray<FString> AnimatedPropertyNames;
	CurveAPI.GetNodeAnimatedPropertyNameArray(NodeName, AnimatedPropertyNames);
		
	for (auto AnimatedPropertyName : AnimatedPropertyNames)
	{
		ImportFBXProperty(NodeName, AnimatedPropertyName, ObjectBinding, CurveAPI, InMovieScene, InSequencer);
	}
	
	ImportFBXTransform(NodeName, ObjectBinding, CurveAPI, InMovieScene);

	return true;
}

void GetCameras( FbxNode* Parent, TArray<FbxCamera*>& Cameras )
{
	FbxCamera* Camera = Parent->GetCamera();
	if( Camera )
	{
		Cameras.Add(Camera);
	}

	int32 NodeCount = Parent->GetChildCount();
	for ( int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex )
	{
		FbxNode* Child = Parent->GetChild( NodeIndex );
		GetCameras(Child, Cameras);
	}
}

FbxCamera* FindCamera( FbxNode* Parent )
{
	FbxCamera* Camera = Parent->GetCamera();
	if( !Camera )
	{
		int32 NodeCount = Parent->GetChildCount();
		for ( int32 NodeIndex = 0; NodeIndex < NodeCount && !Camera; ++NodeIndex )
		{
			FbxNode* Child = Parent->GetChild( NodeIndex );
			Camera = Child->GetCamera();
		}
	}

	return Camera;
}

FbxNode* RetrieveObjectFromName(const TCHAR* ObjectName, FbxNode* Root)
{
	if (!Root)
	{
		return nullptr;
	}
	
	for (int32 ChildIndex=0;ChildIndex<Root->GetChildCount();++ChildIndex)
	{
		FbxNode* Node = Root->GetChild(ChildIndex);
		if (Node)
		{
			FString NodeName = FString(Node->GetName());

			if ( !FCString::Strcmp(ObjectName,UTF8_TO_TCHAR(Node->GetName())))
			{
				return Node;
			}

			if (FbxNode* NextNode = RetrieveObjectFromName(ObjectName,Node))
			{
				return NextNode;
			}
		}
	}

	return nullptr;
}


void ImportFBXCamera(UnFbx::FFbxImporter* FbxImporter, UMovieScene* InMovieScene, ISequencer& InSequencer, TMap<FGuid, FString>& InObjectBindingMap)
{
	const UMovieSceneUserImportFBXSettings* ImportFBXSettings = GetDefault<UMovieSceneUserImportFBXSettings>();
	if (ImportFBXSettings->bCreateCameras)
	{
		TArray<FbxCamera*> AllCameras;
		GetCameras(FbxImporter->Scene->GetRootNode(), AllCameras);

		// Find unmatched cameras
		TArray<FbxCamera*> UnmatchedCameras;
		for (auto Camera : AllCameras)
		{
			FString NodeName = FString(Camera->GetName());

			bool bMatched = false;
			for (auto InObjectBinding : InObjectBindingMap)
			{		
				FString ObjectName = InObjectBinding.Value;
				if ( !FCString::Strcmp(*ObjectName,UTF8_TO_TCHAR(Camera->GetName())))
				{
					bMatched = true;
					break;
				}
			}

			if (!bMatched)
			{
				UnmatchedCameras.Add(Camera);
			}
		}

		// Add any unmatched cameras
		UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
		for (auto UnmatchedCamera : UnmatchedCameras)
		{
			FString CameraName = FString(UnmatchedCamera->GetName());

			FActorSpawnParameters SpawnParams;
			SpawnParams.Name = *CameraName;
			ACineCameraActor* NewCamera = World->SpawnActor<ACineCameraActor>(SpawnParams);
			NewCamera->SetActorLabel(*CameraName);

			TArray<TWeakObjectPtr<AActor> > NewCameras;
			NewCameras.Add(NewCamera);
			TArray<FGuid> NewCameraGuids = InSequencer.AddActors(NewCameras);

			InObjectBindingMap.Add(NewCameraGuids[0]);
			InObjectBindingMap[NewCameraGuids[0]] = CameraName;
		}

		// refresh?
	}


	for (auto InObjectBinding : InObjectBindingMap)
	{
		TArrayView<TWeakObjectPtr<>> BoundObjects = InSequencer.FindBoundObjects(InObjectBinding.Key, InSequencer.GetFocusedTemplateID());
		
		FString ObjectName = InObjectBinding.Value;
		FbxCamera* CameraNode = nullptr;
		FbxNode* Node = RetrieveObjectFromName(*ObjectName, FbxImporter->Scene->GetRootNode());
		if (Node)
		{
			CameraNode = FindCamera(Node);
		}

		if (!CameraNode)
		{
			CameraNode = FindCamera(FbxImporter->Scene->GetRootNode());
			if (CameraNode)
			{
				UE_LOG(LogMovieScene, Warning, TEXT("Fbx Import: Failed to find exact matching camera for (%s). Using first camera from fbx (%s)"), *ObjectName, UTF8_TO_TCHAR(CameraNode->GetName()));
			}
			else
			{
				UE_LOG(LogMovieScene, Error, TEXT("Fbx Import: Failed to find any matching camera for (%s)."), *ObjectName);
				continue;
			}
		}

		if (!CameraNode)
		{
			continue;
		}

		float FieldOfView;
		float FocalLength;

		if (CameraNode->GetApertureMode() == FbxCamera::eFocalLength)
		{
			FocalLength = CameraNode->FocalLength.Get();
			FieldOfView = CameraNode->ComputeFieldOfView(FocalLength);
		}
		else
		{
			FieldOfView = CameraNode->FieldOfView.Get();
			FocalLength = CameraNode->ComputeFocalLength(FieldOfView);
		}

		float ApertureWidth = CameraNode->GetApertureWidth();
		float ApertureHeight = CameraNode->GetApertureHeight();

		for (TWeakObjectPtr<>& WeakObject : BoundObjects)
		{
			UObject* FoundObject = WeakObject.Get();
			if (FoundObject && FoundObject->IsA(ACineCameraActor::StaticClass()))
			{
				ACineCameraActor* CineCameraActor = Cast<ACineCameraActor>(FoundObject);
				UCineCameraComponent* CineCameraComponent = CineCameraActor->GetCineCameraComponent();

				CineCameraComponent->SetProjectionMode(CameraNode->ProjectionType.Get() == FbxCamera::ePerspective ? ECameraProjectionMode::Perspective : ECameraProjectionMode::Orthographic);
				CineCameraComponent->SetAspectRatio(CameraNode->AspectWidth.Get() / CameraNode->AspectHeight.Get());
				CineCameraComponent->SetOrthoNearClipPlane(CameraNode->NearPlane.Get());
				CineCameraComponent->SetOrthoFarClipPlane(CameraNode->FarPlane.Get());
				CineCameraComponent->SetOrthoWidth(CameraNode->OrthoZoom.Get());
				CineCameraComponent->SetFieldOfView(FieldOfView);
				CineCameraComponent->FilmbackSettings.SensorWidth = FUnitConversion::Convert(ApertureWidth, EUnit::Inches, EUnit::Millimeters);
				CineCameraComponent->FilmbackSettings.SensorHeight = FUnitConversion::Convert(ApertureHeight, EUnit::Inches, EUnit::Millimeters);
				if (FocalLength < CineCameraComponent->LensSettings.MinFocalLength)
				{
					CineCameraComponent->LensSettings.MinFocalLength = FocalLength;
				}
				if (FocalLength > CineCameraComponent->LensSettings.MaxFocalLength)
				{
					CineCameraComponent->LensSettings.MaxFocalLength = FocalLength;
				}

				// Set the default value of the current focal length section
				FGuid PropertyOwnerGuid = InSequencer.GetHandleToObject(CineCameraComponent);
				if (!PropertyOwnerGuid.IsValid())
				{
					continue;
				}

				UMovieSceneFloatTrack* FloatTrack = InMovieScene->FindTrack<UMovieSceneFloatTrack>(PropertyOwnerGuid, TEXT("CurrentFocalLength"));
				if (FloatTrack)
				{
					bool bSectionAdded = false;
					UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->FindOrAddSection(0.f, bSectionAdded));
					if (!FloatSection)
					{
						continue;
					}

					if (bSectionAdded)
					{
						FloatSection->SetIsInfinite(true);
					}

					FloatSection->SetDefault(FocalLength);
				}
			}
		}
	}
}


class SMovieSceneImportFBXSettings : public SCompoundWidget, public FGCObject
{
	SLATE_BEGIN_ARGS(SMovieSceneImportFBXSettings) {}
		SLATE_ARGUMENT(FString, ImportFilename)
		SLATE_ARGUMENT(UMovieScene*, MovieScene)
		SLATE_ARGUMENT(ISequencer*, Sequencer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ViewIdentifier = "Import FBX Settings";

		DetailView = PropertyEditor.CreateDetailView(DetailsViewArgs);

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			[
				DetailView.ToSharedRef()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(10, 5))
				.Text(NSLOCTEXT("MovieSceneTools", "ImportFBXButtonText", "Import"))
				.OnClicked(this, &SMovieSceneImportFBXSettings::OnImportFBXClicked)
			]
			
		];

		ImportFilename = InArgs._ImportFilename;
		MovieScene = InArgs._MovieScene;
		Sequencer = InArgs._Sequencer;

		UMovieSceneUserImportFBXSettings* ImportFBXSettings = GetMutableDefault<UMovieSceneUserImportFBXSettings>();
		DetailView->SetObject(ImportFBXSettings);
	}

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject(MovieScene);
		Collector.AddReferencedObject(Sequencer);
	}

	void SetObjectBindingMap(const TMap<FGuid, FString>& InObjectBindingMap)
	{
		ObjectBindingMap = InObjectBindingMap;
	}

private:

	FReply OnImportFBXClicked()
	{
		UMovieSceneUserImportFBXSettings* ImportFBXSettings = GetMutableDefault<UMovieSceneUserImportFBXSettings>();

		FEditorDirectories::Get().SetLastDirectory( ELastDirectory::FBX, FPaths::GetPath( ImportFilename ) ); // Save path as default for next time.
	
		UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();

		UnFbx::FBXImportOptions* ImportOptions = FbxImporter->GetImportOptions();
		bool bConvertSceneBackup = ImportOptions->bConvertScene;
		bool bConvertSceneUnitBackup = ImportOptions->bConvertSceneUnit;
		bool bForceFrontXAxisBackup = ImportOptions->bForceFrontXAxis;


		ImportOptions->bConvertScene = true;
		ImportOptions->bConvertSceneUnit = true;
		ImportOptions->bForceFrontXAxis = ImportFBXSettings->bForceFrontXAxis;

		const FString FileExtension = FPaths::GetExtension(ImportFilename);
		if (!FbxImporter->ImportFromFile(*ImportFilename, FileExtension, true))
		{
			// Log the error message and fail the import.
			FbxImporter->ReleaseScene();
			ImportOptions->bConvertScene = bConvertSceneBackup;
			ImportOptions->bConvertSceneUnit = bConvertSceneUnitBackup;
			ImportOptions->bForceFrontXAxis = bForceFrontXAxisBackup;
			return FReply::Unhandled();
		}

		const FScopedTransaction Transaction( NSLOCTEXT( "MovieSceneTools", "ImportFBXTransaction", "Import FBX" ) );

		// Import static cameras first
		ImportFBXCamera(FbxImporter, MovieScene, *Sequencer, ObjectBindingMap);

		UnFbx::FFbxCurvesAPI CurveAPI;
		FbxImporter->PopulateAnimatedCurveData(CurveAPI);
		TArray<FString> AllNodeNames;
		CurveAPI.GetAllNodeNameArray(AllNodeNames);

		for (FString NodeName : AllNodeNames)
		{
			ImportFBXNode(NodeName, CurveAPI, MovieScene, *Sequencer, ObjectBindingMap);
		}

		FbxImporter->ReleaseScene();
		ImportOptions->bConvertScene = bConvertSceneBackup;
		ImportOptions->bConvertSceneUnit = bConvertSceneUnitBackup;
		ImportOptions->bForceFrontXAxis = bForceFrontXAxisBackup;

		FWidgetPath WidgetPath;
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared(), WidgetPath);

		if ( Window.IsValid() )
		{
			Window->RequestDestroyWindow();
		}

		return FReply::Handled();
	}

	TSharedPtr<IDetailsView> DetailView;
	FString ImportFilename;
	UMovieScene* MovieScene;
	ISequencer* Sequencer;
	TMap<FGuid, FString> ObjectBindingMap;
};


bool MovieSceneToolHelpers::ImportFBX(UMovieScene* InMovieScene, ISequencer& InSequencer, const TMap<FGuid, FString>& InObjectBindingMap)
{
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("FBX (*.fbx)|*.fbx|");

		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("MovieSceneTools", "ImportFBX", "Import FBX from...").ToString(), 
			FEditorDirectories::Get().GetLastDirectory(ELastDirectory::FBX),
			TEXT(""), 
			*ExtensionStr,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}
	if (!bOpen)
	{
		return false;
	}

	if (!OpenFilenames.Num())
	{
		return false;
	}

	const FText TitleText = NSLOCTEXT("MovieSceneTools", "ImportFBXTitle", "Import FBX");

	// Create the window to choose our options
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(TitleText)
		.HasCloseButton(true)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(400.0f, 200.0f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false);

	TSharedRef<SMovieSceneImportFBXSettings> DialogWidget = SNew(SMovieSceneImportFBXSettings)
		.ImportFilename(OpenFilenames[0])
		.MovieScene(InMovieScene)
		.Sequencer(&InSequencer);
	DialogWidget->SetObjectBindingMap(InObjectBindingMap);
	Window->SetContent(DialogWidget);

	FSlateApplication::Get().AddWindow(Window);

	return true;
}
