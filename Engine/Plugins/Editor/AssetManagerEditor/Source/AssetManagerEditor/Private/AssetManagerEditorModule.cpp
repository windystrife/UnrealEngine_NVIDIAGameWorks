// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetManagerEditorModule.h"
#include "ModuleManager.h"
#include "AssetRegistryModule.h"
#include "PrimaryAssetTypeCustomization.h"
#include "PrimaryAssetIdCustomization.h"
#include "SAssetAuditBrowser.h"
#include "Engine/PrimaryAssetLabel.h"

#include "JsonReader.h"
#include "JsonSerializer.h"
#include "CollectionManagerModule.h"
#include "GameDelegates.h"
#include "ICollectionManager.h"
#include "ARFilter.h"
#include "FileHelper.h"
#include "ProfilingHelpers.h"
#include "StatsMisc.h"
#include "Engine/AssetManager.h"
#include "PropertyEditorModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SToolTip.h"
#include "PropertyCustomizationHelpers.h"
#include "ReferenceViewer.h"
#include "GraphEditorModule.h"
#include "AssetData.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "IPlatformFileSandboxWrapper.h"
#include "HAL/PlatformFilemanager.h"
#include "Serialization/ArrayReader.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "SGraphPin.h"

#define LOCTEXT_NAMESPACE "AssetManagerEditor"

DEFINE_LOG_CATEGORY(LogAssetManagerEditor);

// Static functions/variables defeined in the interface

class FAssetManagerGraphPanelPinFactory : public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == TBaseStructure<FPrimaryAssetId>::Get())
		{
			return SNew(SPrimaryAssetIdGraphPin, InPin);
		}
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == TBaseStructure<FPrimaryAssetType>::Get())
		{
			return SNew(SPrimaryAssetTypeGraphPin, InPin);
		}

		return nullptr;
	}
};

const FName IAssetManagerEditorModule::ResourceSizeName = FName("ResourceSize");
const FName IAssetManagerEditorModule::DiskSizeName = FName("DiskSize");
const FName IAssetManagerEditorModule::ManagedResourceSizeName = FName("ManagedResourceSize");
const FName IAssetManagerEditorModule::ManagedDiskSizeName = FName("ManagedDiskSize");
const FName IAssetManagerEditorModule::TotalUsageName = FName("TotalUsage");
const FName IAssetManagerEditorModule::CookRuleName = FName("CookRule");
const FName IAssetManagerEditorModule::ChunksName = FName("Chunks");

TSharedRef<SWidget> IAssetManagerEditorModule::MakePrimaryAssetTypeSelector(FOnGetPrimaryAssetDisplayText OnGetDisplayText, FOnSetPrimaryAssetType OnSetType, bool bAllowClear)
{
	FOnGetPropertyComboBoxStrings GetStrings = FOnGetPropertyComboBoxStrings::CreateStatic(&IAssetManagerEditorModule::GeneratePrimaryAssetTypeComboBoxStrings, bAllowClear);
	FOnGetPropertyComboBoxValue GetValue = FOnGetPropertyComboBoxValue::CreateLambda([OnGetDisplayText]
	{
		return OnGetDisplayText.Execute().ToString();
	});
	FOnPropertyComboBoxValueSelected SetValue = FOnPropertyComboBoxValueSelected::CreateLambda([OnSetType](const FString& StringValue)
	{
		OnSetType.Execute(FPrimaryAssetType(*StringValue));
	});

	return PropertyCustomizationHelpers::MakePropertyComboBox(nullptr, GetStrings, GetValue, SetValue);
}

TSharedRef<SWidget> IAssetManagerEditorModule::MakePrimaryAssetIdSelector(FOnGetPrimaryAssetDisplayText OnGetDisplayText, FOnSetPrimaryAssetId OnSetId, bool bAllowClear, TArray<FPrimaryAssetType> AllowedTypes)
{
	FOnGetContent OnCreateMenuContent = FOnGetContent::CreateLambda([OnGetDisplayText, OnSetId, bAllowClear, AllowedTypes]()
	{
		FOnShouldFilterAsset AssetFilter = FOnShouldFilterAsset::CreateStatic(&IAssetManagerEditorModule::OnShouldFilterPrimaryAsset, AllowedTypes);
		FOnSetObject OnSetObject = FOnSetObject::CreateLambda([OnSetId](const FAssetData& AssetData)
		{
			FSlateApplication::Get().DismissAllMenus();
			UAssetManager& Manager = UAssetManager::Get();

			FPrimaryAssetId AssetId;
			if (AssetData.IsValid())
			{
				AssetId = Manager.GetPrimaryAssetIdForData(AssetData);
				ensure(AssetId.IsValid());
			}

			OnSetId.Execute(AssetId);
		});

		TArray<const UClass*> AllowedClasses;
		TArray<UFactory*> NewAssetFactories;

		return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
			FAssetData(),
			bAllowClear,
			AllowedClasses,
			NewAssetFactories,
			AssetFilter,
			OnSetObject,
			FSimpleDelegate());
	});

	TAttribute<FText> OnGetObjectText = TAttribute<FText>::Create(OnGetDisplayText);

	return SNew(SComboButton)
	.OnGetMenuContent(OnCreateMenuContent)
	.ButtonContent()
		[
			SNew(STextBlock)
			.Text(OnGetObjectText)
		];
}

void IAssetManagerEditorModule::GeneratePrimaryAssetTypeComboBoxStrings(TArray< TSharedPtr<FString> >& OutComboBoxStrings, TArray<TSharedPtr<class SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems, bool bAllowClear)
{
	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetTypeInfo> TypeInfos;

	AssetManager.GetPrimaryAssetTypeInfoList(TypeInfos);
	TypeInfos.Sort([](const FPrimaryAssetTypeInfo& LHS, const FPrimaryAssetTypeInfo& RHS) { return LHS.PrimaryAssetType < RHS.PrimaryAssetType; });

	// Can the field be cleared
	if (bAllowClear)
	{
		// Add None
		OutComboBoxStrings.Add(MakeShared<FString>(FPrimaryAssetType().ToString()));
		OutToolTips.Add(SNew(SToolTip).Text(LOCTEXT("NoType", "NoType")));
		OutRestrictedItems.Add(false);
	}

	for (FPrimaryAssetTypeInfo& Info : TypeInfos)
	{
		OutComboBoxStrings.Add(MakeShared<FString>(Info.PrimaryAssetType.ToString()));

		FText TooltipText = FText::Format(LOCTEXT("ToolTipFormat", "{0}:{1}{2}"),
			FText::FromString(Info.PrimaryAssetType.ToString()),
			Info.bIsEditorOnly ? LOCTEXT("EditorOnly", " EditorOnly") : FText(),
			Info.bHasBlueprintClasses ? LOCTEXT("Blueprints", " Blueprints") : FText());

		OutToolTips.Add(SNew(SToolTip).Text(TooltipText));
		OutRestrictedItems.Add(false);
	}
}

bool IAssetManagerEditorModule::OnShouldFilterPrimaryAsset(const FAssetData& InAssetData, TArray<FPrimaryAssetType> AllowedTypes)
{
	// Make sure it has a primary asset id, and do type check
	UAssetManager& Manager = UAssetManager::Get();

	if (InAssetData.IsValid())
	{
		FPrimaryAssetId AssetId = Manager.GetPrimaryAssetIdForData(InAssetData);
		if (AssetId.IsValid())
		{
			if (AllowedTypes.Num() > 0)
			{
				if (!AllowedTypes.Contains(AssetId.PrimaryAssetType))
				{
					return true;
				}
			}

			return false;
		}
	}

	return true;
}

// Concrete implementation

class FAssetManagerEditorModule : public IAssetManagerEditorModule
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End of IModuleInterface interface

	void PerformAuditConsoleCommand(const TArray<FString>& Args);
	void PerformDependencyChainConsoleCommand(const TArray<FString>& Args);
	void PerformDependencyClassConsoleCommand(const TArray<FString>& Args);
	void DumpAssetDependencies(const TArray<FString>& Args);

	/** Opens asset management UI, with selected assets */
	void OpenAssetManagementUI(TArray<FAssetData> SelectedAssets);
	void OpenAssetManagementUI(TArray<FName> SelectedAssets);

	FString GetValueForCustomColumn(FAssetData& AssetData, FName ColumnName, ITargetPlatform* TargetPlatform, const FAssetRegistryState* PlatformState) override;
	void GetAvailableTargetPlatforms(TArray<ITargetPlatform*>& AvailablePlatforms) override;
	FAssetRegistryState* GetAssetRegistryStateForTargetPlatform(ITargetPlatform* TargetPlatform) override;
