// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "EngineGlobals.h"
#include "Engine/SkeletalMesh.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/Engine.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "EdGraph/EdGraph.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "Editor/UnrealEdEngine.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "Preferences/MaterialEditorOptions.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "MaterialEditor/PreviewMaterial.h"
#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"
#include "Particles/ParticleSystemComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/TextureCube.h"
#include "Dialogs/Dialogs.h"
#include "UnrealEdGlobals.h"
#include "Editor.h"
#include "MaterialEditorModule.h"
#include "MaterialEditingLibrary.h"
#include "HAL/PlatformApplicationMisc.h"


#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionParticleSubUV.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionTextureSampleParameterSubUV.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"

#include "MaterialEditorActions.h"
#include "MaterialExpressionClasses.h"
#include "MaterialCompiler.h"
#include "EditorSupportDelegates.h"
#include "AssetRegistryModule.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "SMaterialEditorTitleBar.h"
#include "ScopedTransaction.h"
#include "BusyCursor.h"

#include "PropertyEditorModule.h"
#include "MaterialEditorDetailCustomization.h"
#include "MaterialInstanceEditor.h"

#include "EditorViewportCommands.h"

#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Logging/TokenizedMessage.h"
#include "EdGraphUtilities.h"
#include "SNodePanel.h"
#include "MaterialEditorUtilities.h"
#include "SMaterialPalette.h"
#include "FindInMaterial.h"
#include "Misc/FeedbackContext.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Colors/SColorPicker.h"
#include "EditorClassUtils.h"
#include "IDocumentation.h"
#include "Widgets/Docking/SDockTab.h"

#include "Developer/MessageLog/Public/IMessageLogListing.h"
#include "Developer/MessageLog/Public/MessageLogInitializationOptions.h"
#include "Developer/MessageLog/Public/MessageLogModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "CanvasTypes.h"
#include "Engine/Selection.h"
#include "Materials/Material.h"
#include "AdvancedPreviewSceneModule.h"

#define LOCTEXT_NAMESPACE "MaterialEditor"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialEditor, Log, All);

static TAutoConsoleVariable<int32> CVarMaterialEdUseDevShaders(
	TEXT("r.MaterialEditor.UseDevShaders"),
	1,
	TEXT("Toggles whether the material editor will use shaders that include extra overhead incurred by the editor. Material editor must be re-opened if changed at runtime."),
	ECVF_RenderThreadSafe);

const FName FMaterialEditor::PreviewTabId( TEXT( "MaterialEditor_Preview" ) );
const FName FMaterialEditor::GraphCanvasTabId( TEXT( "MaterialEditor_GraphCanvas" ) );
const FName FMaterialEditor::PropertiesTabId( TEXT( "MaterialEditor_MaterialProperties" ) );
const FName FMaterialEditor::HLSLCodeTabId( TEXT( "MaterialEditor_HLSLCode" ) );
const FName FMaterialEditor::PaletteTabId( TEXT( "MaterialEditor_Palette" ) );
const FName FMaterialEditor::StatsTabId( TEXT( "MaterialEditor_Stats" ) );
const FName FMaterialEditor::FindTabId( TEXT( "MaterialEditor_Find" ) );
const FName FMaterialEditor::PreviewSettingsTabId( TEXT ("MaterialEditor_PreviewSettings" ) );

///////////////////////////
// FMatExpressionPreview //
///////////////////////////

bool FMatExpressionPreview::ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
{
	if(VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
	{
		// we only need the non-light-mapped, base pass, local vertex factory shaders for drawing an opaque Material Tile
		// @todo: Added a FindShaderType by fname or something"

		if (IsMobilePlatform(Platform))
		{
			if (FCString::Stristr(ShaderType->GetName(), TEXT("BasePassForForwardShadingVSFNoLightMapPolicy")) ||
				FCString::Stristr(ShaderType->GetName(), TEXT("BasePassForForwardShadingPSFNoLightMapPolicy")))
			{
				return true;
			}
		}
		else
		{
			if (FCString::Stristr(ShaderType->GetName(), TEXT("BasePassVSFNoLightMapPolicy")) ||
				FCString::Stristr(ShaderType->GetName(), TEXT("BasePassHSFNoLightMapPolicy")) ||
				FCString::Stristr(ShaderType->GetName(), TEXT("BasePassDSFNoLightMapPolicy")))
			{
				return true;
			}
			else if (FCString::Stristr(ShaderType->GetName(), TEXT("BasePassPSFNoLightMapPolicy")))
			{
				return true;
			}
			else if (FCString::Stristr(ShaderType->GetName(), TEXT("Simple")))
			{
				return true;
			}
		}
	}

	return false;
}

int32 FMatExpressionPreview::CompilePropertyAndSetMaterialProperty(EMaterialProperty Property, FMaterialCompiler* Compiler, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime) const
{
	// needs to be called in this function!!
	Compiler->SetMaterialProperty(Property, OverrideShaderFrequency, bUsePreviousFrameTime);

	int32 Ret = INDEX_NONE;

	if( Property == MP_EmissiveColor && Expression.IsValid())
	{
		// Hardcoding output 0 as we don't have the UI to specify any other output
		const int32 OutputIndex = 0;
		// Get back into gamma corrected space, as DrawTile does not do this adjustment.
		Ret = Compiler->Power(Compiler->Max(Expression->CompilePreview(Compiler, OutputIndex), Compiler->Constant(0)), Compiler->Constant(1.f / 2.2f));
	}
	else if (Property == MP_WorldPositionOffset)
	{
		//set to 0 to prevent off by 1 pixel errors
		Ret = Compiler->Constant(0.0f);
	}
	else if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
	{
		const int32 TextureCoordinateIndex = Property - MP_CustomizedUVs0;
		Ret = Compiler->TextureCoordinate(TextureCoordinateIndex, false, false);
	}
	else
	{
		Ret = Compiler->Constant(1.0f);
	}

	// output should always be the right type for this property
	return Compiler->ForceCast(Ret, FMaterialAttributeDefinitionMap::GetValueType(Property));
}

void FMatExpressionPreview::NotifyCompilationFinished()
{
	if (Expression.IsValid() && Expression->GraphNode)
	{
		CastChecked<UMaterialGraphNode>(Expression->GraphNode)->bPreviewNeedsUpdate = true;
	}
}

/////////////////////
// FMaterialEditor //
/////////////////////

void FMaterialEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MaterialEditor", "Material Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	
	InTabManager->RegisterTabSpawner( PreviewTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_Preview) )
		.SetDisplayName( LOCTEXT("ViewportTab", "Viewport") )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));
	
	InTabManager->RegisterTabSpawner( GraphCanvasTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_GraphCanvas) )
		.SetDisplayName( LOCTEXT("GraphCanvasTab", "Graph") )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "GraphEditor.EventGraph_16x"));
	
	InTabManager->RegisterTabSpawner( PropertiesTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_MaterialProperties) )
		.SetDisplayName( LOCTEXT("DetailsTab", "Details") )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner( PaletteTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_Palette) )
		.SetDisplayName( LOCTEXT("PaletteTab", "Palette") )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Tabs.Palette"));

	InTabManager->RegisterTabSpawner( StatsTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_Stats) )
		.SetDisplayName( LOCTEXT("StatsTab", "Stats") )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
	
	InTabManager->RegisterTabSpawner(FindTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_Find))
		.SetDisplayName(LOCTEXT("FindTab", "Find Results"))
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Tabs.FindResults"));

	InTabManager->RegisterTabSpawner( HLSLCodeTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_HLSLCode) )
		.SetDisplayName( LOCTEXT("HLSLCodeTab", "HLSL Code") )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "MaterialEditor.Tabs.HLSLCode"));

	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FMaterialEditor::SpawnTab_PreviewSettings))
		.SetDisplayName( LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings") )
		.SetGroup( WorkspaceMenuCategoryRef )
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));


	OnRegisterTabSpawners().Broadcast(InTabManager);
}


void FMaterialEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( PreviewTabId );
	InTabManager->UnregisterTabSpawner( GraphCanvasTabId );
	InTabManager->UnregisterTabSpawner( PropertiesTabId );
	InTabManager->UnregisterTabSpawner( PaletteTabId );
	InTabManager->UnregisterTabSpawner( StatsTabId );
	InTabManager->UnregisterTabSpawner( FindTabId );
	InTabManager->UnregisterTabSpawner( HLSLCodeTabId );
	InTabManager->UnregisterTabSpawner( PreviewSettingsTabId );

	OnUnregisterTabSpawners().Broadcast(InTabManager);
}

void FMaterialEditor::InitEditorForMaterial(UMaterial* InMaterial)
{
	check(InMaterial);

	OriginalMaterial = InMaterial;
	MaterialFunction = NULL;
	OriginalMaterialObject = InMaterial;

	ExpressionPreviewMaterial = NULL;
	
	// Create a copy of the material for preview usage (duplicating to a different class than original!)
	// Propagate all object flags except for RF_Standalone, otherwise the preview material won't GC once
	// the material editor releases the reference.
	Material = (UMaterial*)StaticDuplicateObject(OriginalMaterial, GetTransientPackage(), NAME_None, ~RF_Standalone, UPreviewMaterial::StaticClass()); 
	
	Material->CancelOutstandingCompilation();	//The material is compiled later on anyway so no need to do it in Duplication/PostLoad. 
												//I'm hackily canceling the jobs here but we should really not add the jobs in the first place. <<--- TODO
												
	Material->bAllowDevelopmentShaderCompile = CVarMaterialEdUseDevShaders.GetValueOnGameThread();

	// Remove NULL entries, so the rest of the material editor can assume all entries of Material->Expressions are valid
	// This can happen if an expression class was removed
	for (int32 ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
	{
		if (!Material->Expressions[ExpressionIndex])
		{
			Material->Expressions.RemoveAt(ExpressionIndex);
		}
	}

	TArray<FString> Groups;
	GetAllMaterialExpressionGroups(&Groups);
}

void FMaterialEditor::InitEditorForMaterialFunction(UMaterialFunction* InMaterialFunction)
{
	check(InMaterialFunction);

	Material = NULL;
	MaterialFunction = InMaterialFunction;
	OriginalMaterialObject = InMaterialFunction;

	ExpressionPreviewMaterial = NULL;

	// Create a temporary material to preview the material function
	Material = NewObject<UMaterial>(); 
	{
		FArchiveUObject DummyArchive;
		// Hack: serialize the new material with an archive that does nothing so that its material resources are created
		Material->Serialize(DummyArchive);
	}
	Material->SetShadingModel(MSM_Unlit);

	// Propagate all object flags except for RF_Standalone, otherwise the preview material function won't GC once
	// the material editor releases the reference.
	MaterialFunction = (UMaterialFunction*)StaticDuplicateObject(InMaterialFunction, GetTransientPackage(), NAME_None, ~RF_Standalone, UMaterialFunction::StaticClass()); 
	MaterialFunction->ParentFunction = InMaterialFunction;

	OriginalMaterial = Material;

	TArray<FString> Groups;
	GetAllMaterialExpressionGroups(&Groups);
}

void FMaterialEditor::InitMaterialEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit )
{
	EditorOptions = NULL;
	bMaterialDirty = false;
	bStatsFromPreviewMaterial = false;
	ColorPickerObject = NULL;

	// Support undo/redo
	Material->SetFlags(RF_Transactional);

	GEditor->RegisterForUndo(this);

	if (!Material->MaterialGraph)
	{
		Material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(Material, NAME_None, UMaterialGraph::StaticClass(), UMaterialGraphSchema::StaticClass()));
	}
	Material->MaterialGraph->Material = Material;
	Material->MaterialGraph->MaterialFunction = MaterialFunction;
	Material->MaterialGraph->RealtimeDelegate.BindSP(this, &FMaterialEditor::IsToggleRealTimeExpressionsChecked);
	Material->MaterialGraph->MaterialDirtyDelegate.BindSP(this, &FMaterialEditor::SetMaterialDirty);
	Material->MaterialGraph->ToggleCollapsedDelegate.BindSP(this, &FMaterialEditor::ToggleCollapsed);

	// copy material usage
	for( int32 Usage=0; Usage < MATUSAGE_MAX; Usage++ )
	{
		const EMaterialUsage UsageEnum = (EMaterialUsage)Usage;
		if( OriginalMaterial->GetUsageByFlag(UsageEnum) )
		{
			bool bNeedsRecompile=false;
			Material->SetMaterialUsage(bNeedsRecompile,UsageEnum);
		}
	}
	// Manually copy bUsedAsSpecialEngineMaterial as it is duplicate transient to prevent accidental creation of new special engine materials
	Material->bUsedAsSpecialEngineMaterial = OriginalMaterial->bUsedAsSpecialEngineMaterial;
	
	// Register our commands. This will only register them if not previously registered
	FGraphEditorCommands::Register();
	FMaterialEditorCommands::Register();
	FMaterialEditorSpawnNodeCommands::Register();

	FEditorSupportDelegates::MaterialUsageFlagsChanged.AddRaw(this, &FMaterialEditor::OnMaterialUsageFlagsChanged);
	FEditorSupportDelegates::VectorParameterDefaultChanged.AddRaw(this, &FMaterialEditor::OnVectorParameterDefaultChanged);
	FEditorSupportDelegates::ScalarParameterDefaultChanged.AddRaw(this, &FMaterialEditor::OnScalarParameterDefaultChanged);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	
	AssetRegistryModule.Get().OnAssetRenamed().AddSP( this, &FMaterialEditor::RenameAssetFromRegistry );

	CreateInternalWidgets();

	// Do setup previously done in SMaterialEditorCanvas
	SetPreviewMaterial(Material);
	Material->bIsPreviewMaterial = true;
	FMaterialEditorUtilities::InitExpressions(Material);

	UpdatePreviewViewportsVisibility();

	BindCommands();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_MaterialEditor_Layout_v6")
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell( true )
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Vertical) ->SetSizeCoefficient(0.2f)
				->Split
				(
					FTabManager::NewStack()
					->SetHideTabWell( true )
					->AddTab( PreviewTabId, ETabState::OpenedTab )
				)
				->Split
				(
					FTabManager::NewStack()
					->AddTab( PropertiesTabId, ETabState::OpenedTab )
					->AddTab( PreviewSettingsTabId, ETabState::ClosedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation( Orient_Vertical )
				->SetSizeCoefficient(0.80f)
				->Split
				(
					FTabManager::NewStack() 
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell( true )
					->AddTab( GraphCanvasTabId, ETabState::OpenedTab )
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.20f )
					->AddTab( StatsTabId, ETabState::ClosedTab )
					->AddTab( FindTabId, ETabState::ClosedTab )
				)
				
			)
			->Split
			(
				FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal) ->SetSizeCoefficient(0.2f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab( PaletteTabId, ETabState::OpenedTab )
				)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	// Add the preview material to the objects being edited, so that we can find this editor from the temporary material graph
	TArray< UObject* > ObjectsToEdit;
	ObjectsToEdit.Add(ObjectToEdit);
	ObjectsToEdit.Add(Material);
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, MaterialEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit, false );

	AddMenuExtender(GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
	AddMenuExtender(MaterialEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// @todo toolkit world centric editing
	/*if( IsWorldCentricAssetEditor() )
	{
		SpawnToolkitTab(GetToolbarTabId(), FString(), EToolkitTabSpot::ToolBar);
		SpawnToolkitTab(PreviewTabId, FString(), EToolkitTabSpot::Viewport);
		SpawnToolkitTab(GraphCanvasTabId, FString(), EToolkitTabSpot::Document);
		SpawnToolkitTab(PropertiesTabId, FString(), EToolkitTabSpot::Details);
	}*/
	

	// Load editor settings from disk.
	LoadEditorSettings();
	
	// Set the preview mesh for the material.  This call must occur after the toolbar is initialized.
	if (!SetPreviewAssetByName(*Material->PreviewMesh.ToString()))
	{
		// The material preview mesh couldn't be found or isn't loaded.  Default to the one of the primitive types.
		SetPreviewAsset( GUnrealEd->GetThumbnailManager()->EditorSphere );
	}

	// Initialize expression previews.
	if (MaterialFunction)
	{
		// Support undo/redo for the material function if it exists
		MaterialFunction->SetFlags(RF_Transactional);

		Material->Expressions = MaterialFunction->FunctionExpressions;
		Material->EditorComments = MaterialFunction->FunctionEditorComments;

		// Remove NULL entries, so the rest of the material editor can assume all entries of Material->Expressions are valid
		// This can happen if an expression class was removed
		for (int32 ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
		{
			if (!Material->Expressions[ExpressionIndex])
			{
				Material->Expressions.RemoveAt(ExpressionIndex);
			}
		}

		if (Material->Expressions.Num() == 0)
		{
			// If this is an empty functions, create an output by default and start previewing it
			if (GraphEditor.IsValid())
			{
				check(!bMaterialDirty);
				UMaterialExpression* Expression = CreateNewMaterialExpression(UMaterialExpressionFunctionOutput::StaticClass(), FVector2D(200, 300), false, true);
				SetPreviewExpression(Expression);
				// This shouldn't count as having dirtied the material, so reset the flag
				bMaterialDirty = false;
			}
		}
		else
		{
			bool bSetPreviewExpression = false;
			UMaterialExpressionFunctionOutput* FirstOutput = NULL;
			for (int32 ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
			{
				UMaterialExpression* Expression = Material->Expressions[ExpressionIndex];

				// Setup the expression to be used with the preview material instead of the function
				Expression->Function = NULL;
				Expression->Material = Material;

				UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression);
				if (FunctionOutput)
				{
					FirstOutput = FunctionOutput;
					if (FunctionOutput->bLastPreviewed)
					{
						bSetPreviewExpression = true;

						// Preview the last output previewed
						SetPreviewExpression(FunctionOutput);
					}
				}
			}

			if (!bSetPreviewExpression && FirstOutput)
			{
				SetPreviewExpression(FirstOutput);
			}
		}
	}

	// Store the name of this material (for the tutorial widget meta)
	Material->MaterialGraph->OriginalMaterialFullName = OriginalMaterial->GetName();
	Material->MaterialGraph->RebuildGraph();
	RecenterEditor();

	//Make sure the preview material is initialized.
	UpdatePreviewMaterial(true);
	RegenerateCodeView(true);

	ForceRefreshExpressionPreviews();
}

FMaterialEditor::FMaterialEditor()
	: bMaterialDirty(false)
	, bStatsFromPreviewMaterial(false)
	, Material(NULL)
	, OriginalMaterial(NULL)
	, ExpressionPreviewMaterial(NULL)
	, EmptyMaterial(NULL)
	, PreviewExpression(NULL)
	, MaterialFunction(NULL)
	, OriginalMaterialObject(NULL)
	, EditorOptions(NULL)
	, ScopedTransaction(NULL)
	, bAlwaysRefreshAllPreviews(false)
	, bHideUnusedConnectors(false)
	, bLivePreview(true)
	, bIsRealtime(false)
	, bShowStats(true)
	, bShowBuiltinStats(false)
	, bShowMobileStats(false)
	, MenuExtensibilityManager(new FExtensibilityManager)
	, ToolBarExtensibilityManager(new FExtensibilityManager)
{
}

FMaterialEditor::~FMaterialEditor()
{
	// Broadcast that this editor is going down to all listeners
	OnMaterialEditorClosed().Broadcast();

	for (int32 ParameterIndex = 0; ParameterIndex < OverriddenVectorParametersToRevert.Num(); ParameterIndex++)
	{
		SetVectorParameterDefaultOnDependentMaterials(OverriddenVectorParametersToRevert[ParameterIndex], FLinearColor::Black, false);
	}

	for (int32 ParameterIndex = 0; ParameterIndex < OverriddenScalarParametersToRevert.Num(); ParameterIndex++)
	{
		SetScalarParameterDefaultOnDependentMaterials(OverriddenScalarParametersToRevert[ParameterIndex], 0, false);
	}

	// Unregister this delegate
	FEditorSupportDelegates::MaterialUsageFlagsChanged.RemoveAll(this);
	FEditorSupportDelegates::VectorParameterDefaultChanged.RemoveAll(this);
	FEditorSupportDelegates::ScalarParameterDefaultChanged.RemoveAll(this);

	// Null out the expression preview material so they can be GC'ed
	ExpressionPreviewMaterial = NULL;

	// Save editor settings to disk.
	SaveEditorSettings();

	MaterialDetailsView.Reset();

	{
		SCOPED_SUSPEND_RENDERING_THREAD(true);
	
		ExpressionPreviews.Empty();
	}
	
	check( !ScopedTransaction );
	
	GEditor->UnregisterForUndo( this );
}

void FMaterialEditor::GetAllMaterialExpressionGroups(TArray<FString>* OutGroups)
{
	for (int32 MaterialExpressionIndex = 0; MaterialExpressionIndex < Material->Expressions.Num(); ++MaterialExpressionIndex)
	{
		UMaterialExpression* MaterialExpression = Material->Expressions[ MaterialExpressionIndex ];
		UMaterialExpressionParameter *Switch = Cast<UMaterialExpressionParameter>(MaterialExpression);
		UMaterialExpressionTextureSampleParameter *TextureS = Cast<UMaterialExpressionTextureSampleParameter>(MaterialExpression);
		UMaterialExpressionFontSampleParameter *FontS = Cast<UMaterialExpressionFontSampleParameter>(MaterialExpression);
		if(Switch)
		{
			OutGroups->AddUnique(Switch->Group.ToString());
			Material->AttemptInsertNewGroupName(Switch->Group.ToString());
		}
		if(TextureS)
		{
			OutGroups->AddUnique(TextureS->Group.ToString());
			Material->AttemptInsertNewGroupName(TextureS->Group.ToString());
		}
		if(FontS)
		{
			OutGroups->AddUnique(FontS->Group.ToString());
			Material->AttemptInsertNewGroupName(FontS->Group.ToString());
		}
	}
}

void FMaterialEditor::UpdatePreviewViewportsVisibility()
{
	if( Material->IsUIMaterial() )
	{
		PreviewViewport->SetVisibility(EVisibility::Collapsed);
		PreviewUIViewport->SetVisibility(EVisibility::Visible);
	}
	else
	{
		PreviewViewport->SetVisibility(EVisibility::Visible);
		PreviewUIViewport->SetVisibility(EVisibility::Collapsed);
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FMaterialEditor::CreateInternalWidgets()
{
	PreviewViewport = SNew(SMaterialEditor3DPreviewViewport)
		.MaterialEditor(SharedThis(this));

	PreviewUIViewport = SNew(SMaterialEditorUIPreviewViewport, Material);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	GraphEditor = CreateGraphEditorWidget();
	// Manually set zoom level to avoid deferred zooming
	GraphEditor->SetViewLocation(FVector2D::ZeroVector, 1);

	const FDetailsViewArgs DetailsViewArgs( false, false, true, FDetailsViewArgs::HideNameArea, true, this );
	MaterialDetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );

	FOnGetDetailCustomizationInstance LayoutExpressionParameterDetails = FOnGetDetailCustomizationInstance::CreateStatic(
		&FMaterialExpressionParameterDetails::MakeInstance, FOnCollectParameterGroups::CreateSP(this, &FMaterialEditor::GetAllMaterialExpressionGroups) );

	MaterialDetailsView->RegisterInstancedCustomPropertyLayout( 
		UMaterialExpressionParameter::StaticClass(), 
		LayoutExpressionParameterDetails
		);

	MaterialDetailsView->RegisterInstancedCustomPropertyLayout( 
		UMaterialExpressionFontSampleParameter::StaticClass(), 
		LayoutExpressionParameterDetails
		);

	MaterialDetailsView->RegisterInstancedCustomPropertyLayout( 
		UMaterialExpressionTextureSampleParameter::StaticClass(), 
		LayoutExpressionParameterDetails
		);

	FOnGetDetailCustomizationInstance LayoutCollectionParameterDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FMaterialExpressionCollectionParameterDetails::MakeInstance);

	MaterialDetailsView->RegisterInstancedCustomPropertyLayout( 
		UMaterialExpressionCollectionParameter::StaticClass(), 
		LayoutCollectionParameterDetails
		);

	MaterialDetailsView->OnFinishedChangingProperties().AddSP(this, &FMaterialEditor::OnFinishedChangingProperties);

	PropertyEditorModule.RegisterCustomClassLayout( UMaterial::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMaterialDetailCustomization::MakeInstance ) );

	Palette = SNew(SMaterialPalette, SharedThis(this));

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogOptions;
	// Show Pages so that user is never allowed to clear log messages
	LogOptions.bShowPages = false;
	LogOptions.bShowFilters = false; //TODO - Provide custom filters? E.g. "Critical Errors" vs "Errors" needed for materials?
	LogOptions.bAllowClear = false;
	LogOptions.MaxPageCount = 1;
	StatsListing = MessageLogModule.CreateLogListing( "MaterialEditorStats", LogOptions );

	Stats = MessageLogModule.CreateLogListingWidget( StatsListing.ToSharedRef() );

	FindResults =
		SNew(SFindInMaterial, SharedThis(this));

	CodeViewUtility =
		SNew(SVerticalBox)
		// Copy Button
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f, 0.0f )
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text( LOCTEXT("CopyHLSLButton", "Copy") )
				.ToolTipText( LOCTEXT("CopyHLSLButtonToolTip", "Copies all HLSL code to the clipboard.") )
				.ContentPadding(3)
				.OnClicked(this, &FMaterialEditor::CopyCodeViewTextToClipboard)
			]
		]
		// Separator
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SSeparator)
		];

	CodeView =
		SNew(SScrollBox)
		+SScrollBox::Slot() .Padding(5)
		[
			SNew(STextBlock)
			.Text(this, &FMaterialEditor::GetCodeViewText)
		];

	RegenerateCodeView();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FMaterialEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		UStructProperty* Property = Cast<UStructProperty>(PropertyChangedEvent.Property);

		if (Property != nullptr)
		{
			if (Property->Struct->GetFName() == TEXT("LinearColor") || Property->Struct->GetFName() == TEXT("Color")) // if we changed a color property refresh the previews
			{
				RefreshExpressionPreviews();
			}
		}
	}
}

