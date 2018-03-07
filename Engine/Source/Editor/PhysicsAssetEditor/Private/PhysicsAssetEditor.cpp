// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetEditor.h"
#include "Framework/MultiBox/MultiBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EngineGlobals.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "EditorStyleSet.h"
#include "Preferences/PhysicsAssetEditorOptions.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsAssetEditorModule.h"
#include "ScopedTransaction.h"
#include "PhysicsAssetEditorActions.h"
#include "PhysicsAssetEditorSkeletalMeshComponent.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "WorkflowOrientedApp/SContentReference.h"
#include "MeshUtilities.h"

#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/AnalyticsEventAttribute.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "Widgets/Docking/SDockTab.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/ConstraintUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/Selection.h"
#include "PersonaModule.h"

#include "PhysicsAssetEditorAnimInstance.h"
#include "PhysicsAssetEditorAnimInstanceProxy.h"


#include "PhysicsAssetEditorMode.h"
#include "PersonaModule.h"
#include "IAssetFamily.h"
#include "ISkeletonEditorModule.h"
#include "IPersonaToolkit.h"
#include "IPersonaPreviewScene.h"
#include "PhysicsAssetEditorSkeletonTreeBuilder.h"
#include "BoneProxy.h"
#include "SPhysicsAssetGraph.h"
#include "PhysicsAssetEditorEditMode.h"
#include "AssetEditorModeManager.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "ISkeletonTreeItem.h"
#include "Algo/Transform.h"
#include "SkeletonTreeSelection.h"
#include "SkeletonTreePhysicsBodyItem.h"
#include "SkeletonTreePhysicsShapeItem.h"
#include "SkeletonTreePhysicsConstraintItem.h"
#include "ScopedSlowTask.h"
#include "PhysicsAssetGenerationSettings.h"

const FName PhysicsAssetEditorModes::PhysicsAssetEditorMode("PhysicsAssetEditorMode");

const FName PhysicsAssetEditorAppIdentifier = FName(TEXT("PhysicsAssetEditorApp"));

DEFINE_LOG_CATEGORY(LogPhysicsAssetEditor);
#define LOCTEXT_NAMESPACE "PhysicsAssetEditor"

namespace PhysicsAssetEditor
{
	const float	DefaultPrimSize = 15.0f;
	const float	DuplicateXOffset = 10.0f;
}

void FPhysicsAssetEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_PhysicsAssetEditor", "PhysicsAssetEditor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

FPhysicsAssetEditor::~FPhysicsAssetEditor()
{
	if( SharedData->bRunningSimulation )
	{
		// Disable simulation when shutting down
		ImpToggleSimulation();
	}

	GEditor->UnregisterForUndo(this);
}

void FPhysicsAssetEditor::InitPhysicsAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UPhysicsAsset* ObjectToEdit)
{
	SelectedSimulation = false;

	SharedData = MakeShareable(new FPhysicsAssetEditorSharedData());

	SharedData->SelectionChangedEvent.AddRaw(this, &FPhysicsAssetEditor::HandleViewportSelectionChanged);
	SharedData->HierarchyChangedEvent.AddRaw(this, &FPhysicsAssetEditor::RefreshHierachyTree);
	SharedData->PreviewChangedEvent.AddRaw(this, &FPhysicsAssetEditor::RefreshPreviewViewport);
	SharedData->PhysicsAsset = ObjectToEdit;

	SharedData->CachePreviewMesh();

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");

	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(this, &FPhysicsAssetEditor::HandlePreviewSceneCreated);

	PersonaToolkit = PersonaModule.CreatePersonaToolkit(SharedData->PhysicsAsset, PersonaToolkitArgs);

	TSharedRef<IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(ObjectToEdit);
	AssetFamily->RecordAssetOpened(FAssetData(ObjectToEdit));

	FSkeletonTreeArgs SkeletonTreeArgs;
	SkeletonTreeArgs.OnSelectionChanged = FOnSkeletonTreeSelectionChanged::CreateSP(this, &FPhysicsAssetEditor::HandleSelectionChanged);
	SkeletonTreeArgs.PreviewScene = PersonaToolkit->GetPreviewScene();
	SkeletonTreeArgs.bShowBlendProfiles = false;
	SkeletonTreeArgs.bAllowMeshOperations = false;
	SkeletonTreeArgs.bAllowSkeletonOperations = false;
	SkeletonTreeArgs.OnGetFilterText = FOnGetFilterText::CreateSP(this, &FPhysicsAssetEditor::HandleGetFilterLabel);
	SkeletonTreeArgs.Extenders = MakeShared<FExtender>();
	SkeletonTreeArgs.Extenders->AddMenuExtension("FilterOptions", EExtensionHook::After, GetToolkitCommands(), FMenuExtensionDelegate::CreateSP(this, &FPhysicsAssetEditor::HandleExtendFilterMenu));
	SkeletonTreeArgs.Extenders->AddMenuExtension("SkeletonTreeContextMenu", EExtensionHook::After, GetToolkitCommands(), FMenuExtensionDelegate::CreateSP(this, &FPhysicsAssetEditor::HandleExtendContextMenu));
	SkeletonTreeArgs.Builder = SkeletonTreeBuilder = MakeShared<FPhysicsAssetEditorSkeletonTreeBuilder>(ObjectToEdit);

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::GetModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	SkeletonTree = SkeletonEditorModule.CreateSkeletonTree(PersonaToolkit->GetSkeleton(), SkeletonTreeArgs);

	bSelecting = false;

	GEditor->RegisterForUndo(this);

	// Register our commands. This will only register them if not previously registered
	FPhysicsAssetEditorCommands::Register();

	BindCommands();

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, PhysicsAssetEditorAppIdentifier, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit);

	AddApplicationMode(
		PhysicsAssetEditorModes::PhysicsAssetEditorMode,
		MakeShareable(new FPhysicsAssetEditorMode(SharedThis(this), SkeletonTree.ToSharedRef(), PersonaToolkit->GetPreviewScene())));

	SetCurrentMode(PhysicsAssetEditorModes::PhysicsAssetEditorMode);

	// Force disable simulation as InitArticulated can be called during viewport creation
	SharedData->ForceDisableSimulation();

	GetAssetEditorModeManager()->SetDefaultMode(FPhysicsAssetEditorEditMode::ModeName);
	GetAssetEditorModeManager()->ActivateMode(FPersonaEditModes::SkeletonSelection);
	GetAssetEditorModeManager()->ActivateMode(FPhysicsAssetEditorEditMode::ModeName);
	static_cast<FPhysicsAssetEditorEditMode*>(GetAssetEditorModeManager()->GetActiveMode(FPhysicsAssetEditorEditMode::ModeName))->SetSharedData(SharedThis(this), *SharedData.Get());

	IPhysicsAssetEditorModule* PhysicsAssetEditorModule = &FModuleManager::LoadModuleChecked<IPhysicsAssetEditorModule>( "PhysicsAssetEditor" );
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

TSharedPtr<FPhysicsAssetEditorSharedData> FPhysicsAssetEditor::GetSharedData() const
{
	return SharedData;
}

void FPhysicsAssetEditor::HandleViewportSelectionChanged(const TArray<FPhysicsAssetEditorSharedData::FSelection>& InSelectedBodies, const TArray<FPhysicsAssetEditorSharedData::FSelection>& InSelectedConstraints)
{
	if (!bSelecting)
	{
		TGuardValue<bool> RecursionGuard(bSelecting, true);

		if (SkeletonTree.IsValid())
		{
			SkeletonTree->DeselectAll();
		}

		if(InSelectedBodies.Num() == 0 && InSelectedConstraints.Num() == 0)
		{
			if (PhysAssetProperties.IsValid())
			{
				PhysAssetProperties->SetObject(nullptr);
			}

			if (PhysicsAssetGraph.IsValid())
			{
				PhysicsAssetGraph.Pin()->SelectObjects(TArray<USkeletalBodySetup*>(), TArray<UPhysicsConstraintTemplate*>());
			}
		}
		else
		{
			TArray<UObject*> Objects;
			TSet<USkeletalBodySetup*> Bodies;
			TSet<UPhysicsConstraintTemplate*> Constraints;
			Algo::Transform(InSelectedBodies, Objects, [this](const FPhysicsAssetEditorSharedData::FSelection& InItem) 
			{ 
				return SharedData->PhysicsAsset->SkeletalBodySetups[InItem.Index];
			});
			Algo::Transform(InSelectedConstraints, Objects, [this](const FPhysicsAssetEditorSharedData::FSelection& InItem) 
			{ 
				return SharedData->PhysicsAsset->ConstraintSetup[InItem.Index];
			});
			Algo::Transform(InSelectedBodies, Bodies, [this](const FPhysicsAssetEditorSharedData::FSelection& InItem) 
			{ 
				return SharedData->PhysicsAsset->SkeletalBodySetups[InItem.Index];
			});
			Algo::Transform(InSelectedConstraints, Constraints, [this](const FPhysicsAssetEditorSharedData::FSelection& InItem) 
			{ 
				return SharedData->PhysicsAsset->ConstraintSetup[InItem.Index];
			});

			if (PhysAssetProperties.IsValid())
			{
				PhysAssetProperties->SetObjects(Objects);
			}

			if (SkeletonTree.IsValid())
			{
				SkeletonTree->SelectItemsBy([this, &InSelectedBodies, &Constraints](const TSharedRef<ISkeletonTreeItem>& InItem, bool& bInOutExpand)
				{
					if(InItem->IsOfType<FSkeletonTreePhysicsBodyItem>() || InItem->IsOfType<FSkeletonTreePhysicsShapeItem>())
					{
						for (const FPhysicsAssetEditorSharedData::FSelection& SelectedBody : InSelectedBodies)
						{
							USkeletalBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SelectedBody.Index];
							int32 GeomCount = BodySetup->AggGeom.SphereElems.Num() + BodySetup->AggGeom.SphylElems.Num() + BodySetup->AggGeom.BoxElems.Num() + BodySetup->AggGeom.ConvexElems.Num();
							if (BodySetup == InItem->GetObject())
							{
								if(InItem->IsOfType<FSkeletonTreePhysicsShapeItem>())
								{
									TSharedRef<FSkeletonTreePhysicsShapeItem> SkeletonTreePhysicsShapeItem = StaticCastSharedRef<FSkeletonTreePhysicsShapeItem>(InItem);
									if(SkeletonTreePhysicsShapeItem->GetShapeIndex() == SelectedBody.PrimitiveIndex && SkeletonTreePhysicsShapeItem->GetShapeType() == SelectedBody.PrimitiveType && GeomCount > 1)
									{
										bInOutExpand = true;
										return true;
									}
								}
								else if(GeomCount <= 1)
								{
									bInOutExpand = true;
									return true;
								}
							}
						}
					}
					else if(InItem->IsOfType<FSkeletonTreePhysicsConstraintItem>())
					{
						for (UPhysicsConstraintTemplate* Constraint : Constraints)
						{
							if (Constraint == InItem->GetObject())
							{
								bInOutExpand = true;
								return true;
							}
						}
					}

					return false;
				});
			}

			if (PhysicsAssetGraph.IsValid())
			{
				PhysicsAssetGraph.Pin()->SelectObjects(Bodies.Array(), Constraints.Array());
			}
		}
	}
}

void FPhysicsAssetEditor::RefreshHierachyTree()
{
	if(SkeletonTree.IsValid())
	{
		SkeletonTree->Refresh();
	}
}

void FPhysicsAssetEditor::RefreshPreviewViewport()
{
	if (PersonaToolkit.IsValid())
	{
		PersonaToolkit->GetPreviewScene()->InvalidateViews();
	}
}

FName FPhysicsAssetEditor::GetToolkitFName() const
{
	return FName("PhysicsAssetEditor");
}

FText FPhysicsAssetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Physics Asset Editor");
}

FString FPhysicsAssetEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT( "WorldCentricTabPrefix", "Physics Asset Editor ").ToString();
}

FLinearColor FPhysicsAssetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void PopulateLayoutMenu(FMenuBuilder& MenuBuilder, const TSharedRef<SDockTabStack>& DockTabStack)
{

}

void FPhysicsAssetEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	SharedData->AddReferencedObjects(Collector);
}

void FPhysicsAssetEditor::PostUndo(bool bSuccess)
{
	OnPostUndo.Broadcast();

	SharedData->PostUndo();
	RefreshHierachyTree();

	SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
}


void FPhysicsAssetEditor::PostRedo( bool bSuccess )
{
	OnPostUndo.Broadcast();

	for (int32 BodyIdx=0; BodyIdx < SharedData->PhysicsAsset->SkeletalBodySetups.Num(); ++BodyIdx)
	{
		UBodySetup* Body = SharedData->PhysicsAsset->SkeletalBodySetups[BodyIdx];
		
		bool bRecreate = false;
		for (int32 ElemIdx=0; ElemIdx < Body->AggGeom.ConvexElems.Num(); ++ElemIdx)
		{
			FKConvexElem& Element = Body->AggGeom.ConvexElems[ElemIdx];

			if (Element.GetConvexMesh() == NULL)
			{
				bRecreate = true;
				break;
			}
		}

		if (bRecreate)
		{
			Body->InvalidatePhysicsData();
			Body->CreatePhysicsMeshes();
		}

	}

	PostUndo(bSuccess);
}

void FPhysicsAssetEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// Update bounds bodies and setup when bConsiderForBounds was changed
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBodySetup, bConsiderForBounds))
	{
		SharedData->PhysicsAsset->UpdateBoundsBodiesArray();
		SharedData->PhysicsAsset->UpdateBodySetupIndexMap();
	}

	RecreatePhysicsState();

	RefreshPreviewViewport();
}

FText FPhysicsAssetEditor::GetRepeatLastSimulationToolTip() const
{
	if(SelectedSimulation)
	{
		return FPhysicsAssetEditorCommands::Get().SelectedSimulation->GetDescription();
	}
	else
	{
		if(SharedData->bNoGravitySimulation)
		{
			return FPhysicsAssetEditorCommands::Get().SimulationNoGravity->GetDescription();
		}
		else
		{
			return FPhysicsAssetEditorCommands::Get().SimulationAll->GetDescription();
		}
	}
}

FSlateIcon FPhysicsAssetEditor::GetRepeatLastSimulationIcon() const
{
	if(SelectedSimulation)
	{
		return FPhysicsAssetEditorCommands::Get().SelectedSimulation->GetIcon();
	}
	else
	{
		if(SharedData->bNoGravitySimulation)
		{
			return FPhysicsAssetEditorCommands::Get().SimulationNoGravity->GetIcon();
		}
		else
		{
			return FPhysicsAssetEditorCommands::Get().SimulationAll->GetIcon();
		}
	}
}

void FPhysicsAssetEditor::ExtendToolbar()
{
	struct Local
	{
		static TSharedRef< SWidget > FillSimulateOptions(TSharedRef<FUICommandList> InCommandList)
		{
			const bool bShouldCloseWindowAfterMenuSelection = true;
			FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, InCommandList );

			const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

			//selected simulation
			MenuBuilder.BeginSection("Simulation", LOCTEXT("SimulationHeader", "Simulation"));
			{
				MenuBuilder.AddMenuEntry(Commands.SimulationAll);
				MenuBuilder.AddMenuEntry(Commands.SelectedSimulation);
			}
			MenuBuilder.EndSection();
			MenuBuilder.BeginSection("SimulationOptions", LOCTEXT("SimulationOptionsHeader", "Simulation Options"));
			{
				MenuBuilder.AddMenuEntry(Commands.SimulationNoGravity);
			}
			MenuBuilder.EndSection();

			return MenuBuilder.MakeWidget();
		}

		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, TSharedPtr<FPhysicsAssetEditorSharedData> SharedData, FPhysicsAssetEditor * PhysicsAssetEditor )
		{
			const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();
			TSharedRef<FUICommandList> InCommandList = PhysicsAssetEditor->GetToolkitCommands();

			FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
			PersonaModule.AddCommonToolbarExtensions(ToolbarBuilder, PhysicsAssetEditor->PersonaToolkit.ToSharedRef());

			ToolbarBuilder.BeginSection("PhysicsAssetEditorBodyTools");
			{
				ToolbarBuilder.AddToolBarButton(Commands.EnableCollision);
				ToolbarBuilder.AddToolBarButton(Commands.DisableCollision);

				ToolbarBuilder.AddComboButton(
					FUIAction(FExecuteAction(), FCanExecuteAction::CreateSP(PhysicsAssetEditor, &FPhysicsAssetEditor::IsNotSimulation)),
					FOnGetContent::CreateLambda([PhysicsAssetEditor]()
					{
						return PhysicsAssetEditor->BuildPhysicalMaterialAssetPicker(true);
					}),
					Commands.ApplyPhysicalMaterial->GetLabel(), 
					Commands.ApplyPhysicalMaterial->GetDescription(),
					Commands.ApplyPhysicalMaterial->GetIcon()
				);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("PhysicsAssetEditorConstraintTools");
			{
				ToolbarBuilder.AddToolBarButton(Commands.ConvertToBallAndSocket);
				ToolbarBuilder.AddToolBarButton(Commands.ConvertToHinge);
				ToolbarBuilder.AddToolBarButton(Commands.ConvertToPrismatic);
				ToolbarBuilder.AddToolBarButton(Commands.ConvertToSkeletal);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("PhysicsAssetEditorSimulation");
			{
				// Simulate
				ToolbarBuilder.AddToolBarButton( 
					Commands.RepeatLastSimulation, 
					NAME_None, 
					LOCTEXT("RepeatLastSimulation","Simulate"),
					TAttribute< FText >::Create( TAttribute< FText >::FGetter::CreateSP( PhysicsAssetEditor, &FPhysicsAssetEditor::GetRepeatLastSimulationToolTip) ),
					TAttribute< FSlateIcon >::Create( TAttribute< FSlateIcon >::FGetter::CreateSP( PhysicsAssetEditor, &FPhysicsAssetEditor::GetRepeatLastSimulationIcon) )
					);

				//simulate mode combo
				FUIAction SimulationMode;
				SimulationMode.CanExecuteAction = FCanExecuteAction::CreateSP(PhysicsAssetEditor, &FPhysicsAssetEditor::IsNotSimulation);
				{
					ToolbarBuilder.AddComboButton(
						SimulationMode,
						FOnGetContent::CreateStatic( &FillSimulateOptions, InCommandList ),
						LOCTEXT( "SimulateCombo_Label", "Simulate Options" ),
						LOCTEXT( "SimulateComboToolTip", "Options for Simulation" ),
						FSlateIcon(),
						true
						);
				}
			}
			ToolbarBuilder.EndSection();
		}
	};

	// If the ToolbarExtender is valid, remove it before rebuilding it
	if ( ToolbarExtender.IsValid() )
	{
		RemoveToolbarExtender( ToolbarExtender );
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);
	
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar, SharedData, this)
		);

	AddToolbarExtender(ToolbarExtender);

	IPhysicsAssetEditorModule* PhysicsAssetEditorModule = &FModuleManager::LoadModuleChecked<IPhysicsAssetEditorModule>( "PhysicsAssetEditor" );
	AddToolbarExtender(PhysicsAssetEditorModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ParentToolbarBuilder)
	{
		FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
		TSharedRef<class IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(SharedData->PhysicsAsset);
		AddToolbarWidget(PersonaModule.CreateAssetFamilyShortcutWidget(SharedThis(this), AssetFamily));
	}
	));
}

void FPhysicsAssetEditor::ExtendMenu()
{
	struct Local
	{
		static void FillEdit( FMenuBuilder& MenuBarBuilder )
		{
			const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();
			MenuBarBuilder.BeginSection("Selection", LOCTEXT("PhatEditSelection", "Selection"));
			MenuBarBuilder.AddMenuEntry(Commands.SelectAllBodies);
			MenuBarBuilder.AddMenuEntry(Commands.SelectAllConstraints);
			MenuBarBuilder.AddMenuEntry(Commands.DeselectAll);
			MenuBarBuilder.EndSection();
		}
	};
	MenuExtender = MakeShareable(new FExtender);
	MenuExtender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		GetToolkitCommands(), 
		FMenuExtensionDelegate::CreateStatic( &Local::FillEdit ) );

	AddMenuExtender(MenuExtender);

	IPhysicsAssetEditorModule* PhysicsAssetEditorModule = &FModuleManager::LoadModuleChecked<IPhysicsAssetEditorModule>( "PhysicsAssetEditor" );
	AddMenuExtender(PhysicsAssetEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FPhysicsAssetEditor::BindCommands()
{
	const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.RegenerateBodies,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::ResetBoneCollision),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.AddBodies,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::ResetBoneCollision),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.CopyProperties,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCopyProperties),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanCopyProperties),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsCopyProperties));

	ToolkitCommands->MapAction(
		Commands.PasteProperties,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnPasteProperties),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanPasteProperties));

	ToolkitCommands->MapAction(
		Commands.RepeatLastSimulation,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnRepeatLastSimulation),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsToggleSimulation));

	ToolkitCommands->MapAction(
		Commands.SimulationNoGravity,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnToggleSimulationNoGravity),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsNoGravitySimulationEnabled));

	ToolkitCommands->MapAction(
		Commands.SelectedSimulation,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnToggleSimulation, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsSelectedSimulation));

	ToolkitCommands->MapAction(
		Commands.SimulationAll,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnToggleSimulation, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsFullSimulation));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_Solid,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnMeshRenderingMode, EPhysicsAssetEditorRenderMode::Solid, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsMeshRenderingMode, EPhysicsAssetEditorRenderMode::Solid, false));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_Wireframe,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnMeshRenderingMode, EPhysicsAssetEditorRenderMode::Wireframe, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsMeshRenderingMode, EPhysicsAssetEditorRenderMode::Wireframe, false));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_None,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnMeshRenderingMode, EPhysicsAssetEditorRenderMode::None, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsMeshRenderingMode, EPhysicsAssetEditorRenderMode::None, false));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_Solid,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCollisionRenderingMode, EPhysicsAssetEditorRenderMode::Solid, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsCollisionRenderingMode, EPhysicsAssetEditorRenderMode::Solid, false));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_Wireframe,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCollisionRenderingMode, EPhysicsAssetEditorRenderMode::Wireframe, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsCollisionRenderingMode, EPhysicsAssetEditorRenderMode::Wireframe, false));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_None,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCollisionRenderingMode, EPhysicsAssetEditorRenderMode::None, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsCollisionRenderingMode, EPhysicsAssetEditorRenderMode::None, false));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_None,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::None, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::None, false));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_AllPositions,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::AllPositions, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::AllPositions, false));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_AllLimits,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::AllLimits, false),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::AllLimits, false));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_Simulation_Solid,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnMeshRenderingMode, EPhysicsAssetEditorRenderMode::Solid, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsMeshRenderingMode, EPhysicsAssetEditorRenderMode::Solid, true));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_Simulation_Wireframe,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnMeshRenderingMode, EPhysicsAssetEditorRenderMode::Wireframe, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsMeshRenderingMode, EPhysicsAssetEditorRenderMode::Wireframe, true));

	ToolkitCommands->MapAction(
		Commands.MeshRenderingMode_Simulation_None,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnMeshRenderingMode, EPhysicsAssetEditorRenderMode::None, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsMeshRenderingMode, EPhysicsAssetEditorRenderMode::None, true));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_Simulation_Solid,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCollisionRenderingMode, EPhysicsAssetEditorRenderMode::Solid, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsCollisionRenderingMode, EPhysicsAssetEditorRenderMode::Solid, true));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_Simulation_Wireframe,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCollisionRenderingMode, EPhysicsAssetEditorRenderMode::Wireframe, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsCollisionRenderingMode, EPhysicsAssetEditorRenderMode::Wireframe, true));

	ToolkitCommands->MapAction(
		Commands.CollisionRenderingMode_Simulation_None,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCollisionRenderingMode, EPhysicsAssetEditorRenderMode::None, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsCollisionRenderingMode, EPhysicsAssetEditorRenderMode::None, true));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_Simulation_None,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::None, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::None, true));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_Simulation_AllPositions,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::AllPositions, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::AllPositions, true));

	ToolkitCommands->MapAction(
		Commands.ConstraintRenderingMode_Simulation_AllLimits,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::AllLimits, true),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsConstraintRenderingMode, EPhysicsAssetEditorConstraintViewMode::AllLimits, true));

	ToolkitCommands->MapAction(
		Commands.RenderOnlySelectedSolid,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::ToggleRenderOnlySelectedSolid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsRenderingOnlySelectedSolid));

	ToolkitCommands->MapAction(
		Commands.DrawConstraintsAsPoints,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::ToggleDrawConstraintsAsPoints),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsDrawingConstraintsAsPoints));

	ToolkitCommands->MapAction(
		Commands.ToggleMassProperties,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnToggleMassProperties),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsToggleMassProperties));

	ToolkitCommands->MapAction(
		Commands.DisableCollision,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSetCollision, false),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanSetCollision, false));

	ToolkitCommands->MapAction(
		Commands.DisableCollisionAll,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSetCollisionAll, false),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanSetCollisionAll, false));

	ToolkitCommands->MapAction(
		Commands.EnableCollision,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSetCollision, true),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanSetCollision, true));

	ToolkitCommands->MapAction(
		Commands.EnableCollisionAll,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSetCollisionAll, true),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanSetCollisionAll, true));

	ToolkitCommands->MapAction(
		Commands.WeldToBody,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnWeldToBody),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanWeldToBody));

	ToolkitCommands->MapAction(
		Commands.AddSphere,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnAddSphere),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanAddPrimitive));

	ToolkitCommands->MapAction(
		Commands.AddSphyl,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnAddSphyl),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanAddPrimitive));

	ToolkitCommands->MapAction(
		Commands.AddBox,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnAddBox),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanAddPrimitive));

	ToolkitCommands->MapAction(
		Commands.DeletePrimitive,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnDeletePrimitive),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::HasSelectedBodyAndIsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.DuplicatePrimitive,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnDuplicatePrimitive),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanDuplicatePrimitive));

	ToolkitCommands->MapAction(
		Commands.ResetConstraint,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnResetConstraint),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::HasSelectedConstraintAndIsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.SnapConstraint,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSnapConstraint),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::HasSelectedConstraintAndIsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.ConvertToBallAndSocket,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConvertToBallAndSocket),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanEditConstraintProperties));

	ToolkitCommands->MapAction(
		Commands.ConvertToHinge,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConvertToHinge),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanEditConstraintProperties));

	ToolkitCommands->MapAction(
		Commands.ConvertToPrismatic,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConvertToPrismatic),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanEditConstraintProperties));

	ToolkitCommands->MapAction(
		Commands.ConvertToSkeletal,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnConvertToSkeletal),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::CanEditConstraintProperties));

	ToolkitCommands->MapAction(
		Commands.DeleteConstraint,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnDeleteConstraint),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::HasSelectedConstraintAndIsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.MakeBodyKinematic,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSetBodyPhysicsType, EPhysicsType::PhysType_Kinematic ),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsBodyPhysicsType, EPhysicsType::PhysType_Kinematic ) );

	ToolkitCommands->MapAction(
		Commands.MakeBodySimulated,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSetBodyPhysicsType, EPhysicsType::PhysType_Simulated ),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsBodyPhysicsType, EPhysicsType::PhysType_Simulated ) );

	ToolkitCommands->MapAction(
		Commands.MakeBodyDefault,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSetBodyPhysicsType, EPhysicsType::PhysType_Default ),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsBodyPhysicsType, EPhysicsType::PhysType_Default ) );

	ToolkitCommands->MapAction(
		Commands.KinematicAllBodiesBelow,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::SetBodiesBelowSelectedPhysicsType, EPhysicsType::PhysType_Kinematic, true),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.SimulatedAllBodiesBelow,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::SetBodiesBelowSelectedPhysicsType, EPhysicsType::PhysType_Simulated, true),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.MakeAllBodiesBelowDefault,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::SetBodiesBelowSelectedPhysicsType, EPhysicsType::PhysType_Default, true), 
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.DeleteBody,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnDeleteBody),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.DeleteAllBodiesBelow,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnDeleteAllBodiesBelow),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.DeleteSelected,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnDeleteSelection),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.CycleConstraintOrientation,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCycleConstraintOrientation),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.CycleConstraintActive,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnCycleConstraintActive),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.ToggleSwing1,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnToggleSwing1),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsSwing1Locked));

	ToolkitCommands->MapAction(
		Commands.ToggleSwing2,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnToggleSwing2),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsSwing2Locked));

	ToolkitCommands->MapAction(
		Commands.ToggleTwist,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnToggleTwist),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation),
		FIsActionChecked::CreateSP(this, &FPhysicsAssetEditor::IsTwistLocked));

	ToolkitCommands->MapAction(
		Commands.SelectAllBodies,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSelectAllBodies),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.SelectAllConstraints,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnSelectAllConstraints),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.DeselectAll,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::OnDeselectAll),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation));

	ToolkitCommands->MapAction(
		Commands.Mirror,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::Mirror),
		FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation)
		);

	ToolkitCommands->MapAction(
		Commands.ShowBodies,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::HandleToggleShowBodies),
		FCanExecuteAction(),
		FGetActionCheckState::CreateSP(this, &FPhysicsAssetEditor::GetShowBodiesChecked)
	);

	ToolkitCommands->MapAction(
		Commands.ShowConstraints,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::HandleToggleShowConstraints),
		FCanExecuteAction(),
		FGetActionCheckState::CreateSP(this, &FPhysicsAssetEditor::GetShowConstraintsChecked)
	);

	ToolkitCommands->MapAction(
		Commands.ShowPrimitives,
		FExecuteAction::CreateSP(this, &FPhysicsAssetEditor::HandleToggleShowPrimitives),
		FCanExecuteAction(),
		FGetActionCheckState::CreateSP(this, &FPhysicsAssetEditor::GetShowPrimitivesChecked)
	);
}

