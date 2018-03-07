// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "USDImporter.h"
#include "ScopedSlowTask.h"
#include "AssetSelection.h"
#include "SUniformGridPanel.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "ObjectTools.h"
#include "MessageLogModule.h"
#include "IMessageLogListing.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"
#include "USDConversionUtils.h"
#include "StaticMeshImporter.h"
#include "Engine/StaticMesh.h"
#include "SBox.h"
#include "SButton.h"
#include "SlateApplication.h"
#include "FileManager.h"
#include "USDImporterProjectSettings.h"



#define LOCTEXT_NAMESPACE "USDImportPlugin"

DEFINE_LOG_CATEGORY(LogUSDImport);


class SUSDOptionsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUSDOptionsWindow)
		: _ImportOptions(nullptr)
	{}

	SLATE_ARGUMENT(UObject*, ImportOptions)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs)
	{
		ImportOptions = InArgs._ImportOptions;
		Window = InArgs._WidgetWindow;
		bShouldImport = false;

		TSharedPtr<SBox> DetailsViewBox;
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SAssignNew(DetailsViewBox, SBox)
				.MaxDesiredHeight(450.0f)
				.MinDesiredWidth(550.0f)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("USDOptionWindow_Import", "Import"))
					.OnClicked(this, &SUSDOptionsWindow::OnImport)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("USDOptionWindow_Cancel", "Cancel"))
					.ToolTipText(LOCTEXT("USDOptionWindow_Cancel_ToolTip", "Cancels importing this USD file"))
					.OnClicked(this, &SUSDOptionsWindow::OnCancel)
				]
			]
		];

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		TSharedPtr<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

		DetailsViewBox->SetContent(DetailsView.ToSharedRef());
		DetailsView->SetObject(ImportOptions);

	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	FReply OnImport()
	{
		bShouldImport = true;
		if (Window.IsValid())
		{
			Window.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}


	FReply OnCancel()
	{
		bShouldImport = false;
		if (Window.IsValid())
		{
			Window.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldImport() const
	{
		return bShouldImport;
	}

private:
	UObject* ImportOptions;
	TWeakPtr< SWindow > Window;
	bool bShouldImport;
};



const FUsdGeomData* FUsdPrimToImport::GetGeomData(int32 LODIndex, double Time) const
{
	if (NumLODs == 0)
	{
		return Prim->GetGeometryData(Time);
	}
	else
	{
		IUsdPrim* Child = Prim->GetLODChild(LODIndex);
		return Child->GetGeometryData(Time);
	}
}

UUSDImporter::UUSDImporter(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

UObject* UUSDImporter::ImportMeshes(FUsdImportContext& ImportContext, const TArray<FUsdPrimToImport>& PrimsToImport)
{
	IUsdPrim* RootPrim = ImportContext.RootPrim;

	FScopedSlowTask SlowTask(1.0f, LOCTEXT("ImportingUSDMeshes", "Importing USD Meshes"));
	SlowTask.Visibility = ESlowTaskVisibility::ForceVisible;
	int32 MeshCount = 0;

	const FTransform& ConversionTransform = ImportContext.ConversionTransform;

	EUsdMeshImportType MeshImportType = ImportContext.ImportOptions->MeshImportType;

	// Make unique names
	TMap<FString, int> ExistingNamesToCount;

	ImportContext.PathToImportAssetMap.Reserve(PrimsToImport.Num());

	const FString& ContentDirectoryLocation = ImportContext.ImportPathName;

	for (const FUsdPrimToImport& PrimToImport : PrimsToImport)
	{
		FString FinalPackagePathName = ContentDirectoryLocation;
		SlowTask.EnterProgressFrame(1.0f / PrimsToImport.Num(), FText::Format(LOCTEXT("ImportingUSDMesh", "Importing Mesh {0} of {1}"), MeshCount + 1, PrimsToImport.Num()));

		FString NewPackageName;

		bool bShouldImport = false;

		// when importing only one mesh we just use the existing package and name created
		if (PrimsToImport.Num() > 1 || ImportContext.ImportOptions->bGenerateUniquePathPerUSDPrim)
		{
			FString RawPrimName = USDToUnreal::ConvertString(PrimToImport.Prim->GetPrimName());
			FString MeshName = RawPrimName;

			if (ImportContext.ImportOptions->bGenerateUniquePathPerUSDPrim)
			{
				FString USDPath = USDToUnreal::ConvertString(PrimToImport.Prim->GetPrimPath());
				USDPath.RemoveFromStart(TEXT("/"));
				USDPath.RemoveFromEnd(RawPrimName);
				FinalPackagePathName /= USDPath;
			}
			else
			{
				// Make unique names
				int* ExistingCount = ExistingNamesToCount.Find(MeshName);
				if (ExistingCount)
				{
					MeshName += TEXT("_");
					MeshName.AppendInt(*ExistingCount);
					++(*ExistingCount);
				}
				else
				{
					ExistingNamesToCount.Add(MeshName, 1);
				}
			}

			MeshName = ObjectTools::SanitizeObjectName(MeshName);

			NewPackageName = PackageTools::SanitizePackageName(FinalPackagePathName / MeshName);
		
			// Once we've already imported it we dont need to import it again
			if(!ImportContext.PathToImportAssetMap.Contains(NewPackageName))
			{
				UPackage* Package = CreatePackage(nullptr, *NewPackageName);

				Package->FullyLoad();

				ImportContext.Parent = Package;
				ImportContext.ObjectName = MeshName;

				bShouldImport = true;
			}

		}
		else
		{
			bShouldImport = true;
		}

		if(bShouldImport)
		{
			UObject* NewMesh = ImportSingleMesh(ImportContext, MeshImportType, PrimToImport);

			if (NewMesh)
			{
				FAssetRegistryModule::AssetCreated(NewMesh);

				NewMesh->MarkPackageDirty();
				ImportContext.PathToImportAssetMap.Add(NewPackageName, NewMesh);
				++MeshCount;
			}
		}
	}

	// Return the first one on success.  
	return ImportContext.PathToImportAssetMap.Num() ? ImportContext.PathToImportAssetMap.CreateIterator().Value() : nullptr;
}

UObject* UUSDImporter::ImportSingleMesh(FUsdImportContext& ImportContext, EUsdMeshImportType ImportType, const FUsdPrimToImport& PrimToImport)
{
	UObject* NewMesh = nullptr;

	if (ImportType == EUsdMeshImportType::StaticMesh)
	{
		NewMesh = FUSDStaticMeshImporter::ImportStaticMesh(ImportContext, PrimToImport);
	}

	return NewMesh;
}

bool UUSDImporter::ShowImportOptions(UObject& ImportOptions)
{
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("USDImportSettings", "USD Import Options"))
		.SizingRule(ESizingRule::Autosized);


	TSharedPtr<SUSDOptionsWindow> OptionsWindow;
	Window->SetContent
	(
		SAssignNew(OptionsWindow, SUSDOptionsWindow)
		.ImportOptions(&ImportOptions)
		.WidgetWindow(Window)
	);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	return OptionsWindow->ShouldImport();
}

IUsdStage* UUSDImporter::ReadUSDFile(FUsdImportContext& ImportContext, const FString& Filename)
{
	FString FilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*Filename);
	FilePath = FPaths::GetPath(FilePath) + TEXT("/");
	FString CleanFilename = FPaths::GetCleanFilename(Filename);

	IUsdPrim* RootPrim = nullptr;
	IUsdStage* Stage = UnrealUSDWrapper::ImportUSDFile(TCHAR_TO_ANSI(*FilePath), TCHAR_TO_ANSI(*CleanFilename));

	const char* Errors = UnrealUSDWrapper::GetErrors();
	if (Errors)
	{
		FString ErrorStr = USDToUnreal::ConvertString(Errors);
		ImportContext.AddErrorMessage(EMessageSeverity::Error, FText::Format(LOCTEXT("CouldNotImportUSDFile", "Could not import USD file {0}\n {1}"), FText::FromString(CleanFilename), FText::FromString(ErrorStr)));
	}
	return Stage;
}

void FUsdImportContext::Init(UObject* InParent, const FString& InName, IUsdStage* InStage)
{
	Parent = InParent;
	ObjectName = InName;
	ImportPathName = InParent->GetOutermost()->GetName();

	// Path should not include the filename
	ImportPathName.RemoveFromEnd(FString(TEXT("/"))+InName);

	ImportObjectFlags = RF_Public | RF_Standalone | RF_Transactional;

	TSubclassOf<UUSDPrimResolver> ResolverClass = GetDefault<UUSDImporterProjectSettings>()->CustomPrimResolver;
	if (!ResolverClass)
	{
		ResolverClass = UUSDPrimResolver::StaticClass();
	}

	PrimResolver = NewObject<UUSDPrimResolver>(GetTransientPackage(), ResolverClass);
	PrimResolver->Init();

	if(InStage->GetUpAxis() == EUsdUpAxis::ZAxis)
	{
		// A matrix that converts Z up right handed coordinate system to Z up left handed (unreal)
		ConversionTransform =
			FTransform(FMatrix
			(
				FPlane(1, 0, 0, 0),
				FPlane(0, -1, 0, 0),
				FPlane(0, 0, 1, 0),
				FPlane(0, 0, 0, 1)
			));
	}
	else
	{
		// A matrix that converts Y up right handed coordinate system to Z up left handed (unreal)
		ConversionTransform =
			FTransform(FMatrix
			(
				FPlane(1, 0, 0, 0),
				FPlane(0, 0, 1, 0),
				FPlane(0, -1, 0, 0),
				FPlane(0, 0, 0, 1)
			));
	}
	Stage = InStage;
	RootPrim = InStage->GetRootPrim();

	bApplyWorldTransformToGeometry = false;
	bFindUnrealAssetReferences = false;
}

void FUsdImportContext::AddErrorMessage(EMessageSeverity::Type MessageSeverity, FText ErrorMessage)
{
	TokenizedErrorMessages.Add(FTokenizedMessage::Create(MessageSeverity, ErrorMessage));
}

void FUsdImportContext::DisplayErrorMessages(bool bAutomated)
{
	if(!bAutomated)
	{
		//Always clear the old message after an import or re-import
		const TCHAR* LogTitle = TEXT("USDImport");
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		TSharedPtr<class IMessageLogListing> LogListing = MessageLogModule.GetLogListing(LogTitle);
		LogListing->SetLabel(FText::FromString("USD Import"));
		LogListing->ClearMessages();

		if (TokenizedErrorMessages.Num() > 0)
		{
			LogListing->AddMessages(TokenizedErrorMessages);
			MessageLogModule.OpenMessageLog(LogTitle);
		}
	}
	else
	{
		for (const TSharedRef<FTokenizedMessage>& Message : TokenizedErrorMessages)
		{
			UE_LOG(LogUSDImport, Error, TEXT("%s"), *Message->ToText().ToString());
		}
	}
}

void FUsdImportContext::ClearErrorMessages()
{
	TokenizedErrorMessages.Empty();
}

#undef LOCTEXT_NAMESPACE