FName FMaterialEditor::GetToolkitFName() const
{
	return FName("MaterialEditor");
}

FText FMaterialEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Material Editor");
}

FText FMaterialEditor::GetToolkitName() const
{
	const UObject* EditingObject = GetEditingObjects()[0];

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	// Overridden to accommodate editing of multiple objects (original and preview materials)
	FFormatNamedArguments Args;
	Args.Add( TEXT("ObjectName"), FText::FromString( EditingObject->GetName() ) );
	Args.Add( TEXT("DirtyState"), bDirtyState ? FText::FromString( TEXT( "*" ) ) : FText::GetEmpty() );
	return FText::Format( LOCTEXT("MaterialEditorAppLabel", "{ObjectName}{DirtyState}"), Args );
}

FText FMaterialEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObjects()[0];

	// Overridden to accommodate editing of multiple objects (original and preview materials)
	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FMaterialEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Material ").ToString();
}

FLinearColor FMaterialEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}

void FMaterialEditor::Tick( float InDeltaTime )
{
	UpdateMaterialInfoList();
	UpdateGraphNodeStates();
}

TStatId FMaterialEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FMaterialEditor, STATGROUP_Tickables);
}

void FMaterialEditor::UpdateThumbnailInfoPreviewMesh(UMaterialInterface* MatInterface)
{
	if ( MatInterface )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass( MatInterface->GetClass() );
		if ( AssetTypeActions.IsValid() )
		{
			USceneThumbnailInfoWithPrimitive* OriginalThumbnailInfo = Cast<USceneThumbnailInfoWithPrimitive>(AssetTypeActions.Pin()->GetThumbnailInfo(MatInterface));
			if ( OriginalThumbnailInfo )
			{
				OriginalThumbnailInfo->PreviewMesh = MatInterface->PreviewMesh;
				MatInterface->PostEditChange();
			}
		}
	}
}

void FMaterialEditor::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Apply");
			{
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().Apply);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("Search");
			{
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().FindInMaterial);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Graph");
			{
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().CameraHome);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().CleanUnusedExpressions);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ShowHideConnectors); 
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleLivePreview);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleRealtimeExpressions);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().AlwaysRefreshAllPreviews);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleMaterialStats);
				ToolbarBuilder.AddToolBarButton(FMaterialEditorCommands::Get().ToggleMobileStats);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar )
		);
	
	AddToolbarExtender(ToolbarExtender);

	AddToolbarExtender(GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
	AddToolbarExtender(MaterialEditorModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}


UMaterialInterface* FMaterialEditor::GetMaterialInterface() const
{
	return Material;
}

bool FMaterialEditor::ApproveSetPreviewAsset(UObject* InAsset)
{
	bool bApproved = true;

	// Only permit the use of a skeletal mesh if the material has bUsedWithSkeltalMesh.
	if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(InAsset))
	{
		if (!Material->bUsedWithSkeletalMesh)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_MaterialEditor_CantPreviewOnSkelMesh", "Can't preview on the specified skeletal mesh because the material has not been compiled with bUsedWithSkeletalMesh."));
			bApproved = false;
		}
	}

	return bApproved;
}

void FMaterialEditor::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	if ((MaterialFunction != nullptr) && MaterialFunction->ParentFunction)
	{
		OutObjects.Add(MaterialFunction->ParentFunction);
	}
	else
	{
		OutObjects.Add(OriginalMaterial);
	}
}

void FMaterialEditor::SaveAsset_Execute()
{
	UE_LOG(LogMaterialEditor, Log, TEXT("Saving and Compiling material %s"), *GetEditingObjects()[0]->GetName());
	
	if (bMaterialDirty)
	{
		UpdateOriginalMaterial();
	}

	IMaterialEditor::SaveAsset_Execute();
}

void FMaterialEditor::SaveAssetAs_Execute()
{
	UE_LOG(LogMaterialEditor, Log, TEXT("Saving and Compiling material %s"), *GetEditingObjects()[0]->GetName());

	if (bMaterialDirty)
	{
		UpdateOriginalMaterial();
	}

	IMaterialEditor::SaveAssetAs_Execute();
}

bool FMaterialEditor::OnRequestClose()
{
	DestroyColorPicker();

	if (bMaterialDirty)
	{
		// find out the user wants to do with this dirty material
		EAppReturnType::Type YesNoCancelReply = FMessageDialog::Open(EAppMsgType::YesNoCancel,
			FText::Format(
				NSLOCTEXT("UnrealEd", "Prompt_MaterialEditorClose", "Would you like to apply changes to this material to the original material?\n{0}\n(No will lose all changes!)"),
				FText::FromString(OriginalMaterialObject->GetPathName()) ));

		// act on it
		switch (YesNoCancelReply)
		{
		case EAppReturnType::Yes:
			// update material and exit
			UpdateOriginalMaterial();
			break;
				
		case EAppReturnType::No:
			// exit
			bMaterialDirty = false;
			break;
				
		case EAppReturnType::Cancel:
			// don't exit
			return false;
		}
	}

	return true;
}


