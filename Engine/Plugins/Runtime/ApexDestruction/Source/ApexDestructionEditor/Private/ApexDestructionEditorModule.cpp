// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ApexDestructionEditorModule.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "IDestructibleMeshEditor.h"
#include "DestructibleMeshEditor.h"
#include "Misc/MessageDialog.h"

#include "IAssetTools.h"
#include "AssetToolsModule.h"

#include "DestructibleMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSpeedTree.h"

#include "AssetToolsModule.h"
#include "AssetTypeActions_DestructibleMesh.h"
#include "ContentBrowserDelegates.h"
#include "MultiBoxBuilder.h"
#include "ContentBrowserModule.h"

#include "DestructibleMeshThumbnailRenderer.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "DestructibleMeshComponentBroker.h"
#include "ApexDestructionStyle.h"
#include "PropertyEditorModule.h"
#include "DestructibleMeshDetails.h"

IMPLEMENT_MODULE( FDestructibleMeshEditorModule, DestructibleMeshEditor );

#define LOCTEXT_NAMESPACE "DestructibleMeshEditor"

const FName DestructibleMeshEditorAppIdentifier = FName(TEXT("DestructibleMeshEditorApp"));


void FDestructibleMeshEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetAction = MakeShareable(new FAssetTypeActions_DestructibleMesh);
	AssetTools.RegisterAssetTypeActions(AssetAction.ToSharedRef());

	if (!IsRunningCommandlet())
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FDestructibleMeshEditorModule::OnExtendContentBrowserAssetSelectionMenu));
		ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
	}

	UThumbnailManager::Get().RegisterCustomRenderer(UDestructibleMesh::StaticClass(), UDestructibleMeshThumbnailRenderer::StaticClass());

	DestructibleMeshComponentBroker = MakeShareable(new FDestructibleMeshComponentBroker);
	FComponentAssetBrokerage::RegisterBroker(DestructibleMeshComponentBroker, UDestructibleComponent::StaticClass(), false, true);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(UDestructibleMesh::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FDestructibleMeshDetails::MakeInstance));

	FApexDestructionStyle::Initialize();
}

TSharedRef<FExtender> FDestructibleMeshEditorModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());

	// Run thru the assets to determine if any meet our criteria
	bool bAnyStaticMesh = false;
	for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
	{
		const FAssetData& Asset = *AssetIt;
		bAnyStaticMesh = bAnyStaticMesh || (Asset.AssetClass == UStaticMesh::StaticClass()->GetFName());
	}

	if (bAnyStaticMesh)
	{
		// Add the sprite actions sub-menu extender
		Extender->AddMenuExtension("GetAssetActions", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda([SelectedAssets](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_CreateDestructibleMesh", "Create Destructible Mesh"),
				NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_CreateDestructibleMeshTooltip", "Creates a DestructibleMesh from the StaticMesh and opens it in the DestructibleMesh editor."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.DestructibleComponent"),
				FUIAction(FExecuteAction::CreateStatic(&FAssetTypeActions_DestructibleMesh::ExecuteCreateDestructibleMeshes, SelectedAssets), FCanExecuteAction()));
		}));
	}

	return Extender;
}


void FDestructibleMeshEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(AssetAction.ToSharedRef());
	}

	FApexDestructionStyle::Shutdown();

	if(FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });
	}
	

	if (UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UDestructibleMesh::StaticClass());
		FComponentAssetBrokerage::UnregisterBroker(DestructibleMeshComponentBroker);
	}
}


TSharedRef<IDestructibleMeshEditor> FDestructibleMeshEditorModule::CreateDestructibleMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UDestructibleMesh* Table )
{
	TSharedRef< FDestructibleMeshEditor > NewDestructibleMeshEditor( new FDestructibleMeshEditor() );
	NewDestructibleMeshEditor->InitDestructibleMeshEditor( Mode, InitToolkitHost, Table );
	return NewDestructibleMeshEditor;
}

