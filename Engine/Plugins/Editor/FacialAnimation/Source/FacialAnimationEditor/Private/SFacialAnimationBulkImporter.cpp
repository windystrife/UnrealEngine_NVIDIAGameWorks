// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SFacialAnimationBulkImporter.h"
#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "DesktopPlatformModule.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/Paths.h"
#include "Dialogs/DlgPickPath.h"
#include "FacialAnimationBulkImporterSettings.h"
#include "FbxAnimUtils.h"
#include "Logging/MessageLog.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Factories/SoundFactory.h"
#include "IDetailsView.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Curves/RichCurve.h"
#include "Sound/SoundWave.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Utils.h"
#include "Engine/CurveTable.h"
#include "Misc/PackageName.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "SFacialAnimationBulkImporter"

void SFacialAnimationBulkImporter::Construct(const FArguments& InArgs)
{
	FDetailsViewArgs DetailsViewArgs(false, false, true, FDetailsViewArgs::HideNameArea);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(GetMutableDefault<UFacialAnimationBulkImporterSettings>());

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
		.Padding(4.0f)
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
			.ForegroundColor(FLinearColor::White)
			.ContentPadding(FMargin(6, 2))
			.IsEnabled(this, &SFacialAnimationBulkImporter::IsImportButtonEnabled)
			.OnClicked(this, &SFacialAnimationBulkImporter::HandleImportClicked)
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
				.Text(LOCTEXT("ImportAllButton", "Import All"))
			]
		]
	];
}

bool SFacialAnimationBulkImporter::IsImportButtonEnabled() const
{
	const UFacialAnimationBulkImporterSettings* FacialAnimationBulkImporterSettings = GetDefault<UFacialAnimationBulkImporterSettings>();
	return !FacialAnimationBulkImporterSettings->SourceImportPath.Path.IsEmpty() && !FacialAnimationBulkImporterSettings->TargetImportPath.Path.IsEmpty();
}

struct FImportItem
{
	bool Import()
	{
		if (!FbxFile.IsEmpty() && !WaveFile.IsEmpty())
		{
			return ImportCurvesEmbeddedInSoundWave();
		}

		return false;
	}

private:
	USoundWave* ImportSoundWave(const FString& InSoundWavePackageName, const FString& InSoundWaveAssetName, const FString& InWavFilename) const
	{
		// Find or create the package to host the sound wave
		UPackage* const SoundWavePackage = CreatePackage(nullptr, *InSoundWavePackageName);
		if (!SoundWavePackage)
		{
			FMessageLog("Import").Error(FText::Format(LOCTEXT("SoundWavePackageError", "Failed to create a sound wave package '{0}'."), FText::FromString(InSoundWavePackageName)));
			return nullptr;
		}

		// Make sure the destination package is loaded
		SoundWavePackage->FullyLoad();

		// We set the correct options in the constructor, so run the import silently
		USoundFactory* SoundWaveFactory = NewObject<USoundFactory>();
		SoundWaveFactory->SuppressImportOverwriteDialog();

		// Perform the actual import
		USoundWave* const SoundWave = ImportObject<USoundWave>(SoundWavePackage, *InSoundWaveAssetName, RF_Public | RF_Standalone, *InWavFilename, nullptr, SoundWaveFactory);
		if (!SoundWave)
		{
			FMessageLog("Import").Error(FText::Format(LOCTEXT("SoundWaveImportError", "Failed to import the sound wave asset '{0}.{1}' from '{2}'"), FText::FromString(InSoundWavePackageName), FText::FromString(InSoundWaveAssetName), FText::FromString(InWavFilename)));
			return nullptr;
		}

		// Compress to whatever formats the active target platforms want prior to saving the asset
		{
			ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
			if (TPM)
			{
				const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();
				for (ITargetPlatform* Platform : Platforms)
				{
					SoundWave->GetCompressedData(Platform->GetWaveFormat(SoundWave));
				}
			}
		}

		FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistry.AssetCreated(SoundWave);

		return SoundWave;
	}

	bool ImportCurvesEmbeddedInSoundWave()
	{
		// find/create our sound wave
		USoundWave* const SoundWave = ImportSoundWave(TargetPackageName, TargetAssetName, WaveFile);
		if (SoundWave)
		{
			// create our curve table internal to the sound wave
			static const FName InternalCurveTableName("InternalCurveTable");
			SoundWave->Curves = NewObject<UCurveTable>(SoundWave, InternalCurveTableName);
			SoundWave->Curves->ClearFlags(RF_Public | RF_Standalone);
			SoundWave->Curves->SetFlags(SoundWave->Curves->GetFlags() | RF_Transactional);
			SoundWave->InternalCurves = SoundWave->Curves;

			// import curves from FBX
			float PreRollTime = 0.0f;
			if (FbxAnimUtils::ImportCurveTableFromNode(FbxFile, GetDefault<UFacialAnimationBulkImporterSettings>()->CurveNodeName, SoundWave->Curves, PreRollTime))
			{
				// we will need to add a curve to tell us the time we want to start playing audio
				FRichCurve* AudioCurve = SoundWave->Curves->RowMap.Add(TEXT("Audio"), new FRichCurve());
				AudioCurve->AddKey(PreRollTime, 1.0f);

				return true;
			}
		}

		return false;
	}

public:
	FString FbxFile;
	FString WaveFile;
	FString TargetPackageName;
	FString TargetAssetName;
};

FReply SFacialAnimationBulkImporter::HandleImportClicked()
{
	// Iterate over the chosen source directory adding items to import
	struct FFbxVisitor : public IPlatformFile::FDirectoryVisitor
	{
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (!bIsDirectory)
			{
				FString FbxFilename(FilenameOrDirectory);
				if (FPaths::GetExtension(FbxFilename).Equals(TEXT("FBX"), ESearchCase::IgnoreCase))
				{
					// check for counterpart wave file
					FString WaveFilename = FPaths::ChangeExtension(FbxFilename, TEXT("WAV"));

					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					if (!PlatformFile.FileExists(*WaveFilename))
					{
						// no wave file so clear it out, this will be a standalone animation
						WaveFilename.Empty();
					}

					// build target package/asset name
					FString TargetAssetName = FPaths::GetBaseFilename(FbxFilename);

					const UFacialAnimationBulkImporterSettings* FacialAnimationBulkImporterSettings = GetDefault<UFacialAnimationBulkImporterSettings>();
					FString CurrentDirectory = FPaths::GetPath(FbxFilename);
					FString PartialPath = CurrentDirectory.RightChop(FacialAnimationBulkImporterSettings->SourceImportPath.Path.Len());
					FString TargetPackageName = FacialAnimationBulkImporterSettings->TargetImportPath.Path / PartialPath / TargetAssetName;

					FString Filename;
					if (FPackageName::TryConvertLongPackageNameToFilename(TargetPackageName, Filename))
					{
						ItemsToImport.Add({ FbxFilename, WaveFilename, TargetPackageName, TargetAssetName });
					}
				}
			}
			return true;
		}

		TArray<FImportItem> ItemsToImport;
	};

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FFbxVisitor Visitor;
	PlatformFile.IterateDirectoryRecursively(*GetDefault<UFacialAnimationBulkImporterSettings>()->SourceImportPath.Path, Visitor);
	
	// @TODO: check for a valid skeleton if we have standalone animations to import

	for (FImportItem& ImportItem : Visitor.ItemsToImport)
	{
		ImportItem.Import();
	}

	return FReply::Handled();
}



#undef LOCTEXT_NAMESPACE