void FMaterialEditor::DrawMaterialInfoStrings(
	FCanvas* Canvas, 
	const UMaterial* Material, 
	const FMaterialResource* MaterialResource, 
	const TArray<FString>& CompileErrors, 
	int32 &DrawPositionY,
	bool bDrawInstructions)
{
	check(Material && MaterialResource);

	ERHIFeatureLevel::Type FeatureLevel = MaterialResource->GetFeatureLevel();
	FString FeatureLevelName;
	GetFeatureLevelName(FeatureLevel,FeatureLevelName);

	// The font to use when displaying info strings
	UFont* FontToUse = GEngine->GetTinyFont();
	const int32 SpacingBetweenLines = 13;

	if (bDrawInstructions)
	{
		// Display any errors and messages in the upper left corner of the viewport.
		TArray<FString> Descriptions;
		TArray<int32> InstructionCounts;
		MaterialResource->GetRepresentativeInstructionCounts(Descriptions, InstructionCounts);

		for (int32 InstructionIndex = 0; InstructionIndex < Descriptions.Num(); InstructionIndex++)
		{
			FString InstructionCountString = FString::Printf(TEXT("%s: %u instructions"),*Descriptions[InstructionIndex],InstructionCounts[InstructionIndex]);
			Canvas->DrawShadowedString(5, DrawPositionY, *InstructionCountString, FontToUse, FLinearColor(1, 1, 0));
			DrawPositionY += SpacingBetweenLines;
		}

		// Display the number of samplers used by the material.
		const int32 SamplersUsed = MaterialResource->GetSamplerUsage();

		if (SamplersUsed >= 0)
		{
			int32 MaxSamplers = GetExpectedFeatureLevelMaxTextureSamplers(MaterialResource->GetFeatureLevel());

			Canvas->DrawShadowedString(
				5,
				DrawPositionY,
				*FString::Printf(TEXT("%s samplers: %u/%u"), FeatureLevel <= ERHIFeatureLevel::ES3_1 ? TEXT("Mobile texture") : TEXT("Texture"), SamplersUsed, MaxSamplers),
				FontToUse,
				SamplersUsed > MaxSamplers ? FLinearColor(1,0,0) : FLinearColor(1,1,0)
				);
			DrawPositionY += SpacingBetweenLines;
		}
	}

	for(int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
	{
		Canvas->DrawShadowedString(5, DrawPositionY, *FString::Printf(TEXT("[%s] %s"), *FeatureLevelName, *CompileErrors[ErrorIndex]), FontToUse, FLinearColor(1, 0, 0));
		DrawPositionY += SpacingBetweenLines;
	}
}

void FMaterialEditor::DrawMessages( FViewport* InViewport, FCanvas* Canvas )
{
	if( PreviewExpression != NULL )
	{
		Canvas->PushAbsoluteTransform( FTranslationMatrix(FVector(0.0f, 30.0f,0.0f) ) );

		// The message to display in the viewport.
		FString Name = FString::Printf( TEXT("Previewing: %s"), *PreviewExpression->GetName() );

		// Size of the tile we are about to draw.  Should extend the length of the view in X.
		const FIntPoint TileSize( InViewport->GetSizeXY().X, 25);

		const FColor PreviewColor( 70,100,200 );
		const FColor FontColor( 255,255,128 );

		UFont* FontToUse = GEditor->EditorFont;

		Canvas->DrawTile(  0.0f, 0.0f, TileSize.X, TileSize.Y, 0.0f, 0.0f, 0.0f, 0.0f, PreviewColor );

		int32 XL, YL;
		StringSize( FontToUse, XL, YL, *Name );
		if( XL > TileSize.X )
		{
			// There isn't enough room to show the preview expression name
			Name = TEXT("Previewing");
			StringSize( FontToUse, XL, YL, *Name );
		}

		// Center the string in the middle of the tile.
		const FIntPoint StringPos( (TileSize.X-XL)/2, ((TileSize.Y-YL)/2)+1 );
		// Draw the preview message
		Canvas->DrawShadowedString(  StringPos.X, StringPos.Y, *Name, FontToUse, FontColor );

		Canvas->PopTransform();
	}
}

void FMaterialEditor::RecenterEditor()
{
	UEdGraphNode* FocusNode = NULL;

	if (MaterialFunction)
	{
		bool bSetPreviewExpression = false;
		UMaterialExpressionFunctionOutput* FirstOutput = NULL;
		for (int32 ExpressionIndex = Material->Expressions.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
		{
			UMaterialExpression* Expression = Material->Expressions[ExpressionIndex];

			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression);
			if (FunctionOutput)
			{
				FirstOutput = FunctionOutput;
				if (FunctionOutput->bLastPreviewed)
				{
					bSetPreviewExpression = true;
					FocusNode = FunctionOutput->GraphNode;
				}
			}
		}

		if (!bSetPreviewExpression && FirstOutput)
		{
			FocusNode = FirstOutput->GraphNode;
		}
	}
	else
	{
		FocusNode = Material->MaterialGraph->RootNode;
	}

	if (FocusNode)
	{
		JumpToNode(FocusNode);
	}
	else
	{
		// Get current view location so that we don't change the zoom amount
		FVector2D CurrLocation;
		float CurrZoomLevel;
		GraphEditor->GetViewLocation(CurrLocation, CurrZoomLevel);
		GraphEditor->SetViewLocation(FVector2D::ZeroVector, CurrZoomLevel);
	}
}

bool FMaterialEditor::SetPreviewAsset(UObject* InAsset)
{
	if (PreviewViewport.IsValid())
	{
		return PreviewViewport->SetPreviewAsset(InAsset);
	}
	return false;
}

bool FMaterialEditor::SetPreviewAssetByName(const TCHAR* InAssetName)
{
	if (PreviewViewport.IsValid())
	{
		return PreviewViewport->SetPreviewAssetByName(InAssetName);
	}
	return false;
}

void FMaterialEditor::SetPreviewMaterial(UMaterialInterface* InMaterialInterface)
{
	if (Material->IsUIMaterial())
	{
		if (PreviewUIViewport.IsValid())
		{
			PreviewUIViewport->SetPreviewMaterial(InMaterialInterface);
		}
	}
	else
	{
		if (PreviewViewport.IsValid())
		{
			PreviewViewport->SetPreviewMaterial(InMaterialInterface);
		}
	}
}

void FMaterialEditor::RefreshPreviewViewport()
{
	if (PreviewViewport.IsValid())
	{
		PreviewViewport->RefreshViewport();
	}
}

void FMaterialEditor::LoadEditorSettings()
{
	EditorOptions = NewObject<UMaterialEditorOptions>();
	
	if (EditorOptions->bHideUnusedConnectors) {OnShowConnectors();}
	if (bLivePreview != EditorOptions->bLivePreviewUpdate)
	{
		ToggleLivePreview();
	}
	if (EditorOptions->bAlwaysRefreshAllPreviews) {OnAlwaysRefreshAllPreviews();}
	if (EditorOptions->bRealtimeExpressionViewport) {ToggleRealTimeExpressions();}

	if ( PreviewViewport.IsValid() )
	{
		if (EditorOptions->bShowGrid)
		{
			PreviewViewport->TogglePreviewGrid();
		}

		if (EditorOptions->bShowBackground)
		{
			PreviewViewport->TogglePreviewBackground();
		}

		if (EditorOptions->bRealtimeMaterialViewport)
		{
			PreviewViewport->OnToggleRealtime();
		}
	}

	if (EditorOptions->bShowMobileStats)
	{
		ToggleMobileStats();
	}


	// Primitive type
	int32 PrimType;
	if(GConfig->GetInt(TEXT("MaterialEditor"), TEXT("PrimType"), PrimType, GEditorPerProjectIni))
	{
		PreviewViewport->OnSetPreviewPrimitive((EThumbnailPrimType)PrimType);
	}
}

void FMaterialEditor::SaveEditorSettings()
{
	// Save the preview scene
	check( PreviewViewport.IsValid() );

	if ( EditorOptions )
	{
		EditorOptions->bShowGrid					= PreviewViewport->IsTogglePreviewGridChecked();
		EditorOptions->bShowBackground				= PreviewViewport->IsTogglePreviewBackgroundChecked();
		EditorOptions->bRealtimeMaterialViewport	= PreviewViewport->IsRealtime();
		EditorOptions->bShowMobileStats				= bShowMobileStats;
		EditorOptions->bHideUnusedConnectors		= !IsOnShowConnectorsChecked();
		EditorOptions->bAlwaysRefreshAllPreviews	= IsOnAlwaysRefreshAllPreviews();
		EditorOptions->bRealtimeExpressionViewport	= IsToggleRealTimeExpressionsChecked();
		EditorOptions->bLivePreviewUpdate		= IsToggleLivePreviewChecked();
		EditorOptions->SaveConfig();
	}

	GConfig->SetInt(TEXT("MaterialEditor"), TEXT("PrimType"), PreviewViewport->PreviewPrimType, GEditorPerProjectIni);
}

FText FMaterialEditor::GetCodeViewText() const
{
	return FText::FromString(HLSLCode);
}

FReply FMaterialEditor::CopyCodeViewTextToClipboard()
{
	FPlatformApplicationMisc::ClipboardCopy(*HLSLCode);
	return FReply::Handled();
}

void FMaterialEditor::RegenerateCodeView(bool bForce)
{
#define MARKTAG TEXT("/*MARK_")
#define MARKTAGLEN 7

	HLSLCode = TEXT("");

	if (!CodeTab.IsValid() || (!bLivePreview && !bForce))
	{
		//When bLivePreview is false then the source can be out of date. 
		return;
	}

	FString MarkupSource;
	if (Material->GetMaterialResource(GMaxRHIFeatureLevel)->GetMaterialExpressionSource(MarkupSource))
	{
		// Remove line-feeds and leave just CRs so the character counts match the selection ranges.
		MarkupSource.ReplaceInline(TEXT("\r"), TEXT(""));

		// Improve formatting: Convert tab to 4 spaces since STextBlock (currently) doesn't show tab characters
		MarkupSource.ReplaceInline(TEXT("\t"), TEXT("    "));

		// Extract highlight ranges from markup tags

		// Make a copy so we can insert null terminators.
		TCHAR* MarkupSourceCopy = new TCHAR[MarkupSource.Len()+1];
		FCString::Strcpy(MarkupSourceCopy, MarkupSource.Len()+1, *MarkupSource);

		TCHAR* Ptr = MarkupSourceCopy;
		while( Ptr && *Ptr != '\0' )
		{
			TCHAR* NextTag = FCString::Strstr( Ptr, MARKTAG );
			if( !NextTag )
			{
				// No more tags, so we're done!
				HLSLCode += Ptr;
				break;
			}

			// Copy the text up to the tag.
			*NextTag = '\0';
			HLSLCode += Ptr;

			// Advance past the markup tag to see what type it is (beginning or end)
			NextTag += MARKTAGLEN;
			int32 TagNumber = FCString::Atoi(NextTag+1);
			Ptr = FCString::Strstr(NextTag, TEXT("*/")) + 2;
		}

		delete[] MarkupSourceCopy;
	}
}

void FMaterialEditor::UpdatePreviewMaterial( bool bForce )
{
	if (!bLivePreview && !bForce)
	{
		//Don't update the preview material
		return;
	}

	bStatsFromPreviewMaterial = true;

	if( PreviewExpression && ExpressionPreviewMaterial )
	{
		PreviewExpression->ConnectToPreviewMaterial(ExpressionPreviewMaterial,0);
	}

	if(PreviewExpression)
	{
		check(ExpressionPreviewMaterial);

		// The preview material's expressions array must stay up to date before recompiling 
		// So that RebuildMaterialFunctionInfo will see all the nested material functions that may need to be updated
		ExpressionPreviewMaterial->Expressions = Material->Expressions;

		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::SyncWithRenderingThread);
		UpdateContext.AddMaterial(ExpressionPreviewMaterial);

		// If we are previewing an expression, update the expression preview material
		ExpressionPreviewMaterial->PreEditChange( NULL );
		ExpressionPreviewMaterial->PostEditChange();
	}
	
	{
		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::SyncWithRenderingThread);
		UpdateContext.AddMaterial(Material);

		// Update the regular preview material even when previewing an expression to allow code view regeneration.
		Material->PreEditChange(NULL);
		Material->PostEditChange();
	}

	if (!PreviewExpression)
	{
		UpdateStatsMaterials();

		// Null out the expression preview material so they can be GC'ed
		ExpressionPreviewMaterial = NULL;
	}


	// Reregister all components that use the preview material, since UMaterial::PEC does not reregister components using a bIsPreviewMaterial=true material
	RefreshPreviewViewport();
}



void FMaterialEditor::UpdateOriginalMaterial()
{
	// If the Material has compilation errors, warn the user
	for (int32 i = ERHIFeatureLevel::SM5; i >= 0; --i)
	{
		ERHIFeatureLevel::Type FeatureLevel = (ERHIFeatureLevel::Type)i;
		if( Material->GetMaterialResource(FeatureLevel)->GetCompileErrors().Num() > 0 )
		{
			FString FeatureLevelName;
			GetFeatureLevelName(FeatureLevel, FeatureLevelName);
			FSuppressableWarningDialog::FSetupInfo Info(
				FText::Format(NSLOCTEXT("UnrealEd", "Warning_CompileErrorsInMaterial", "The current material has compilation errors, so it will not render correctly in feature level {0}.\nAre you sure you wish to continue?"),FText::FromString(*FeatureLevelName)),
				NSLOCTEXT("UnrealEd", "Warning_CompileErrorsInMaterial_Title", "Warning: Compilation errors in this Material" ), "Warning_CompileErrorsInMaterial");
			Info.ConfirmText = NSLOCTEXT("ModalDialogs", "CompileErrorsInMaterialConfirm", "Continue");
			Info.CancelText = NSLOCTEXT("ModalDialogs", "CompileErrorsInMaterialCancel", "Abort");

			FSuppressableWarningDialog CompileErrorsWarning( Info );
			if( CompileErrorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel )
			{
				return;
			}
		}
	}

	// Make sure any graph position changes that might not have been copied are taken into account
	Material->MaterialGraph->LinkMaterialExpressionsFromGraph();

	//remove any memory copies of shader files, so they will be reloaded from disk
	//this way the material editor can be used for quick shader iteration
	FlushShaderFileCache();

	//recompile and refresh the preview material so it will be updated if there was a shader change
	//Force it even if bLivePreview is false.
	UpdatePreviewMaterial(true);
	RegenerateCodeView(true);

	const FScopedBusyCursor BusyCursor;

	const FText LocalizedMaterialEditorApply = NSLOCTEXT("UnrealEd", "ToolTip_MaterialEditorApply", "Apply changes to original material and its use in the world.");
	GWarn->BeginSlowTask( LocalizedMaterialEditorApply, true );
	GWarn->StatusUpdate( 1, 1, LocalizedMaterialEditorApply );

	// Handle propagation of the material function being edited
	if (MaterialFunction)
	{
		// Copy the expressions back from the preview material
		MaterialFunction->FunctionExpressions = Material->Expressions;
		MaterialFunction->FunctionEditorComments = Material->EditorComments;

		// Preserve the thumbnail info
		UThumbnailInfo* OriginalThumbnailInfo = MaterialFunction->ParentFunction->ThumbnailInfo;
		UThumbnailInfo* ThumbnailInfo = MaterialFunction->ThumbnailInfo;
		MaterialFunction->ParentFunction->ThumbnailInfo = NULL;
		MaterialFunction->ThumbnailInfo = NULL;

		// overwrite the original material function in place by constructing a new one with the same name
		MaterialFunction->ParentFunction = (UMaterialFunction*)StaticDuplicateObject(
			MaterialFunction, 
			MaterialFunction->ParentFunction->GetOuter(), 
			MaterialFunction->ParentFunction->GetFName(), 
			RF_AllFlags, 
			MaterialFunction->ParentFunction->GetClass());

		// Restore the thumbnail info
		MaterialFunction->ParentFunction->ThumbnailInfo = OriginalThumbnailInfo;
		MaterialFunction->ThumbnailInfo = ThumbnailInfo;

		// Restore RF_Standalone on the original material function, as it had been removed from the preview material so that it could be GC'd.
		MaterialFunction->ParentFunction->SetFlags( RF_Standalone );

		for (int32 ExpressionIndex = 0; ExpressionIndex < MaterialFunction->ParentFunction->FunctionExpressions.Num(); ExpressionIndex++)
		{
			UMaterialExpression* CurrentExpression = MaterialFunction->ParentFunction->FunctionExpressions[ExpressionIndex];
			ensureMsgf(CurrentExpression, TEXT("Invalid expression at index [%i] whilst saving material function."), ExpressionIndex);

			// Link the expressions back to their function
			if (CurrentExpression)
			{
				CurrentExpression->Material = NULL;
				CurrentExpression->Function = MaterialFunction->ParentFunction;
			}	
		}
		for (int32 ExpressionIndex = 0; ExpressionIndex < MaterialFunction->ParentFunction->FunctionEditorComments.Num(); ExpressionIndex++)
		{
			UMaterialExpressionComment* CurrentExpression = MaterialFunction->ParentFunction->FunctionEditorComments[ExpressionIndex];
			ensureMsgf(CurrentExpression, TEXT("Invalid comment at index [%i] whilst saving material function."), ExpressionIndex);

			// Link the expressions back to their function
			if (CurrentExpression)
			{
				CurrentExpression->Material = NULL;
				CurrentExpression->Function = MaterialFunction->ParentFunction;
			}
		}

		// clear the dirty flag
		bMaterialDirty = false;
		bStatsFromPreviewMaterial = false;

		UMaterialEditingLibrary::UpdateMaterialFunction(MaterialFunction->ParentFunction, Material);
	}
	// Handle propagation of the material being edited
	else
	{
		FNavigationLockContext NavUpdateLock(ENavigationLockReason::MaterialUpdate);

		// ensure the original copy of the material is removed from the editor's selection set
		// or it will end up containing a stale, invalid entry
		if ( OriginalMaterial->IsSelected() )
		{
			GEditor->GetSelectedObjects()->Deselect( OriginalMaterial );
		}

		// Preserve the thumbnail info
		UThumbnailInfo* OriginalThumbnailInfo = OriginalMaterial->ThumbnailInfo;
		UThumbnailInfo* ThumbnailInfo = Material->ThumbnailInfo;
		OriginalMaterial->ThumbnailInfo = NULL;
		Material->ThumbnailInfo = NULL;

		// A bit hacky, but disable material compilation in post load when we duplicate the material.
		UMaterial::ForceNoCompilationInPostLoad(true);

		// overwrite the original material in place by constructing a new one with the same name
		OriginalMaterial = (UMaterial*)StaticDuplicateObject( Material, OriginalMaterial->GetOuter(), OriginalMaterial->GetFName(), 
			RF_AllFlags, 
			OriginalMaterial->GetClass());

		// Post load has been called, allow materials to be compiled in PostLoad.
		UMaterial::ForceNoCompilationInPostLoad(false);

		// Restore the thumbnail info
		OriginalMaterial->ThumbnailInfo = OriginalThumbnailInfo;
		Material->ThumbnailInfo = ThumbnailInfo;

		// Change the original material object to the new original material
		OriginalMaterialObject = OriginalMaterial;

		// Restore RF_Standalone on the original material, as it had been removed from the preview material so that it could be GC'd.
		OriginalMaterial->SetFlags( RF_Standalone );

		// Manually copy bUsedAsSpecialEngineMaterial as it is duplicate transient to prevent accidental creation of new special engine materials
		OriginalMaterial->bUsedAsSpecialEngineMaterial = Material->bUsedAsSpecialEngineMaterial;

		// If we are showing stats for mobile materials, compile the full material for ES2 here. That way we can see if permutations
		// not used for preview materials fail to compile.
		if (bShowMobileStats)
		{
			OriginalMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2,true);
		}

		UMaterialEditingLibrary::RecompileMaterial(OriginalMaterial);

		// clear the dirty flag
		bMaterialDirty = false;
		bStatsFromPreviewMaterial = false;
	}

	GWarn->EndSlowTask();
}