private:

	static bool GetDependencyTypeArg(const FString& Arg, EAssetRegistryDependencyType::Type& OutDepType);

	//Prints all dependency chains from assets in the search path to the target package.
	static void FindReferenceChains(IAssetRegistry& AssetRegistry, FName TargetPackageName, FName RootSearchPath, EAssetRegistryDependencyType::Type DependencyType);

	//Prints all dependency chains from the PackageName to any dependency of one of the given class names.
	//If the package name is a path rather than a package, then it will do this for each package in the path.
	static void FindClassDependencies(IAssetRegistry& AssetRegistry, FName PackagePath, const TArray<FName>& TargetClasses, EAssetRegistryDependencyType::Type DependencyType);

	static bool GetPackageDependencyChain(IAssetRegistry& AssetRegistry, FName SourcePackage, FName TargetPackage, TArray<FName>& VisitedPackages, TArray<FName>& OutDependencyChain, EAssetRegistryDependencyType::Type DependencyType);
	static void GetPackageDependenciesPerClass(IAssetRegistry& AssetRegistry, FName SourcePackage, const TArray<FName>& TargetClasses, TArray<FName>& VisitedPackages, TArray<FName>& OutDependentPackages, EAssetRegistryDependencyType::Type DependencyType);

	void LogAssetsWithMultipleLabels();
	void PrintSizeSummaries(const FString& PlatformName);
	void RecreateCollections();
	bool CreateOrEmptyCollection(FName CollectionName);
	void WriteProfileFile(const FString& Extension, const FString& FileContents);
	void WriteCollection(FName CollectionName, const TArray<FName>& PackageNames);
	void WriteSizeSortedList(TArray<FName>& PackageNames) const;
	FString GetSavedAssetRegistryPath(ITargetPlatform* TargetPlatform);

	TArray<IConsoleObject*> AuditCmds;

	static const TCHAR* FindDepChainHelpText;
	static const TCHAR* FindClassDepHelpText;
	static const FName AssetManagementTabName;

	FDelegateHandle ContentBrowserExtenderDelegateHandle;
	FDelegateHandle ReferenceViewerDelegateHandle;

	TWeakPtr<SDockTab> AssetManagementTab;
	TWeakPtr<SAssetAuditBrowser> AssetManagementUI;
	TMap<ITargetPlatform*, FAssetRegistryState> AssetRegistryStateMap;
	FSandboxPlatformFile* CookedSandbox;
	FSandboxPlatformFile* EditorCookedSandbox;

	void CreateAssetManagerContentBrowserMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);
	void CreateReferenceViewerMenu(FMenuBuilder& MenuBuilder, TArray<FAssetIdentifier> SelectedAssets);
	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
	TSharedRef<FExtender> OnExtendReferenceViewerSelectionMenu(const TSharedRef<class FUICommandList>, const class UEdGraph* Graph, const class UEdGraphNode* Node, const UEdGraphPin* Pin, bool bConst);

	TSharedRef<SDockTab> SpawnAssetManagementTab(const FSpawnTabArgs& Args);
};

const TCHAR* FAssetManagerEditorModule::FindDepChainHelpText = TEXT("Finds all dependency chains from assets in the given search path, to the target package.\n Usage: FindDepChain TargetPackagePath SearchRootPath (optional: -hardonly/-softonly)\n e.g. FindDepChain /game/characters/heroes/muriel/meshes/muriel /game/cards ");
const TCHAR* FAssetManagerEditorModule::FindClassDepHelpText = TEXT("Finds all dependencies of a certain set of classes to the target asset.\n Usage: FindDepClasses TargetPackagePath ClassName1 ClassName2 etc (optional: -hardonly/-softonly) \n e.g. FindDepChain /game/characters/heroes/muriel/meshes/muriel /game/cards");
const FName FAssetManagerEditorModule::AssetManagementTabName = TEXT("AssetManagementUI");

///////////////////////////////////////////

IMPLEMENT_MODULE(FAssetManagerEditorModule, AssetManagerEditor);


void FAssetManagerEditorModule::StartupModule()
{
	CookedSandbox = nullptr;
	EditorCookedSandbox = nullptr;

	if (!IsRunningCommandlet())
	{
		AuditCmds.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("AssetManager.AssetAudit"),
			TEXT("Dumps statistics about assets to the log."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAssetManagerEditorModule::PerformAuditConsoleCommand),
			ECVF_Default
			));

		AuditCmds.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("AssetManager.FindDepChain"),
			FindDepChainHelpText,
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAssetManagerEditorModule::PerformDependencyChainConsoleCommand),
			ECVF_Default
			));

		AuditCmds.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("AssetManager.FindDepClasses"),
			FindClassDepHelpText,
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAssetManagerEditorModule::PerformDependencyClassConsoleCommand),
			ECVF_Default
			));

		AuditCmds.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("AssetManager.DumpAssetDependencies"),
			TEXT("Shows a list of all primary assets and the secondary assets that they depend on. Also writes out a .graphviz file"),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAssetManagerEditorModule::DumpAssetDependencies),
			ECVF_Default
			));

		// Register customizations
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout("PrimaryAssetType", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPrimaryAssetTypeCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("PrimaryAssetId", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPrimaryAssetIdCustomization::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();

		// Register Pins
		TSharedPtr<FAssetManagerGraphPanelPinFactory> AssetManagerGraphPanelPinFactory = MakeShareable(new FAssetManagerGraphPanelPinFactory());
		FEdGraphUtilities::RegisterVisualPinFactory(AssetManagerGraphPanelPinFactory);

		// Register content browser hook
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(this, &FAssetManagerEditorModule::OnExtendContentBrowserAssetSelectionMenu));
		ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();

		// Register reference viewer hook
		FGraphEditorModule& GraphEdModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>(TEXT("GraphEditor"));
		TArray<FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode>& ReferenceViewerMenuExtenderDelegates = GraphEdModule.GetAllGraphEditorContextMenuExtender();

		ReferenceViewerMenuExtenderDelegates.Add(FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateRaw(this, &FAssetManagerEditorModule::OnExtendReferenceViewerSelectionMenu));
		ReferenceViewerDelegateHandle = ReferenceViewerMenuExtenderDelegates.Last().GetHandle();

		// Add nomad tab
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AssetManagementTabName, FOnSpawnTab::CreateRaw(this, &FAssetManagerEditorModule::SpawnAssetManagementTab))
			.SetDisplayName(LOCTEXT("AssetManagementTitle", "Asset Audit"))
			.SetTooltipText(LOCTEXT("AssetManagementTooltip", "Open Asset Audit window, allows viewing information about assets."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());
	}
}

void FAssetManagerEditorModule::ShutdownModule()
{
	if (CookedSandbox)
	{
		delete CookedSandbox;
		CookedSandbox = nullptr;
	}

	if (EditorCookedSandbox)
	{
		delete EditorCookedSandbox;
		EditorCookedSandbox = nullptr;
	}

	for (IConsoleObject* AuditCmd : AuditCmds)
	{
		IConsoleManager::Get().UnregisterConsoleObject(AuditCmd);
	}
	AuditCmds.Empty();

	if (UObjectInitialized() && FSlateApplication::IsInitialized())
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });

		FGraphEditorModule& GraphEdModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>(TEXT("GraphEditor"));
		TArray<FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode>& ReferenceViewerMenuExtenderDelegates = GraphEdModule.GetAllGraphEditorContextMenuExtender();
		ReferenceViewerMenuExtenderDelegates.RemoveAll([this](const FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode& Delegate) { return Delegate.GetHandle() == ReferenceViewerDelegateHandle; });

		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AssetManagementTabName);

		if (AssetManagementTab.IsValid())
		{
			AssetManagementTab.Pin()->RequestCloseTab();
		}
	}
}