UDestructibleMesh* FDestructibleMeshEditorModule::CreateDestructibleMeshFromStaticMesh(UObject* InParent, UStaticMesh* StaticMesh, FName Name, EObjectFlags Flags, FText& OutErrorMsg)
{
	if (StaticMesh == NULL)
	{
		OutErrorMsg = LOCTEXT( "StaticMeshInvalid", "Static Mesh is Invalid!" );
		return NULL;
	}

	// We can't use any speedtree materials for destructibles due to UV requirements, so check the graph here
	TArray<UMaterial*> SpeedTreeMaterials;
	for(FStaticMaterial& StaticMaterial : StaticMesh->StaticMaterials)
	{
		if(UMaterialInterface* Mat = StaticMaterial.MaterialInterface)
		{
			if(UMaterial* BaseMat = Mat->GetBaseMaterial())
			{
				for(UMaterialExpression* Expression : BaseMat->Expressions)
				{
					if(Cast<UMaterialExpressionSpeedTree>(Expression))
					{
						SpeedTreeMaterials.Add(BaseMat);
						break;
					}
				}
			}
		}
	}
	
	if(SpeedTreeMaterials.Num() > 0)
	{
		// Invalid, can't create a mesh from this
		FTextBuilder TextBuilder;
		TextBuilder.AppendLine(FText::Format(LOCTEXT("StaticMeshInvalid_SpeedTree", "The static mesh '{0}' uses SpeedTree materials which are not compatible with destructible meshes. Cannot create destructible.\n\nList of Materials:\n"), FText::FromString(StaticMesh->GetName())));
	
		for(UMaterial* SpeedTreeMat : SpeedTreeMaterials)
		{
			TextBuilder.AppendLine(SpeedTreeMat->GetName());
		}
	
		FMessageDialog::Open(EAppMsgType::Ok, TextBuilder.ToText());
		return nullptr;
	}


	FString DestructibleName;
	
	if (Name == NAME_None)
	{
		DestructibleName = StaticMesh->GetName();
		DestructibleName += TEXT("_DM");
	}
	else
	{
		DestructibleName = *Name.ToString();
	}

	UDestructibleMesh* DestructibleMesh = FindObject<UDestructibleMesh>( InParent, *DestructibleName );

	// If there is an existing mesh, ask the user if they want to use the existing mesh, replace the existing mesh, or create a new mesh.
	if (DestructibleMesh != NULL)
	{
		enum ENameConflictDialogReturnType
		{
			NCD_None = 0,
			NCD_Use,
			NCD_Replace,
			NCD_Create
		};
#if 0	// BRG - removed SSimpleDialog, needs replacing
		FSimpleDialogModule& SimpleDialogModule = FModuleManager::LoadModuleChecked<FSimpleDialogModule>( TEXT("SimpleDialog") );
		SimpleDialogModule.CreateSimpleDialog(NSLOCTEXT("SimpleDialogModule", "DestructibleMeshAlreadyExistsTitle", "DestructibleMesh Already Exists").ToString(), NSLOCTEXT("SimpleDialogModule", "DestructibleMeshAlreadyExistsMessage", "A DestructibleMesh by the same name already exists.  Select an action.").ToString());
		SimpleDialogModule.AddButton(NCD_Use, NSLOCTEXT("SimpleDialogModule", "UseExistingDestructibleMesh", "Use").ToString(), NSLOCTEXT("SimpleDialogModule", "UseExistingDestructibleMeshTip", "Open the DestructibleMesh editor with the existing mesh").ToString());
		SimpleDialogModule.AddButton(NCD_Replace, NSLOCTEXT("SimpleDialogModule", "ReplaceExistingDestructibleMesh", "Replace").ToString(), NSLOCTEXT("SimpleDialogModule", "ReplaceExistingDestructibleMeshTip", "Replace the existing DestructibleMesh with a new one, and show in the DestructibleMesh editor").ToString());
		SimpleDialogModule.AddButton(NCD_Create, NSLOCTEXT("SimpleDialogModule", "CreateNewDestructibleMesh", "Create").ToString(), NSLOCTEXT("SimpleDialogModule", "CreateNewDestructibleMeshTip", "Create a new DestructibleMesh with a different name, and show in the DestructibleMesh editor").ToString());
		const uint32 UserResponse = SimpleDialogModule.ShowSimpleDialog();
#endif
		const uint32 UserResponse = NCD_None;
		CA_SUPPRESS(6326);
		switch (UserResponse)
		{
		default:	// Default to using the same mesh
		case NCD_Use:
			return DestructibleMesh;
		case NCD_Replace:
			break;
		case NCD_Create:
			Name = MakeUniqueObjectName(StaticMesh->GetOuter(), UDestructibleMesh::StaticClass(), Name);
			DestructibleName = *Name.ToString();
			DestructibleMesh = NULL;	// So that one will be created
			break;
		}
	}
	
	// Create the new UDestructibleMesh object if we still haven't found one after a possible resolution above
	if (DestructibleMesh == NULL)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		DestructibleMesh = Cast<UDestructibleMesh>(AssetToolsModule.Get().CreateAsset(DestructibleName, FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetPathName()), UDestructibleMesh::StaticClass(), NULL));

		if(!DestructibleMesh)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Name"), FText::FromString( StaticMesh->GetName() ));

			OutErrorMsg = FText::Format( LOCTEXT( "DestructibleMeshFailed", "Failed to Create Destructible Mesh Asset from {Name}!" ), Arguments );
			return NULL;
		}
	}

	DestructibleMesh->BuildFromStaticMesh(*StaticMesh);
	
	return DestructibleMesh;
}

#undef LOCTEXT_NAMESPACE