void FMaterialEditor::UpdateMaterialInfoList(bool bForceDisplay)
{
	TArray< TSharedRef<class FTokenizedMessage> > Messages;

	TArray<TSharedPtr<FMaterialInfo>> TempMaterialInfoList;

	ERHIFeatureLevel::Type FeatureLevelsToDisplay[2];
	int32 NumFeatureLevels = 0;
	// Always show basic features so that errors aren't hidden
	FeatureLevelsToDisplay[NumFeatureLevels++] = GMaxRHIFeatureLevel;
	if (bShowMobileStats)
	{
		FeatureLevelsToDisplay[NumFeatureLevels++] = ERHIFeatureLevel::ES2;
	}

	if (NumFeatureLevels > 0)
	{
		UMaterial* MaterialForStats = bStatsFromPreviewMaterial ? Material : OriginalMaterial;

		for (int32 i = 0; i < NumFeatureLevels; ++i)
		{
			TArray<FString> CompileErrors;
			ERHIFeatureLevel::Type FeatureLevel = FeatureLevelsToDisplay[i];
			const FMaterialResource* MaterialResource = MaterialForStats->GetMaterialResource(FeatureLevel);

			if (MaterialFunction && ExpressionPreviewMaterial)
			{
				// Add a compile error message for functions missing an output
				CompileErrors = ExpressionPreviewMaterial->GetMaterialResource(FeatureLevel)->GetCompileErrors();

				bool bFoundFunctionOutput = false;
				for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ExpressionIndex++)
				{
					if (Material->Expressions[ExpressionIndex]->IsA(UMaterialExpressionFunctionOutput::StaticClass()))
					{
						bFoundFunctionOutput = true;
						break;
					}
				}

				if (!bFoundFunctionOutput)
				{
					CompileErrors.Add(TEXT("Missing a function output"));
				}
			}
			else
			{
				CompileErrors = MaterialResource->GetCompileErrors();
			}

			// Only show general info if stats enabled
			if (!MaterialFunction && bShowStats)
			{
				// Display any errors and messages in the upper left corner of the viewport.
				TArray<FString> Descriptions;
				TArray<int32> InstructionCounts;
				TArray<FString> EmptyDescriptions;
				TArray<int32> EmptyInstructionCounts;

				MaterialResource->GetRepresentativeInstructionCounts(Descriptions, InstructionCounts);

				//Built in stats is no longer exposed to the UI but may still be useful so they're still in the code.
				bool bBuiltinStats = false;
				const FMaterialResource* EmptyMaterialResource = EmptyMaterial ? EmptyMaterial->GetMaterialResource(FeatureLevel) : NULL;
				if (bShowBuiltinStats && bStatsFromPreviewMaterial && EmptyMaterialResource && InstructionCounts.Num() > 0)
				{
					EmptyMaterialResource->GetRepresentativeInstructionCounts(EmptyDescriptions, EmptyInstructionCounts);

					if (EmptyInstructionCounts.Num() > 0)
					{
						//The instruction counts should match. If not, the preview material has been changed without the EmptyMaterial being updated to match.
						if (ensure(InstructionCounts.Num() == EmptyInstructionCounts.Num()))
						{
							bBuiltinStats = true;
						}
					}
				}

				for (int32 InstructionIndex = 0; InstructionIndex < Descriptions.Num(); InstructionIndex++)
				{
					FString InstructionCountString = FString::Printf(TEXT("%s: %u instructions"),*Descriptions[InstructionIndex], InstructionCounts[InstructionIndex]);
					if (bBuiltinStats)
					{
						InstructionCountString += FString::Printf(TEXT(" - Built-in instructions: %u"), EmptyInstructionCounts[InstructionIndex]);
					}
					TempMaterialInfoList.Add(MakeShareable(new FMaterialInfo(InstructionCountString, FLinearColor::Yellow)));
					TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Info);
					Line->AddToken(FTextToken::Create(FText::FromString(InstructionCountString)));
					Messages.Add(Line);
				}

				// Display the number of samplers used by the material.
				const int32 SamplersUsed = MaterialResource->GetSamplerUsage();

				if (SamplersUsed >= 0)
				{
					int32 MaxSamplers = GetExpectedFeatureLevelMaxTextureSamplers(MaterialResource->GetFeatureLevel());
					FString SamplersString = FString::Printf(TEXT("%s samplers: %u/%u"), FeatureLevel <= ERHIFeatureLevel::ES3_1 ? TEXT("Mobile texture") : TEXT("Texture"), SamplersUsed, MaxSamplers);
					TempMaterialInfoList.Add(MakeShareable(new FMaterialInfo(SamplersString, FLinearColor::Yellow)));
					TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create( EMessageSeverity::Info );
					Line->AddToken( FTextToken::Create( FText::FromString( SamplersString ) ) );
					Messages.Add(Line);
				}

				// Display the number of custom/user interpolators used by the material.
				uint32 UVScalarsUsed, CustomInterpolatorScalarsUsed;
				MaterialResource->GetUserInterpolatorUsage(UVScalarsUsed, CustomInterpolatorScalarsUsed);

				if (UVScalarsUsed > 0 || CustomInterpolatorScalarsUsed > 0)
				{
					uint32 TotalScalars = UVScalarsUsed + CustomInterpolatorScalarsUsed;
					uint32 MaxScalars = FMath::DivideAndRoundUp(TotalScalars, 4u) * 4;

					FString InterpolatorsString = FString::Printf(TEXT("User interpolators: %u/%u Scalars (%u/4 Vectors) (TexCoords: %i, Custom: %i)"),
						TotalScalars, MaxScalars, MaxScalars / 4, UVScalarsUsed, CustomInterpolatorScalarsUsed);

					TempMaterialInfoList.Add(MakeShareable(new FMaterialInfo(InterpolatorsString, FLinearColor::Yellow)));
					TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create( EMessageSeverity::Info );
					Line->AddToken(FTextToken::Create(FText::FromString(InterpolatorsString)));
					Messages.Add(Line);
				}
			}

			FString FeatureLevelName;
			GetFeatureLevelName(FeatureLevel,FeatureLevelName);
			for(int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
			{
				FString ErrorString = FString::Printf(TEXT("[%s] %s"), *FeatureLevelName, *CompileErrors[ErrorIndex]);
				TempMaterialInfoList.Add(MakeShareable(new FMaterialInfo(ErrorString, FLinearColor::Red)));
				TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create( EMessageSeverity::Error );
				Line->AddToken( FTextToken::Create( FText::FromString( ErrorString ) ) );
				Messages.Add(Line);
				bForceDisplay = true;
			}
		}
	}

	bool bNeedsRefresh = false;
	if (TempMaterialInfoList.Num() != MaterialInfoList.Num())
	{
		bNeedsRefresh = true;
	}

	for (int32 Index = 0; !bNeedsRefresh && Index < TempMaterialInfoList.Num(); ++Index)
	{
		if (TempMaterialInfoList[Index]->Color != MaterialInfoList[Index]->Color)
		{
			bNeedsRefresh = true;
			break;
		}

		if (TempMaterialInfoList[Index]->Text != MaterialInfoList[Index]->Text)
		{
			bNeedsRefresh = true;
			break;
		}
	}

	if (bNeedsRefresh)
	{
		MaterialInfoList = TempMaterialInfoList;
		/*TSharedPtr<SWidget> TitleBar = GraphEditor->GetTitleBar();
		if (TitleBar.IsValid())
		{
			StaticCastSharedPtr<SMaterialEditorTitleBar>(TitleBar)->RequestRefresh();
		}*/

		StatsListing->ClearMessages();
		StatsListing->AddMessages(Messages);

		if (bForceDisplay)
		{
			TabManager->InvokeTab(StatsTabId);
		}
	}
}

void FMaterialEditor::UpdateGraphNodeStates()
{
	const FMaterialResource* ErrorMaterialResource = PreviewExpression ? ExpressionPreviewMaterial->GetMaterialResource(GMaxRHIFeatureLevel) : Material->GetMaterialResource(GMaxRHIFeatureLevel);
	const FMaterialResource* ErrorMaterialResourceES2 = NULL;
	if (bShowMobileStats)
	{
		ErrorMaterialResourceES2 = PreviewExpression ? ExpressionPreviewMaterial->GetMaterialResource(ERHIFeatureLevel::ES2) : Material->GetMaterialResource(ERHIFeatureLevel::ES2);
	}

	bool bUpdatedErrorState = false;

	// Have to loop through everything here as there's no way to be notified when the material resource updates
	for (int32 Index = 0; Index < Material->MaterialGraph->Nodes.Num(); ++Index)
	{
		UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Material->MaterialGraph->Nodes[Index]);
		if (MaterialNode)
		{
			MaterialNode->bIsPreviewExpression = (PreviewExpression == MaterialNode->MaterialExpression);
			MaterialNode->bIsErrorExpression = (ErrorMaterialResource->GetErrorExpressions().Find(MaterialNode->MaterialExpression) != INDEX_NONE)
												|| (ErrorMaterialResourceES2 && ErrorMaterialResourceES2->GetErrorExpressions().Find(MaterialNode->MaterialExpression) != INDEX_NONE);

			if (MaterialNode->bIsErrorExpression && !MaterialNode->bHasCompilerMessage)
			{
				check(MaterialNode->MaterialExpression);

				bUpdatedErrorState = true;
				MaterialNode->bHasCompilerMessage = true;
				MaterialNode->ErrorMsg = MaterialNode->MaterialExpression->LastErrorText;
				MaterialNode->ErrorType = EMessageSeverity::Error;
			}
			else if (!MaterialNode->bIsErrorExpression && MaterialNode->bHasCompilerMessage)
			{
				bUpdatedErrorState = true;
				MaterialNode->bHasCompilerMessage = false;
			}
		}
	}

	if (bUpdatedErrorState)
	{
		// Rebuild the SGraphNodes to display/hide error block
		GraphEditor->NotifyGraphChanged();
	}
}

void FMaterialEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( EditorOptions );
	Collector.AddReferencedObject( Material );
	Collector.AddReferencedObject( OriginalMaterial );
	Collector.AddReferencedObject( MaterialFunction );
	Collector.AddReferencedObject( ExpressionPreviewMaterial );
	Collector.AddReferencedObject( EmptyMaterial );
}

void FMaterialEditor::BindCommands()
{
	const FMaterialEditorCommands& Commands = FMaterialEditorCommands::Get();
	
	ToolkitCommands->MapAction(
		Commands.Apply,
		FExecuteAction::CreateSP( this, &FMaterialEditor::OnApply ),
		FCanExecuteAction::CreateSP( this, &FMaterialEditor::OnApplyEnabled ) );

	ToolkitCommands->MapAction(
		FEditorViewportCommands::Get().ToggleRealTime,
		FExecuteAction::CreateSP( PreviewViewport.ToSharedRef(), &SMaterialEditor3DPreviewViewport::OnToggleRealtime ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( PreviewViewport.ToSharedRef(), &SMaterialEditor3DPreviewViewport::IsRealtime ) );

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP(this, &FMaterialEditor::UndoGraphAction));

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Redo,
		FExecuteAction::CreateSP(this, &FMaterialEditor::RedoGraphAction));

	ToolkitCommands->MapAction(
		Commands.CameraHome,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnCameraHome),
		FCanExecuteAction() );

	ToolkitCommands->MapAction(
		Commands.CleanUnusedExpressions,
		FExecuteAction::CreateSP(this, &FMaterialEditor::CleanUnusedExpressions),
		FCanExecuteAction() );

	ToolkitCommands->MapAction(
		Commands.ShowHideConnectors,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnShowConnectors),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsOnShowConnectorsChecked));

	ToolkitCommands->MapAction(
		Commands.ToggleLivePreview,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleLivePreview),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleLivePreviewChecked));

	ToolkitCommands->MapAction(
		Commands.ToggleRealtimeExpressions,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleRealTimeExpressions),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleRealTimeExpressionsChecked));

	ToolkitCommands->MapAction(
		Commands.AlwaysRefreshAllPreviews,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnAlwaysRefreshAllPreviews),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsOnAlwaysRefreshAllPreviews));

	ToolkitCommands->MapAction(
		Commands.ToggleMaterialStats,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleStatsChecked));

	ToolkitCommands->MapAction(
		Commands.ToggleMobileStats,
		FExecuteAction::CreateSP(this, &FMaterialEditor::ToggleMobileStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMaterialEditor::IsToggleMobileStatsChecked));

	ToolkitCommands->MapAction(
		Commands.UseCurrentTexture,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnUseCurrentTexture));

	ToolkitCommands->MapAction(
		Commands.ConvertObjects,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertObjects));

	ToolkitCommands->MapAction(
		Commands.ConvertToTextureObjects,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertTextures));

	ToolkitCommands->MapAction(
		Commands.ConvertToTextureSamples,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertTextures));

	ToolkitCommands->MapAction(
		Commands.ConvertToConstant,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertObjects));

	ToolkitCommands->MapAction(
		Commands.StopPreviewNode,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnPreviewNode));

	ToolkitCommands->MapAction(
		Commands.StartPreviewNode,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnPreviewNode));

	ToolkitCommands->MapAction(
		Commands.EnableRealtimePreviewNode,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnToggleRealtimePreview));

	ToolkitCommands->MapAction(
		Commands.DisableRealtimePreviewNode,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnToggleRealtimePreview));

	ToolkitCommands->MapAction(
		Commands.SelectDownstreamNodes,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnSelectDownstreamNodes));

	ToolkitCommands->MapAction(
		Commands.SelectUpstreamNodes,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnSelectUpstreamNodes));

	ToolkitCommands->MapAction(
		Commands.RemoveFromFavorites,
		FExecuteAction::CreateSP(this, &FMaterialEditor::RemoveSelectedExpressionFromFavorites));
		
	ToolkitCommands->MapAction(
		Commands.AddToFavorites,
		FExecuteAction::CreateSP(this, &FMaterialEditor::AddSelectedExpressionToFavorites));

	ToolkitCommands->MapAction(
		Commands.ForceRefreshPreviews,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnForceRefreshPreviews));

	ToolkitCommands->MapAction(
		Commands.FindInMaterial,
		FExecuteAction::CreateSP(this, &FMaterialEditor::OnFindInMaterial));
}

void FMaterialEditor::OnApply()
{
	UE_LOG(LogMaterialEditor, Log, TEXT("Applying material %s"), *GetEditingObjects()[0]->GetName());

	UpdateOriginalMaterial();
}

bool FMaterialEditor::OnApplyEnabled() const
{
	return bMaterialDirty == true;
}

void FMaterialEditor::OnCameraHome()
{
	RecenterEditor();
}

void FMaterialEditor::OnShowConnectors()
{
	bHideUnusedConnectors = !bHideUnusedConnectors;
	GraphEditor->SetPinVisibility(bHideUnusedConnectors ? SGraphEditor::Pin_HideNoConnection : SGraphEditor::Pin_Show);
}

bool FMaterialEditor::IsOnShowConnectorsChecked() const
{
	return bHideUnusedConnectors == false;
}

void FMaterialEditor::ToggleLivePreview()
{
	bLivePreview = !bLivePreview;
	if (bLivePreview)
	{
		UpdatePreviewMaterial();
		RegenerateCodeView();
	}
}

bool FMaterialEditor::IsToggleLivePreviewChecked() const
{
	return bLivePreview;
}

void FMaterialEditor::ToggleRealTimeExpressions()
{
	bIsRealtime = !bIsRealtime;
}

bool FMaterialEditor::IsToggleRealTimeExpressionsChecked() const
{
	return bIsRealtime == true;
}

void FMaterialEditor::OnAlwaysRefreshAllPreviews()
{
	bAlwaysRefreshAllPreviews = !bAlwaysRefreshAllPreviews;
	if ( bAlwaysRefreshAllPreviews )
	{
		RefreshExpressionPreviews();
	}
}

bool FMaterialEditor::IsOnAlwaysRefreshAllPreviews() const
{
	return bAlwaysRefreshAllPreviews == true;
}

void FMaterialEditor::ToggleStats()
{
	// Toggle the showing of material stats each time the user presses the show stats button
	bShowStats = !bShowStats;
	UpdateMaterialInfoList(bShowStats);
}

bool FMaterialEditor::IsToggleStatsChecked() const
{
	return bShowStats == true;
}

void FMaterialEditor::ToggleMobileStats()
{
	// Toggle the showing of material stats each time the user presses the show stats button
	bShowMobileStats = !bShowMobileStats;
	UPreviewMaterial* PreviewMaterial = Cast<UPreviewMaterial>(Material);
	if (PreviewMaterial)
	{
		{
			// Sync with the rendering thread but don't reregister components. We will manually do so.
			FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::SyncWithRenderingThread);
			UpdateContext.AddMaterial(PreviewMaterial);
			PreviewMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2,bShowMobileStats);
			PreviewMaterial->ForceRecompileForRendering();
			if (!bStatsFromPreviewMaterial)
			{
				OriginalMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2,bShowMobileStats);
				OriginalMaterial->ForceRecompileForRendering();
			}
		}
		UpdateStatsMaterials();
		RefreshPreviewViewport();
	}
	UpdateMaterialInfoList(bShowMobileStats);
}

bool FMaterialEditor::IsToggleMobileStatsChecked() const
{
	return bShowMobileStats == true;
}

void FMaterialEditor::OnUseCurrentTexture()
{
	// Set the currently selected texture in the generic browser
	// as the texture to use in all selected texture sample expressions.
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	UTexture* SelectedTexture = GEditor->GetSelectedObjects()->GetTop<UTexture>();
	if ( SelectedTexture )
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "UseCurrentTexture", "Use Current Texture") );
		const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode && GraphNode->MaterialExpression->IsA(UMaterialExpressionTextureBase::StaticClass()) )
			{
				UMaterialExpressionTextureBase* TextureBase = static_cast<UMaterialExpressionTextureBase*>(GraphNode->MaterialExpression);
				TextureBase->Modify();
				TextureBase->Texture = SelectedTexture;
				TextureBase->AutoSetSampleType();
			}
		}

		// Update the current preview material. 
		UpdatePreviewMaterial();
		Material->MarkPackageDirty();
		RegenerateCodeView();
		RefreshExpressionPreviews();
		SetMaterialDirty();
	}
}