TSharedRef<SDockTab> FAssetManagerEditorModule::SpawnAssetManagementTab(const FSpawnTabArgs& Args)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	if (!UAssetManager::IsValid())
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BadAssetAuditUI", "Cannot load Asset Audit if there is no asset manager!"))
			];
	}
	
	return SAssignNew(AssetManagementTab, SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SAssignNew(AssetManagementUI, SAssetAuditBrowser)
		];
}

void FAssetManagerEditorModule::OpenAssetManagementUI(TArray<FAssetData> SelectedAssets)
{
	FGlobalTabmanager::Get()->InvokeTab(AssetManagementTabName);

	if (AssetManagementUI.IsValid())
	{
		AssetManagementUI.Pin()->AddAssetsToList(SelectedAssets, false);
	}
}

void FAssetManagerEditorModule::OpenAssetManagementUI(TArray<FName> SelectedAssets)
{
	FGlobalTabmanager::Get()->InvokeTab(AssetManagementTabName);

	if (AssetManagementUI.IsValid())
	{
		AssetManagementUI.Pin()->AddAssetsToList(SelectedAssets, false);
	}
}

void FAssetManagerEditorModule::CreateAssetManagerContentBrowserMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.AddMenuEntry
	(
		LOCTEXT("CodeEditorTabTitle", "Audit Assets"),
		LOCTEXT("CodeEditorTooltipText", "Opens the Asset Audit UI with these assets."),
		FSlateIcon(),
		FUIAction
		(
			FExecuteAction::CreateRaw(this, &FAssetManagerEditorModule::OpenAssetManagementUI, SelectedAssets)
		)
	);
}

void FAssetManagerEditorModule::CreateReferenceViewerMenu(FMenuBuilder& MenuBuilder, TArray<FAssetIdentifier> SelectedAssets)
{
	TArray<FName> PackageNames;

	for (FAssetIdentifier Identifier : SelectedAssets)
	{
		if (Identifier.PackageName.IsValid())
		{
			PackageNames.Add(Identifier.PackageName);
		}
	}

	MenuBuilder.AddMenuEntry
	(
		LOCTEXT("CodeEditorTabTitle", "Audit Assets"),
		LOCTEXT("CodeEditorTooltipText", "Opens the Asset Audit UI with these assets."),
		FSlateIcon(),
		FUIAction
		(
			FExecuteAction::CreateRaw(this, &FAssetManagerEditorModule::OpenAssetManagementUI, PackageNames)
		)
	);
}

TSharedRef<FExtender> FAssetManagerEditorModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());

	Extender->AddMenuExtension(
		"AssetContextAdvancedActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateRaw(this, &FAssetManagerEditorModule::CreateAssetManagerContentBrowserMenu, SelectedAssets));

	return Extender;
}

TSharedRef<FExtender> FAssetManagerEditorModule::OnExtendReferenceViewerSelectionMenu(const TSharedRef<class FUICommandList>, const class UEdGraph* Graph, const class UEdGraphNode* Node, const UEdGraphPin* Pin, bool bConst)
{
	IReferenceViewerModule& ReferenceViewer = IReferenceViewerModule::Get();

	TArray<FAssetIdentifier> SelectedAssets;

	TSharedRef<FExtender> Extender(new FExtender());

	if (ReferenceViewer.GetSelectedAssetsForMenuExtender(Graph, Node, SelectedAssets))
	{
		Extender->AddMenuExtension(
			"ContextMenu",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FAssetManagerEditorModule::CreateReferenceViewerMenu, SelectedAssets));
	}

	return Extender;
}

FString FAssetManagerEditorModule::GetValueForCustomColumn(FAssetData& AssetData, FName ColumnName, ITargetPlatform* TargetPlatform, const FAssetRegistryState* PlatformState)
{
	UAssetManager& AssetManager = UAssetManager::Get();
	IAssetRegistry& AssetRegistry = AssetManager.GetAssetRegistry();

	FString ReturnString;

	if (ColumnName == ManagedResourceSizeName || ColumnName == ManagedDiskSizeName)
	{
		FName SizeTag = (ColumnName == ManagedResourceSizeName) ? ResourceSizeName : DiskSizeName;

		FPrimaryAssetId PrimaryAssetId = AssetManager.GetPrimaryAssetIdForData(AssetData);

		if (!PrimaryAssetId.IsValid())
		{
			// Just return exclusive
			return GetValueForCustomColumn(AssetData, SizeTag, TargetPlatform, PlatformState);
		}

		TArray<FName> AssetPackageArray;
		AssetManager.GetManagedPackageList(PrimaryAssetId, AssetPackageArray);

		int64 TotalSize = 0;

		for (FName PackageName : AssetPackageArray)
		{
			TArray<FAssetData> FoundData;
			FARFilter AssetFilter;
			AssetFilter.PackageNames.Add(PackageName);
			AssetFilter.bIncludeOnlyOnDiskAssets = true;

			if (AssetRegistry.GetAssets(AssetFilter, FoundData) && FoundData.Num() > 0)
			{
				// Use first one
				FAssetData& ManagedAssetData = FoundData[0];

				FString DataString = GetValueForCustomColumn(ManagedAssetData, SizeTag, TargetPlatform, PlatformState);

				int64 PackageSize = 0;
				Lex::FromString(PackageSize, *DataString);
				TotalSize += PackageSize;
			}
		}

		ReturnString = Lex::ToString(TotalSize);
	}
	else if (ColumnName == DiskSizeName)
	{
		const FAssetPackageData* FoundData = PlatformState ? PlatformState->GetAssetPackageData(AssetData.PackageName) : AssetRegistry.GetAssetPackageData(AssetData.PackageName);

		if (FoundData)
		{
			ReturnString = Lex::ToString((FoundData->DiskSize + 512) / 1024);
		}
	}
	else if (ColumnName == TotalUsageName)
	{
		int64 TotalWeight = 0;

		TSet<FPrimaryAssetId> ReferencingPrimaryAssets;

		AssetManager.GetPackageManagers(AssetData.PackageName, false, ReferencingPrimaryAssets);

		for (const FPrimaryAssetId& PrimaryAssetId : ReferencingPrimaryAssets)
		{
			FPrimaryAssetRules Rules = AssetManager.GetPrimaryAssetRules(PrimaryAssetId);

			if (!Rules.IsDefault())
			{
				TotalWeight += Rules.Priority;
			}
		}

		ReturnString = Lex::ToString(TotalWeight);
	}
	else if (ColumnName == CookRuleName)
	{
		EPrimaryAssetCookRule CookRule;

		CookRule = AssetManager.GetPackageCookRule(AssetData.PackageName);

		switch (CookRule)
		{
		case EPrimaryAssetCookRule::AlwaysCook: return TEXT("Always");
		case EPrimaryAssetCookRule::DevelopmentCook: return TEXT("Development");
		case EPrimaryAssetCookRule::NeverCook: return TEXT("Never");
		}
	}
	else if (ColumnName == ChunksName)
	{
		TArray<int32> FoundChunks;

		if (PlatformState)
		{
			const FAssetData* PlatformData = PlatformState->GetAssetByObjectPath(AssetData.ObjectPath);
			if (PlatformData)
			{
				FoundChunks = PlatformData->ChunkIDs;
			}
		}
		else
		{
			AssetManager.GetPackageChunkIds(AssetData.PackageName, TargetPlatform, AssetData.ChunkIDs, FoundChunks);
		}
		
		FoundChunks.Sort();

		for (int32 Chunk : FoundChunks)
		{
			if (!ReturnString.IsEmpty())
			{
				ReturnString += TEXT("+");
			}
			ReturnString += Lex::ToString(Chunk);
		}
	}
	else
	{
		// Get base value of asset tag
		AssetData.GetTagValue(ColumnName, ReturnString);
	}

	return ReturnString;
}