void FPhysicsAssetEditor::Mirror()
{
	SharedData->Mirror();

	RecreatePhysicsState();
	RefreshHierachyTree();
	RefreshPreviewViewport();
}

void FPhysicsAssetEditor::BuildMenuWidgetBody(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.PushCommandList(GetToolkitCommands());
	{
		const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

		struct FLocal
		{
			static void FillPhysicsTypeMenu(FMenuBuilder& InSubMenuBuilder)
			{
				const FPhysicsAssetEditorCommands& PhysicsAssetEditorCommands = FPhysicsAssetEditorCommands::Get();

				InSubMenuBuilder.BeginSection("BodyPhysicsTypeActions", LOCTEXT("BodyPhysicsTypeHeader", "Body Physics Type"));
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.MakeBodyKinematic);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.MakeBodySimulated);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.MakeBodyDefault);
				InSubMenuBuilder.EndSection();

				InSubMenuBuilder.BeginSection("BodiesBelowPhysicsTypeActions", LOCTEXT("BodiesBelowPhysicsTypeHeader", "Bodies Below Physics Type"));
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.KinematicAllBodiesBelow);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.SimulatedAllBodiesBelow);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.MakeAllBodiesBelowDefault);
				InSubMenuBuilder.EndSection();
			}

			static void FillAddShapeMenu(FMenuBuilder& InSubMenuBuilder)
			{
				const FPhysicsAssetEditorCommands& PhysicsAssetEditorCommands = FPhysicsAssetEditorCommands::Get();

				InSubMenuBuilder.BeginSection("ShapeTypeHeader", LOCTEXT("ShapeTypeHeader", "Shape Type"));
				InSubMenuBuilder.AddMenuEntry( PhysicsAssetEditorCommands.AddBox );
				InSubMenuBuilder.AddMenuEntry( PhysicsAssetEditorCommands.AddSphere );
				InSubMenuBuilder.AddMenuEntry( PhysicsAssetEditorCommands.AddSphyl );
				InSubMenuBuilder.EndSection();
			}

			static void FillCollisionMenu(FMenuBuilder& InSubMenuBuilder)
			{
				const FPhysicsAssetEditorCommands& PhysicsAssetEditorCommands = FPhysicsAssetEditorCommands::Get();

				InSubMenuBuilder.BeginSection("CollisionHeader", LOCTEXT("CollisionHeader", "Collision"));
				InSubMenuBuilder.AddMenuEntry( PhysicsAssetEditorCommands.WeldToBody );
				InSubMenuBuilder.AddMenuEntry( PhysicsAssetEditorCommands.EnableCollision );
				InSubMenuBuilder.AddMenuEntry( PhysicsAssetEditorCommands.EnableCollisionAll );
				InSubMenuBuilder.AddMenuEntry( PhysicsAssetEditorCommands.DisableCollision );
				InSubMenuBuilder.AddMenuEntry( PhysicsAssetEditorCommands.DisableCollisionAll );
				InSubMenuBuilder.EndSection();
			}
		};

		InMenuBuilder.BeginSection( "BodyActions", LOCTEXT( "BodyHeader", "Body" ) );
		InMenuBuilder.AddMenuEntry( Commands.RegenerateBodies );
		InMenuBuilder.AddSubMenu( LOCTEXT("AddShapeMenu", "Add Shape"), LOCTEXT("AddShapeMenu_ToolTip", "Add shapes to this body"),
			FNewMenuDelegate::CreateStatic( &FLocal::FillAddShapeMenu ) );
		InMenuBuilder.AddSubMenu( LOCTEXT("CollisionMenu", "Collision"), LOCTEXT("CollisionMenu_ToolTip", "Adjust body/body collision"),
			FNewMenuDelegate::CreateStatic( &FLocal::FillCollisionMenu ) );	

		InMenuBuilder.AddSubMenu( LOCTEXT("ConstraintMenu", "Constraints"), LOCTEXT("ConstraintMenu_ToolTip", "Constraint Operations"),
			FNewMenuDelegate::CreateSP( this, &FPhysicsAssetEditor::BuildMenuWidgetNewConstraint ) );	

		InMenuBuilder.AddSubMenu( LOCTEXT("BodyPhysicsTypeMenu", "Physics Type"), LOCTEXT("BodyPhysicsTypeMenu_ToolTip", "Physics Type"),
			FNewMenuDelegate::CreateStatic( &FLocal::FillPhysicsTypeMenu ) );	

		InMenuBuilder.AddSubMenu(
			Commands.ApplyPhysicalMaterial->GetLabel(), 
			LOCTEXT("ApplyPhysicalMaterialSelected", "Apply a physical material to the selected bodies"), 
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InSubMenuBuilder)
			{
				InSubMenuBuilder.AddWidget(BuildPhysicalMaterialAssetPicker(false), FText(), true);
			}),
			FUIAction(FExecuteAction(), FCanExecuteAction::CreateSP(this, &FPhysicsAssetEditor::IsNotSimulation)),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		InMenuBuilder.AddMenuEntry( Commands.CopyProperties );
		InMenuBuilder.AddMenuEntry( Commands.PasteProperties );
		InMenuBuilder.AddMenuEntry( Commands.DeleteBody );
		InMenuBuilder.AddMenuEntry( Commands.DeleteAllBodiesBelow );
		InMenuBuilder.AddMenuEntry( Commands.Mirror );
		InMenuBuilder.EndSection();

		InMenuBuilder.BeginSection( "PhysicalAnimationProfile", LOCTEXT( "PhysicalAnimationProfileHeader", "Physical Animation Profile" ) );
		InMenuBuilder.AddMenuEntry( Commands.AddBodyToPhysicalAnimationProfile );
		InMenuBuilder.AddMenuEntry( Commands.RemoveBodyFromPhysicalAnimationProfile );
		InMenuBuilder.EndSection();

		InMenuBuilder.BeginSection( "Advanced", LOCTEXT( "AdvancedHeading", "Advanced" ) );
		InMenuBuilder.AddSubMenu(
			LOCTEXT("AddCollisionfromStaticMesh", "Copy Collision From StaticMesh"), 
			LOCTEXT("AddCollisionfromStaticMesh_Tooltip", "Copy convex collision from a specified static mesh"), 
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InSubMenuBuilder)
			{
				InSubMenuBuilder.AddWidget(BuildStaticMeshAssetPicker(), FText(), true);
			})
		);
		InMenuBuilder.EndSection();
	}
	InMenuBuilder.PopCommandList();
}


void FPhysicsAssetEditor::BuildMenuWidgetPrimitives(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.PushCommandList(GetToolkitCommands());
	{
		const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

		InMenuBuilder.BeginSection("PrimitiveActions", LOCTEXT("PrimitivesHeader", "Primitives"));
		InMenuBuilder.AddMenuEntry(Commands.DuplicatePrimitive);
		InMenuBuilder.AddMenuEntry(Commands.DeletePrimitive);
		InMenuBuilder.EndSection();
	}
	InMenuBuilder.PopCommandList();
}

void FPhysicsAssetEditor::BuildMenuWidgetConstraint(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.PushCommandList(GetToolkitCommands());
	{
		const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

		struct FLocal
		{
			static void FillAxesAndLimitsMenu(FMenuBuilder& InSubMenuBuilder)
			{
				const FPhysicsAssetEditorCommands& PhysicsAssetEditorCommands = FPhysicsAssetEditorCommands::Get();

				InSubMenuBuilder.BeginSection("AxesAndLimitsHeader", LOCTEXT("AxesAndLimitsHeader", "Axes and Limits"));
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.CycleConstraintOrientation);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.CycleConstraintActive);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.ToggleSwing1);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.ToggleSwing2);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.ToggleTwist);
				InSubMenuBuilder.EndSection();
			}

			static void FillConvertMenu(FMenuBuilder& InSubMenuBuilder)
			{
				const FPhysicsAssetEditorCommands& PhysicsAssetEditorCommands = FPhysicsAssetEditorCommands::Get();

				InSubMenuBuilder.BeginSection("ConvertHeader", LOCTEXT("ConvertHeader", "Convert"));
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.ConvertToBallAndSocket);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.ConvertToHinge);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.ConvertToPrismatic);
				InSubMenuBuilder.AddMenuEntry(PhysicsAssetEditorCommands.ConvertToSkeletal);
				InSubMenuBuilder.EndSection();
			}
		};

		InMenuBuilder.BeginSection("EditTypeActions", LOCTEXT("ConstraintEditTypeHeader", "Edit"));

		InMenuBuilder.AddMenuEntry(Commands.SnapConstraint);
		InMenuBuilder.AddMenuEntry(Commands.ResetConstraint);
		
		InMenuBuilder.AddSubMenu( LOCTEXT("AxesAndLimitsMenu", "Axes and Limits"), LOCTEXT("AxesAndLimitsMenu_ToolTip", "Edit axes and limits of this constraint"),
			FNewMenuDelegate::CreateStatic( &FLocal::FillAxesAndLimitsMenu ) );	
		InMenuBuilder.AddSubMenu( LOCTEXT("ConvertMenu", "Convert"), LOCTEXT("ConvertMenu_ToolTip", "Convert constraint to various presets"),
			FNewMenuDelegate::CreateStatic( &FLocal::FillConvertMenu ) );
		InMenuBuilder.AddMenuEntry(Commands.CopyProperties);
		InMenuBuilder.AddMenuEntry(Commands.PasteProperties);
		InMenuBuilder.AddMenuEntry(Commands.DeleteConstraint);
		InMenuBuilder.EndSection();

		InMenuBuilder.BeginSection("ConstraintProfile", LOCTEXT( "ConstraintProfileHeader", "Constraint Profile"));
		InMenuBuilder.AddMenuEntry(Commands.AddConstraintToCurrentConstraintProfile);
		InMenuBuilder.AddMenuEntry(Commands.RemoveConstraintFromCurrentConstraintProfile);
		InMenuBuilder.EndSection();
	}
	InMenuBuilder.PopCommandList();
}