void FMaterialEditor::OnConvertObjects()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		const FScopedTransaction Transaction( LOCTEXT("MaterialEditorConvert", "Material Editor: Convert") );
		Material->Modify();
		Material->MaterialGraph->Modify();
		TArray<class UEdGraphNode*> NodesToDelete;
		TArray<class UEdGraphNode*> NodesToSelect;

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode)
			{
				// Look for the supported classes to convert from
				UMaterialExpression* CurrentSelectedExpression = GraphNode->MaterialExpression;
				UMaterialExpressionConstant* Constant1Expression = Cast<UMaterialExpressionConstant>(CurrentSelectedExpression);
				UMaterialExpressionConstant2Vector* Constant2Expression = Cast<UMaterialExpressionConstant2Vector>(CurrentSelectedExpression);
				UMaterialExpressionConstant3Vector* Constant3Expression = Cast<UMaterialExpressionConstant3Vector>(CurrentSelectedExpression);
				UMaterialExpressionConstant4Vector* Constant4Expression = Cast<UMaterialExpressionConstant4Vector>(CurrentSelectedExpression);
				UMaterialExpressionTextureSample* TextureSampleExpression = Cast<UMaterialExpressionTextureSample>(CurrentSelectedExpression);
				UMaterialExpressionComponentMask* ComponentMaskExpression = Cast<UMaterialExpressionComponentMask>(CurrentSelectedExpression);
				UMaterialExpressionParticleSubUV* ParticleSubUVExpression = Cast<UMaterialExpressionParticleSubUV>(CurrentSelectedExpression);
				UMaterialExpressionScalarParameter* ScalarParameterExpression = Cast<UMaterialExpressionScalarParameter>(CurrentSelectedExpression);
				UMaterialExpressionVectorParameter* VectorParameterExpression = Cast<UMaterialExpressionVectorParameter>(CurrentSelectedExpression);

				// Setup the class to convert to
				UClass* ClassToCreate = NULL;
				if (Constant1Expression)
				{
					ClassToCreate = UMaterialExpressionScalarParameter::StaticClass();
				}
				else if (Constant2Expression || Constant3Expression || Constant4Expression)
				{
					ClassToCreate = UMaterialExpressionVectorParameter::StaticClass();
				}
				else if (ParticleSubUVExpression) // Has to come before the TextureSample comparison...
				{
					ClassToCreate = UMaterialExpressionTextureSampleParameterSubUV::StaticClass();
				}
				else if (TextureSampleExpression && TextureSampleExpression->Texture && TextureSampleExpression->Texture->IsA(UTextureCube::StaticClass()))
				{
					ClassToCreate = UMaterialExpressionTextureSampleParameterCube::StaticClass();
				}	
				else if (TextureSampleExpression)
				{
					ClassToCreate = UMaterialExpressionTextureSampleParameter2D::StaticClass();
				}	
				else if (ComponentMaskExpression)
				{
					ClassToCreate = UMaterialExpressionStaticComponentMaskParameter::StaticClass();
				}
				else if (ScalarParameterExpression)
				{
					ClassToCreate = UMaterialExpressionConstant::StaticClass();
				}
				else if (VectorParameterExpression)
				{
					// Technically should be a constant 4 but UMaterialExpressionVectorParameter has an rgb pin, so using Constant3 to avoid a compile error
					ClassToCreate = UMaterialExpressionConstant3Vector::StaticClass();
				}

				if (ClassToCreate)
				{
					UMaterialExpression* NewExpression = CreateNewMaterialExpression(ClassToCreate, FVector2D(GraphNode->NodePosX, GraphNode->NodePosY), false, true );
					if (NewExpression)
					{
						UMaterialGraphNode* NewGraphNode = CastChecked<UMaterialGraphNode>(NewExpression->GraphNode);
						NewGraphNode->ReplaceNode(GraphNode);

						bool bNeedsRefresh = false;

						// Copy over any common values
						if (GraphNode->NodeComment.Len() > 0)
						{
							bNeedsRefresh = true; 
							NewGraphNode->NodeComment = GraphNode->NodeComment;
						}

						// Copy over expression-specific values
						if (Constant1Expression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionScalarParameter>(NewExpression)->DefaultValue = Constant1Expression->R;
						}
						else if (Constant2Expression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = FLinearColor(Constant2Expression->R, Constant2Expression->G, 0);
						}
						else if (Constant3Expression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = Constant3Expression->Constant;
							CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue.A = 1.0f;
						}
						else if (Constant4Expression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionVectorParameter>(NewExpression)->DefaultValue = Constant4Expression->Constant;
						}
						else if (TextureSampleExpression)
						{
							bNeedsRefresh = true;
							UMaterialExpressionTextureSampleParameter* NewTextureExpr = CastChecked<UMaterialExpressionTextureSampleParameter>(NewExpression);
							NewTextureExpr->Texture = TextureSampleExpression->Texture;
							NewTextureExpr->Coordinates = TextureSampleExpression->Coordinates;
							NewTextureExpr->AutoSetSampleType();
							NewTextureExpr->IsDefaultMeshpaintTexture = TextureSampleExpression->IsDefaultMeshpaintTexture;
							NewTextureExpr->TextureObject = TextureSampleExpression->TextureObject;
							NewTextureExpr->MipValue = TextureSampleExpression->MipValue;
							NewTextureExpr->CoordinatesDX = TextureSampleExpression->CoordinatesDX;
							NewTextureExpr->CoordinatesDY = TextureSampleExpression->CoordinatesDY;
							NewTextureExpr->MipValueMode = TextureSampleExpression->MipValueMode;
							NewGraphNode->ReconstructNode();
						}
						else if (ComponentMaskExpression)
						{
							bNeedsRefresh = true;
							UMaterialExpressionStaticComponentMaskParameter* ComponentMask = CastChecked<UMaterialExpressionStaticComponentMaskParameter>(NewExpression);
							ComponentMask->DefaultR = ComponentMaskExpression->R;
							ComponentMask->DefaultG = ComponentMaskExpression->G;
							ComponentMask->DefaultB = ComponentMaskExpression->B;
							ComponentMask->DefaultA = ComponentMaskExpression->A;
						}
						else if (ParticleSubUVExpression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionTextureSampleParameterSubUV>(NewExpression)->Texture = ParticleSubUVExpression->Texture;
						}
						else if (ScalarParameterExpression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionConstant>(NewExpression)->R = ScalarParameterExpression->DefaultValue;
						}
						else if (VectorParameterExpression)
						{
							bNeedsRefresh = true;
							CastChecked<UMaterialExpressionConstant3Vector>(NewExpression)->Constant = VectorParameterExpression->DefaultValue;
						}

						if (bNeedsRefresh)
						{
							// Refresh the expression preview if we changed its properties after it was created
							NewExpression->bNeedToUpdatePreview = true;
							RefreshExpressionPreview( NewExpression, true );
						}

						NodesToDelete.AddUnique(GraphNode);
						NodesToSelect.Add(NewGraphNode);
					}
				}
			}
		}

		// Delete the replaced nodes
		DeleteNodes(NodesToDelete);

		// Select each of the newly converted expressions
		for ( TArray<UEdGraphNode*>::TConstIterator NodeIter(NodesToSelect); NodeIter; ++NodeIter )
		{
			GraphEditor->SetNodeSelection(*NodeIter, true);
		}
	}
}

void FMaterialEditor::OnConvertTextures()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	if (SelectedNodes.Num() > 0)
	{
		const FScopedTransaction Transaction( LOCTEXT("MaterialEditorConvertTexture", "Material Editor: Convert to Texture") );
		Material->Modify();
		Material->MaterialGraph->Modify();
		TArray<class UEdGraphNode*> NodesToDelete;
		TArray<class UEdGraphNode*> NodesToSelect;

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode)
			{
				// Look for the supported classes to convert from
				UMaterialExpression* CurrentSelectedExpression = GraphNode->MaterialExpression;
				UMaterialExpressionTextureSample* TextureSampleExpression = Cast<UMaterialExpressionTextureSample>(CurrentSelectedExpression);
				UMaterialExpressionTextureObject* TextureObjectExpression = Cast<UMaterialExpressionTextureObject>(CurrentSelectedExpression);

				// Setup the class to convert to
				UClass* ClassToCreate = NULL;
				if (TextureSampleExpression)
				{
					ClassToCreate = UMaterialExpressionTextureObject::StaticClass();
				}
				else if (TextureObjectExpression)
				{
					ClassToCreate = UMaterialExpressionTextureSample::StaticClass();
				}

				if (ClassToCreate)
				{
					UMaterialExpression* NewExpression = CreateNewMaterialExpression(ClassToCreate, FVector2D(GraphNode->NodePosX, GraphNode->NodePosY), false, true);
					if (NewExpression)
					{
						UMaterialGraphNode* NewGraphNode = CastChecked<UMaterialGraphNode>(NewExpression->GraphNode);
						NewGraphNode->ReplaceNode(GraphNode);
						bool bNeedsRefresh = false;

						// Copy over expression-specific values
						if (TextureSampleExpression)
						{
							bNeedsRefresh = true;
							UMaterialExpressionTextureObject* NewTextureExpr = CastChecked<UMaterialExpressionTextureObject>(NewExpression);
							NewTextureExpr->Texture = TextureSampleExpression->Texture;
							NewTextureExpr->AutoSetSampleType();
							NewTextureExpr->IsDefaultMeshpaintTexture = TextureSampleExpression->IsDefaultMeshpaintTexture;
						}
						else if (TextureObjectExpression)
						{
							bNeedsRefresh = true;
							UMaterialExpressionTextureSample* NewTextureExpr = CastChecked<UMaterialExpressionTextureSample>(NewExpression);
							NewTextureExpr->Texture = TextureObjectExpression->Texture;
							NewTextureExpr->AutoSetSampleType();
							NewTextureExpr->IsDefaultMeshpaintTexture = TextureObjectExpression->IsDefaultMeshpaintTexture;
							NewTextureExpr->MipValueMode = TMVM_None;
						}

						if (bNeedsRefresh)
						{
							// Refresh the expression preview if we changed its properties after it was created
							NewExpression->bNeedToUpdatePreview = true;
							RefreshExpressionPreview( NewExpression, true );
						}

						NodesToDelete.AddUnique(GraphNode);
						NodesToSelect.Add(NewGraphNode);
					}
				}
			}
		}

		// Delete the replaced nodes
		DeleteNodes(NodesToDelete);

		// Select each of the newly converted expressions
		for ( TArray<UEdGraphNode*>::TConstIterator NodeIter(NodesToSelect); NodeIter; ++NodeIter )
		{
			GraphEditor->SetNodeSelection(*NodeIter, true);
		}
	}
}

void FMaterialEditor::OnPreviewNode()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode)
			{
				GraphEditor->NotifyGraphChanged();
				SetPreviewExpression(GraphNode->MaterialExpression);
			}
		}
	}
}

void FMaterialEditor::OnToggleRealtimePreview()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
			if (GraphNode)
			{
				UMaterialExpression* SelectedExpression = GraphNode->MaterialExpression;
				SelectedExpression->bRealtimePreview = !SelectedExpression->bRealtimePreview;

				if (SelectedExpression->bRealtimePreview)
				{
					SelectedExpression->bCollapsed = false;
				}

				RefreshExpressionPreviews();
				SetMaterialDirty();
			}
		}
	}
}

void FMaterialEditor::OnSelectDownstreamNodes()
{
	TArray<UMaterialGraphNode*> NodesToCheck;
	TArray<UMaterialGraphNode*> CheckedNodes;
	TArray<UMaterialGraphNode*> NodesToSelect;

	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
		if (GraphNode)
		{
			NodesToCheck.Add(GraphNode);
		}
	}

	while (NodesToCheck.Num() > 0)
	{
		UMaterialGraphNode* CurrentNode = NodesToCheck.Last();
		TArray<UEdGraphPin*> OutputPins;
		CurrentNode->GetOutputPins(OutputPins);

		for (int32 Index = 0; Index < OutputPins.Num(); ++Index)
		{
			for (int32 LinkIndex = 0; LinkIndex < OutputPins[Index]->LinkedTo.Num(); ++LinkIndex)
			{
				UMaterialGraphNode* LinkedNode = Cast<UMaterialGraphNode>(OutputPins[Index]->LinkedTo[LinkIndex]->GetOwningNode());
				if (LinkedNode)
				{
					int32 FoundIndex = -1;
					CheckedNodes.Find(LinkedNode, FoundIndex);

					if (FoundIndex < 0)
					{
						NodesToSelect.Add(LinkedNode);
						NodesToCheck.Add(LinkedNode);
					}
				}
			}
		}

		// This graph node has now been examined
		CheckedNodes.Add(CurrentNode);
		NodesToCheck.Remove(CurrentNode);
	}

	for (int32 Index = 0; Index < NodesToSelect.Num(); ++Index)
	{
		GraphEditor->SetNodeSelection(NodesToSelect[Index], true);
	}
}

void FMaterialEditor::OnSelectUpstreamNodes()
{
	TArray<UMaterialGraphNode*> NodesToCheck;
	TArray<UMaterialGraphNode*> CheckedNodes;
	TArray<UMaterialGraphNode*> NodesToSelect;

	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt);
		if (GraphNode)
		{
			NodesToCheck.Add(GraphNode);
		}
	}

	while (NodesToCheck.Num() > 0)
	{
		UMaterialGraphNode* CurrentNode = NodesToCheck.Last();
		TArray<UEdGraphPin*> InputPins;
		CurrentNode->GetInputPins(InputPins);

		for (int32 Index = 0; Index < InputPins.Num(); ++Index)
		{
			for (int32 LinkIndex = 0; LinkIndex < InputPins[Index]->LinkedTo.Num(); ++LinkIndex)
			{
				UMaterialGraphNode* LinkedNode = Cast<UMaterialGraphNode>(InputPins[Index]->LinkedTo[LinkIndex]->GetOwningNode());
				if (LinkedNode)
				{
					int32 FoundIndex = -1;
					CheckedNodes.Find(LinkedNode, FoundIndex);

					if (FoundIndex < 0)
					{
						NodesToSelect.Add(LinkedNode);
						NodesToCheck.Add(LinkedNode);
					}
				}
			}
		}

		// This graph node has now been examined
		CheckedNodes.Add(CurrentNode);
		NodesToCheck.Remove(CurrentNode);
	}

	for (int32 Index = 0; Index < NodesToSelect.Num(); ++Index)
	{
		GraphEditor->SetNodeSelection(NodesToSelect[Index], true);
	}
}

void FMaterialEditor::OnForceRefreshPreviews()
{
	ForceRefreshExpressionPreviews();
	RefreshPreviewViewport();
}

void FMaterialEditor::OnCreateComment()
{
	CreateNewMaterialExpressionComment(GraphEditor->GetPasteLocation());
}

void FMaterialEditor::OnCreateComponentMaskNode()
{
	CreateNewMaterialExpression(UMaterialExpressionComponentMask::StaticClass(), GraphEditor->GetPasteLocation(), true, false);
}

void FMaterialEditor::OnFindInMaterial()
{
	TabManager->InvokeTab(FindTabId);
	FindResults->FocusForUse();
}

UClass* FMaterialEditor::GetOnPromoteToParameterClass(UEdGraphPin* TargetPin)
{
	UMaterialGraphNode_Root* RootPinNode = Cast<UMaterialGraphNode_Root>(TargetPin->GetOwningNode());
	UMaterialGraphNode* OtherPinNode = Cast<UMaterialGraphNode>(TargetPin->GetOwningNode());

	if (RootPinNode != nullptr)
	{
		EMaterialProperty propertyId = (EMaterialProperty)FCString::Atoi(*TargetPin->PinType.PinSubCategory);

		switch (propertyId)
		{
			case MP_Opacity:
			case MP_Metallic:
			case MP_Specular:
			case MP_Roughness:
			case MP_TessellationMultiplier:
			case MP_CustomData0:
			case MP_CustomData1:
			case MP_AmbientOcclusion:
			case MP_Refraction:
			case MP_PixelDepthOffset:
			case MP_OpacityMask: return UMaterialExpressionScalarParameter::StaticClass();

			case MP_WorldPositionOffset:
			case MP_WorldDisplacement:
			case MP_EmissiveColor:
			case MP_BaseColor:
			case MP_SubsurfaceColor:
			case MP_SpecularColor:
			case MP_Normal:	return UMaterialExpressionVectorParameter::StaticClass();
		}
	}
	else if (OtherPinNode)
	{
		const TArray<FExpressionInput*> ExpressionInputs = OtherPinNode->MaterialExpression->GetInputs();
		FString TargetPinName = OtherPinNode->GetShortenPinName(TargetPin->PinName);

		for (int32 Index = 0; Index < ExpressionInputs.Num(); ++Index)
		{
			FExpressionInput* Input = ExpressionInputs[Index];
			FString InputName = OtherPinNode->MaterialExpression->GetInputName(Index);
			InputName = OtherPinNode->GetShortenPinName(InputName);

			if (InputName == TargetPinName)
			{
				switch (OtherPinNode->MaterialExpression->GetInputType(Index))
				{
					case MCT_Float1:
					case MCT_Float: return UMaterialExpressionScalarParameter::StaticClass();

					case MCT_Float2:
					case MCT_Float3:
					case MCT_Float4: return UMaterialExpressionVectorParameter::StaticClass();
					
					case MCT_StaticBool: return UMaterialExpressionStaticBoolParameter::StaticClass();

					case MCT_Texture2D:
					case MCT_TextureCube: 
					case MCT_Texture: return UMaterialExpressionTextureObjectParameter::StaticClass();
				}

				break;
			}
		}
	}

	return nullptr;
}