FString FAssetManagerEditorModule::GetSavedAssetRegistryPath(ITargetPlatform* TargetPlatform)
{
	if (!TargetPlatform)
	{
		return FString();
	}

	FString PlatformName = TargetPlatform->PlatformName();

	// Initialize sandbox wrapper
	if (!CookedSandbox)
	{
		CookedSandbox = new FSandboxPlatformFile(false);

		FString OutputDirectory = FPaths::Combine(*FPaths::ProjectDir(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
		FPaths::NormalizeDirectoryName(OutputDirectory);

		CookedSandbox->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *OutputDirectory));
	}

	if (!EditorCookedSandbox)
	{
		EditorCookedSandbox = new FSandboxPlatformFile(false);

		FString OutputDirectory = FPaths::Combine(*FPaths::ProjectDir(), TEXT("Saved"), TEXT("EditorCooked"), TEXT("[Platform]"));
		FPaths::NormalizeDirectoryName(OutputDirectory);

		EditorCookedSandbox->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *OutputDirectory));
	}

	FString CommandLinePath;
	FParse::Value(FCommandLine::Get(), TEXT("AssetRegistryFile="), CommandLinePath);
	CommandLinePath.ReplaceInline(TEXT("[Platform]"), *PlatformName);
	
	// First try DevelopmentAssetRegistry.bin, then try AssetRegistry.bin
	FString CookedAssetRegistry = FPaths::ProjectDir() / TEXT("AssetRegistry.bin");

	FString CookedPath = CookedSandbox->ConvertToAbsolutePathForExternalAppForWrite(*CookedAssetRegistry).Replace(TEXT("[Platform]"), *PlatformName);
	FString DevCookedPath = CookedPath.Replace(TEXT("AssetRegistry.bin"), TEXT("DevelopmentAssetRegistry.bin"));

	FString EditorCookedPath = EditorCookedSandbox->ConvertToAbsolutePathForExternalAppForWrite(*CookedAssetRegistry).Replace(TEXT("[Platform]"), *PlatformName);
	FString DevEditorCookedPath = EditorCookedPath.Replace(TEXT("AssetRegistry.bin"), TEXT("DevelopmentAssetRegistry.bin"));

	FString SharedCookedPath = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("SharedIterativeBuild"), PlatformName, TEXT("Cooked"), TEXT("AssetRegistry.bin"));
	FString DevSharedCookedPath = SharedCookedPath.Replace(TEXT("AssetRegistry.bin"), TEXT("DevelopmentAssetRegistry.bin"));

	// Try command line, then cooked, then build
	if (!CommandLinePath.IsEmpty() && IFileManager::Get().FileExists(*CommandLinePath))
	{
		return CommandLinePath;
	}

	if (IFileManager::Get().FileExists(*DevCookedPath))
	{
		return DevCookedPath;
	}

	if (IFileManager::Get().FileExists(*CookedPath))
	{
		return CookedPath;
	}

	if (IFileManager::Get().FileExists(*DevEditorCookedPath))
	{
		return DevEditorCookedPath;
	}

	if (IFileManager::Get().FileExists(*EditorCookedPath))
	{
		return EditorCookedPath;
	}

	if (IFileManager::Get().FileExists(*DevSharedCookedPath))
	{
		return DevSharedCookedPath;
	}

	if (IFileManager::Get().FileExists(*SharedCookedPath))
	{
		return SharedCookedPath;
	}

	return FString();
}

void FAssetManagerEditorModule::GetAvailableTargetPlatforms(TArray<ITargetPlatform*>& AvailablePlatforms)
{
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();

	for (ITargetPlatform* CheckPlatform : Platforms)
	{
		FString RegistryPath = GetSavedAssetRegistryPath(CheckPlatform);

		if (!RegistryPath.IsEmpty())
		{
			AvailablePlatforms.Add(CheckPlatform);
		}
	}
}

FAssetRegistryState* FAssetManagerEditorModule::GetAssetRegistryStateForTargetPlatform(ITargetPlatform* TargetPlatform)
{
	FAssetRegistryState* FoundRegistry = AssetRegistryStateMap.Find(TargetPlatform);

	if (FoundRegistry)
	{
		return FoundRegistry;
	}

	FString RegistryPath = GetSavedAssetRegistryPath(TargetPlatform);

	if (RegistryPath.IsEmpty())
	{
		return nullptr;
	}

	FArrayReader SerializedAssetData;
	if (FFileHelper::LoadFileToArray(SerializedAssetData, *RegistryPath))
	{
		FAssetRegistryState& NewState = AssetRegistryStateMap.Add(TargetPlatform);
		FAssetRegistrySerializationOptions Options;
		Options.ModifyForDevelopment();

		NewState.Serialize(SerializedAssetData, Options);

		return &NewState;
	}

	return nullptr;
}

void FAssetManagerEditorModule::PerformAuditConsoleCommand(const TArray<FString>& Args)
{
	// Turn off as it makes diffing hard
	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

	UAssetManager::Get().UpdateManagementDatabase();

	// Now print assets with multiple labels
	LogAssetsWithMultipleLabels();

	// Load the cooker manifest file for platforms and print summaries
	FString PlatformName = TEXT("WindowsNoEditor");
	if (Args.Num() > 0)
	{
		PlatformName = Args[0];
	}

	PrintSizeSummaries(PlatformName);
	RecreateCollections();
}

bool FAssetManagerEditorModule::GetDependencyTypeArg(const FString& Arg, EAssetRegistryDependencyType::Type& OutDepType)
{
	if (Arg.Compare(TEXT("-hardonly"), ESearchCase::IgnoreCase) == 0)
	{
		OutDepType = EAssetRegistryDependencyType::Hard;
		return true;
	}
	else if (Arg.Compare(TEXT("-softonly"), ESearchCase::IgnoreCase) == 0)
	{
		OutDepType = EAssetRegistryDependencyType::Soft;
		return true;
	}
	return false;
}

void FAssetManagerEditorModule::PerformDependencyChainConsoleCommand(const TArray<FString>& Args)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if (Args.Num() < 2)
	{
		UE_LOG(LogAssetManagerEditor, Display, TEXT("FindDepChain given incorrect number of arguments.  Usage: %s"), FindDepChainHelpText);
		return;
	}

	FName TargetPath = FName(*Args[0].ToLower());
	FName SearchRoot = FName(*Args[1].ToLower());

	EAssetRegistryDependencyType::Type DependencyType = EAssetRegistryDependencyType::Packages;
	if (Args.Num() > 2)
	{
		GetDependencyTypeArg(Args[2], DependencyType);
	}

	FindReferenceChains(AssetRegistry, TargetPath, SearchRoot, DependencyType);
}

void FAssetManagerEditorModule::PerformDependencyClassConsoleCommand(const TArray<FString>& Args)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	if (Args.Num() < 2)
	{
		UE_LOG(LogAssetManagerEditor, Display, TEXT("FindDepClasses given incorrect number of arguments.  Usage: %s"), FindClassDepHelpText);
		return;
	}

	EAssetRegistryDependencyType::Type DependencyType = EAssetRegistryDependencyType::Packages;

	FName SourcePackagePath = FName(*Args[0].ToLower());
	TArray<FName> TargetClasses;
	for (int32 i = 1; i < Args.Num(); ++i)
	{
		if (!GetDependencyTypeArg(Args[i], DependencyType))
		{
			TargetClasses.AddUnique(FName(*Args[i]));
		}
	}

	TArray<FName> PackagesToSearch;

	//determine if the user passed us a package, or a directory
	TArray<FAssetData> PackageAssets;
	AssetRegistry.GetAssetsByPackageName(SourcePackagePath, PackageAssets);
	if (PackageAssets.Num() > 0)
	{
		PackagesToSearch.Add(SourcePackagePath);
	}
	else
	{
		TArray<FAssetData> AssetsInSearchPath;
		if (AssetRegistry.GetAssetsByPath(SourcePackagePath, /*inout*/ AssetsInSearchPath, /*bRecursive=*/ true))
		{
			for (const FAssetData& AssetData : AssetsInSearchPath)
			{
				PackagesToSearch.AddUnique(AssetData.PackageName);
			}
		}
	}

	for (FName SourcePackage : PackagesToSearch)
	{
		UE_LOG(LogAssetManagerEditor, Verbose, TEXT("FindDepClasses for: %s"), *SourcePackage.ToString());
		FindClassDependencies(AssetRegistry, SourcePackage, TargetClasses, DependencyType);
	}
}

