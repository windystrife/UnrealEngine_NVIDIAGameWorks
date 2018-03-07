// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PhysicsAssetEditorMode.h"
#include "PhysicsAssetEditor.h"
#include "ISkeletonTree.h"
#include "IPersonaPreviewScene.h"
#include "PersonaModule.h"
#include "ISkeletonEditorModule.h"
#include "PhysicsAssetGraphSummoner.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsAssetEditorActions.h"
#include "SEditorViewportToolBarMenu.h"
#include "PhysicsAssetEditorProfilesSummoner.h"
#include "PropertyEditorModule.h"
#include "Preferences/PhysicsAssetEditorOptions.h"
#include "SSpinBox.h"
#include "ModuleManager.h"
#include "MultiBoxBuilder.h"
#include "PhysicsAssetEditorToolsSummoner.h"

#define LOCTEXT_NAMESPACE "PhysicsAssetEditorMode"

static const FName PhysicsAssetEditorPreviewViewportName("Viewport");
static const FName PhysicsAssetEditorPropertiesName("DetailsTab");
static const FName PhysicsAssetEditorHierarchyName("SkeletonTreeView");
static const FName PhysicsAssetEditorGraphName("PhysicsAssetGraphView");
static const FName PhysicsAssetEditorProfilesName("PhysicsAssetProfilesView");
static const FName PhysicsAssetEditorToolsName("PhysicsAssetTools");
static const FName PhysicsAssetEditorAdvancedPreviewName(TEXT("AdvancedPreviewTab"));