void FMaterialEditor::OnPromoteToParameter()
{
	UEdGraphPin* TargetPin = GraphEditor->GetGraphPinForMenu();
	UMaterialGraphNode_Base* PinNode = Cast<UMaterialGraphNode_Base>(TargetPin->GetOwningNode());

	FMaterialGraphSchemaAction_NewNode Action;	
	Action.MaterialExpressionClass = GetOnPromoteToParameterClass(TargetPin);

	if (Action.MaterialExpressionClass != nullptr)
	{
		check(PinNode);
		UEdGraph* GraphObj = PinNode->GetGraph();
		check(GraphObj);

		const FScopedTransaction Transaction(LOCTEXT("PromoteToParameter", "Promote To Parameter"));
		GraphObj->Modify();

		// Set position of new node to be close to node we clicked on
		FVector2D NewNodePos;
		NewNodePos.X = PinNode->NodePosX - 100;
		NewNodePos.Y = PinNode->NodePosY;

		UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Action.PerformAction(GraphObj, TargetPin, NewNodePos));

		if (MaterialNode->MaterialExpression->HasAParameterName())
		{
			MaterialNode->MaterialExpression->SetParameterName(FName(*TargetPin->PinName));
			MaterialNode->MaterialExpression->ValidateParameterName();
		}
	}
}

bool FMaterialEditor::OnCanPromoteToParameter()
{
	UEdGraphPin* TargetPin = GraphEditor->GetGraphPinForMenu();

	if (ensure(TargetPin) && TargetPin->LinkedTo.Num() == 0)
	{
		return GetOnPromoteToParameterClass(TargetPin) != nullptr;
	}

	return false;
}

FString FMaterialEditor::GetDocLinkForSelectedNode()
{
	FString DocumentationLink;

	TArray<UObject*> SelectedNodes = GraphEditor->GetSelectedNodes().Array();
	if (SelectedNodes.Num() == 1)
	{
		UMaterialGraphNode* SelectedGraphNode = Cast<UMaterialGraphNode>(SelectedNodes[0]);
		if (SelectedGraphNode != NULL)
		{
			FString DocLink = SelectedGraphNode->GetDocumentationLink();
			FString DocExcerpt = SelectedGraphNode->GetDocumentationExcerptName();

			DocumentationLink = FEditorClassUtils::GetDocumentationLinkFromExcerpt(DocLink, DocExcerpt);
		}
	}

	return DocumentationLink;
}

void FMaterialEditor::OnGoToDocumentation()
{
	FString DocumentationLink = GetDocLinkForSelectedNode();
	if (!DocumentationLink.IsEmpty())
	{
		IDocumentation::Get()->Open(DocumentationLink, FDocumentationSourceInfo(TEXT("rightclick_matnode")));
	}
}

bool FMaterialEditor::CanGoToDocumentation()
{
	FString DocumentationLink = GetDocLinkForSelectedNode();
	return !DocumentationLink.IsEmpty();
}

void FMaterialEditor::RenameAssetFromRegistry(const FAssetData& InAddedAssetData, const FString& InNewName)
{
	// Grab the asset class, it will be checked for being a material function.
	UClass* Asset = FindObject<UClass>(ANY_PACKAGE, *InAddedAssetData.AssetClass.ToString());

	if(Asset->IsChildOf(UMaterialFunction::StaticClass()))
	{
		ForceRefreshExpressionPreviews();
	}
}

void FMaterialEditor::OnMaterialUsageFlagsChanged(UMaterial* MaterialThatChanged, int32 FlagThatChanged)
{
	EMaterialUsage Flag = static_cast<EMaterialUsage>(FlagThatChanged);
	if(MaterialThatChanged == OriginalMaterial)
	{
		bool bNeedsRecompile = false;
		Material->SetMaterialUsage(bNeedsRecompile, Flag);
		UpdateStatsMaterials();
	}
}

void FMaterialEditor::SetVectorParameterDefaultOnDependentMaterials(FName ParameterName, const FLinearColor& Value, bool bOverride)
{
	TArray<UMaterial*> MaterialsToOverride;

	if (MaterialFunction)
	{
		// Find all materials that reference this function
		for (TObjectIterator<UMaterial> It; It; ++It)
		{
			UMaterial* CurrentMaterial = *It;

			if (CurrentMaterial != Material)
			{
				bool bUpdate = false;

				for (int32 FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialFunctionInfos.Num(); FunctionIndex++)
				{
					if (CurrentMaterial->MaterialFunctionInfos[FunctionIndex].Function == MaterialFunction->ParentFunction)
					{
						bUpdate = true;
						break;
					}
				}

				if (bUpdate)
				{
					MaterialsToOverride.Add(CurrentMaterial);
				}
			}
		}
	}
	else
	{
		MaterialsToOverride.Add(OriginalMaterial);
	}

	const ERHIFeatureLevel::Type FeatureLevel = GEditor->GetEditorWorldContext().World()->FeatureLevel;

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialsToOverride.Num(); MaterialIndex++)
	{
		UMaterial* CurrentMaterial = MaterialsToOverride[MaterialIndex];

		CurrentMaterial->OverrideVectorParameterDefault(ParameterName, Value, bOverride, FeatureLevel);
	}

	// Update MI's that reference any of the materials affected
	for (TObjectIterator<UMaterialInstance> It; It; ++It)
	{
		UMaterialInstance* CurrentMaterialInstance = *It;

		// Only care about MI's with static parameters, because we are overriding parameter defaults, 
		// And only MI's with static parameters contain uniform expressions, which contain parameter defaults
		if (CurrentMaterialInstance->bHasStaticPermutationResource)
		{
			UMaterial* BaseMaterial = CurrentMaterialInstance->GetMaterial();

			if (MaterialsToOverride.Contains(BaseMaterial))
			{
				CurrentMaterialInstance->OverrideVectorParameterDefault(ParameterName, Value, bOverride, FeatureLevel);
			}
		}
	}
}

void FMaterialEditor::OnVectorParameterDefaultChanged(class UMaterialExpression* Expression, FName ParameterName, const FLinearColor& Value)
{
	check(Expression);

	if (Expression->Material == Material && OriginalMaterial)
	{
		SetVectorParameterDefaultOnDependentMaterials(ParameterName, Value, true);

		OverriddenVectorParametersToRevert.AddUnique(ParameterName);
	}
}

void FMaterialEditor::SetScalarParameterDefaultOnDependentMaterials(FName ParameterName, float Value, bool bOverride)
{
	TArray<UMaterial*> MaterialsToOverride;

	if (MaterialFunction)
	{
		// Find all materials that reference this function
		for (TObjectIterator<UMaterial> It; It; ++It)
		{
			UMaterial* CurrentMaterial = *It;

			if (CurrentMaterial != Material)
			{
				bool bUpdate = false;

				for (int32 FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialFunctionInfos.Num(); FunctionIndex++)
				{
					if (CurrentMaterial->MaterialFunctionInfos[FunctionIndex].Function == MaterialFunction->ParentFunction)
					{
						bUpdate = true;
						break;
					}
				}

				if (bUpdate)
				{
					MaterialsToOverride.Add(CurrentMaterial);
				}
			}
		}
	}
	else
	{
		MaterialsToOverride.Add(OriginalMaterial);
	}

	const ERHIFeatureLevel::Type FeatureLevel = GEditor->GetEditorWorldContext().World()->FeatureLevel;

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialsToOverride.Num(); MaterialIndex++)
	{
		UMaterial* CurrentMaterial = MaterialsToOverride[MaterialIndex];

		CurrentMaterial->OverrideScalarParameterDefault(ParameterName, Value, bOverride, FeatureLevel);
	}

	// Update MI's that reference any of the materials affected
	for (TObjectIterator<UMaterialInstance> It; It; ++It)
	{
		UMaterialInstance* CurrentMaterialInstance = *It;

		// Only care about MI's with static parameters, because we are overriding parameter defaults, 
		// And only MI's with static parameters contain uniform expressions, which contain parameter defaults
		if (CurrentMaterialInstance->bHasStaticPermutationResource)
		{
			UMaterial* BaseMaterial = CurrentMaterialInstance->GetMaterial();

			if (MaterialsToOverride.Contains(BaseMaterial))
			{
				CurrentMaterialInstance->OverrideScalarParameterDefault(ParameterName, Value, bOverride, FeatureLevel);
			}
		}
	}
}

void FMaterialEditor::OnScalarParameterDefaultChanged(class UMaterialExpression* Expression, FName ParameterName, float Value)
{
	check(Expression);

	if (Expression->Material == Material && OriginalMaterial)
	{
		SetScalarParameterDefaultOnDependentMaterials(ParameterName, Value, true);

		OverriddenScalarParametersToRevert.AddUnique(ParameterName);
	}
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabTitle", "Viewport"))
		[
			SNew( SOverlay )
			+ SOverlay::Slot()
			[
				PreviewViewport.ToSharedRef()
			]
			+ SOverlay::Slot()
			[
				PreviewUIViewport.ToSharedRef()
			]
		];

	PreviewViewport->OnAddedToTab( SpawnedTab );

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_GraphCanvas(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("GraphCanvasTitle", "Graph"));

	if (GraphEditor.IsValid())
	{
		SpawnedTab->SetContent(GraphEditor.ToSharedRef());
	}

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_MaterialProperties(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("LevelEditor.Tabs.Details") )
		.Label( LOCTEXT("MaterialDetailsTitle", "Details") )
		[
			MaterialDetailsView.ToSharedRef()
		];

	if (GraphEditor.IsValid())
	{
		// Since we're initialising, make sure nothing is selected
		GraphEditor->ClearSelectionSet();
	}

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_HLSLCode(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("HLSLCodeTitle", "HLSL Code"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				CodeViewUtility.ToSharedRef()
			]
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				CodeView.ToSharedRef()
			]
		];

	CodeTab = SpawnedTab;

	RegenerateCodeView();

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_Palette(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == PaletteTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("Kismet.Tabs.Palette"))
		.Label(LOCTEXT("MaterialPaletteTitle", "Palette"))
		[
			SNew( SBox )
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("MaterialPalette")))
			[
				Palette.ToSharedRef()
			]
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_Stats(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == StatsTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("Kismet.Tabs.CompilerResults"))
		.Label(LOCTEXT("MaterialStatsTitle", "Stats"))
		[
			SNew( SBox )
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("MaterialStats")))
			[
				Stats.ToSharedRef()
			]
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_Find(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == FindTabId);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("Kismet.Tabs.FindResults"))
		.Label(LOCTEXT("MaterialFindTitle", "Find Results"))
		[
			SNew(SBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("MaterialFind")))
			[
				FindResults.ToSharedRef()
			]
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FMaterialEditor::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSettingsTabId);

	TSharedRef<SWidget> InWidget = SNullWidget::NullWidget;
	if (PreviewViewport.IsValid())
	{
		FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");
		InWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(PreviewViewport->GetPreviewScene());
	}

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		[
			SNew(SBox)
			[
				InWidget
			]
		];

	return SpawnedTab;
}

void FMaterialEditor::SetPreviewExpression(UMaterialExpression* NewPreviewExpression)
{
	UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(NewPreviewExpression);

	if (!NewPreviewExpression || PreviewExpression == NewPreviewExpression)
	{
		if (FunctionOutput)
		{
			FunctionOutput->bLastPreviewed = false;
		}
		// If we are already previewing the selected expression toggle previewing off
		PreviewExpression = NULL;
		ExpressionPreviewMaterial->Expressions.Empty();
		SetPreviewMaterial( Material );
		// Recompile the preview material to get changes that might have been made during previewing
		UpdatePreviewMaterial();
	}
	else
	{
		if( ExpressionPreviewMaterial == NULL )
		{
			// Create the expression preview material if it hasnt already been created
			ExpressionPreviewMaterial = NewObject<UPreviewMaterial>(GetTransientPackage(), NAME_None, RF_Public);
			ExpressionPreviewMaterial->bIsPreviewMaterial = true;
			if (Material->IsUIMaterial())
			{
				ExpressionPreviewMaterial->MaterialDomain = MD_UI;
			}
		}

		if (FunctionOutput)
		{
			FunctionOutput->bLastPreviewed = true;
		}
		else
		{
			//Hooking up the output of the break expression doesn't make much sense, preview the expression feeding it instead.
			UMaterialExpressionBreakMaterialAttributes* BreakExpr = Cast<UMaterialExpressionBreakMaterialAttributes>(NewPreviewExpression);
			if( BreakExpr && BreakExpr->GetInput(0) && BreakExpr->GetInput(0)->Expression )
			{
				NewPreviewExpression = BreakExpr->GetInput(0)->Expression;
			}
		}

		// The expression preview material's expressions array must stay up to date before recompiling 
		// So that RebuildMaterialFunctionInfo will see all the nested material functions that may need to be updated
		ExpressionPreviewMaterial->Expressions = Material->Expressions;

		// The preview window should now show the expression preview material
		SetPreviewMaterial( ExpressionPreviewMaterial );

		// Set the preview expression
		PreviewExpression = NewPreviewExpression;

		// Recompile the preview material
		UpdatePreviewMaterial();
	}
}

void FMaterialEditor::JumpToNode(const UEdGraphNode* Node)
{
	GraphEditor->JumpToNode(Node, false);
}

UMaterialExpression* FMaterialEditor::CreateNewMaterialExpression(UClass* NewExpressionClass, const FVector2D& NodePos, bool bAutoSelect, bool bAutoAssignResource)
{
	check( NewExpressionClass->IsChildOf(UMaterialExpression::StaticClass()) );

	if (!IsAllowedExpressionType(NewExpressionClass, MaterialFunction != NULL))
	{
		// Disallowed types should not be visible to the ui to be placed, so we don't need a warning here
		return NULL;
	}

	// Clear the selection
	if ( bAutoSelect )
	{
		GraphEditor->ClearSelectionSet();
	}

	// Create the new expression.
	UMaterialExpression* NewExpression = NULL;
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorNewExpression", "Material Editor: New Expression") );
		Material->Modify();

		UObject* SelectedAsset = nullptr;
		if (bAutoAssignResource)
		{
			SelectedAsset = GEditor->GetSelectedObjects()->GetTop<UObject>();
		}

		NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx(Material, MaterialFunction, NewExpressionClass, SelectedAsset, NodePos.X, NodePos.Y);

		if (NewExpression)
		{
			Material->MaterialGraph->AddExpression(NewExpression);

			// Select the new node.
			if ( bAutoSelect )
			{
				GraphEditor->SetNodeSelection(NewExpression->GraphNode, true);
			}
		}
	}

	RegenerateCodeView();

	// Update the current preview material.
	UpdatePreviewMaterial();
	Material->MarkPackageDirty();

	RefreshExpressionPreviews();
	GraphEditor->NotifyGraphChanged();
	SetMaterialDirty();
	return NewExpression;
}

UMaterialExpressionComment* FMaterialEditor::CreateNewMaterialExpressionComment(const FVector2D& NodePos)
{
	UMaterialExpressionComment* NewComment = NULL;
	{
		Material->Modify();

		UObject* ExpressionOuter = Material;
		if (MaterialFunction)
		{
			ExpressionOuter = MaterialFunction;
		}

		NewComment = NewObject<UMaterialExpressionComment>(ExpressionOuter, NAME_None, RF_Transactional);

		// Add to the list of comments associated with this material.
		Material->EditorComments.Add( NewComment );

		FSlateRect Bounds;
		if (GraphEditor->GetBoundsForSelectedNodes(Bounds, 50.0f))
		{
			NewComment->MaterialExpressionEditorX = Bounds.Left;
			NewComment->MaterialExpressionEditorY = Bounds.Top;

			FVector2D Size = Bounds.GetSize();
			NewComment->SizeX = Size.X;
			NewComment->SizeY = Size.Y;
		}
		else
		{

			NewComment->MaterialExpressionEditorX = NodePos.X;
			NewComment->MaterialExpressionEditorY = NodePos.Y;
			NewComment->SizeX = 400;
			NewComment->SizeY = 100;
		}

		NewComment->Text = NSLOCTEXT("K2Node", "CommentBlock_NewEmptyComment", "Comment").ToString();
	}

	if (NewComment)
	{
		Material->MaterialGraph->AddComment(NewComment, true);

		// Select the new comment.
		GraphEditor->ClearSelectionSet();
		GraphEditor->SetNodeSelection(NewComment->GraphNode, true);
	}

	Material->MarkPackageDirty();
	GraphEditor->NotifyGraphChanged();
	SetMaterialDirty();
	return NewComment;
}

void FMaterialEditor::ForceRefreshExpressionPreviews()
{
	// Initialize expression previews.
	const bool bOldAlwaysRefreshAllPreviews = bAlwaysRefreshAllPreviews;
	bAlwaysRefreshAllPreviews = true;
	RefreshExpressionPreviews();
	bAlwaysRefreshAllPreviews = bOldAlwaysRefreshAllPreviews;
}

void FMaterialEditor::AddToSelection(UMaterialExpression* Expression)
{
	GraphEditor->SetNodeSelection(Expression->GraphNode, true);
}

void FMaterialEditor::SelectAllNodes()
{
	GraphEditor->SelectAllNodes();
}

bool FMaterialEditor::CanSelectAllNodes() const
{
	return GraphEditor.IsValid();
}

void FMaterialEditor::DeleteSelectedNodes()
{
	TArray<UEdGraphNode*> NodesToDelete;
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		NodesToDelete.Add(CastChecked<UEdGraphNode>(*NodeIt));
	}

	DeleteNodes(NodesToDelete);
}