bool FAssetManagerEditorModule::GetPackageDependencyChain(IAssetRegistry& AssetRegistry, FName SourcePackage, FName TargetPackage, TArray<FName>& VisitedPackages, TArray<FName>& OutDependencyChain, EAssetRegistryDependencyType::Type DependencyType)
{
	//avoid crashing from circular dependencies.
	if (VisitedPackages.Contains(SourcePackage))
	{
		return false;
	}
	VisitedPackages.AddUnique(SourcePackage);

	if (SourcePackage == TargetPackage)
	{
		OutDependencyChain.Add(SourcePackage);
		return true;
	}

	TArray<FName> SourceDependencies;
	if (AssetRegistry.GetDependencies(SourcePackage, SourceDependencies, DependencyType) == false)
	{
		return false;
	}

	int32 DependencyCounter = 0;
	while (DependencyCounter < SourceDependencies.Num())
	{
		const FName& ChildPackageName = SourceDependencies[DependencyCounter];
		if (GetPackageDependencyChain(AssetRegistry, ChildPackageName, TargetPackage, VisitedPackages, OutDependencyChain, DependencyType))
		{
			OutDependencyChain.Add(SourcePackage);
			return true;
		}
		++DependencyCounter;
	}

	return false;
}

void FAssetManagerEditorModule::GetPackageDependenciesPerClass(IAssetRegistry& AssetRegistry, FName SourcePackage, const TArray<FName>& TargetClasses, TArray<FName>& VisitedPackages, TArray<FName>& OutDependentPackages, EAssetRegistryDependencyType::Type DependencyType)
{
	//avoid crashing from circular dependencies.
	if (VisitedPackages.Contains(SourcePackage))
	{
		return;
	}
	VisitedPackages.AddUnique(SourcePackage);

	TArray<FName> SourceDependencies;
	if (AssetRegistry.GetDependencies(SourcePackage, SourceDependencies, DependencyType) == false)
	{
		return;
	}

	int32 DependencyCounter = 0;
	while (DependencyCounter < SourceDependencies.Num())
	{
		const FName& ChildPackageName = SourceDependencies[DependencyCounter];
		GetPackageDependenciesPerClass(AssetRegistry, ChildPackageName, TargetClasses, VisitedPackages, OutDependentPackages, DependencyType);
		++DependencyCounter;
	}

	FARFilter Filter;
	Filter.PackageNames.Add(SourcePackage);
	Filter.ClassNames = TargetClasses;
	Filter.bIncludeOnlyOnDiskAssets = true;

	TArray<FAssetData> PackageAssets;
	if (AssetRegistry.GetAssets(Filter, PackageAssets))
	{
		for (const FAssetData& AssetData : PackageAssets)
		{
			OutDependentPackages.AddUnique(SourcePackage);
			break;
		}
	}
}

void FAssetManagerEditorModule::FindReferenceChains(IAssetRegistry& AssetRegistry, FName TargetPackageName, FName RootSearchPath, EAssetRegistryDependencyType::Type DependencyType)
{
	//find all the assets we think might depend on our target through some chain
	TArray<FAssetData> AssetsInSearchPath;
	AssetRegistry.GetAssetsByPath(RootSearchPath, /*inout*/ AssetsInSearchPath, /*bRecursive=*/ true);

	//consolidate assets into a unique set of packages for dependency searching. reduces redundant work.
	TArray<FName> SearchPackages;
	for (const FAssetData& AD : AssetsInSearchPath)
	{
		SearchPackages.AddUnique(AD.PackageName);
	}

	int32 CurrentFoundChain = 0;
	TArray<TArray<FName>> FoundChains;
	FoundChains.AddDefaulted(1);

	//try to find a dependency chain that links each of these packages to our target.
	TArray<FName> VisitedPackages;
	for (const FName& SearchPackage : SearchPackages)
	{
		VisitedPackages.Reset();
		if (GetPackageDependencyChain(AssetRegistry, SearchPackage, TargetPackageName, VisitedPackages, FoundChains[CurrentFoundChain], DependencyType))
		{
			++CurrentFoundChain;
			FoundChains.AddDefaulted(1);
		}
	}

	UE_LOG(LogAssetManagerEditor, Log, TEXT("Found %i, Dependency Chains to %s from directory %s"), CurrentFoundChain, *TargetPackageName.ToString(), *RootSearchPath.ToString());
	for (int32 ChainIndex = 0; ChainIndex < CurrentFoundChain; ++ChainIndex)
	{
		TArray<FName>& FoundChain = FoundChains[ChainIndex];
		UE_LOG(LogAssetManagerEditor, Log, TEXT("Chain %i"), ChainIndex);

		for (FName& Name : FoundChain)
		{
			UE_LOG(LogAssetManagerEditor, Log, TEXT("\t%s"), *Name.ToString());
		}
	}
}

void FAssetManagerEditorModule::FindClassDependencies(IAssetRegistry& AssetRegistry, FName SourcePackageName, const TArray<FName>& TargetClasses, EAssetRegistryDependencyType::Type DependencyType)
{
	TArray<FAssetData> PackageAssets;
	if (!AssetRegistry.GetAssetsByPackageName(SourcePackageName, PackageAssets))
	{
		UE_LOG(LogAssetManagerEditor, Log, TEXT("Couldn't find source package %s. Abandoning class dep search.  "), *SourcePackageName.ToString());
		return;
	}

	TArray<FName> VisitedPackages;
	TArray<FName> DependencyPackages;
	GetPackageDependenciesPerClass(AssetRegistry, SourcePackageName, TargetClasses, VisitedPackages, DependencyPackages, DependencyType);

	if (DependencyPackages.Num() > 0)
	{
		UE_LOG(LogAssetManagerEditor, Log, TEXT("Found %i: dependencies for %s of the target classes"), DependencyPackages.Num(), *SourcePackageName.ToString());
		for (FName DependencyPackage : DependencyPackages)
		{
			UE_LOG(LogAssetManagerEditor, Log, TEXT("\t%s"), *DependencyPackage.ToString());
		}

		for (FName DependencyPackage : DependencyPackages)
		{
			TArray<FName> Chain;
			VisitedPackages.Reset();
			GetPackageDependencyChain(AssetRegistry, SourcePackageName, DependencyPackage, VisitedPackages, Chain, DependencyType);

			UE_LOG(LogAssetManagerEditor, Log, TEXT("Chain to package: %s"), *DependencyPackage.ToString());
			TArray<FAssetData> DepAssets;

			FARFilter Filter;
			Filter.PackageNames.Add(DependencyPackage);
			Filter.ClassNames = TargetClasses;
			Filter.bIncludeOnlyOnDiskAssets = true;

			if (AssetRegistry.GetAssets(Filter, DepAssets))
			{
				for (const FAssetData& DepAsset : DepAssets)
				{
					if (TargetClasses.Contains(DepAsset.AssetClass))
					{
						UE_LOG(LogAssetManagerEditor, Log, TEXT("Asset: %s class: %s"), *DepAsset.AssetName.ToString(), *DepAsset.AssetClass.ToString());
					}
				}
			}

			for (FName DepChainEntry : Chain)
			{
				UE_LOG(LogAssetManagerEditor, Log, TEXT("\t%s"), *DepChainEntry.ToString());
			}
		}
	}
}

void FAssetManagerEditorModule::WriteProfileFile(const FString& Extension, const FString& FileContents)
{
	const FString PathName = *(FPaths::ProfilingDir() + TEXT("AssetAudit/"));
	IFileManager::Get().MakeDirectory(*PathName);

	const FString Filename = CreateProfileFilename(Extension, true);
	const FString FilenameFull = PathName + Filename;

	UE_LOG(LogAssetManagerEditor, Log, TEXT("Saving %s"), *FPaths::ConvertRelativePathToFull(FilenameFull));
	FFileHelper::SaveStringToFile(FileContents, *FilenameFull);
}