void FPhysicsAssetEditor::BuildMenuWidgetSelection(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.PushCommandList(GetToolkitCommands());
	{
		const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

		InMenuBuilder.BeginSection( "Selection", LOCTEXT("Selection", "Selection" ) );
		InMenuBuilder.AddMenuEntry( Commands.SelectAllBodies );
		InMenuBuilder.AddMenuEntry( Commands.SelectAllConstraints );
		InMenuBuilder.EndSection();
	}
	InMenuBuilder.PopCommandList();
}

void FPhysicsAssetEditor::BuildMenuWidgetNewConstraint(FMenuBuilder& InMenuBuilder)
{
	BuildMenuWidgetNewConstraintForBody(InMenuBuilder, INDEX_NONE);
}

void FPhysicsAssetEditor::BuildMenuWidgetNewConstraintForBody(FMenuBuilder& InMenuBuilder, int32 InSourceBodyIndex)
{
	FSkeletonTreeBuilderArgs SkeletonTreeBuilderArgs(false, false, false, false);

	TSharedRef<FPhysicsAssetEditorSkeletonTreeBuilder> Builder = MakeShared<FPhysicsAssetEditorSkeletonTreeBuilder>(SharedData->PhysicsAsset, SkeletonTreeBuilderArgs);
	Builder->bShowBodies = true;
	Builder->bShowConstraints = false;
	Builder->bShowPrimitives = false;

	FSkeletonTreeArgs SkeletonTreeArgs;
	SkeletonTreeArgs.Mode = ESkeletonTreeMode::Picker;
	SkeletonTreeArgs.bAllowMeshOperations = false;
	SkeletonTreeArgs.bAllowSkeletonOperations = false;
	SkeletonTreeArgs.bShowBlendProfiles = false;
	SkeletonTreeArgs.bShowFilterMenu = false;
	SkeletonTreeArgs.Builder = Builder;
	SkeletonTreeArgs.PreviewScene = GetPersonaToolkit()->GetPreviewScene();
	SkeletonTreeArgs.OnSelectionChanged = FOnSkeletonTreeSelectionChanged::CreateLambda([this, InSourceBodyIndex](const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type SelectInfo)
	{
		if(InSelectedItems.Num() > 0)
		{
			TSharedPtr<ISkeletonTreeItem> SelectedItem = InSelectedItems[0];
			check(SelectedItem->IsOfType<FSkeletonTreePhysicsBodyItem>());
			TSharedPtr<FSkeletonTreePhysicsBodyItem> SelectedBody = StaticCastSharedPtr<FSkeletonTreePhysicsBodyItem>(SelectedItem);

			if(InSourceBodyIndex != INDEX_NONE)
			{
				HandleCreateNewConstraint(InSourceBodyIndex, SelectedBody->GetBodySetupIndex());
			}
			else if(SharedData->GetSelectedBody() != nullptr)
			{
				for(const FPhysicsAssetEditorSharedData::FSelection& SourceBody : SharedData->SelectedBodies)
				{
					HandleCreateNewConstraint(SourceBody.Index, SelectedBody->GetBodySetupIndex());
				}
			}
		}

		FSlateApplication::Get().DismissAllMenus();
	});

	InMenuBuilder.BeginSection(TEXT("CreateNewConstraint"), LOCTEXT("CreateNewConstraint", "Create New Constraint With..."));
	{
		ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::GetModuleChecked<ISkeletonEditorModule>("SkeletonEditor");

		InMenuBuilder.AddWidget(
			SNew(SBox)
			.IsEnabled(this, &FPhysicsAssetEditor::IsNotSimulation)
			.WidthOverride(300.0f)
			.HeightOverride(400.0f)
			[
				SkeletonEditorModule.CreateSkeletonTree(SkeletonTree->GetEditableSkeleton(), SkeletonTreeArgs)
			], 
			FText(), true, false);
	}
	InMenuBuilder.EndSection();
}

void FPhysicsAssetEditor::BuildMenuWidgetBone(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.PushCommandList(GetToolkitCommands());
	InMenuBuilder.BeginSection( "BodyActions", LOCTEXT( "BodyHeader", "Body" ) );
	{
		const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();
		InMenuBuilder.AddMenuEntry( Commands.AddBodies );
	}
	InMenuBuilder.EndSection();
	InMenuBuilder.PopCommandList();
}

bool FPhysicsAssetEditor::ShouldFilterAssetBasedOnSkeleton( const FAssetData& AssetData )
{
	// @TODO This is a duplicate of FPersona::ShouldFilterAssetBasedOnSkeleton(), but should go away once PhysicsAssetEditor is integrated with Persona
	const FString SkeletonName = AssetData.GetTagValueRef<FString>("Skeleton");

	if ( !SkeletonName.IsEmpty() )
	{
		USkeletalMesh* EditorSkelMesh = SharedData->PhysicsAsset->GetPreviewMesh();
		if(EditorSkelMesh != nullptr)
		{
			USkeleton* Skeleton = EditorSkelMesh->Skeleton;

			if ( Skeleton && (*SkeletonName) == FString::Printf(TEXT("%s'%s'"), *Skeleton->GetClass()->GetName(), *Skeleton->GetPathName()) )
			{
				return false;
			}
		}
	}

	return true;
}

void FPhysicsAssetEditor::CreateOrConvertConstraint(EPhysicsAssetEditorConstraintType ConstraintType)
{
	//we have to manually call PostEditChange to ensure profiles are updated correctly
	UProperty* DefaultInstanceProperty = FindField<UProperty>(UPhysicsConstraintTemplate::StaticClass(), GET_MEMBER_NAME_CHECKED(UPhysicsConstraintTemplate, DefaultInstance));

	const FScopedTransaction Transaction( LOCTEXT( "CreateConvertConstraint", "Create Or Convert Constraint" ) );

	for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
	{
		UPhysicsConstraintTemplate* ConstraintSetup = SharedData->PhysicsAsset->ConstraintSetup[SharedData->SelectedConstraints[i].Index];
		ConstraintSetup->PreEditChange(DefaultInstanceProperty);

		if(ConstraintType == EPCT_BSJoint)
		{
			ConstraintUtils::ConfigureAsBallAndSocket(ConstraintSetup->DefaultInstance);
		}
		else if(ConstraintType == EPCT_Hinge)
		{
			ConstraintUtils::ConfigureAsHinge(ConstraintSetup->DefaultInstance);
		}
		else if(ConstraintType == EPCT_Prismatic)
		{
			ConstraintUtils::ConfigureAsPrismatic(ConstraintSetup->DefaultInstance);
		}
		else if(ConstraintType == EPCT_SkelJoint)
		{
			ConstraintUtils::ConfigureAsSkelJoint(ConstraintSetup->DefaultInstance);
		}

		FPropertyChangedEvent PropertyChangedEvent(DefaultInstanceProperty);
		ConstraintSetup->PostEditChangeProperty(PropertyChangedEvent);
	}

	RecreatePhysicsState();
	RefreshHierachyTree();
	RefreshPreviewViewport();
}

void FPhysicsAssetEditor::AddNewPrimitive(EAggCollisionShape::Type InPrimitiveType, bool bCopySelected)
{
	check(!bCopySelected || SharedData->SelectedBodies.Num() == 1);	//we only support this for one selection
	int32 NewPrimIndex = 0;
	TArray<FPhysicsAssetEditorSharedData::FSelection> NewSelection;
	{
		// Make sure rendering is done - so we are not changing data being used by collision drawing.
		FlushRenderingCommands();

		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "AddNewPrimitive", "Add New Primitive") );

		//first we need to grab all the bodies we're modifying (removes duplicates from multiple primitives)
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			
			NewSelection.AddUnique(FPhysicsAssetEditorSharedData::FSelection(SharedData->SelectedBodies[i].Index, EAggCollisionShape::Unknown, 0));	//only care about body index for now, we'll later update the primitive index
		}

		for(int32 i=0; i<NewSelection.Num(); ++i)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[NewSelection[i].Index];
			EAggCollisionShape::Type PrimitiveType;
			if (bCopySelected)
			{
				PrimitiveType = SharedData->GetSelectedBody()->PrimitiveType;
			}
			else
			{
				PrimitiveType = InPrimitiveType;
			}


			BodySetup->Modify();

			if (PrimitiveType == EAggCollisionShape::Sphere)
			{
				NewPrimIndex = BodySetup->AggGeom.SphereElems.Add(FKSphereElem());
				NewSelection[i].PrimitiveType = EAggCollisionShape::Sphere;
				NewSelection[i].PrimitiveIndex = NewPrimIndex;
				FKSphereElem* SphereElem = &BodySetup->AggGeom.SphereElems[NewPrimIndex];

				if (!bCopySelected)
				{
					SphereElem->Center = FVector::ZeroVector;

					SphereElem->Radius = PhysicsAssetEditor::DefaultPrimSize;
				}
				else
				{
					SphereElem->Center = BodySetup->AggGeom.SphereElems[SharedData->GetSelectedBody()->PrimitiveIndex].Center;
					SphereElem->Center.X += PhysicsAssetEditor::DuplicateXOffset;

					SphereElem->Radius = BodySetup->AggGeom.SphereElems[SharedData->GetSelectedBody()->PrimitiveIndex].Radius;
				}
			}
			else if (PrimitiveType == EAggCollisionShape::Box)
			{
				NewPrimIndex = BodySetup->AggGeom.BoxElems.Add(FKBoxElem());
				NewSelection[i].PrimitiveType = EAggCollisionShape::Box;
				NewSelection[i].PrimitiveIndex = NewPrimIndex;
				FKBoxElem* BoxElem = &BodySetup->AggGeom.BoxElems[NewPrimIndex];

				if (!bCopySelected)
				{
					BoxElem->SetTransform( FTransform::Identity );

					BoxElem->X = 0.5f * PhysicsAssetEditor::DefaultPrimSize;
					BoxElem->Y = 0.5f * PhysicsAssetEditor::DefaultPrimSize;
					BoxElem->Z = 0.5f * PhysicsAssetEditor::DefaultPrimSize;
				}
				else
				{
					BoxElem->SetTransform( BodySetup->AggGeom.BoxElems[SharedData->GetSelectedBody()->PrimitiveIndex].GetTransform() );
					BoxElem->Center.X += PhysicsAssetEditor::DuplicateXOffset;

					BoxElem->X = BodySetup->AggGeom.BoxElems[SharedData->GetSelectedBody()->PrimitiveIndex].X;
					BoxElem->Y = BodySetup->AggGeom.BoxElems[SharedData->GetSelectedBody()->PrimitiveIndex].Y;
					BoxElem->Z = BodySetup->AggGeom.BoxElems[SharedData->GetSelectedBody()->PrimitiveIndex].Z;
				}
			}
			else if (PrimitiveType == EAggCollisionShape::Sphyl)
			{
				NewPrimIndex = BodySetup->AggGeom.SphylElems.Add(FKSphylElem());
				NewSelection[i].PrimitiveType = EAggCollisionShape::Sphyl;
				NewSelection[i].PrimitiveIndex = NewPrimIndex;
				FKSphylElem* SphylElem = &BodySetup->AggGeom.SphylElems[NewPrimIndex];

				if (!bCopySelected)
				{
					SphylElem->SetTransform( FTransform::Identity );

					SphylElem->Length = PhysicsAssetEditor::DefaultPrimSize;
					SphylElem->Radius = PhysicsAssetEditor::DefaultPrimSize;
				}
				else
				{
					SphylElem->SetTransform( BodySetup->AggGeom.SphylElems[SharedData->GetSelectedBody()->PrimitiveIndex].GetTransform() );
					SphylElem->Center.X += PhysicsAssetEditor::DuplicateXOffset;

					SphylElem->Length = BodySetup->AggGeom.SphylElems[SharedData->GetSelectedBody()->PrimitiveIndex].Length;
					SphylElem->Radius = BodySetup->AggGeom.SphylElems[SharedData->GetSelectedBody()->PrimitiveIndex].Radius;
				}
			}
			else if (PrimitiveType == EAggCollisionShape::Convex)
			{
				check(bCopySelected); //only support copying for Convex primitive, as there is no default vertex data

				NewPrimIndex = BodySetup->AggGeom.ConvexElems.Add(FKConvexElem());
				NewSelection[i].PrimitiveType = EAggCollisionShape::Convex;
				NewSelection[i].PrimitiveIndex = NewPrimIndex;
				FKConvexElem* ConvexElem = &BodySetup->AggGeom.ConvexElems[NewPrimIndex];

				ConvexElem->SetTransform(BodySetup->AggGeom.ConvexElems[SharedData->GetSelectedBody()->PrimitiveIndex].GetTransform());

				// Copy all of the vertices of the convex element
				for (FVector v : BodySetup->AggGeom.ConvexElems[SharedData->GetSelectedBody()->PrimitiveIndex].VertexData)
				{
					v.X += PhysicsAssetEditor::DuplicateXOffset;
					ConvexElem->VertexData.Add(v);
				}
				ConvexElem->UpdateElemBox();

				BodySetup->InvalidatePhysicsData();
				BodySetup->CreatePhysicsMeshes();
			}
			else
			{
				check(0);  //unrecognized primitive type
			}
		}
	} // ScopedTransaction

	//clear selection
	SharedData->ClearSelectedBody();
	for(int32 i=0; i<NewSelection.Num(); ++i)
	{
		SharedData->SetSelectedBody(NewSelection[i], true);
	}

	RecreatePhysicsState();
	RefreshHierachyTree();
	RefreshPreviewViewport();
}