FPhysicsAssetEditorMode::FPhysicsAssetEditorMode(TSharedRef<FWorkflowCentricApplication> InHostingApp, TSharedRef<ISkeletonTree> InSkeletonTree, TSharedRef<IPersonaPreviewScene> InPreviewScene)
	: FApplicationMode(PhysicsAssetEditorModes::PhysicsAssetEditorMode)
{
	PhysicsAssetEditorPtr = StaticCastSharedRef<FPhysicsAssetEditor>(InHostingApp);
	TSharedRef<FPhysicsAssetEditor> PhysicsAssetEditor = StaticCastSharedRef<FPhysicsAssetEditor>(InHostingApp);

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	TabFactories.RegisterFactory(SkeletonEditorModule.CreateSkeletonTreeTabFactory(InHostingApp, InSkeletonTree));

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	TabFactories.RegisterFactory(PersonaModule.CreateDetailsTabFactory(InHostingApp, FOnDetailsCreated::CreateSP(&PhysicsAssetEditor.Get(), &FPhysicsAssetEditor::HandleDetailsCreated)));

	auto ExtendShowMenu = [this](FMenuBuilder& InMenuBuilder)
	{
		const FPhysicsAssetEditorCommands& Actions = FPhysicsAssetEditorCommands::Get();

		InMenuBuilder.PushCommandList(PhysicsAssetEditorPtr.Pin()->GetToolkitCommands());

		InMenuBuilder.BeginSection("PhysicsAssetShowCommands", LOCTEXT("PhysicsShowCommands", "Physics Rendering"));
		{
			InMenuBuilder.AddMenuEntry(Actions.ToggleMassProperties);

			// Mesh, collision and constraint rendering modes
			struct Local
			{
				static void BuildMeshRenderModeMenu(FMenuBuilder& InSubMenuBuilder)
				{
					const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

					InSubMenuBuilder.BeginSection("PhysicsAssetEditorRenderingMode", LOCTEXT("MeshRenderModeHeader", "Mesh Drawing (Edit)"));
					{
						InSubMenuBuilder.AddMenuEntry(Commands.MeshRenderingMode_Solid);
						InSubMenuBuilder.AddMenuEntry(Commands.MeshRenderingMode_Wireframe);
						InSubMenuBuilder.AddMenuEntry(Commands.MeshRenderingMode_None);
					}
					InSubMenuBuilder.EndSection();

					InSubMenuBuilder.BeginSection("PhysicsAssetEditorRenderingModeSim", LOCTEXT("MeshRenderModeSimHeader", "Mesh Drawing (Simulation)"));
					{
						InSubMenuBuilder.AddMenuEntry(Commands.MeshRenderingMode_Simulation_Solid);
						InSubMenuBuilder.AddMenuEntry(Commands.MeshRenderingMode_Simulation_Wireframe);
						InSubMenuBuilder.AddMenuEntry(Commands.MeshRenderingMode_Simulation_None);
					}
					InSubMenuBuilder.EndSection();
				}

				static void BuildCollisionRenderModeMenu(FMenuBuilder& InSubMenuBuilder, TWeakPtr<FPhysicsAssetEditor> PhysicsAssetEditorPtr)
				{
					const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

					InSubMenuBuilder.BeginSection("PhysicsAssetEditorCollisionRenderSettings", LOCTEXT("CollisionRenderSettingsHeader", "Body Drawing"));
					{
						InSubMenuBuilder.AddMenuEntry(Commands.RenderOnlySelectedSolid);

						UPhysicsAssetEditorOptions* Options = PhysicsAssetEditorPtr.Pin()->GetSharedData()->EditorOptions;

						TSharedPtr<SWidget> CollisionOpacityWidget = 
							SNew(SBox)
							.HAlign(HAlign_Right)
							[
								SNew(SBox)
								.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
								.WidthOverride(100.0f)
								[
									SNew(SSpinBox<float>)
									.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
									.MinValue(0.0f)
									.MaxValue(1.0f)
									.Value_Lambda([Options]() { return Options->CollisionOpacity; })
									.OnValueChanged_Lambda([Options](float InValue) { Options->CollisionOpacity = InValue; })
									.OnValueCommitted_Lambda([Options](float InValue, ETextCommit::Type InCommitType) 
									{ 
										Options->CollisionOpacity = InValue; 
										Options->SaveConfig(); 
									})
								]
							];

						InSubMenuBuilder.AddWidget(CollisionOpacityWidget.ToSharedRef(), LOCTEXT("CollisionOpacityLabel", "Collision Opacity"));
					}
					InSubMenuBuilder.EndSection();

					InSubMenuBuilder.BeginSection("PhysicsAssetEditorCollisionMode", LOCTEXT("CollisionRenderModeHeader", "Body Drawing (Edit)"));
					{
						InSubMenuBuilder.AddMenuEntry(Commands.CollisionRenderingMode_Solid);
						InSubMenuBuilder.AddMenuEntry(Commands.CollisionRenderingMode_Wireframe);
						InSubMenuBuilder.AddMenuEntry(Commands.CollisionRenderingMode_None);
					}
					InSubMenuBuilder.EndSection();

					InSubMenuBuilder.BeginSection("PhysicsAssetEditorCollisionModeSim", LOCTEXT("CollisionRenderModeSimHeader", "Body Drawing (Simulation)"));
					{
						InSubMenuBuilder.AddMenuEntry(Commands.CollisionRenderingMode_Simulation_Solid);
						InSubMenuBuilder.AddMenuEntry(Commands.CollisionRenderingMode_Simulation_Wireframe);
						InSubMenuBuilder.AddMenuEntry(Commands.CollisionRenderingMode_Simulation_None);
					}
					InSubMenuBuilder.EndSection();
				}

				static void BuildConstraintRenderModeMenu(FMenuBuilder& InSubMenuBuilder, TWeakPtr<FPhysicsAssetEditor> PhysicsAssetEditorPtr)
				{
					const FPhysicsAssetEditorCommands& Commands = FPhysicsAssetEditorCommands::Get();

					InSubMenuBuilder.BeginSection("PhysicsAssetEditorConstraints", LOCTEXT("ConstraintHeader", "Constraints"));
					{
						InSubMenuBuilder.AddMenuEntry(Commands.DrawConstraintsAsPoints);

						UPhysicsAssetEditorOptions* Options = PhysicsAssetEditorPtr.Pin()->GetSharedData()->EditorOptions;

						TSharedPtr<SWidget> ConstraintScaleWidget = 
							SNew(SBox)
							.HAlign(HAlign_Right)
							[
								SNew(SBox)
								.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
								.WidthOverride(100.0f)
								[
									SNew(SSpinBox<float>)
									.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
									.MinValue(0.0f)
									.MaxValue(4.0f)
									.Value_Lambda([Options]() { return Options->ConstraintDrawSize; })
									.OnValueChanged_Lambda([Options](float InValue) { Options->ConstraintDrawSize = InValue; })
									.OnValueCommitted_Lambda([Options](float InValue, ETextCommit::Type InCommitType) { Options->ConstraintDrawSize = InValue; Options->SaveConfig(); })
								]
							];

						InSubMenuBuilder.AddWidget(ConstraintScaleWidget.ToSharedRef(), LOCTEXT("ConstraintScaleLabel", "Constraint Scale"));
					}
					InSubMenuBuilder.EndSection();

					InSubMenuBuilder.BeginSection("PhysicsAssetEditorConstraintMode", LOCTEXT("ConstraintRenderModeHeader", "Constraint Drawing (Edit)"));
					{
						InSubMenuBuilder.AddMenuEntry(Commands.ConstraintRenderingMode_None);
						InSubMenuBuilder.AddMenuEntry(Commands.ConstraintRenderingMode_AllPositions);
						InSubMenuBuilder.AddMenuEntry(Commands.ConstraintRenderingMode_AllLimits);
					}
					InSubMenuBuilder.EndSection();

					InSubMenuBuilder.BeginSection("PhysicsAssetEditorConstraintModeSim", LOCTEXT("ConstraintRenderModeSimHeader", "Constraint Drawing (Simulation)"));
					{
						InSubMenuBuilder.AddMenuEntry(Commands.ConstraintRenderingMode_Simulation_None);
						InSubMenuBuilder.AddMenuEntry(Commands.ConstraintRenderingMode_Simulation_AllPositions);
						InSubMenuBuilder.AddMenuEntry(Commands.ConstraintRenderingMode_Simulation_AllLimits);
					}
					InSubMenuBuilder.EndSection();
				}
			};

			InMenuBuilder.AddSubMenu(LOCTEXT("MeshRenderModeSubMenu", "Mesh"), FText::GetEmpty(), FNewMenuDelegate::CreateStatic(&Local::BuildMeshRenderModeMenu));
			InMenuBuilder.AddSubMenu(LOCTEXT("CollisionRenderModeSubMenu", "Bodies"), FText::GetEmpty(), FNewMenuDelegate::CreateStatic(&Local::BuildCollisionRenderModeMenu, PhysicsAssetEditorPtr));
			InMenuBuilder.AddSubMenu(LOCTEXT("ConstraintRenderModeSubMenu", "Constraints"), FText::GetEmpty(), FNewMenuDelegate::CreateStatic(&Local::BuildConstraintRenderModeMenu, PhysicsAssetEditorPtr));
		}
		InMenuBuilder.EndSection();

		InMenuBuilder.PopCommandList();
	};

	auto ExtendMenuBar = [this](FMenuBuilder& InMenuBuilder)
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		TSharedPtr<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		DetailsView->SetObject(PhysicsAssetEditorPtr.Pin()->GetSharedData()->EditorOptions);
		DetailsView->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& InEvent) { PhysicsAssetEditorPtr.Pin()->GetSharedData()->EditorOptions->SaveConfig(); });
		InMenuBuilder.AddWidget(DetailsView.ToSharedRef(), FText(), true);
			
	};

	TArray<TSharedPtr<FExtender>> ViewportExtenders;
	ViewportExtenders.Add(MakeShared<FExtender>());
	ViewportExtenders[0]->AddMenuExtension("AnimViewportGeneralShowFlags", EExtensionHook::After, PhysicsAssetEditor->GetToolkitCommands(), FMenuExtensionDelegate::CreateLambda(ExtendShowMenu));
	ViewportExtenders[0]->AddMenuExtension("AnimViewportShowMenu", EExtensionHook::After, PhysicsAssetEditor->GetToolkitCommands(), FMenuExtensionDelegate::CreateLambda(ExtendMenuBar));


	FPersonaViewportArgs ViewportArgs(InSkeletonTree, InPreviewScene, PhysicsAssetEditor->OnPostUndo);
	ViewportArgs.bAlwaysShowTransformToolbar = true;
	ViewportArgs.bShowStats = false;
	ViewportArgs.bShowTurnTable = false;
	ViewportArgs.bShowPhysicsMenu = true;
	ViewportArgs.Extenders = ViewportExtenders;

	TabFactories.RegisterFactory(PersonaModule.CreatePersonaViewportTabFactory(InHostingApp, ViewportArgs));

	TabFactories.RegisterFactory(PersonaModule.CreateAdvancedPreviewSceneTabFactory(InHostingApp, InPreviewScene));

	TabFactories.RegisterFactory(MakeShared<FPhysicsAssetGraphSummoner>(InHostingApp, CastChecked<UPhysicsAsset>((*PhysicsAssetEditor->GetObjectsCurrentlyBeingEdited())[0]), InSkeletonTree->GetEditableSkeleton(), FOnPhysicsAssetGraphCreated::CreateSP(&PhysicsAssetEditor.Get(), &FPhysicsAssetEditor::HandlePhysicsAssetGraphCreated), FOnGraphObjectsSelected::CreateSP(&PhysicsAssetEditor.Get(), &FPhysicsAssetEditor::HandleGraphObjectsSelected)));

	TabFactories.RegisterFactory(MakeShared<FPhysicsAssetEditorProfilesSummoner>(InHostingApp, CastChecked<UPhysicsAsset>((*PhysicsAssetEditor->GetObjectsCurrentlyBeingEdited())[0])));

	TabFactories.RegisterFactory(MakeShared<FPhysicsAssetEditorToolsSummoner>(InHostingApp));

	TabLayout = FTabManager::NewLayout("Standalone_PhysicsAssetEditor_Layout_v5.2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
				->AddTab(InHostingApp->GetToolbarTabId(), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.9f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
				    FTabManager::NewSplitter()
				    ->SetSizeCoefficient(0.2f)
				    ->SetOrientation(Orient_Vertical)
				    ->Split
				    (
					    FTabManager::NewStack()
					    ->SetSizeCoefficient(0.6f)
					    ->AddTab(PhysicsAssetEditorHierarchyName, ETabState::OpenedTab)
					)
					->Split
					(
					    FTabManager::NewStack()
					    ->SetSizeCoefficient(0.4f)
					    ->AddTab(PhysicsAssetEditorGraphName, ETabState::OpenedTab)
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->SetHideTabWell(true)
					->AddTab(PhysicsAssetEditorPreviewViewportName, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.2f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.6f)
						->AddTab(PhysicsAssetEditorPropertiesName, ETabState::OpenedTab)
						->AddTab(PhysicsAssetEditorAdvancedPreviewName, ETabState::OpenedTab)
						->SetForegroundTab(PhysicsAssetEditorPropertiesName)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.4f)
						->AddTab(PhysicsAssetEditorToolsName, ETabState::OpenedTab)
						->AddTab(PhysicsAssetEditorProfilesName, ETabState::OpenedTab)
						->SetForegroundTab(PhysicsAssetEditorToolsName)
					)
				)
			)
		);
}

void FPhysicsAssetEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FPhysicsAssetEditor> PhysicsAssetEditor = PhysicsAssetEditorPtr.Pin();
	PhysicsAssetEditor->RegisterTabSpawners(InTabManager.ToSharedRef());
	PhysicsAssetEditor->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

#undef LOCTEXT_NAMESPACE