void FAssetManagerEditorModule::LogAssetsWithMultipleLabels()
{
	UAssetManager& Manager = UAssetManager::Get();
	IAssetRegistry& AssetRegistry = Manager.GetAssetRegistry();

	TMap<FName, TArray<FPrimaryAssetId>> PackageToLabelMap;
	TArray<FPrimaryAssetId> LabelNames;

	Manager.GetPrimaryAssetIdList(UAssetManager::PrimaryAssetLabelType, LabelNames);

	for (const FPrimaryAssetId& Label : LabelNames)
	{
		TArray<FName> LabeledPackages;

		Manager.GetManagedPackageList(Label, LabeledPackages);

		for (FName Package : LabeledPackages)
		{
			PackageToLabelMap.FindOrAdd(Package).AddUnique(Label);
		}
	}

	PackageToLabelMap.KeySort(TLess<FName>());

	UE_LOG(LogAssetManagerEditor, Log, TEXT("\nAssets with multiple labels follow"));

	// Print them out
	for (TPair<FName, TArray<FPrimaryAssetId>> Pair : PackageToLabelMap)
	{
		if (Pair.Value.Num() > 1)
		{
			FString TagString;
			for (const FPrimaryAssetId& Label : Pair.Value)
			{
				if (!TagString.IsEmpty())
				{
					TagString += TEXT(", ");
				}
				TagString += Label.ToString();
			}

			UE_LOG(LogAssetManagerEditor, Log, TEXT("%s has %s"), *Pair.Key.ToString(), *TagString);
		}		
	}
}

void FAssetManagerEditorModule::DumpAssetDependencies(const TArray<FString>& Args)
{
	if (!UAssetManager::IsValid())
	{
		return;
	}

	UAssetManager& Manager = UAssetManager::Get();
	IAssetRegistry& AssetRegistry = Manager.GetAssetRegistry();
	TArray<FPrimaryAssetTypeInfo> TypeInfos;

	Manager.UpdateManagementDatabase();

	Manager.GetPrimaryAssetTypeInfoList(TypeInfos);

	TypeInfos.Sort([](const FPrimaryAssetTypeInfo& LHS, const FPrimaryAssetTypeInfo& RHS) { return LHS.PrimaryAssetType < RHS.PrimaryAssetType; });

	UE_LOG(LogAssetManagerEditor, Log, TEXT("=========== Asset Manager Dependencies ==========="));

	TArray<FString> ReportLines;

	ReportLines.Add(TEXT("digraph { "));

	for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfos)
	{
		struct FDependencyInfo
		{
			FName AssetName;
			FString AssetListString;

			FDependencyInfo(FName InAssetName, const FString& InAssetListString) : AssetName(InAssetName), AssetListString(InAssetListString) {}
		};

		TArray<FDependencyInfo> DependencyInfos;
		TArray<FPrimaryAssetId> PrimaryAssetIds;

		Manager.GetPrimaryAssetIdList(TypeInfo.PrimaryAssetType, PrimaryAssetIds);

		for (const FPrimaryAssetId& PrimaryAssetId : PrimaryAssetIds)
		{
			TArray<FAssetIdentifier> FoundDependencies;
			TArray<FString> DependencyStrings;

			AssetRegistry.GetDependencies(PrimaryAssetId, FoundDependencies, EAssetRegistryDependencyType::Manage);

			for (const FAssetIdentifier& Identifier : FoundDependencies)
			{
				FString ReferenceString = Identifier.ToString();
				DependencyStrings.Add(ReferenceString);

				ReportLines.Add(FString::Printf(TEXT("\t\"%s\" -> \"%s\";"), *PrimaryAssetId.ToString(), *ReferenceString));
			}

			DependencyStrings.Sort();

			DependencyInfos.Emplace(PrimaryAssetId.PrimaryAssetName, *FString::Join(DependencyStrings, TEXT(", ")));
		}

		if (DependencyInfos.Num() > 0)
		{
			UE_LOG(LogAssetManagerEditor, Log, TEXT("  Type %s:"), *TypeInfo.PrimaryAssetType.ToString());

			DependencyInfos.Sort([](const FDependencyInfo& LHS, const FDependencyInfo& RHS) { return LHS.AssetName < RHS.AssetName; });

			for (FDependencyInfo& DependencyInfo : DependencyInfos)
			{
				UE_LOG(LogAssetManagerEditor, Log, TEXT("    %s: depends on %s"), *DependencyInfo.AssetName.ToString(), *DependencyInfo.AssetListString);
			}
		}
	}

	ReportLines.Add(TEXT("}"));

	Manager.WriteCustomReport(FString::Printf(TEXT("PrimaryAssetReferences%s.gv"), *FDateTime::Now().ToString()), ReportLines);
}