void FPhysicsAssetEditor::SetBodiesBelowSelectedPhysicsType( EPhysicsType InPhysicsType, bool bMarkAsDirty)
{
	TArray<int32> Indices;
	for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
	{
		Indices.Add(SharedData->SelectedBodies[i].Index);
	}

	SetBodiesBelowPhysicsType(InPhysicsType, Indices, bMarkAsDirty);
}

void FPhysicsAssetEditor::SetBodiesBelowPhysicsType( EPhysicsType InPhysicsType, const TArray<int32> & Indices, bool bMarkAsDirty)
{
	USkeletalMesh* EditorSkelMesh = SharedData->PhysicsAsset->GetPreviewMesh();
	if(EditorSkelMesh != nullptr)
	{
		TArray<int32> BelowBodies;
	
		for(int32 i=0; i<Indices.Num(); ++i)
		{
			// Get the index of this body
			UBodySetup* BaseSetup = SharedData->PhysicsAsset->SkeletalBodySetups[Indices[i]];
			SharedData->PhysicsAsset->GetBodyIndicesBelow(BelowBodies, BaseSetup->BoneName, EditorSkelMesh);

			// Now reset our skeletal mesh, as we don't re-init the physics state when simulating
			bool bSimulate = InPhysicsType == PhysType_Simulated || (InPhysicsType == EPhysicsType::PhysType_Default && SharedData->EditorSkelComp->BodyInstance.bSimulatePhysics);
			SharedData->EditorSkelComp->SetAllBodiesBelowSimulatePhysics(BaseSetup->BoneName, bSimulate, true);
		}

		// Make sure that the body setups are also correctly setup (the above loop just does the instances)
		for (int32 i = 0; i < BelowBodies.Num(); ++i)
		{
			int32 BodyIndex = BelowBodies[i];
			UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[BodyIndex];
			if (bMarkAsDirty)
			{
				BodySetup->Modify();
			}

			BodySetup->PhysicsType = InPhysicsType;
		}
	}

	RecreatePhysicsState();
	RefreshHierachyTree();
}

bool FPhysicsAssetEditor::IsNotSimulation() const
{
	return !SharedData->bRunningSimulation;
}

bool FPhysicsAssetEditor::HasSelectedBodyAndIsNotSimulation() const
{
	return IsNotSimulation() && (SharedData->GetSelectedBody());
}

bool FPhysicsAssetEditor::CanEditConstraintProperties() const
{
	if(IsNotSimulation() && SharedData->PhysicsAsset && SharedData->GetSelectedConstraint())
	{
		//If we are currently editing a constraint profile, make sure all selected constraints belong to the profile
		if(SharedData->PhysicsAsset->CurrentConstraintProfileName != NAME_None)
		{
			for (const FPhysicsAssetEditorSharedData::FSelection& Selection : SharedData->SelectedConstraints)
			{
				UPhysicsConstraintTemplate* CS = SharedData->PhysicsAsset->ConstraintSetup[Selection.Index];
				if(!CS || !CS->ContainsConstraintProfile(SharedData->PhysicsAsset->CurrentConstraintProfileName))
				{
					//missing at least one constraint from profile so don't allow editing
					return false;
				}
			}
		}
		
		//no constraint profile so editing is fine
		return true;
	}

	return false;
}

bool FPhysicsAssetEditor::HasSelectedConstraintAndIsNotSimulation() const
{
	return IsNotSimulation() && (SharedData->GetSelectedConstraint());
}

bool FPhysicsAssetEditor::IsSelectedEditMode() const
{
	return HasSelectedBodyAndIsNotSimulation() || HasSelectedConstraintAndIsNotSimulation();
}

void FPhysicsAssetEditor::OnChangeDefaultMesh(USkeletalMesh* OldPreviewMesh, USkeletalMesh* NewPreviewMesh)
{
	if(NewPreviewMesh != nullptr)
	{
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		// Update various infos based on the mesh
		MeshUtilities.CalcBoneVertInfos(NewPreviewMesh, SharedData->DominantWeightBoneInfos, true);
		MeshUtilities.CalcBoneVertInfos(NewPreviewMesh, SharedData->AnyWeightBoneInfos, false);

		RefreshHierachyTree();
	}
}

void FPhysicsAssetEditor::ResetBoneCollision()
{
	USkeletalMesh* EditorSkelMesh = SharedData->PhysicsAsset->GetPreviewMesh();
	if(EditorSkelMesh == nullptr)
	{
		return;
	}

	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	const FPhysAssetCreateParams& NewBodyData = GetDefault<UPhysicsAssetGenerationSettings>()->CreateParams;

	if(SharedData->SelectedBodies.Num() > 0)
	{
		TArray<int32> BodyIndices;
		const FScopedTransaction Transaction( LOCTEXT("ResetBoneCollision", "Reset Bone Collision") );

		FScopedSlowTask SlowTask((float)SharedData->SelectedBodies.Num());
		SlowTask.MakeDialog();
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[i].Index];
			check(BodySetup);
			SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("ResetCollsionStepInfo", "Generating collision for {0}"), FText::FromName(BodySetup->BoneName)));
			BodySetup->Modify();

			int32 BoneIndex = EditorSkelMesh->RefSkeleton.FindBoneIndex(BodySetup->BoneName);
			check(BoneIndex != INDEX_NONE);

			const FBoneVertInfo& UseVertInfo = NewBodyData.VertWeight == EVW_DominantWeight ? SharedData->DominantWeightBoneInfos[BoneIndex] : SharedData->AnyWeightBoneInfos[BoneIndex];
			if(FPhysicsAssetUtils::CreateCollisionFromBone(BodySetup, EditorSkelMesh, BoneIndex, NewBodyData, UseVertInfo))
			{
				BodyIndices.AddUnique(SharedData->SelectedBodies[i].Index);
			}
			else
			{
				FPhysicsAssetUtils::DestroyBody(SharedData->PhysicsAsset, SharedData->SelectedBodies[i].Index);
			}
		}

		//deselect first
		SharedData->ClearSelectedBody();
		for(int32 i=0; i<BodyIndices.Num(); ++i)
		{
			SharedData->SetSelectedBodyAnyPrim(BodyIndices[i], true);
		}
	}
	else
	{
		TArray<TSharedPtr<ISkeletonTreeItem>> Items = SkeletonTree->GetSelectedItems();
		FSkeletonTreeSelection Selection(Items);
		TArray<TSharedPtr<ISkeletonTreeItem>> BoneItems = Selection.GetSelectedItemsByTypeId("FSkeletonTreeBoneItem");

		// If we have bones selected, make new bodies for them
		if(BoneItems.Num() > 0)
		{
			const FScopedTransaction Transaction( LOCTEXT("AddNewPrimitive", "Add New Bodies") );

			FScopedSlowTask SlowTask((float)BoneItems.Num());
			SlowTask.MakeDialog();
			for(TSharedPtr<ISkeletonTreeItem> BoneItem : BoneItems)
			{
				SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("ResetCollsionStepInfo", "Generating collision for {0}"), FText::FromName(BoneItem->GetRowItemName())));

				UBoneProxy* BoneProxy = CastChecked<UBoneProxy>(BoneItem->GetObject());

				int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(BoneProxy->BoneName);
				if (BoneIndex != INDEX_NONE)
				{
					SharedData->MakeNewBody(BoneIndex);
				}
			}
		}
		else
		{
			const FScopedTransaction Transaction( LOCTEXT("ResetAllBoneCollision", "Reset All Collision") );

			SharedData->PhysicsAsset->Modify();

			// Deselect everything.
			SharedData->ClearSelectedBody();
			SharedData->ClearSelectedConstraints();	

			// Empty current asset data.
			SharedData->PhysicsAsset->SkeletalBodySetups.Empty();
			SharedData->PhysicsAsset->BodySetupIndexMap.Empty();
			SharedData->PhysicsAsset->ConstraintSetup.Empty();

			FText ErrorMessage;
			if (FPhysicsAssetUtils::CreateFromSkeletalMesh(SharedData->PhysicsAsset, EditorSkelMesh, NewBodyData, ErrorMessage, /*bSetToMesh=*/false) == false)
			{
				FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
			}
		}
	}

	RecreatePhysicsState();
	SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
	RefreshPreviewViewport();
	RefreshHierachyTree();
}

void FPhysicsAssetEditor::OnCopyProperties()
{
	if(SharedData->SelectedBodies.Num() == 1)
	{
		SharedData->CopyBody();
	}
	else if(SharedData->SelectedConstraints.Num() == 1)
	{
		SharedData->CopyConstraint();
	}
	
	RefreshPreviewViewport();
}

void FPhysicsAssetEditor::OnPasteProperties()
{
	if(SharedData->SelectedBodies.Num() == 1)
	{
		SharedData->PasteBodyProperties();
	}
	else if (SharedData->SelectedConstraints.Num() == 1)
	{
		SharedData->PasteConstraintProperties();
	}
	
	RefreshPreviewViewport();
}

bool FPhysicsAssetEditor::CanCopyProperties() const
{
	if(IsSelectedEditMode())
	{
		if(SharedData->SelectedBodies.Num() == 1 && SharedData->SelectedConstraints.Num() == 0)
		{
			return true;
		}
		else if(SharedData->SelectedConstraints.Num() == 1 && SharedData->SelectedBodies.Num() == 0)
		{
			return true;
		}
	}

	return false;
}

bool FPhysicsAssetEditor::CanPasteProperties() const
{
	return IsSelectedEditMode() && IsCopyProperties();
}

bool FPhysicsAssetEditor::IsCopyProperties() const
{
	return (SharedData->CopiedBodySetup) || (SharedData->CopiedConstraintTemplate);
}

//We need to save and restore physics states based on the mode we use to simulate
void FPhysicsAssetEditor::FixPhysicsState()
{
	UPhysicsAsset * PhysicsAsset = SharedData->PhysicsAsset;
	TArray<USkeletalBodySetup*>& BodySetup = PhysicsAsset->SkeletalBodySetups;

	if(!SharedData->bRunningSimulation)
	{
		PhysicsTypeState.Reset();
		for(int32 i=0; i<SharedData->PhysicsAsset->SkeletalBodySetups.Num(); ++i)
		{
			PhysicsTypeState.Add(BodySetup[i]->PhysicsType);
		}
	}else
	{
		for(int32 i=0; i<PhysicsTypeState.Num(); ++i)
		{
			BodySetup[i]->PhysicsType = PhysicsTypeState[i];
		}
	}
}

void FPhysicsAssetEditor::ImpToggleSimulation()
{
	static const int32 PrevMaxFPS = GEngine->GetMaxFPS();

	if(!SharedData->bRunningSimulation)
	{
		GEngine->SetMaxFPS(SharedData->EditorOptions->MaxFPS);
	}
	else
	{
		GEngine->SetMaxFPS(PrevMaxFPS);
	}

	SharedData->ToggleSimulation();

	// add to analytics record
	OnAddPhatRecord(TEXT("ToggleSimulate"), true, true);
}

void FPhysicsAssetEditor::OnRepeatLastSimulation()
{
	OnToggleSimulation(SelectedSimulation);
}

void FPhysicsAssetEditor::OnToggleSimulation(bool bInSelected)
{
	SelectedSimulation = bInSelected;

	// this stores current physics types before simulate
	// and recovers to the previous physics types
	// so after this one, we can modify physics types fine
	FixPhysicsState();
	if (IsSelectedSimulation())
	{
		SetupSelectedSimulation();
	}
	ImpToggleSimulation();
}

void FPhysicsAssetEditor::OnToggleSimulationNoGravity()
{
	SharedData->bNoGravitySimulation = !SharedData->bNoGravitySimulation;
}

bool FPhysicsAssetEditor::IsNoGravitySimulationEnabled() const
{
	return SharedData->bNoGravitySimulation;
}

bool FPhysicsAssetEditor::IsFullSimulation() const
{
	return !SelectedSimulation;
}

bool FPhysicsAssetEditor::IsSelectedSimulation() const
{
	return SelectedSimulation;
}