void FMaterialEditor::DeleteNodes(const TArray<UEdGraphNode*>& NodesToDelete)
{
	if (NodesToDelete.Num() > 0)
	{
		if (!CheckExpressionRemovalWarnings(NodesToDelete))
		{
			return;
		}

		// If we are previewing an expression and the expression being previewed was deleted
		bool bHaveExpressionsToDelete			= false;
		bool bPreviewExpressionDeleted			= false;

		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorDelete", "Material Editor: Delete") );
			Material->Modify();

			for (int32 Index = 0; Index < NodesToDelete.Num(); ++Index)
			{
				if (NodesToDelete[Index]->CanUserDeleteNode())
				{
					// Break all node links first so that we don't update the material before deleting
					NodesToDelete[Index]->BreakAllNodeLinks();

					FBlueprintEditorUtils::RemoveNode(NULL, NodesToDelete[Index], true);

					if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(NodesToDelete[Index]))
					{
						UMaterialExpression* MaterialExpression = GraphNode->MaterialExpression;

						bHaveExpressionsToDelete = true;

						DestroyColorPicker();

						if( PreviewExpression == MaterialExpression )
						{
							// The expression being previewed is also being deleted
							bPreviewExpressionDeleted = true;
						}

						MaterialExpression->Modify();
						Material->Expressions.Remove( MaterialExpression );
						Material->RemoveExpressionParameter(MaterialExpression);
						// Make sure the deleted expression is caught by gc
						MaterialExpression->MarkPendingKill();
					}
					else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(NodesToDelete[Index]))
					{
						CommentNode->MaterialExpressionComment->Modify();
						Material->EditorComments.Remove( CommentNode->MaterialExpressionComment );
					}
				}
			}

			Material->MaterialGraph->LinkMaterialExpressionsFromGraph();
		} // ScopedTransaction

		// Deselect all expressions and comments.
		GraphEditor->ClearSelectionSet();
		GraphEditor->NotifyGraphChanged();

		if ( bHaveExpressionsToDelete )
		{
			if( bPreviewExpressionDeleted )
			{
				// The preview expression was deleted.  Null out our reference to it and reset to the normal preview material
				PreviewExpression = NULL;
				SetPreviewMaterial( Material );
			}
			RegenerateCodeView();
		}
		UpdatePreviewMaterial();
		Material->MarkPackageDirty();
		SetMaterialDirty();

		if ( bHaveExpressionsToDelete )
		{
			RefreshExpressionPreviews();
		}
	}
}

bool FMaterialEditor::CanDeleteNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (Cast<UMaterialGraphNode_Root>(*NodeIt))
			{
				// Return false if only root node is selected, as it can't be deleted
				return false;
			}
		}
	}

	return SelectedNodes.Num() > 0;
}

void FMaterialEditor::DeleteSelectedDuplicatableNodes()
{
	// Cache off the old selection
	const FGraphPanelSelectionSet OldSelectedNodes = GraphEditor->GetSelectedNodes();

	// Clear the selection and only select the nodes that can be duplicated
	FGraphPanelSelectionSet RemainingNodes;
	GraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if ((Node != NULL) && Node->CanDuplicateNode())
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
		else
		{
			RemainingNodes.Add(Node);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	// Reselect whatever's left from the original selection after the deletion
	GraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(RemainingNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
	}
}

void FMaterialEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	FString ExportedText;

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		if(UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			Node->PrepareForCopying();
		}
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes, /*out*/ ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

	// Make sure Material remains the owner of the copied nodes
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UMaterialGraphNode* Node = Cast<UMaterialGraphNode>(*SelectedIter))
		{
			Node->PostCopyNode();
		}
		else if (UMaterialGraphNode_Comment* Comment = Cast<UMaterialGraphNode_Comment>(*SelectedIter))
		{
			Comment->PostCopyNode();
		}
	}
}

bool FMaterialEditor::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if ((Node != NULL) && Node->CanDuplicateNode())
		{
			return true;
		}
	}
	return false;
}

void FMaterialEditor::PasteNodes()
{
	PasteNodesHere(GraphEditor->GetPasteLocation());
}

void FMaterialEditor::PasteNodesHere(const FVector2D& Location)
{
	// Undo/Redo support
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorPaste", "Material Editor: Paste") );
	Material->MaterialGraph->Modify();
	Material->Modify();

	// Clear the selection set (newly pasted stuff will be selected)
	GraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(Material->MaterialGraph, TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f,0.0f);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		AvgNodePosition.X += Node->NodePosX;
		AvgNodePosition.Y += Node->NodePosY;
	}

	if ( PastedNodes.Num() > 0 )
	{
		float InvNumNodes = 1.0f/float(PastedNodes.Num());
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Node))
		{
			// These are not copied and we must account for expressions pasted between different materials anyway
			GraphNode->RealtimeDelegate = Material->MaterialGraph->RealtimeDelegate;
			GraphNode->MaterialDirtyDelegate = Material->MaterialGraph->MaterialDirtyDelegate;
			GraphNode->bPreviewNeedsUpdate = false;

			UMaterialExpression* NewExpression = GraphNode->MaterialExpression;
			NewExpression->Material = Material;
			NewExpression->Function = MaterialFunction;

			// Make sure the param name is valid after the paste
			if (NewExpression->HasAParameterName())
			{
				NewExpression->ValidateParameterName();
			}

			Material->Expressions.Add(NewExpression);

			// There can be only one default mesh paint texture.
			UMaterialExpressionTextureBase* TextureSample = Cast<UMaterialExpressionTextureBase>( NewExpression );
			if( TextureSample )
			{
				TextureSample->IsDefaultMeshpaintTexture = false;
			}

			NewExpression->UpdateParameterGuid(true, true);
			Material->AddExpressionParameter(NewExpression, Material->EditorParameters);

			UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>( NewExpression );
			if( FunctionInput )
			{
				FunctionInput->ConditionallyGenerateId(true);
				FunctionInput->ValidateName();
			}

			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>( NewExpression );
			if( FunctionOutput )
			{
				FunctionOutput->ConditionallyGenerateId(true);
				FunctionOutput->ValidateName();
			}

			UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>( NewExpression );
			if( FunctionCall )
			{
				// When pasting new nodes, we don't want to break all node links as this information is used by UpdateMaterialAfterGraphChange() below,
				// to recreate all the connections in the pasted group.
				// Just update the function input/outputs here.
				const bool bRecreateAndLinkNode = false;
				FunctionCall->UpdateFromFunctionResource(bRecreateAndLinkNode);

				// If an unknown material function has been pasted, remove the graph node pins (as the expression will also have had its inputs/outputs removed).
				// This will be displayed as an orphaned "Unspecified Function" node.
				if (FunctionCall->MaterialFunction == nullptr &&
					FunctionCall->FunctionInputs.Num() == 0 &&
					FunctionCall->FunctionOutputs.Num() == 0)
				{
					GraphNode->Pins.Empty();
				}
			}
		}
		else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(Node))
		{
			CommentNode->MaterialDirtyDelegate = Material->MaterialGraph->MaterialDirtyDelegate;
			CommentNode->MaterialExpressionComment->Material = Material;
			CommentNode->MaterialExpressionComment->Function = MaterialFunction;
			Material->EditorComments.Add(CommentNode->MaterialExpressionComment);
		}

		// Select the newly pasted stuff
		GraphEditor->SetNodeSelection(Node, true);

		Node->NodePosX = (Node->NodePosX - AvgNodePosition.X) + Location.X ;
		Node->NodePosY = (Node->NodePosY - AvgNodePosition.Y) + Location.Y ;

		Node->SnapToGrid(SNodePanel::GetSnapGridSize());

		// Give new node a different Guid from the old one
		Node->CreateNewGuid();
	}

	UpdateMaterialAfterGraphChange();

	// Update UI
	GraphEditor->NotifyGraphChanged();
}

bool FMaterialEditor::CanPasteNodes() const
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(Material->MaterialGraph, ClipboardContent);
}

void FMaterialEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	// Cut should only delete nodes that can be duplicated
	DeleteSelectedDuplicatableNodes();
}

bool FMaterialEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FMaterialEditor::DuplicateNodes()
{
	// Copy and paste current selection
	CopySelectedNodes();
	PasteNodes();
}

bool FMaterialEditor::CanDuplicateNodes() const
{
	return CanCopyNodes();
}

FText FMaterialEditor::GetOriginalObjectName() const
{
	return FText::FromString(GetEditingObjects()[0]->GetName());
}

void FMaterialEditor::UpdateMaterialAfterGraphChange()
{
	Material->MaterialGraph->LinkMaterialExpressionsFromGraph();

	// Update the current preview material.
	UpdatePreviewMaterial();

	Material->MarkPackageDirty();
	RegenerateCodeView();
	RefreshExpressionPreviews();
	SetMaterialDirty();
}

int32 FMaterialEditor::GetNumberOfSelectedNodes() const
{
	return GraphEditor->GetSelectedNodes().Num();
}

FMaterialRenderProxy* FMaterialEditor::GetExpressionPreview(UMaterialExpression* InExpression)
{
	bool bNewlyCreated;
	return GetExpressionPreview(InExpression, bNewlyCreated);
}

void FMaterialEditor::UndoGraphAction()
{
	int32 NumExpressions = Material->Expressions.Num();
	GEditor->UndoTransaction();

	if(NumExpressions != Material->Expressions.Num())
	{
		Material->BuildEditorParameterList();
	}
}

void FMaterialEditor::RedoGraphAction()
{
	// Clear selection, to avoid holding refs to nodes that go away
	GraphEditor->ClearSelectionSet();

	int32 NumExpressions = Material->Expressions.Num();
	GEditor->RedoTransaction();

	if(NumExpressions != Material->Expressions.Num())
	{
		Material->BuildEditorParameterList();
	}

}

void FMaterialEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{	
		GraphEditor->ClearSelectionSet();
		
		Material->BuildEditorParameterList();

		// Update the current preview material.
		UpdatePreviewMaterial();

		UpdatePreviewViewportsVisibility();

		RefreshExpressionPreviews();
		GraphEditor->NotifyGraphChanged();
		SetMaterialDirty();
	}
}

void FMaterialEditor::NotifyPreChange(UProperty* PropertyAboutToChange)
{
	check( !ScopedTransaction );
	ScopedTransaction = new FScopedTransaction( NSLOCTEXT("UnrealEd", "MaterialEditorEditProperties", "Material Editor: Edit Properties") );
	FlushRenderingCommands();
}

void FMaterialEditor::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	check( ScopedTransaction );

	if ( PropertyThatChanged )
	{
		const FName NameOfPropertyThatChanged( *PropertyThatChanged->GetName() );
		if ((NameOfPropertyThatChanged == GET_MEMBER_NAME_CHECKED(UMaterialInterface, PreviewMesh)) ||
			(NameOfPropertyThatChanged == GET_MEMBER_NAME_CHECKED(UMaterial, bUsedWithSkeletalMesh)))
		{
			// SetPreviewMesh will return false if the material has bUsedWithSkeletalMesh and
			// a skeleton was requested, in which case revert to a sphere static mesh.
			if (!SetPreviewAssetByName(*Material->PreviewMesh.ToString()))
			{
				SetPreviewAsset(GUnrealEd->GetThumbnailManager()->EditorSphere);
			}
		}
		else if( NameOfPropertyThatChanged == GET_MEMBER_NAME_CHECKED(UMaterial, MaterialDomain) ||
				 NameOfPropertyThatChanged == GET_MEMBER_NAME_CHECKED(UMaterial, ShadingModel) )
		{
			Material->MaterialGraph->RebuildGraph();
			TArray<TWeakObjectPtr<UObject>> SelectedObjects = MaterialDetailsView->GetSelectedObjects();
			MaterialDetailsView->SetObjects( SelectedObjects, true );

			if (ExpressionPreviewMaterial)
			{
				if (Material->IsUIMaterial())
				{
					ExpressionPreviewMaterial->MaterialDomain = MD_UI;
				}
				else
				{
					ExpressionPreviewMaterial->MaterialDomain = MD_Surface;
				}

				SetPreviewMaterial(ExpressionPreviewMaterial);
			}

			UpdatePreviewViewportsVisibility();
		}

		FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			UMaterialGraphNode* SelectedNode = Cast<UMaterialGraphNode>(*NodeIt);

			if (SelectedNode && SelectedNode->MaterialExpression)
			{
				if(NameOfPropertyThatChanged == FName(TEXT("ParameterName")))
				{
					Material->UpdateExpressionParameterName(SelectedNode->MaterialExpression);
				}
				else if (SelectedNode->MaterialExpression->IsA(UMaterialExpressionDynamicParameter::StaticClass()))
				{
					Material->UpdateExpressionDynamicParameters(SelectedNode->MaterialExpression);
				}
				else
				{
					Material->PropagateExpressionParameterChanges(SelectedNode->MaterialExpression);
				}
			}
		}
	}

	// Prevent constant recompilation of materials while properties are being interacted with
	if( PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		// Also prevent recompilation when properties have no effect on material output
		const FName PropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
		if (PropertyName != GET_MEMBER_NAME_CHECKED(UMaterialExpressionComment, Text)
		&& PropertyName != GET_MEMBER_NAME_CHECKED(UMaterialExpressionComment, CommentColor)
		&& PropertyName != GET_MEMBER_NAME_CHECKED(UMaterialExpression, Desc))
		{
			// Update the current preview material.
			UpdatePreviewMaterial();
			RefreshExpressionPreviews();
			RegenerateCodeView();
		}

		GetDefault<UMaterialGraphSchema>()->ForceVisualizationCacheClear();
	}

	delete ScopedTransaction;
	ScopedTransaction = NULL;

	Material->MarkPackageDirty();
	SetMaterialDirty();
}

void FMaterialEditor::ToggleCollapsed(UMaterialExpression* MaterialExpression)
{
	check( MaterialExpression );
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorToggleCollapsed", "Material Editor: Toggle Collapsed") );
		MaterialExpression->Modify();
		MaterialExpression->bCollapsed = !MaterialExpression->bCollapsed;
	}
	MaterialExpression->PreEditChange( NULL );
	MaterialExpression->PostEditChange();
	MaterialExpression->MarkPackageDirty();
	SetMaterialDirty();

	// Update the preview.
	RefreshExpressionPreview( MaterialExpression, true );
	RefreshPreviewViewport();
}

void FMaterialEditor::RefreshExpressionPreviews()
{
	const FScopedBusyCursor BusyCursor;

	if ( bAlwaysRefreshAllPreviews )
	{
		// we need to make sure the rendering thread isn't drawing these tiles
		SCOPED_SUSPEND_RENDERING_THREAD(true);

		// Refresh all expression previews.
		ExpressionPreviews.Empty();

		for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ++ExpressionIndex)
		{
			UMaterialExpression* MaterialExpression = Material->Expressions[ExpressionIndex];

			UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(MaterialExpression->GraphNode);
			if (GraphNode)
			{
				GraphNode->InvalidatePreviewMaterialDelegate.ExecuteIfBound();
			}
		}
	}
	else
	{
		// Only refresh expressions that are marked for realtime update.
		for ( int32 ExpressionIndex = 0 ; ExpressionIndex < Material->Expressions.Num() ; ++ExpressionIndex )
		{
			UMaterialExpression* MaterialExpression = Material->Expressions[ ExpressionIndex ];
			RefreshExpressionPreview( MaterialExpression, false );
		}
	}

	TArray<FMatExpressionPreview*> ExpressionPreviewsBeingCompiled;
	ExpressionPreviewsBeingCompiled.Empty(50);

	// Go through all expression previews and create new ones as needed, and maintain a list of previews that are being compiled
	for( int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ++ExpressionIndex )
	{
		UMaterialExpression* MaterialExpression = Material->Expressions[ ExpressionIndex ];
		if (MaterialExpression && !MaterialExpression->IsA(UMaterialExpressionComment::StaticClass()) )
		{
			bool bNewlyCreated;
			FMatExpressionPreview* Preview = GetExpressionPreview( MaterialExpression, bNewlyCreated );
			if (bNewlyCreated && Preview)
			{
				ExpressionPreviewsBeingCompiled.Add(Preview);
			}
		}
	}
}

void FMaterialEditor::RefreshExpressionPreview(UMaterialExpression* MaterialExpression, bool bRecompile)
{
	if ( (MaterialExpression->bRealtimePreview || MaterialExpression->bNeedToUpdatePreview) && !MaterialExpression->bCollapsed )
	{
		for( int32 PreviewIndex = 0 ; PreviewIndex < ExpressionPreviews.Num() ; ++PreviewIndex )
		{
			FMatExpressionPreview& ExpressionPreview = ExpressionPreviews[PreviewIndex];
			if( ExpressionPreview.GetExpression() == MaterialExpression )
			{
				// we need to make sure the rendering thread isn't drawing this tile
				SCOPED_SUSPEND_RENDERING_THREAD(true);
				ExpressionPreviews.RemoveAt( PreviewIndex );
				MaterialExpression->bNeedToUpdatePreview = false;

				if (bRecompile)
				{
					bool bNewlyCreated;
					GetExpressionPreview(MaterialExpression, bNewlyCreated);
				}

				UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(MaterialExpression->GraphNode);
				if (GraphNode)
				{
					GraphNode->InvalidatePreviewMaterialDelegate.ExecuteIfBound();
				}

				break;
			}
		}
	}
}

FMatExpressionPreview* FMaterialEditor::GetExpressionPreview(UMaterialExpression* MaterialExpression, bool& bNewlyCreated)
{
	bNewlyCreated = false;
	if (!MaterialExpression->bHidePreviewWindow && !MaterialExpression->bCollapsed)
	{
		FMatExpressionPreview*	Preview = NULL;
		for( int32 PreviewIndex = 0 ; PreviewIndex < ExpressionPreviews.Num() ; ++PreviewIndex )
		{
			FMatExpressionPreview& ExpressionPreview = ExpressionPreviews[PreviewIndex];
			if( ExpressionPreview.GetExpression() == MaterialExpression )
			{
				Preview = &ExpressionPreviews[PreviewIndex];
				break;
			}
		}

		if( !Preview )
		{
			bNewlyCreated = true;
			Preview = new(ExpressionPreviews) FMatExpressionPreview(MaterialExpression);
			Preview->CacheShaders(GMaxRHIShaderPlatform, true);
		}
		return Preview;
	}

	return NULL;
}