void FAssetManagerEditorModule::PrintSizeSummaries(const FString& PlatformName)
{
	// Files that only have one tag
/*	TMap<UPrimaryAssetLabel*, int64> TotalUniqueSizes;

	TMap<UPrimaryAssetLabel*, TMap<FName, FAssetList>> UniqueSizeAssetsPerClass;

	// Files that are shared across multiple tags (these get N x counted, so OverallExtraSharedSize is the 'only counts once' sum)
	TMap<UPrimaryAssetLabel*, int64> TotalSharedSizes;

	TMap<UPrimaryAssetLabel*, TMap<FName, FAssetList>> SharedSizeAssetsPerClass;

	// Asset types
	TMap<FName, FAssetList> TotalAssetSizes;

	// The sum of the files shared across multiple tags
	int64 OverallExtraSharedSize = 0;
	TMap<FName, FAssetList> ExtraSharedAssetsPerClass;

	// The files that were cooked but have no tags
	int64 UntaggedSize = 0;
	TMap<FName, FAssetList> UntaggedAssetsPerClass;

	// The sum of all files
	int64 TotalSize = 0;

	// The sum of files that are shared but not owned by any seed
	int64 SharedAndTaggedButNotOwnedSize = 0;
	TMap<FName, FAssetList> SharedNotOwnedAssetsPerClass;

	// The sum of all assets
	int64 TotalAssetSize = 0;

	// Sum everything up
	for (const auto& CookedAssetKV : CookedAssetSizes)
	{
		const FCookedAsset& Asset = CookedAssetKV.Value;

		if (Asset.Size < 0)
		{
			UE_LOG(LogAssetManagerEditor, Warning, TEXT("Asset %s has an invalid size %lld (might be a redirector)"), *CookedAssetKV.Key.ToString(), Asset.Size);
			continue;
		}

		TotalSize += Asset.Size;

		if (TArray<UPrimaryAssetLabel*>* pTags = TaggedAssets.Find(CookedAssetKV.Key))
		{
			TArray<UPrimaryAssetLabel*>& Tags = *pTags;
			check(Tags.Num() > 0);

			const bool bShared = Tags.Num() > 1;
			UPrimaryAssetLabel* UniqueOwner = UniqueOwningTags.FindRef(CookedAssetKV.Key);

			for (UPrimaryAssetLabel* Tag : Tags)
			{
				if (bShared)
				{
					TotalSharedSizes.FindOrAdd(Tag) += Asset.Size;
					SharedSizeAssetsPerClass.FindOrAdd(Tag).FindOrAdd(Asset.Class).TotalSize += Asset.Size;
					SharedSizeAssetsPerClass.FindOrAdd(Tag).FindOrAdd(Asset.Class).Assets.Add(CookedAssetKV.Key);
				}

				if (!bShared || (Tag == UniqueOwner))
				{
					TotalUniqueSizes.FindOrAdd(Tag) += Asset.Size;
					UniqueSizeAssetsPerClass.FindOrAdd(Tag).FindOrAdd(Asset.Class).TotalSize += Asset.Size;
					UniqueSizeAssetsPerClass.FindOrAdd(Tag).FindOrAdd(Asset.Class).Assets.Add(CookedAssetKV.Key);
				}
			}

			if (UniqueOwner != nullptr)
			{
				if (!bShared)
				{
					check(UniqueOwner == Tags[0]);
				}
			}
			else
			{
				if (bShared)
				{
					SharedAndTaggedButNotOwnedSize += Asset.Size;
					SharedNotOwnedAssetsPerClass.FindOrAdd(Asset.Class).TotalSize += Asset.Size;
					SharedNotOwnedAssetsPerClass.FindOrAdd(Asset.Class).Assets.Add(CookedAssetKV.Key);
				}
			}

			if (bShared)
			{
				OverallExtraSharedSize += Asset.Size;
				ExtraSharedAssetsPerClass.FindOrAdd(Asset.Class).TotalSize += Asset.Size;
				ExtraSharedAssetsPerClass.FindOrAdd(Asset.Class).Assets.Add(CookedAssetKV.Key);
			}
		}
		else
		{
			UntaggedSize += Asset.Size;
			UntaggedAssetsPerClass.FindOrAdd(Asset.Class).TotalSize += Asset.Size;
			UntaggedAssetsPerClass.FindOrAdd(Asset.Class).Assets.Add(CookedAssetKV.Key);
		}

		TotalAssetSizes.FindOrAdd(Asset.Class).TotalSize += Asset.Size;
		TotalAssetSizes.FindOrAdd(Asset.Class).Assets.Add(CookedAssetKV.Key);
		TotalAssetSize += Asset.Size;
	}

	TArray<UPrimaryAssetLabel*> TagOrder = AllTags;
	TagOrder.Sort([](const UPrimaryAssetLabel& A, const UPrimaryAssetLabel& B) { return A.GetPrimaryAssetId().ToString() < B.GetPrimaryAssetId().ToString(); });

	// Print it out (csv)
	{
		FString ResultsCSV;

		ResultsCSV += TEXT("TagName,UniqueSizeBytes,SharedSizeBytes\n");
		for (UPrimaryAssetLabel* Tag : TagOrder)
		{
			ResultsCSV += FString::Printf(TEXT("%s,%lld,%lld\n"), *Tag->GetPrimaryAssetId().ToString(), TotalUniqueSizes.FindRef(Tag), TotalSharedSizes.FindRef(Tag));
		}
		ResultsCSV += FString::Printf(TEXT("ZZZ_Shared,%lld,%lld\n"), SharedAndTaggedButNotOwnedSize, OverallExtraSharedSize);
		ResultsCSV += FString::Printf(TEXT("ZZZ_Untagged,%lld,0\n"), UntaggedSize);

		const FString ProfileExtensionCSV = FString::Printf(TEXT("_Tags.%s.csv"), *PlatformName);
		WriteProfileFile(ProfileExtensionCSV, ResultsCSV);
	}
	{
		FString ResultsCSV;

		ResultsCSV += TEXT("TagName,UniqueSizeBytes\n");
		for (auto KV : TotalAssetSizes)
		{
			ResultsCSV += FString::Printf(TEXT("%s,%lld\n"), *KV.Key.ToString(), KV.Value.TotalSize);
		}

		const FString ProfileExtensionCSV = FString::Printf(TEXT("_Assets.%s.csv"), *PlatformName);
		WriteProfileFile(ProfileExtensionCSV, ResultsCSV);
	}

	// Print it out (html)
	{
		const double BytesToMB = 1.0 / (1024.0 * 1024.0);
		FString ResultsHTML = TEXT("<html><body><table>\n");

		ResultsHTML += TEXT("\t<tr><td>TagName</td><td>UniqueSizeMB</td><td>SharedSizeMB</td></tr>\n");
		for (UPrimaryAssetLabel* Tag : TagOrder)
		{
			double UniqueSize = TotalUniqueSizes.FindRef(Tag) * BytesToMB;
			double SharedSize = TotalSharedSizes.FindRef(Tag) * BytesToMB;
			ResultsHTML += FString::Printf(TEXT("\t<tr><td>%s</td><td>%.2f</td><td>%.2f</td></tr>\n"), *Tag->GetPrimaryAssetId().ToString(), UniqueSize, SharedSize);
		}

		ResultsHTML += FString::Printf(TEXT("\t<tr><td>%s</td><td>%.2f</td><td>%.2f</td></tr>\n"), TEXT("ZZZ_Shared"), SharedAndTaggedButNotOwnedSize * BytesToMB, OverallExtraSharedSize * BytesToMB);
		ResultsHTML += FString::Printf(TEXT("\t<tr><td>%s</td><td>%.2f</td><td>%.2f</td></tr>\n"), TEXT("ZZZ_Untagged"), UntaggedSize * BytesToMB, 0.0);
		ResultsHTML += FString::Printf(TEXT("\t<tr><td>%s</td><td>%.2f</td><td>%.2f</td></tr>\n"), TEXT("TotalSize"), TotalSize * BytesToMB, 0.0);
		ResultsHTML += TEXT("</table></body></html>\n");

		const FString ProfileExtensionHTML = FString::Printf(TEXT("_Tags.%s.html"), *PlatformName);
		WriteProfileFile(ProfileExtensionHTML, ResultsHTML);
	}
	{
		const double BytesToMB = 1.0 / (1024.0 * 1024.0);
		FString ResultsHTML = TEXT("<html><body><table>\n");

		ResultsHTML += TEXT("\t<tr><td>AssetClass</td><td>UniqueSizeMB</td></tr>\n");
		for (auto KV : TotalAssetSizes)
		{
			double UniqueSize = KV.Value.TotalSize * BytesToMB;
			ResultsHTML += FString::Printf(TEXT("\t<tr><td>%s</td><td>%.2f</td></tr>\n"), *KV.Key.ToString(), UniqueSize);
		}

		ResultsHTML += FString::Printf(TEXT("\t<tr><td>%s</td><td>%.2f</td></tr>\n"), TEXT("TotalSize"), TotalAssetSize * BytesToMB);

		ResultsHTML += TEXT("</table></body></html>\n");

		const FString ProfileExtensionHTML = FString::Printf(TEXT("_Assets.%s.html"), *PlatformName);
		WriteProfileFile(ProfileExtensionHTML, ResultsHTML);
	}

	// Print out the asset lists
	{
		const double BytesToMB = 1.0 / (1024.0 * 1024.0);
		FString ResultsHTML = TEXT("<html><body><ul>\n");

		for (UPrimaryAssetLabel* Tag : TagOrder)
		{
			double UniqueSize = TotalUniqueSizes.FindRef(Tag) * BytesToMB;
			double SharedSize = TotalSharedSizes.FindRef(Tag) * BytesToMB;
			ResultsHTML += FString::Printf(TEXT("\t<li>%s</li><ul><li>Unique (%.2f)</li><ul>\n"), *Tag->GetPrimaryAssetId().ToString(), UniqueSize);
			for (auto KV : UniqueSizeAssetsPerClass.FindRef(Tag))
			{
				ResultsHTML += FString::Printf(TEXT("\t\t<li>%s</li><ul>\n"), *KV.Key.ToString());
				KV.Value.Assets.Sort([&](const FName& A, const FName& B) { return CookedAssetSizes.FindRef(A).Size > CookedAssetSizes.FindRef(B).Size; });
				for (auto L : KV.Value.Assets)
				{
					ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li>\n"), *L.ToString(), CookedAssetSizes.FindRef(L).Size * BytesToMB);
				}
				ResultsHTML += TEXT("</ul>\n");
			}
			ResultsHTML += FString::Printf(TEXT("</ul><li>Shared (%.2f)</li><ul>\n"), SharedSize);
			for (auto KV : SharedSizeAssetsPerClass.FindRef(Tag))
			{
				ResultsHTML += FString::Printf(TEXT("\t\t<li>%s</li><ul>\n"), *KV.Key.ToString());
				KV.Value.Assets.Sort([&](const FName& A, const FName& B) { return CookedAssetSizes.FindRef(A).Size > CookedAssetSizes.FindRef(B).Size; });
				for (auto L : KV.Value.Assets)
				{
					ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li>\n"), *L.ToString(), CookedAssetSizes.FindRef(L).Size * BytesToMB);
				}
				ResultsHTML += TEXT("</ul>\n");
			}
			ResultsHTML += TEXT("</ul></ul>\n");
		}

		ResultsHTML += FString::Printf(TEXT("\t<li>%s (%.2f)</li><ul>\n"), TEXT("ZZZ_Shared"), SharedAndTaggedButNotOwnedSize * BytesToMB);
		for (auto KV : SharedNotOwnedAssetsPerClass)
		{
			ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li><ul>\n"), *KV.Key.ToString(), KV.Value.TotalSize * BytesToMB);
			KV.Value.Assets.Sort([&](const FName& A, const FName& B) { return CookedAssetSizes.FindRef(A).Size > CookedAssetSizes.FindRef(B).Size; });
			for (auto L : KV.Value.Assets)
			{
				ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li>\n"), *L.ToString(), CookedAssetSizes.FindRef(L).Size * BytesToMB);
			}
			ResultsHTML += TEXT("</ul>\n");
		}
		ResultsHTML += TEXT("</ul>\n");

		ResultsHTML += FString::Printf(TEXT("\t<li>%s (%.2f)</li><ul>\n"), TEXT("ZZZ_ExtraShared"), OverallExtraSharedSize * BytesToMB);
		for (auto KV : ExtraSharedAssetsPerClass)
		{
			ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li><ul>\n"), *KV.Key.ToString(), KV.Value.TotalSize * BytesToMB);
			KV.Value.Assets.Sort([&](const FName& A, const FName& B) { return CookedAssetSizes.FindRef(A).Size > CookedAssetSizes.FindRef(B).Size; });
			for (auto L : KV.Value.Assets)
			{
				ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li>\n"), *L.ToString(), CookedAssetSizes.FindRef(L).Size * BytesToMB);
			}
			ResultsHTML += TEXT("</ul>\n");
		}
		ResultsHTML += TEXT("</ul>\n");

		ResultsHTML += FString::Printf(TEXT("\t<li>%s (%.2f)</li><ul>\n"), TEXT("ZZZ_Untagged"), UntaggedSize * BytesToMB);
		for (auto KV : UntaggedAssetsPerClass)
		{
			ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li><ul>\n"), *KV.Key.ToString(), KV.Value.TotalSize * BytesToMB);
			KV.Value.Assets.Sort([&](const FName& A, const FName& B) { return CookedAssetSizes.FindRef(A).Size > CookedAssetSizes.FindRef(B).Size; });
			for (auto L : KV.Value.Assets)
			{
				ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li>\n"), *L.ToString(), CookedAssetSizes.FindRef(L).Size * BytesToMB);
			}
			ResultsHTML += TEXT("</ul>\n");
		}
		ResultsHTML += TEXT("</ul>\n");
		ResultsHTML += TEXT("</ul></body></html>\n");

		const FString ProfileExtensionHTML = FString::Printf(TEXT("_TagList.%s.html"), *PlatformName);
		WriteProfileFile(ProfileExtensionHTML, ResultsHTML);
	}
	{
		const double BytesToMB = 1.0 / (1024.0 * 1024.0);
		FString ResultsHTML = TEXT("<html><body><ul>\n");

		ResultsHTML += FString::Printf(TEXT("\t<li>%s (%.2f)</li><ul>\n"), TEXT("Asset Classes"), TotalAssetSize * BytesToMB);
		for (auto KV : TotalAssetSizes)
		{
			ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li><ul>\n"), *KV.Key.ToString(), KV.Value.TotalSize * BytesToMB);
			KV.Value.Assets.Sort([&](const FName& A, const FName& B) { return CookedAssetSizes.FindRef(A).Size > CookedAssetSizes.FindRef(B).Size; });
			for (auto L : KV.Value.Assets)
			{
				ResultsHTML += FString::Printf(TEXT("\t\t<li>%s (%.2f)</li>\n"), *L.ToString(), CookedAssetSizes.FindRef(L).Size * BytesToMB);
			}
			ResultsHTML += TEXT("</ul>\n");
		}
		ResultsHTML += TEXT("</ul>\n");
		ResultsHTML += TEXT("</ul></body></html>\n");

		const FString ProfileExtensionHTML = FString::Printf(TEXT("_AssetsList.%s.html"), *PlatformName);
		WriteProfileFile(ProfileExtensionHTML, ResultsHTML);
	}*/
}