void FPhysicsAssetEditor::SetupSelectedSimulation()
{
	//Before starting we modify the PhysicsType so that selected are unfixed and the rest are fixed
	if(SharedData->bRunningSimulation == false)
	{
		UPhysicsAsset * PhysicsAsset = SharedData->PhysicsAsset;
		TArray<USkeletalBodySetup*>& BodySetup = PhysicsAsset->SkeletalBodySetups;

		//first we fix all the bodies
		for(int32 i=0; i<SharedData->PhysicsAsset->SkeletalBodySetups.Num(); ++i)
		{
			BodySetup[i]->PhysicsType = PhysType_Kinematic;
		}

		//Bodies already have a function that does this
		SetBodiesBelowSelectedPhysicsType(PhysType_Simulated, false);

		//constraints need some more work
		TArray<int32> BodyIndices;
		TArray<UPhysicsConstraintTemplate*> & ConstraintSetup = PhysicsAsset->ConstraintSetup;
		for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
		{
			int32 ConstraintIndex = SharedData->SelectedConstraints[i].Index;
			FName ConstraintBone1 = ConstraintSetup[ConstraintIndex]->DefaultInstance.ConstraintBone1;	//we only unfix the child bodies

			for(int32 j=0; j<BodySetup.Num(); ++j)
			{
				if(BodySetup[j]->BoneName == ConstraintBone1)
				{
					BodyIndices.Add(j);
				}
			}
		}

		SetBodiesBelowPhysicsType(PhysType_Simulated, BodyIndices, false);
	}
}


bool FPhysicsAssetEditor::IsToggleSimulation() const
{
	return SharedData->bRunningSimulation;
}

void FPhysicsAssetEditor::OnMeshRenderingMode(EPhysicsAssetEditorRenderMode Mode, bool bSimulation)
{
	if (bSimulation)
	{
		SharedData->EditorOptions->SimulationMeshViewMode = Mode;
	}
	else
	{
		SharedData->EditorOptions->MeshViewMode = Mode;
	}

	SharedData->EditorOptions->SaveConfig();

	// Changing the mesh rendering mode requires the skeletal mesh component to change its render state, which is an operation
	// which is deferred until after render. Hence we need to trigger another viewport refresh on the following frame.
	RefreshPreviewViewport();
}

bool FPhysicsAssetEditor::IsMeshRenderingMode(EPhysicsAssetEditorRenderMode Mode, bool bSimulation) const
{
	return Mode == SharedData->GetCurrentMeshViewMode(bSimulation);
}

void FPhysicsAssetEditor::OnCollisionRenderingMode(EPhysicsAssetEditorRenderMode Mode, bool bSimulation)
{
	if (bSimulation)
	{
		SharedData->EditorOptions->SimulationCollisionViewMode = Mode;
	}
	else
	{
		SharedData->EditorOptions->CollisionViewMode = Mode;
	}

	SharedData->EditorOptions->SaveConfig();

	RefreshPreviewViewport();
}

bool FPhysicsAssetEditor::IsCollisionRenderingMode(EPhysicsAssetEditorRenderMode Mode, bool bSimulation) const
{
	return Mode == SharedData->GetCurrentCollisionViewMode(bSimulation);
}

void FPhysicsAssetEditor::OnConstraintRenderingMode(EPhysicsAssetEditorConstraintViewMode Mode, bool bSimulation)
{
	if (bSimulation)
	{
		SharedData->EditorOptions->SimulationConstraintViewMode = Mode;
	}
	else
	{
		SharedData->EditorOptions->ConstraintViewMode = Mode;
	}

	SharedData->EditorOptions->SaveConfig();

	RefreshPreviewViewport();
}

void FPhysicsAssetEditor::ToggleDrawConstraintsAsPoints()
{
	SharedData->EditorOptions->bShowConstraintsAsPoints = !SharedData->EditorOptions->bShowConstraintsAsPoints;
	SharedData->EditorOptions->SaveConfig();
}

bool FPhysicsAssetEditor::IsDrawingConstraintsAsPoints() const
{
	return SharedData->EditorOptions->bShowConstraintsAsPoints;
}

void FPhysicsAssetEditor::ToggleRenderOnlySelectedSolid()
{
	SharedData->EditorOptions->bSolidRenderingForSelectedOnly = !SharedData->EditorOptions->bSolidRenderingForSelectedOnly;
	SharedData->EditorOptions->SaveConfig();
}

bool FPhysicsAssetEditor::IsRenderingOnlySelectedSolid() const
{
	return SharedData->EditorOptions->bSolidRenderingForSelectedOnly;
}

bool FPhysicsAssetEditor::IsConstraintRenderingMode(EPhysicsAssetEditorConstraintViewMode Mode, bool bSimulation) const
{
	return Mode == SharedData->GetCurrentConstraintViewMode(bSimulation);
}

void FPhysicsAssetEditor::OnToggleMassProperties()
{
	SharedData->bShowCOM = !SharedData->bShowCOM;

	RefreshPreviewViewport();
}

bool FPhysicsAssetEditor::IsToggleMassProperties() const
{
	return SharedData->bShowCOM;
}

void FPhysicsAssetEditor::OnSetCollision(bool bEnable)
{
	SharedData->SetCollisionBetweenSelected(bEnable);
}

bool FPhysicsAssetEditor::CanSetCollision(bool bEnable) const
{
	return SharedData->CanSetCollisionBetweenSelected(bEnable);
}

void FPhysicsAssetEditor::OnSetCollisionAll(bool bEnable)
{
	SharedData->SetCollisionBetweenSelectedAndAll(bEnable);
}

bool FPhysicsAssetEditor::CanSetCollisionAll(bool bEnable) const
{
	return SharedData->CanSetCollisionBetweenSelectedAndAll(bEnable);
}

void FPhysicsAssetEditor::OnWeldToBody()
{
	SharedData->WeldSelectedBodies();
}

bool FPhysicsAssetEditor::CanWeldToBody()
{
	return HasSelectedBodyAndIsNotSimulation() && SharedData->WeldSelectedBodies(false);
}

void FPhysicsAssetEditor::OnAddSphere()
{
	AddNewPrimitive(EAggCollisionShape::Sphere);
}

void FPhysicsAssetEditor::OnAddSphyl()
{
	AddNewPrimitive(EAggCollisionShape::Sphyl);
}

void FPhysicsAssetEditor::OnAddBox()
{
	AddNewPrimitive(EAggCollisionShape::Box);
}

bool FPhysicsAssetEditor::CanAddPrimitive() const
{
	return IsNotSimulation();
}

void FPhysicsAssetEditor::OnDeletePrimitive()
{
	SharedData->DeleteCurrentPrim();
	RecreatePhysicsState();
}

void FPhysicsAssetEditor::OnDuplicatePrimitive()
{
	AddNewPrimitive(EAggCollisionShape::Unknown, true);
}

bool FPhysicsAssetEditor::CanDuplicatePrimitive() const
{
	return HasSelectedBodyAndIsNotSimulation() && SharedData->SelectedBodies.Num() == 1;
}

void FPhysicsAssetEditor::OnResetConstraint()
{
	SharedData->SetSelectedConstraintRelTM(FTransform::Identity);
	RefreshPreviewViewport();
}

void FPhysicsAssetEditor::OnSnapConstraint()
{
	const FScopedTransaction Transaction( LOCTEXT( "SnapConstraints", "Snap Constraints" ) );

	for(int32 i=0; i<SharedData->SelectedConstraints.Num(); ++i)
	{
		SnapConstraintToBone(&SharedData->SelectedConstraints[i]);
	}
	
	RefreshPreviewViewport();
}

void FPhysicsAssetEditor::OnConvertToBallAndSocket()
{
	CreateOrConvertConstraint(EPCT_BSJoint);
}

void FPhysicsAssetEditor::OnConvertToHinge()
{
	CreateOrConvertConstraint(EPCT_Hinge);
}

void FPhysicsAssetEditor::OnConvertToPrismatic()
{
	CreateOrConvertConstraint(EPCT_Prismatic);
}

void FPhysicsAssetEditor::OnConvertToSkeletal()
{
	CreateOrConvertConstraint(EPCT_SkelJoint);
}

void FPhysicsAssetEditor::OnDeleteConstraint()
{
	SharedData->DeleteCurrentConstraint();
}

void FPhysicsAssetEditor::OnSetBodyPhysicsType( EPhysicsType InPhysicsType )
{
	if (SharedData->GetSelectedBody())
	{
		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[i].Index];
			BodySetup->Modify();
			BodySetup->PhysicsType = InPhysicsType;
		}

		RecreatePhysicsState();
		RefreshPreviewViewport();
	}
}

bool FPhysicsAssetEditor::IsBodyPhysicsType( EPhysicsType InPhysicsType )
{
	for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
	{
		UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[i].Index];
		if(BodySetup->PhysicsType == InPhysicsType)
		{
			return true;
		}

	}
	
	return false;
}

void FPhysicsAssetEditor::OnDeleteBody()
{
	if(SharedData->SelectedBodies.Num())
	{
		//first build the bodysetup array because deleting bodies modifies the selected array
		TArray<UBodySetup*> BodySetups;
		BodySetups.Reserve(SharedData->SelectedBodies.Num());

		for(int32 i=0; i<SharedData->SelectedBodies.Num(); ++i)
		{
			BodySetups.Add( SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[i].Index] );
		}

		const FScopedTransaction Transaction( LOCTEXT( "DeleteBodies", "Delete Bodies" ) );

		for(int32 i=0; i<BodySetups.Num(); ++i)
		{
			int32 BodyIndex = SharedData->PhysicsAsset->FindBodyIndex(BodySetups[i]->BoneName);
			if(BodyIndex != INDEX_NONE)
			{
				// Use PhysicsAssetEditor function to delete action (so undo works etc)
				SharedData->DeleteBody(BodyIndex, false);
			}
		}

		SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
	}
}

void FPhysicsAssetEditor::OnDeleteAllBodiesBelow()
{
	USkeletalMesh* EditorSkelMesh = SharedData->PhysicsAsset->GetPreviewMesh();
	if(EditorSkelMesh == nullptr)
	{
		return;
	}

	TArray<UBodySetup*> BodySetups;

	for (FPhysicsAssetEditorSharedData::FSelection SelectedBody : SharedData->SelectedBodies)
	{
		UBodySetup* BaseSetup = SharedData->PhysicsAsset->SkeletalBodySetups[SelectedBody.Index];
		
		// Build a list of BodySetups below this one
		TArray<int32> BelowBodies;
		SharedData->PhysicsAsset->GetBodyIndicesBelow(BelowBodies, BaseSetup->BoneName, EditorSkelMesh);

		for (const int32 BodyIndex : BelowBodies)
		{
			UBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[BodyIndex];
			BodySetups.Add(BodySetup);
		}
	}

	if(BodySetups.Num())
	{
		const FScopedTransaction Transaction( LOCTEXT( "DeleteBodiesBelow", "Delete Bodies Below" ) );

		// Now remove each one
		for (UBodySetup* BodySetup : BodySetups)
		{
			// Use PhysicsAssetEditor function to delete action (so undo works etc)
			int32 Index = SharedData->PhysicsAsset->FindBodyIndex(BodySetup->BoneName);
			if(Index != INDEX_NONE)
			{
				SharedData->DeleteBody(Index, false);
			}
		}

		SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
	}
	
}

void FPhysicsAssetEditor::OnDeleteSelection()
{
	SharedData->DeleteCurrentPrim();
	SharedData->DeleteCurrentConstraint();
}

void FPhysicsAssetEditor::OnCycleConstraintOrientation()
{
	if(SharedData->GetSelectedConstraint())
	{
		SharedData->CycleCurrentConstraintOrientation();
	}
}

void FPhysicsAssetEditor::OnCycleConstraintActive()
{
	if(SharedData->GetSelectedConstraint())
	{
		SharedData->CycleCurrentConstraintActive();
	}
}

void FPhysicsAssetEditor::OnToggleSwing1()
{
	if(SharedData->GetSelectedConstraint())
	{
		SharedData->ToggleConstraint(FPhysicsAssetEditorSharedData::PCT_Swing1);
	}
}

void FPhysicsAssetEditor::OnToggleSwing2()
{
	if(SharedData->GetSelectedConstraint())
	{
		SharedData->ToggleConstraint(FPhysicsAssetEditorSharedData::PCT_Swing2);
	}
}

void FPhysicsAssetEditor::OnToggleTwist()
{
	if(SharedData->GetSelectedConstraint())
	{
		SharedData->ToggleConstraint(FPhysicsAssetEditorSharedData::PCT_Twist);
	}
}

bool FPhysicsAssetEditor::IsSwing1Locked() const
{
	return SharedData->IsAngularConstraintLocked(FPhysicsAssetEditorSharedData::PCT_Swing1);
}

bool FPhysicsAssetEditor::IsSwing2Locked() const
{
	return SharedData->IsAngularConstraintLocked(FPhysicsAssetEditorSharedData::PCT_Swing2);
}

bool FPhysicsAssetEditor::IsTwistLocked() const
{
	return SharedData->IsAngularConstraintLocked(FPhysicsAssetEditorSharedData::PCT_Twist);
}