void FMaterialEditor::PreColorPickerCommit(FLinearColor LinearColor)
{
	// Begin a property edit transaction.
	if ( GEditor )
	{
		GEditor->BeginTransaction( LOCTEXT("ModifyColorPicker", "Modify Color Picker Value") );
	}

	NotifyPreChange(NULL);

	UObject* Object = ColorPickerObject.Get(false);
	if( Object )
	{
		Object->PreEditChange(NULL);
	}
}

void FMaterialEditor::OnColorPickerCommitted(FLinearColor LinearColor)
{	
	UObject* Object = ColorPickerObject.Get(false);
	if( Object )
	{
		Object->MarkPackageDirty();
		FPropertyChangedEvent Event(ColorPickerProperty.Get(false));
		Object->PostEditChangeProperty(Event);
	}

	NotifyPostChange(NULL,NULL);

	if ( GEditor )
	{
		GEditor->EndTransaction();
	}

	RefreshExpressionPreviews();
}

TSharedRef<SGraphEditor> FMaterialEditor::CreateGraphEditorWidget()
{
	GraphEditorCommands = MakeShareable( new FUICommandList );
	{
		// Editing commands
		GraphEditorCommands->MapAction( FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateSP( this, &FMaterialEditor::SelectAllNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanSelectAllNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP( this, &FMaterialEditor::DeleteSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanDeleteNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Copy,
			FExecuteAction::CreateSP( this, &FMaterialEditor::CopySelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanCopyNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Paste,
			FExecuteAction::CreateSP( this, &FMaterialEditor::PasteNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanPasteNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Cut,
			FExecuteAction::CreateSP( this, &FMaterialEditor::CutSelectedNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanCutNodes )
			);

		GraphEditorCommands->MapAction( FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateSP( this, &FMaterialEditor::DuplicateNodes ),
			FCanExecuteAction::CreateSP( this, &FMaterialEditor::CanDuplicateNodes )
			);

		// Graph Editor Commands
		GraphEditorCommands->MapAction( FGraphEditorCommands::Get().CreateComment,
			FExecuteAction::CreateSP( this, &FMaterialEditor::OnCreateComment )
			);

		// Material specific commands
		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().UseCurrentTexture,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnUseCurrentTexture)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ConvertObjects,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertObjects)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ConvertToTextureObjects,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertTextures)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ConvertToTextureSamples,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertTextures)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ConvertToConstant,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnConvertObjects)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().StopPreviewNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnPreviewNode)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().StartPreviewNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnPreviewNode)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().EnableRealtimePreviewNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnToggleRealtimePreview)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().DisableRealtimePreviewNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnToggleRealtimePreview)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().SelectDownstreamNodes,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnSelectDownstreamNodes)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().SelectUpstreamNodes,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnSelectUpstreamNodes)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().RemoveFromFavorites,
			FExecuteAction::CreateSP(this, &FMaterialEditor::RemoveSelectedExpressionFromFavorites)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().AddToFavorites,
			FExecuteAction::CreateSP(this, &FMaterialEditor::AddSelectedExpressionToFavorites)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().ForceRefreshPreviews,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnForceRefreshPreviews)
			);

		GraphEditorCommands->MapAction( FMaterialEditorCommands::Get().CreateComponentMaskNode,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnCreateComponentMaskNode)
			);

		GraphEditorCommands->MapAction(FGraphEditorCommands::Get().GoToDocumentation,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnGoToDocumentation),
			FCanExecuteAction::CreateSP(this, &FMaterialEditor::CanGoToDocumentation)
			);


		GraphEditorCommands->MapAction(FMaterialEditorCommands::Get().PromoteToParameter,
			FExecuteAction::CreateSP(this, &FMaterialEditor::OnPromoteToParameter),
			FCanExecuteAction::CreateSP(this, &FMaterialEditor::OnCanPromoteToParameter)
			);

	}

	FGraphAppearanceInfo AppearanceInfo;
	
	if (MaterialFunction)
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_MaterialFunction", "MATERIAL FUNCTION");
	}
	else
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_Material", "MATERIAL");
	}

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FMaterialEditor::OnSelectedNodesChanged);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FMaterialEditor::OnNodeDoubleClicked);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FMaterialEditor::OnNodeTitleCommitted);
	InEvents.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &FMaterialEditor::OnVerifyNodeTextCommit);
	InEvents.OnSpawnNodeByShortcut = SGraphEditor::FOnSpawnNodeByShortcut::CreateSP(this, &FMaterialEditor::OnSpawnGraphNodeByShortcut, static_cast<UEdGraph*>(Material->MaterialGraph));

	// Create the title bar widget
	TSharedPtr<SWidget> TitleBarWidget = SNew(SMaterialEditorTitleBar)
		.TitleText(this, &FMaterialEditor::GetOriginalObjectName);
		//.MaterialInfoList(&MaterialInfoList);

	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.TitleBar(TitleBarWidget)
		.Appearance(AppearanceInfo)
		.GraphToEdit(Material->MaterialGraph)
		.GraphEvents(InEvents)
		.ShowGraphStateOverlay(false);
}

void FMaterialEditor::CleanUnusedExpressions()
{
	TArray<UEdGraphNode*> UnusedNodes;

	Material->MaterialGraph->GetUnusedExpressions(UnusedNodes);

	if (UnusedNodes.Num() > 0 && CheckExpressionRemovalWarnings(UnusedNodes))
	{
		{
			// Kill off expressions referenced by the material that aren't reachable.
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "MaterialEditorCleanUnusedExpressions", "Material Editor: Clean Unused Expressions") );
				
			Material->Modify();
			Material->MaterialGraph->Modify();

			for (int32 Index = 0; Index < UnusedNodes.Num(); ++Index)
			{
				UMaterialGraphNode* GraphNode = CastChecked<UMaterialGraphNode>(UnusedNodes[Index]);
				UMaterialExpression* MaterialExpression = GraphNode->MaterialExpression;

				FBlueprintEditorUtils::RemoveNode(NULL, GraphNode, true);

				if (PreviewExpression == MaterialExpression)
				{
					SetPreviewExpression(NULL);
				}

				MaterialExpression->Modify();
				Material->Expressions.Remove(MaterialExpression);
				Material->RemoveExpressionParameter(MaterialExpression);
				// Make sure the deleted expression is caught by gc
				MaterialExpression->MarkPendingKill();
			}

			Material->MaterialGraph->LinkMaterialExpressionsFromGraph();
		} // ScopedTransaction

		GraphEditor->ClearSelectionSet();
		GraphEditor->NotifyGraphChanged();

		SetMaterialDirty();
	}
}

bool FMaterialEditor::CheckExpressionRemovalWarnings(const TArray<UEdGraphNode*>& NodesToRemove)
{
	FString FunctionWarningString;
	bool bFirstExpression = true;
	for (int32 Index = 0; Index < NodesToRemove.Num(); ++Index)
	{
		if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(NodesToRemove[Index]))
		{
			UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(GraphNode->MaterialExpression);
			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(GraphNode->MaterialExpression);

			if (FunctionInput)
			{
				if (!bFirstExpression)
				{
					FunctionWarningString += TEXT(", ");
				}
				bFirstExpression = false;
				FunctionWarningString += FunctionInput->InputName;
			}

			if (FunctionOutput)
			{
				if (!bFirstExpression)
				{
					FunctionWarningString += TEXT(", ");
				}
				bFirstExpression = false;
				FunctionWarningString += FunctionOutput->OutputName;
			}
		}
	}

	if (FunctionWarningString.Len() > 0)
	{
		if (EAppReturnType::Yes != FMessageDialog::Open( EAppMsgType::YesNo,
				FText::Format(
					NSLOCTEXT("UnrealEd", "Prompt_MaterialEditorDeleteFunctionInputs", "Delete function inputs or outputs \"{0}\"?\nAny materials which use this function will lose their connections to these function inputs or outputs once deleted."),
					FText::FromString(FunctionWarningString) )))
		{
			// User said don't delete
			return false;
		}
	}

	return true;
}

void FMaterialEditor::RemoveSelectedExpressionFromFavorites()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt))
			{
				MaterialExpressionClasses::Get()->RemoveMaterialExpressionFromFavorites(GraphNode->MaterialExpression->GetClass());
				EditorOptions->FavoriteExpressions.Remove(GraphNode->MaterialExpression->GetClass()->GetName());
				EditorOptions->SaveConfig();
			}
		}
	}
}

void FMaterialEditor::AddSelectedExpressionToFavorites()
{
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	if (SelectedNodes.Num() == 1)
	{
		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*NodeIt))
			{
				MaterialExpressionClasses::Get()->AddMaterialExpressionToFavorites(GraphNode->MaterialExpression->GetClass());
				EditorOptions->FavoriteExpressions.AddUnique(GraphNode->MaterialExpression->GetClass()->GetName());
				EditorOptions->SaveConfig();
			}
		}
	}
}

void FMaterialEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	TArray<UObject*> SelectedObjects;

	UObject* EditObject = Material;
	if (MaterialFunction)
	{
		EditObject = MaterialFunction;
	}

	if( NewSelection.Num() == 0 )
	{
		SelectedObjects.Add(EditObject);
	}
	else
	{
		for(TSet<class UObject*>::TConstIterator SetIt(NewSelection);SetIt;++SetIt)
		{
			if (UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(*SetIt))
			{
				SelectedObjects.Add(GraphNode->MaterialExpression);
			}
			else if (UMaterialGraphNode_Comment* CommentNode = Cast<UMaterialGraphNode_Comment>(*SetIt))
			{
				SelectedObjects.Add(CommentNode->MaterialExpressionComment);
			}
			else
			{
				SelectedObjects.Add(EditObject);
			}
		}
	}

	GetDetailView()->SetObjects( SelectedObjects, true );
}

void FMaterialEditor::OnNodeDoubleClicked(class UEdGraphNode* Node)
{
	UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Node);

	if (GraphNode)
	{
		UMaterialExpressionConstant3Vector* Constant3Expression = Cast<UMaterialExpressionConstant3Vector>(GraphNode->MaterialExpression);
		UMaterialExpressionConstant4Vector* Constant4Expression = Cast<UMaterialExpressionConstant4Vector>(GraphNode->MaterialExpression);
		UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(GraphNode->MaterialExpression);
		UMaterialExpressionVectorParameter* VectorExpression = Cast<UMaterialExpressionVectorParameter>(GraphNode->MaterialExpression);

		FColorChannels ChannelEditStruct;

		// Reset to default
		ColorPickerProperty = NULL;

		if( Constant3Expression )
		{
			ChannelEditStruct.Red = &Constant3Expression->Constant.R;
			ChannelEditStruct.Green = &Constant3Expression->Constant.G;
			ChannelEditStruct.Blue = &Constant3Expression->Constant.B;
		}
		else if( Constant4Expression )
		{
			ChannelEditStruct.Red = &Constant4Expression->Constant.R;
			ChannelEditStruct.Green = &Constant4Expression->Constant.G;
			ChannelEditStruct.Blue = &Constant4Expression->Constant.B;
			ChannelEditStruct.Alpha = &Constant4Expression->Constant.A;
		}
		else if (InputExpression)
		{
			ChannelEditStruct.Red = &InputExpression->PreviewValue.X;
			ChannelEditStruct.Green = &InputExpression->PreviewValue.Y;
			ChannelEditStruct.Blue = &InputExpression->PreviewValue.Z;
			ChannelEditStruct.Alpha = &InputExpression->PreviewValue.W;
		}
		else if (VectorExpression)
		{
			ChannelEditStruct.Red = &VectorExpression->DefaultValue.R;
			ChannelEditStruct.Green = &VectorExpression->DefaultValue.G;
			ChannelEditStruct.Blue = &VectorExpression->DefaultValue.B;
			ChannelEditStruct.Alpha = &VectorExpression->DefaultValue.A;
			static FName DefaultValueName = FName(TEXT("DefaultValue"));
			// Store off the property the color picker will be manipulating, so we can construct a useful PostEditChangeProperty later
			ColorPickerProperty = VectorExpression->GetClass()->FindPropertyByName(DefaultValueName);
		}

		if (ChannelEditStruct.Red || ChannelEditStruct.Green || ChannelEditStruct.Blue || ChannelEditStruct.Alpha)
		{
			TArray<FColorChannels> Channels;
			Channels.Add(ChannelEditStruct);

			ColorPickerObject = GraphNode->MaterialExpression;

			// Open a color picker that only sends updates when Ok is clicked, 
			// Since it is too slow to recompile preview expressions as the user is picking different colors
			FColorPickerArgs PickerArgs;
			PickerArgs.ParentWidget = GraphEditor;//AsShared();
			PickerArgs.bUseAlpha = Constant4Expression != NULL || VectorExpression != NULL;
			PickerArgs.bOnlyRefreshOnOk = false;
			PickerArgs.bOnlyRefreshOnMouseUp = true;
			PickerArgs.bExpandAdvancedSection = true;
			PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
			PickerArgs.ColorChannelsArray = &Channels;
			PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FMaterialEditor::OnColorPickerCommitted);
			PickerArgs.PreColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FMaterialEditor::PreColorPickerCommit);

			OpenColorPicker(PickerArgs);
		}

		UMaterialExpressionTextureSample* TextureExpression = Cast<UMaterialExpressionTextureSample>(GraphNode->MaterialExpression);
		UMaterialExpressionTextureSampleParameter* TextureParameterExpression = Cast<UMaterialExpressionTextureSampleParameter>(GraphNode->MaterialExpression);
		UMaterialExpressionMaterialFunctionCall* FunctionExpression = Cast<UMaterialExpressionMaterialFunctionCall>(GraphNode->MaterialExpression);
		UMaterialExpressionCollectionParameter* CollectionParameter = Cast<UMaterialExpressionCollectionParameter>(GraphNode->MaterialExpression);

		TArray<UObject*> ObjectsToView;
		UObject* ObjectToEdit = NULL;

		if (TextureExpression && TextureExpression->Texture)
		{
			ObjectsToView.Add(TextureExpression->Texture);
		}
		else if (TextureParameterExpression && TextureParameterExpression->Texture)
		{
			ObjectsToView.Add(TextureParameterExpression->Texture);
		}
		else if (FunctionExpression && FunctionExpression->MaterialFunction)
		{
			ObjectToEdit = FunctionExpression->MaterialFunction;
		}
		else if (CollectionParameter && CollectionParameter->Collection)
		{
			ObjectToEdit = CollectionParameter->Collection;
		}

		if (ObjectsToView.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(ObjectsToView);
		}
		if (ObjectToEdit)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(ObjectToEdit);
		}
	}
}

void FMaterialEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		const FScopedTransaction Transaction( LOCTEXT( "RenameNode", "Rename Node" ) );
		NodeBeingChanged->Modify();
		NodeBeingChanged->OnRenameNode(NewText.ToString());
	}
}

bool FMaterialEditor::OnVerifyNodeTextCommit(const FText& NewText, UEdGraphNode* NodeBeingChanged, FText& OutErrorMessage)
{
	bool bValid = true;

	UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(NodeBeingChanged);
	if( MaterialNode && MaterialNode->MaterialExpression && MaterialNode->MaterialExpression->IsA<UMaterialExpressionParameter>() )
	{
		if( NewText.ToString().Len() > NAME_SIZE )
		{
			OutErrorMessage = FText::Format( LOCTEXT("MaterialEditorExpressionError_NameTooLong", "Parameter names must be less than {0} characters"), FText::AsNumber(NAME_SIZE));
			bValid = false;
		}
	}

	return bValid;
}

FReply FMaterialEditor::OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph)
{
	UEdGraph* Graph = InGraph;

	TSharedPtr< FEdGraphSchemaAction > Action = FMaterialEditorSpawnNodeCommands::Get().GetGraphActionByChord(InChord, InGraph);

	if(Action.IsValid())
	{
		TArray<UEdGraphPin*> DummyPins;
		Action->PerformAction(Graph, DummyPins, InPosition);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FMaterialEditor::UpdateStatsMaterials()
{
	if (bShowBuiltinStats && bStatsFromPreviewMaterial)
	{	
		UMaterial* StatsMaterial = Material;
		FString EmptyMaterialName = FString(TEXT("MEStatsMaterial_Empty_")) + Material->GetName();
		EmptyMaterial = (UMaterial*)StaticDuplicateObject(Material, GetTransientPackage(), *EmptyMaterialName, ~RF_Standalone, UPreviewMaterial::StaticClass());

		EmptyMaterial->SetFeatureLevelToCompile(ERHIFeatureLevel::ES2, bShowMobileStats);

		EmptyMaterial->Expressions.Empty();

		//Disconnect all properties from the expressions
		for (int32 PropIdx = 0; PropIdx < MP_MAX; ++PropIdx)
		{
			FExpressionInput* ExpInput = EmptyMaterial->GetExpressionInputForProperty((EMaterialProperty)PropIdx);
			if(ExpInput)
			{
				ExpInput->Expression = NULL;
			}
		}
		EmptyMaterial->bAllowDevelopmentShaderCompile = Material->bAllowDevelopmentShaderCompile;
		EmptyMaterial->PreEditChange(NULL);
		EmptyMaterial->PostEditChange();
	}
}

#undef LOCTEXT_NAMESPACE