bool FAssetManagerEditorModule::CreateOrEmptyCollection(FName CollectionName)
{
	ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();

	if (CollectionManager.CollectionExists(CollectionName, ECollectionShareType::CST_Local))
	{
		return CollectionManager.EmptyCollection(CollectionName, ECollectionShareType::CST_Local);
	}
	else if (CollectionManager.CreateCollection(CollectionName, ECollectionShareType::CST_Local, ECollectionStorageMode::Static))
	{
		return true;
	}

	return false;
}

void FAssetManagerEditorModule::WriteCollection(FName CollectionName, const TArray<FName>& PackageNames)
{
	if (CreateOrEmptyCollection(CollectionName))
	{
		TArray<FName> AssetNames = PackageNames;

		// Convert package names to asset names
		for (FName& Name : AssetNames)
		{
			FString PackageName = Name.ToString();
			int32 LastPathDelimiter;
			if (PackageName.FindLastChar(TEXT('/'), /*out*/ LastPathDelimiter))
			{
				const FString AssetName = PackageName.Mid(LastPathDelimiter + 1);
				PackageName = PackageName + FString(TEXT(".")) + AssetName;
				Name = *PackageName;
			}
		}

		ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
		CollectionManager.AddToCollection(CollectionName, ECollectionShareType::CST_Local, AssetNames);

		UE_LOG(LogAssetManagerEditor, Log, TEXT("Updated collection %s"), *CollectionName.ToString());
	}
	else
	{
		UE_LOG(LogAssetManagerEditor, Warning, TEXT("Failed to update collection %s"), *CollectionName.ToString());
	}
}

void FAssetManagerEditorModule::RecreateCollections()
{
	ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();

/*	// Create collections for untagged assets
	{
		TArray<FName> UntaggedPaths;
		UntaggedPaths.Empty(CookedAssetSizes.Num());

		for (auto CookedAssetKV : CookedAssetSizes)
		{
			if (!TaggedAssets.Contains(CookedAssetKV.Key))
			{
				UntaggedPaths.Add(CookedAssetKV.Key);
			}
		}
		WriteCollection(TEXT("Audit_CookedButUntagged"), UntaggedPaths);
		WriteSizeSortedList(UntaggedPaths);
	}

	// Create collections for cooked assets
	{
		TArray<FName> CookedPaths;
		CookedPaths.Empty(CookedAssetSizes.Num());

		for (auto CookedAssetKV : CookedAssetSizes)
		{
			CookedPaths.Add(CookedAssetKV.Key);
		}

		WriteCollection(TEXT("Audit_Cooked"), CookedPaths);
	}

	// Create a collection for developer folder assets
	{
		TArray<FName> DeveloperPaths;
		DeveloperPaths.Empty(CookedAssetSizes.Num());

		const FString DeveloperFolderPrefix(TEXT("/Game/Developers/"));

		for (auto CookedAssetKV : CookedAssetSizes)
		{
			if (CookedAssetKV.Key.ToString().StartsWith(DeveloperFolderPrefix, ESearchCase::CaseSensitive))
			{
				DeveloperPaths.Add(CookedAssetKV.Key);
			}
		}

		WriteCollection(TEXT("Audit_CookedDeveloperFolders"), DeveloperPaths);
	}

	// Create a collection for WIP assets
	{
		WriteCollection(TEXT("Audit_WorkInProgress"), WorksInProgress.Array());
	}

	// Create a collection for WIP assets that are incorrectly referenced by non-WIP assets
	{
		TArray<FName> HygieneErrors;
		HygieneErrors.Empty(CookedAssetSizes.Num());

		for (const auto& AssetKV : TaggedAssets)
		{
			const FName& AssetName = AssetKV.Key;

			if (WorksInProgress.Contains(AssetName))
			{
				// It's a work in progress, see if it's used by any seeds who are *not* a work in progress
				const TArray<UPrimaryAssetLabel*>& AssetTags = AssetKV.Value;

				bool bUsedByNonWIP = false;
				for (const UPrimaryAssetLabel* Tag : AssetTags)
				{
					if (!Tag->bIncompleteWorkInProgress)
					{
						bUsedByNonWIP = true;
						break;
					}
				}

				if (bUsedByNonWIP)
				{
					HygieneErrors.Add(AssetName);
				}
			}
		}

		WriteCollection(TEXT("Audit_ErrorUsingWIP"), HygieneErrors);
	}

	*/
}

void FAssetManagerEditorModule::WriteSizeSortedList(TArray<FName>& PackageNames) const
{
/*	PackageNames.Sort([&](const FName& A, const FName& B) { return CookedAssetSizes.FindRef(A).Size > CookedAssetSizes.FindRef(B).Size; });

	for (const FName& AssetName : PackageNames)
	{
		UE_LOG(LogAssetManagerEditor, Log, TEXT("%s,%lld"), *AssetName.ToString(), CookedAssetSizes.FindRef(AssetName).Size);
	}*/
}

#undef LOCTEXT_NAMESPACE