TSharedRef<SWidget> FPhysicsAssetEditor::BuildStaticMeshAssetPicker()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(UStaticMesh::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &FPhysicsAssetEditor::OnAssetSelectedFromStaticMeshAssetPicker);
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
	AssetPickerConfig.bShowBottomToolbar = false;
	AssetPickerConfig.SelectionMode = ESelectionMode::Single;

	return SNew(SBox)
		.IsEnabled(this, &FPhysicsAssetEditor::IsNotSimulation)
		.WidthOverride(300)
		.HeightOverride(400)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

void FPhysicsAssetEditor::OnAssetSelectedFromStaticMeshAssetPicker( const FAssetData& AssetData )
{
	FSlateApplication::Get().DismissAllMenus();
	
	const FScopedTransaction Transaction( LOCTEXT("Import Convex", "Import Convex") );
	// Make sure rendering is done - so we are not changing data being used by collision drawing.
	FlushRenderingCommands();

	if (SharedData->GetSelectedBody())
	{
		UStaticMesh* SM = Cast<UStaticMesh>(AssetData.GetAsset());

		if (SM && SM->BodySetup && SM->BodySetup->AggGeom.GetElementCount() > 0)
		{
			SharedData->PhysicsAsset->Modify();

			for (int32 SelectedBodyIndex = 0; SelectedBodyIndex < SharedData->SelectedBodies.Num(); ++SelectedBodyIndex)
			{
				UBodySetup* BaseSetup = SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[SelectedBodyIndex].Index];
				BaseSetup->Modify();
				BaseSetup->AddCollisionFrom(SM->BodySetup);
				BaseSetup->InvalidatePhysicsData();
				BaseSetup->CreatePhysicsMeshes();
			}

			SharedData->RefreshPhysicsAssetChange(SharedData->PhysicsAsset);
			RefreshHierachyTree();
		}
		else
		{
			UE_LOG(LogPhysics, Warning, TEXT("Failed to import body from static mesh %s. Mesh probably has no collision setup."), *AssetData.AssetName.ToString());
		}
	}
}

TSharedRef<SWidget> FPhysicsAssetEditor::BuildPhysicalMaterialAssetPicker(bool bForAllBodies)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(UPhysicalMaterial::StaticClass()->GetFName());
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &FPhysicsAssetEditor::OnAssetSelectedFromPhysicalMaterialAssetPicker, bForAllBodies);
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
	AssetPickerConfig.bShowBottomToolbar = false;
	AssetPickerConfig.SelectionMode = ESelectionMode::Single;

	// Find a suitable default if any
	UPhysicalMaterial* SelectedPhysicalMaterial = nullptr;
	if(bForAllBodies)
	{
		if(SharedData->PhysicsAsset->SkeletalBodySetups.Num() > 0)
		{
			SelectedPhysicalMaterial = SharedData->PhysicsAsset->SkeletalBodySetups[0]->PhysMaterial;
			for (int32 SelectedBodyIndex = 0; SelectedBodyIndex < SharedData->PhysicsAsset->SkeletalBodySetups.Num(); ++SelectedBodyIndex)
			{
				USkeletalBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SelectedBodyIndex];
				if(BodySetup->PhysMaterial != SelectedPhysicalMaterial)
				{
					SelectedPhysicalMaterial = nullptr;
					break;
				}
			}
		}
	}
	else
	{
		if(SharedData->SelectedBodies.Num())
		{
			SelectedPhysicalMaterial = SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[0].Index]->PhysMaterial;
			for (int32 SelectedBodyIndex = 0; SelectedBodyIndex < SharedData->SelectedBodies.Num(); ++SelectedBodyIndex)
			{
				USkeletalBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[SelectedBodyIndex].Index];
				if(BodySetup->PhysMaterial != SelectedPhysicalMaterial)
				{
					SelectedPhysicalMaterial = nullptr;
					break;
				}
			}
		}
	}

	AssetPickerConfig.InitialAssetSelection = FAssetData(SelectedPhysicalMaterial);

	return SNew(SBox)
		.IsEnabled(this, &FPhysicsAssetEditor::IsNotSimulation)
		.WidthOverride(300)
		.HeightOverride(400)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

void FPhysicsAssetEditor::OnAssetSelectedFromPhysicalMaterialAssetPicker( const FAssetData& AssetData, bool bForAllBodies )
{
	FSlateApplication::Get().DismissAllMenus();

	if (SharedData->GetSelectedBody() || bForAllBodies)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetPhysicalMaterial", "Set Physical Material"));

		UPhysicalMaterial* PhysicalMaterial = Cast<UPhysicalMaterial>(AssetData.GetAsset());
		if(PhysicalMaterial)
		{
			if(bForAllBodies)
			{
				for (int32 SelectedBodyIndex = 0; SelectedBodyIndex < SharedData->PhysicsAsset->SkeletalBodySetups.Num(); ++SelectedBodyIndex)
				{
					USkeletalBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SelectedBodyIndex];
					BodySetup->Modify();
					BodySetup->PhysMaterial = PhysicalMaterial;
				}
			}
			else
			{
				for (int32 SelectedBodyIndex = 0; SelectedBodyIndex < SharedData->SelectedBodies.Num(); ++SelectedBodyIndex)
				{
					USkeletalBodySetup* BodySetup = SharedData->PhysicsAsset->SkeletalBodySetups[SharedData->SelectedBodies[SelectedBodyIndex].Index];
					BodySetup->Modify();
					BodySetup->PhysMaterial = PhysicalMaterial;
				}
			}
		}
	}
}

void FPhysicsAssetEditor::OnSelectAllBodies()
{
	UPhysicsAsset * const PhysicsAsset = SharedData->EditorSkelComp->GetPhysicsAsset();

	// Block selection broadcast until we have selected all, as this can be an expensive operation
	FPhysicsAssetEditorSharedData::FSelectionChanged SelectionChangedEvent = SharedData->SelectionChangedEvent;
	SharedData->SelectionChangedEvent = FPhysicsAssetEditorSharedData::FSelectionChanged();
	
	//Bodies
	//first deselect everything
	SharedData->ClearSelectedBody();

	//go through every body and add every geom
	for (int32 i = 0; i < PhysicsAsset->SkeletalBodySetups.Num(); ++i)
	{
		int32 BoneIndex = SharedData->EditorSkelComp->GetBoneIndex(PhysicsAsset->SkeletalBodySetups[i]->BoneName);

		// If we found a bone for it, add all geom
		if (BoneIndex != INDEX_NONE)
		{
			FKAggregateGeom* AggGeom = &PhysicsAsset->SkeletalBodySetups[i]->AggGeom;

			for (int32 j = 0; j < AggGeom->SphereElems.Num(); ++j)
			{
				FPhysicsAssetEditorSharedData::FSelection Selection(i, EAggCollisionShape::Sphere, j);
				SharedData->SetSelectedBody(Selection, true);
			}

			for (int32 j = 0; j < AggGeom->BoxElems.Num(); ++j)
			{
				FPhysicsAssetEditorSharedData::FSelection Selection(i, EAggCollisionShape::Box, j);
				SharedData->SetSelectedBody(Selection, true);
			}

			for (int32 j = 0; j < AggGeom->SphylElems.Num(); ++j)
			{
				FPhysicsAssetEditorSharedData::FSelection Selection(i, EAggCollisionShape::Sphyl, j);
				SharedData->SetSelectedBody(Selection, true);
			}

			for (int32 j = 0; j < AggGeom->ConvexElems.Num(); ++j)
			{
				FPhysicsAssetEditorSharedData::FSelection Selection(i, EAggCollisionShape::Convex, j);
				SharedData->SetSelectedBody(Selection, true);
			}
		}
	}
	
	SharedData->SelectionChangedEvent = SelectionChangedEvent;
	SharedData->SelectionChangedEvent.Broadcast(SharedData->SelectedBodies, SharedData->SelectedConstraints);
}

void FPhysicsAssetEditor::OnSelectAllConstraints()
{
	UPhysicsAsset * const PhysicsAsset = SharedData->EditorSkelComp->GetPhysicsAsset();

	// Block selection broadcast until we have selected all, as this can be an expensive operation
	FPhysicsAssetEditorSharedData::FSelectionChanged SelectionChangedEvent = SharedData->SelectionChangedEvent;
	SharedData->SelectionChangedEvent = FPhysicsAssetEditorSharedData::FSelectionChanged();

	//Constraints
	//Deselect everything first
	SharedData->ClearSelectedConstraints();

	//go through every constraint and add it
	for (int32 i = 0; i < PhysicsAsset->ConstraintSetup.Num(); ++i)
	{
		int32 BoneIndex1 = SharedData->EditorSkelComp->GetBoneIndex(PhysicsAsset->ConstraintSetup[i]->DefaultInstance.ConstraintBone1);
		int32 BoneIndex2 = SharedData->EditorSkelComp->GetBoneIndex(PhysicsAsset->ConstraintSetup[i]->DefaultInstance.ConstraintBone2);
		// if bone doesn't exist, do not draw it. It crashes in random points when we try to manipulate. 
		if (BoneIndex1 != INDEX_NONE && BoneIndex2 != INDEX_NONE)
		{
			SharedData->SetSelectedConstraint(i, true);
		}
	}

	SharedData->SelectionChangedEvent = SelectionChangedEvent;
	SharedData->SelectionChangedEvent.Broadcast(SharedData->SelectedBodies, SharedData->SelectedConstraints);
}

void FPhysicsAssetEditor::OnDeselectAll()
{
	SharedData->ClearSelectedBody();
	SharedData->ClearSelectedConstraints();
}

// record if simulating or not, or mode changed or not, or what mode it is in while simulating and what kind of simulation options
void FPhysicsAssetEditor::OnAddPhatRecord(const FString& Action, bool bRecordSimulate, bool bRecordMode)
{
	// Don't attempt to report usage stats if analytics isn't available
	if( Action.IsEmpty() == false && SharedData.IsValid() && FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attribs;
		if (bRecordSimulate)
		{
			Attribs.Add(FAnalyticsEventAttribute(TEXT("Simulation"), SharedData->bRunningSimulation? TEXT("ON") : TEXT("OFF")));
			if ( SharedData->bRunningSimulation )
			{
				Attribs.Add(FAnalyticsEventAttribute(TEXT("Selected"), IsSelectedSimulation()? TEXT("ON") : TEXT("OFF")));
				Attribs.Add(FAnalyticsEventAttribute(TEXT("Gravity"), SharedData->bNoGravitySimulation ? TEXT("ON") : TEXT("OFF")));
			}
		}

		FString EventString = FString::Printf(TEXT("Editor.Usage.PHAT.%s"), *Action);
		FEngineAnalytics::GetProvider().RecordEvent(EventString, Attribs);
	}
}

void FPhysicsAssetEditor::Tick(float DeltaTime)
{
	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

TStatId FPhysicsAssetEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysicsAssetEditor, STATGROUP_Tickables);
}

void FPhysicsAssetEditor::HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView)
{
	PhysAssetProperties = InDetailsView;

	PhysAssetProperties->SetObject(nullptr);
	PhysAssetProperties->OnFinishedChangingProperties().AddSP(this, &FPhysicsAssetEditor::OnFinishedChangingProperties);
	PhysAssetProperties->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this](){ return !SharedData->bRunningSimulation; })));
}

void FPhysicsAssetEditor::HandlePhysicsAssetGraphCreated(const TSharedRef<SPhysicsAssetGraph>& InPhysicsAssetGraph)
{
	PhysicsAssetGraph = InPhysicsAssetGraph;
}

void FPhysicsAssetEditor::HandleGraphObjectsSelected(const TArrayView<UObject*>& InObjects)
{
	if (!bSelecting)
	{
		TGuardValue<bool> RecursionGuard(bSelecting, true);

		SkeletonTree->DeselectAll();

		TArray<UObject*> Objects;
		Algo::TransformIf(InObjects, Objects, [](UObject* InItem) { return InItem != nullptr; }, [](UObject* InItem) { return InItem; });

		if (PhysAssetProperties.IsValid())
		{
			PhysAssetProperties->SetObjects(Objects);
		}

		// Block selection broadcast until we have selected all, as this can be an expensive operation
		FPhysicsAssetEditorSharedData::FSelectionChanged SelectionChangedEvent = SharedData->SelectionChangedEvent;
		SharedData->SelectionChangedEvent = FPhysicsAssetEditorSharedData::FSelectionChanged();

		// clear selection
		SharedData->SelectedBodies.Empty();
		SharedData->SelectedConstraints.Empty();

		TArray<USkeletalBodySetup*> SelectedBodies;
		TArray<UPhysicsConstraintTemplate*> SelectedConstraints;
		for (UObject* SelectedObject : Objects)
		{
			if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(SelectedObject))
			{
				SelectedBodies.Add(BodySetup);
				for (int32 BodySetupIndex = 0; BodySetupIndex < SharedData->PhysicsAsset->SkeletalBodySetups.Num(); ++BodySetupIndex)
				{
					if (SharedData->PhysicsAsset->SkeletalBodySetups[BodySetupIndex] == BodySetup)
					{
						SharedData->SetSelectedBodyAnyPrim(BodySetupIndex, true);
					}
				}
			}
			else if (UPhysicsConstraintTemplate* Constraint = Cast<UPhysicsConstraintTemplate>(SelectedObject))
			{
				SelectedConstraints.Add(Constraint);
				for (int32 ConstraintIndex = 0; ConstraintIndex < SharedData->PhysicsAsset->ConstraintSetup.Num(); ++ConstraintIndex)
				{
					if (SharedData->PhysicsAsset->ConstraintSetup[ConstraintIndex] == Constraint)
					{
						SharedData->SetSelectedConstraint(ConstraintIndex, true);
					}
				}
			}
		}

		SharedData->SelectionChangedEvent = SelectionChangedEvent;
		SharedData->SelectionChangedEvent.Broadcast(SharedData->SelectedBodies, SharedData->SelectedConstraints);

		SkeletonTree->SelectItemsBy([&SelectedBodies, &SelectedConstraints](const TSharedRef<ISkeletonTreeItem>& InItem, bool& bInOutExpand)
		{
			if(InItem->IsOfType<FSkeletonTreePhysicsBodyItem>())
			{
				for (USkeletalBodySetup* SelectedBody : SelectedBodies)
				{
					if (SelectedBody == Cast<USkeletalBodySetup>(InItem->GetObject()))
					{
						bInOutExpand = true;
						return true;
					}
				}
			}
			else if(InItem->IsOfType<FSkeletonTreePhysicsConstraintItem>())
			{
				for (UPhysicsConstraintTemplate* SelectedConstraint : SelectedConstraints)
				{
					if (SelectedConstraint == Cast<UPhysicsConstraintTemplate>(InItem->GetObject()))
					{
						bInOutExpand = true;
						return true;
					}
				}
			}

			bInOutExpand = false;
			return false;
		});
	}
}

void FPhysicsAssetEditor::HandleSelectionChanged(const TArrayView<TSharedPtr<ISkeletonTreeItem>>& InSelectedItems, ESelectInfo::Type InSelectInfo)
{
	if (!bSelecting)
	{
		TGuardValue<bool> RecursionGuard(bSelecting, true);

		// Always set the details customization object, regardless of selection type
		// We do this because the tree may have been rebuilt and objects invalidated
		TArray<UObject*> Objects;
		Algo::TransformIf(InSelectedItems, Objects, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject() != nullptr; }, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject(); });

		if (PhysAssetProperties.IsValid())
		{
			PhysAssetProperties->SetObjects(Objects);
		}

		// Only a user selection should change other view's selections
		if (InSelectInfo != ESelectInfo::Direct)
		{
			// Block selection broadcast until we have selected all, as this can be an expensive operation
			FPhysicsAssetEditorSharedData::FSelectionChanged SelectionChangedEvent = SharedData->SelectionChangedEvent;
			SharedData->SelectionChangedEvent = FPhysicsAssetEditorSharedData::FSelectionChanged();

			// clear selection
			SharedData->ClearSelectedBody();
			SharedData->ClearSelectedConstraints();

			bool bBoneSelected = false;
			for (const TSharedPtr<ISkeletonTreeItem>& Item : InSelectedItems)
			{
				if (Item->IsOfType<FSkeletonTreePhysicsBodyItem>())
				{
					TSharedPtr<FSkeletonTreePhysicsBodyItem> SkeletonTreePhysicsBodyItem = StaticCastSharedPtr<FSkeletonTreePhysicsBodyItem>(Item);
					SharedData->SetSelectedBodyAnyPrim(SkeletonTreePhysicsBodyItem->GetBodySetupIndex(), true);
				}
				else if (Item->IsOfType<FSkeletonTreePhysicsShapeItem>())
				{
					TSharedPtr<FSkeletonTreePhysicsShapeItem> SkeletonTreePhysicsShapeItem = StaticCastSharedPtr<FSkeletonTreePhysicsShapeItem>(Item);
					FPhysicsAssetEditorSharedData::FSelection Selection(SkeletonTreePhysicsShapeItem->GetBodySetupIndex(), SkeletonTreePhysicsShapeItem->GetShapeType(), SkeletonTreePhysicsShapeItem->GetShapeIndex());
					SharedData->SetSelectedBody(Selection, true);
				}
				else if (Item->IsOfType<FSkeletonTreePhysicsConstraintItem>())
				{
					TSharedPtr<FSkeletonTreePhysicsConstraintItem> SkeletonTreePhysicsConstraintItem = StaticCastSharedPtr<FSkeletonTreePhysicsConstraintItem>(Item);
					SharedData->SetSelectedConstraint(SkeletonTreePhysicsConstraintItem->GetConstraintIndex(), true);
				}
				else if(Item->IsOfTypeByName(TEXT("FSkeletonTreeBoneItem")))
				{
					bBoneSelected = true;
				}
			}

			SharedData->SelectionChangedEvent = SelectionChangedEvent;
			SharedData->SelectionChangedEvent.Broadcast(SharedData->SelectedBodies, SharedData->SelectedConstraints);

			if(!bBoneSelected)
			{
				GetPersonaToolkit()->GetPreviewScene()->ClearSelectedBone();
			}

			if (PhysicsAssetGraph.IsValid())
			{
				TSet<USkeletalBodySetup*> Bodies;
				TSet<UPhysicsConstraintTemplate*> Constraints;
				Algo::TransformIf(InSelectedItems, Bodies, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject() && InItem->GetObject()->IsA<USkeletalBodySetup>(); }, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return Cast<USkeletalBodySetup>(InItem->GetObject()); });
				Algo::TransformIf(InSelectedItems, Constraints, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return InItem->GetObject() && InItem->GetObject()->IsA<UPhysicsConstraintTemplate>(); }, [](const TSharedPtr<ISkeletonTreeItem>& InItem) { return Cast<UPhysicsConstraintTemplate>(InItem->GetObject()); });
				PhysicsAssetGraph.Pin()->SelectObjects(Bodies.Array(), Constraints.Array());
			}
		}
	}
}

void FPhysicsAssetEditor::HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
{
	InPersonaPreviewScene->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &FPhysicsAssetEditor::OnChangeDefaultMesh));

	SharedData->Initialize(InPersonaPreviewScene);

	AActor* Actor = InPersonaPreviewScene->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	InPersonaPreviewScene->SetActor(Actor);

	// Create the preview component
	SharedData->EditorSkelComp = NewObject<UPhysicsAssetEditorSkeletalMeshComponent>(Actor);
	SharedData->EditorSkelComp->SharedData = SharedData.Get();
	SharedData->EditorSkelComp->SetSkeletalMesh(SharedData->PhysicsAsset->GetPreviewMesh());
	SharedData->EditorSkelComp->SetPhysicsAsset(SharedData->PhysicsAsset, true);
	InPersonaPreviewScene->SetPreviewMeshComponent(SharedData->EditorSkelComp);
	InPersonaPreviewScene->AddComponent(SharedData->EditorSkelComp, FTransform::Identity);
		
	// set root component, so we can attach to it. 
	Actor->SetRootComponent(SharedData->EditorSkelComp);

	SharedData->EditorSkelComp->Stop();

	SharedData->EditorSkelComp->SetAnimationMode(EAnimationMode::AnimationCustomMode);
	SharedData->EditorSkelComp->PreviewInstance = NewObject<UPhysicsAssetEditorAnimInstance>(SharedData->EditorSkelComp, TEXT("PhatAnimScriptInstance"));
	SharedData->EditorSkelComp->AnimScriptInstance = SharedData->EditorSkelComp->PreviewInstance;
	SharedData->EditorSkelComp->AnimScriptInstance->InitializeAnimation();
	SharedData->EditorSkelComp->InitAnim(true);

	SharedData->PhysicalAnimationComponent = NewObject<UPhysicalAnimationComponent>(Actor);
	SharedData->PhysicalAnimationComponent->SetSkeletalMeshComponent(SharedData->EditorSkelComp);
	InPersonaPreviewScene->AddComponent(SharedData->PhysicalAnimationComponent, FTransform::Identity);

	SharedData->ResetTM = SharedData->EditorSkelComp->GetComponentToWorld();

	// Register handle component
	SharedData->MouseHandle->RegisterComponentWithWorld(InPersonaPreviewScene->GetWorld());

	SharedData->EnableSimulation(false);

	// Make sure the floor mesh has collision (BlockAllDynamic may have been overriden)
	static FName CollisionProfileName(TEXT("PhysicsActor"));
	UStaticMeshComponent* FloorMeshComponent = const_cast<UStaticMeshComponent*>(InPersonaPreviewScene->GetFloorMeshComponent());
	FloorMeshComponent->SetCollisionProfileName(CollisionProfileName);
	FloorMeshComponent->RecreatePhysicsState();
}

void FPhysicsAssetEditor::HandleExtendContextMenu(FMenuBuilder& InMenuBuilder)
{
	TArray<TSharedPtr<ISkeletonTreeItem>> SelectedItems = SkeletonTree->GetSelectedItems();
	FSkeletonTreeSelection Selection(SelectedItems);
	
	TArray<TSharedPtr<FSkeletonTreePhysicsBodyItem>> SelectedBodies = Selection.GetSelectedItems<FSkeletonTreePhysicsBodyItem>();
	TArray<TSharedPtr<FSkeletonTreePhysicsConstraintItem>> SelectedConstraints = Selection.GetSelectedItems<FSkeletonTreePhysicsConstraintItem>();
	TArray<TSharedPtr<FSkeletonTreePhysicsShapeItem>> SelectedShapes = Selection.GetSelectedItems<FSkeletonTreePhysicsShapeItem>();
	TArray<TSharedPtr<ISkeletonTreeItem>> SelectedBones = Selection.GetSelectedItemsByTypeId("FSkeletonTreeBoneItem");
	if (SelectedBodies.Num() > 0)
	{
		BuildMenuWidgetBody(InMenuBuilder);
	}
	else if (SelectedShapes.Num() > 0)
	{
		BuildMenuWidgetPrimitives(InMenuBuilder);
	}
	else if(SelectedConstraints.Num() > 0)
	{
		BuildMenuWidgetConstraint(InMenuBuilder);
	}
	else if(SelectedBones.Num() > 0)
	{
		BuildMenuWidgetBone(InMenuBuilder);
	}

	BuildMenuWidgetSelection(InMenuBuilder);
}

void FPhysicsAssetEditor::HandleExtendFilterMenu(FMenuBuilder& InMenuBuilder)
{
	const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

	InMenuBuilder.BeginSection(TEXT("PhysicsAssetFilters"), LOCTEXT("PhysicsAssetFiltersHeader", "Physics Asset Filters"));
	{
		InMenuBuilder.AddMenuEntry(Commands.ShowBodies);
		InMenuBuilder.AddMenuEntry(Commands.ShowConstraints);
		InMenuBuilder.AddMenuEntry(Commands.ShowPrimitives);
	}
	InMenuBuilder.EndSection();
}

void FPhysicsAssetEditor::HandleToggleShowBodies()
{
	SkeletonTreeBuilder->bShowBodies = !SkeletonTreeBuilder->bShowBodies;
	SkeletonTree->RefreshFilter();
}

void FPhysicsAssetEditor::HandleToggleShowConstraints()
{
	SkeletonTreeBuilder->bShowConstraints = !SkeletonTreeBuilder->bShowConstraints;
	SkeletonTree->RefreshFilter();
}

void FPhysicsAssetEditor::HandleToggleShowPrimitives()
{
	SkeletonTreeBuilder->bShowPrimitives = !SkeletonTreeBuilder->bShowPrimitives;
	SkeletonTree->RefreshFilter();
}

ECheckBoxState FPhysicsAssetEditor::GetShowBodiesChecked() const
{
	return SkeletonTreeBuilder->bShowBodies ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FPhysicsAssetEditor::GetShowConstraintsChecked() const
{
	return SkeletonTreeBuilder->bShowConstraints ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FPhysicsAssetEditor::GetShowPrimitivesChecked() const
{
	return SkeletonTreeBuilder->bShowPrimitives ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FPhysicsAssetEditor::HandleGetFilterLabel(TArray<FText>& InOutItems) const
{
	if(SkeletonTreeBuilder->bShowBodies)
	{
		InOutItems.Add(LOCTEXT("BodiesFilterLabel", "Bodies"));
	}

	if(SkeletonTreeBuilder->bShowConstraints)
	{
		InOutItems.Add(LOCTEXT("ConstraintsFilterLabel", "Constraints"));
	}

	if(SkeletonTreeBuilder->bShowPrimitives)
	{
		InOutItems.Add(LOCTEXT("PrimitivesFilterLabel", "Primitives"));
	}
}

void FPhysicsAssetEditor::HandleCreateNewConstraint(int32 BodyIndex0, int32 BodyIndex1)
{
	if(BodyIndex0 != BodyIndex1)
	{
		SharedData->MakeNewConstraint(BodyIndex0, BodyIndex1);
	}
}

void FPhysicsAssetEditor::RecreatePhysicsState()
{
	// Flush geometry cache inside the asset (don't want to use cached version of old geometry!)
	SharedData->PhysicsAsset->InvalidateAllPhysicsMeshes();
	SharedData->EditorSkelComp->RecreatePhysicsState();

	// Reset simulation state of body instances so we dont actually simulate outside of 'simulation mode'
	SharedData->ForceDisableSimulation();
}

#undef LOCTEXT_NAMESPACE
