// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Stats/Stats.h"
#include "Misc/Guid.h"
#include "UObject/GCObject.h"
#include "Misc/NotifyHook.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Framework/Commands/InputChord.h"
#include "EditorUndoClient.h"
#include "MaterialShared.h"
#include "Toolkits/IToolkitHost.h"
#include "IMaterialEditor.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "SMaterialEditorViewport.h"
#include "Materials/Material.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Tickable.h"

struct FAssetData;
class FCanvas;
class FMaterialCompiler;
class FScopedTransaction;
class IMessageLogListing;
class SDockableTab;
class SFindInMaterial;
class SGraphEditor;
class SMaterialPalette;
class UEdGraph;
class UFactory;
class UMaterialEditorOptions;
class UMaterialExpressionComment;
class UMaterialInstance;

/**
 * Class for rendering previews of material expressions in the material editor's linked object viewport.
 */
class FMatExpressionPreview : public FMaterial, public FMaterialRenderProxy
{
public:
	FMatExpressionPreview()
	: FMaterial()
	{
		// Register this FMaterial derivative with AddEditorLoadedMaterialResource since it does not have a corresponding UMaterialInterface
		FMaterial::AddEditorLoadedMaterialResource(this);
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GMaxRHIFeatureLevel);
	}

	FMatExpressionPreview(UMaterialExpression* InExpression)
	: FMaterial()
	, Expression(InExpression)
	{
		FMaterial::AddEditorLoadedMaterialResource(this);
		FPlatformMisc::CreateGuid(Id);

		check(InExpression->Material && InExpression->Material->Expressions.Contains(InExpression));
		InExpression->Material->AppendReferencedTextures(ReferencedTextures);
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GMaxRHIFeatureLevel);
	}

	~FMatExpressionPreview()
	{
		CancelCompilation();
		ReleaseResource();
	}

	void AddReferencedObjects( FReferenceCollector& Collector )
	{
		for (int32 TextureIndex = 0; TextureIndex < ReferencedTextures.Num(); TextureIndex++)
		{
			Collector.AddReferencedObject(ReferencedTextures[TextureIndex]);
		}
	}

	/**
	 * Should the shader for this material with the given platform, shader type and vertex 
	 * factory type combination be compiled
	 *
	 * @param Platform		The platform currently being compiled for
	 * @param ShaderType	Which shader is being compiled
	 * @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	 *
	 * @return true if the shader should be compiled
	 */
	virtual bool ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const override;

	virtual const TArray<UTexture*>& GetReferencedTextures() const override
	{
		return ReferencedTextures;
	}

	////////////////
	// FMaterialRenderProxy interface.

	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const override
	{
		if(GetRenderingThreadShaderMap())
		{
			return this;
		}
		else
		{
			return UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(FeatureLevel);
		}
	}

	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const override
	{
		if (Expression.IsValid() && Expression->Material)
		{
			return Expression->Material->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
		}
		return false;
	}

	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const override
	{
		if (Expression.IsValid() && Expression->Material)
		{
			return Expression->Material->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
		}
		return false;
	}

	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const override
	{
		if (Expression.IsValid() && Expression->Material)
		{
			return Expression->Material->GetRenderProxy(0)->GetTextureValue(ParameterName, OutValue, Context);
		}
		return false;
	}
	
	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual int32 CompilePropertyAndSetMaterialProperty(EMaterialProperty Property, FMaterialCompiler* Compiler, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime) const override;

	virtual EMaterialDomain GetMaterialDomain() const override { return MD_Surface; }
	virtual FString GetMaterialUsageDescription() const override { return FString::Printf(TEXT("FMatExpressionPreview %s"), Expression.IsValid() ? *Expression->GetName() : TEXT("NULL")); }
	virtual bool IsTwoSided() const override { return false; }
	virtual bool IsDitheredLODTransition() const override { return false; }
	virtual bool IsLightFunction() const override { return false; }
	virtual bool IsDeferredDecal() const override { return false; }
	virtual bool IsVolumetricPrimitive() const override { return false; }
	virtual bool IsSpecialEngineMaterial() const override { return false; }
	virtual bool IsWireframe() const override { return false; }
	virtual bool IsMasked() const override { return false; }
	virtual enum EBlendMode GetBlendMode() const override { return BLEND_Opaque; }
	virtual enum EMaterialShadingModel GetShadingModel() const override { return MSM_Unlit; }
	virtual float GetOpacityMaskClipValue() const override { return 0.5f; }
	virtual bool GetCastDynamicShadowAsMasked() const override { return false; }
	virtual FString GetFriendlyName() const override { return FString::Printf(TEXT("FMatExpressionPreview %s"), Expression.IsValid() ? *Expression->GetName() : TEXT("NULL")); }
	/**
	 * Should shaders compiled for this material be saved to disk?
	 */
	virtual bool IsPersistent() const override { return false; }
	virtual FGuid GetMaterialId() const override { return Id; }
	const UMaterialExpression* GetExpression() const
	{
		return Expression.Get();
	}

	virtual void NotifyCompilationFinished() override;

	friend FArchive& operator<< ( FArchive& Ar, FMatExpressionPreview& V )
	{
		return Ar << V.Expression;
	}

private:
	TWeakObjectPtr<UMaterialExpression> Expression;
	TArray<UTexture*> ReferencedTextures;
	FGuid Id;
};

/** Wrapper for each material expression, including a trimmed name */
struct FMaterialExpression
{
	FString Name;
	UClass* MaterialClass;
	FText CreationDescription;
	FText CreationName;

	friend bool operator==(const FMaterialExpression& A,const FMaterialExpression& B)
	{
		return A.MaterialClass == B.MaterialClass;
	}
};

/** Static array of categorized material expression classes. */
struct FCategorizedMaterialExpressionNode
{
	FText	CategoryName;
	TArray<FMaterialExpression> MaterialExpressions;
};

/** Used to display material information, compile errors etc. */
struct FMaterialInfo
{
	FString Text;
	FLinearColor Color;

	FMaterialInfo(const FString& InText, const FLinearColor& InColor)
		: Text(InText)
		, Color(InColor)
	{}
};

/**
 * Material Editor class
 */
class FMaterialEditor : public IMaterialEditor, public FGCObject, public FTickableGameObject, public FEditorUndoClient, public FNotifyHook
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

public:
	/** Initializes the editor to use a material. Should be the first thing called. */
	void InitEditorForMaterial(UMaterial* InMaterial);

	/** Initializes the editor to use a material function. Should be the first thing called. */
	void InitEditorForMaterialFunction(UMaterialFunction* InMaterialFunction);

	/**
	 * Edits the specified material object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectToEdit			The material or material function to edit
	 */
	void InitMaterialEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit );

	/** Constructor */
	FMaterialEditor();

	virtual ~FMaterialEditor();
	
	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;

	/** @return the documentation location for this editor */
	virtual FString GetDocumentationLink() const override
	{
		return FString(TEXT("Engine/Rendering/Materials"));
	}

	/** @return Returns the color and opacity to use for the color that appears behind the tab text for this toolkit's tab in world-centric mode. */
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** The material instance applied to the preview mesh. */
	virtual UMaterialInterface* GetMaterialInterface() const override;
	
	/**
	 * Draws material info strings such as instruction count and current errors onto the canvas.
	 */
	static void DrawMaterialInfoStrings(
		FCanvas* Canvas,
		const UMaterial* Material,
		const FMaterialResource* MaterialResource,
		const TArray<FString>& CompileErrors,
		int32 &DrawPositionY,
		bool bDrawInstructions);
	
	/**
	 * Draws messages on the specified viewport and canvas.
	 */
	virtual void DrawMessages( FViewport* Viewport, FCanvas* Canvas ) override;

	/**
	 * Recenter the editor to either the material inputs or the first material function output
	 */
	void RecenterEditor();

	/** Passes instructions to the preview viewport */
	bool SetPreviewAsset(UObject* InAsset);
	bool SetPreviewAssetByName(const TCHAR* InAssetName);
	void SetPreviewMaterial(UMaterialInterface* InMaterialInterface);
	
	/**
	 * Refreshes the viewport containing the preview mesh.
	 */
	void RefreshPreviewViewport();
	
	/** Regenerates the code view widget with new text */
	void RegenerateCodeView(bool bForce=false);
	
	/**
	 * Recompiles the material used in the preview window.
	 */
	void UpdatePreviewMaterial(bool bForce=false);

	/**
	 * Updates the original material with the changes made in the editor
	 */
	void UpdateOriginalMaterial();

	/**
	 * Updates list of Material Info used to show stats
	 *
	 * @param bForceDisplay	Whether we want to ensure stats tab is displayed.
	 */
	void UpdateMaterialInfoList(bool bForceDisplay = false);

	/**
	 * Updates flags on the Material Nodes to avoid expensive look up calls when rendering
	 */
	void UpdateGraphNodeStates();
	
	// Widget Accessors
	TSharedRef<class IDetailsView> GetDetailView() const {return MaterialDetailsView.ToSharedRef();}
	
	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;

	virtual bool IsTickable() const override
	{
		return true;
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}

	virtual bool IsTickableInEditor() const override
	{
		return true;
	}

	virtual TStatId GetStatId() const override;

	/** Pushes the PreviewMesh assigned the the material instance to the thumbnail info */
	static void UpdateThumbnailInfoPreviewMesh(UMaterialInterface* MatInterface);

	/** Sets the expression to be previewed. */
	void SetPreviewExpression(UMaterialExpression* NewPreviewExpression);

	/** Pan the view to center on a particular node */
	void JumpToNode(const UEdGraphNode* Node);

	// IMaterial Editor Interface
	virtual UMaterialExpression* CreateNewMaterialExpression(UClass* NewExpressionClass, const FVector2D& NodePos, bool bAutoSelect, bool bAutoAssignResource) override;
	virtual UMaterialExpressionComment* CreateNewMaterialExpressionComment(const FVector2D& NodePos) override;
	virtual void ForceRefreshExpressionPreviews() override;
	virtual void AddToSelection(UMaterialExpression* Expression) override;
	virtual void DeleteSelectedNodes() override;
	virtual FText GetOriginalObjectName() const override;
	virtual void UpdateMaterialAfterGraphChange() override;
	virtual bool CanPasteNodes() const override;
	virtual void PasteNodesHere(const FVector2D& Location) override;
	virtual int32 GetNumberOfSelectedNodes() const override;
	virtual FMaterialRenderProxy* GetExpressionPreview(UMaterialExpression* InExpression) override;
	virtual void DeleteNodes(const TArray<class UEdGraphNode*>& NodesToDelete) override;


	void UpdateStatsMaterials();

	/** Gets the extensibility managers for outside entities to extend material editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() { return ToolBarExtensibilityManager; }

public:
	/** Set to true when modifications have been made to the material */
	bool bMaterialDirty;

	/** Set to true if stats should be displayed from the preview material. */
	bool bStatsFromPreviewMaterial;

	/** The material applied to the preview mesh. */
	UMaterial* Material;
	
	/** The source material being edited by this material editor. Only will be updated when Material's settings are copied over this material */
	UMaterial* OriginalMaterial;
	
	/** The material applied to the preview mesh when previewing an expression. */
	UMaterial* ExpressionPreviewMaterial;

	/** An empty copy of the preview material. Allows displaying of stats about the built in cost of the current material. */
	UMaterial* EmptyMaterial;

	/** The expression currently being previewed.  This is NULL when not in expression preview mode. */
	UMaterialExpression* PreviewExpression;

	/** 
	 * Material function being edited.  
	 * If this is non-NULL, a function is being edited and Material is being used to preview it.
	 */
	UMaterialFunction* MaterialFunction;
	
	/** The original material or material function being edited by this material editor.. */
	UObject* OriginalMaterialObject;

	/** Configuration class used to store editor settings across sessions. */
	UMaterialEditorOptions* EditorOptions;
	
protected:
	//~ FAssetEditorToolkit interface
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;
	virtual void SaveAsset_Execute() override;
	virtual void SaveAssetAs_Execute() override;
	virtual bool OnRequestClose() override;

protected:
	/** Called when the selection changes in the GraphEditor */
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);

	/**
	 * Called when a node is double clicked
	 *
	 * @param	Node	The Node that was clicked
	 */
	void OnNodeDoubleClicked(class UEdGraphNode* Node);

	/**
	 * Called when a node's title is committed for a rename
	 *
	 * @param	NewText				New title text
	 * @param	CommitInfo			How text was committed
	 * @param	NodeBeingChanged	The node being changed
	 */
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	/**
	 * Verifies that the node text entered is valid for the node
	 *
	 * @param	NewText			New node text
	 * @param	NodeBeingChanged	The node being changed
	 * @param	OutErrorMessage		Error message to display if text is invalid
	 * @return	True if the text is valid, false otherwise
	 */
	bool OnVerifyNodeTextCommit(const FText& NewText, UEdGraphNode* NodeBeingChanged, FText& OutErrorMessage);

	/**
	 * Handles spawning a graph node in the current graph using the passed in chord
	 *
	 * @param	InChord		Chord that was just performed
	 * @param	InPosition	Current cursor position
	 * @param	InGraph		Graph that chord was performed in
	 *
	 * @return	FReply	Whether chord was handled
	 */
	FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph);

	/** Select every node in the graph */
	void SelectAllNodes();
	/** Whether we can select every node */
	bool CanSelectAllNodes() const;

	/** Whether we are able to delete the currently selected nodes */
	bool CanDeleteNodes() const;
	/** Delete only the currently selected nodes that can be duplicated */
	void DeleteSelectedDuplicatableNodes();

	/** Copy the currently selected nodes */
	void CopySelectedNodes();
	/** Whether we are able to copy the currently selected nodes */
	bool CanCopyNodes() const;

	/** Paste the contents of the clipboard */
	void PasteNodes();

	/** Cut the currently selected nodes */
	void CutSelectedNodes();
	/** Whether we are able to cut the currently selected nodes */
	bool CanCutNodes() const;

	/** Duplicate the currently selected nodes */
	void DuplicateNodes();
	/** Whether we are able to duplicate the currently selected nodes */
	bool CanDuplicateNodes() const;

	/** Called to undo the last action */
	void UndoGraphAction();

	/** Called to redo the last undone action */
	void RedoGraphAction();

private:
	/** Builds the toolbar widget for the material editor */
	void ExtendToolbar();

	/** Allows editor to veto the setting of a preview asset */
	virtual bool ApproveSetPreviewAsset(UObject* InAsset) override;

	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();
	
	/** Collects all groups for all material expressions */
	void GetAllMaterialExpressionGroups(TArray<FString>* OutGroups);

	/** Updates the 3D and UI preview viewport visibility based on material domain */
	void UpdatePreviewViewportsVisibility();

public:

private:
	/**
	 * Load editor settings from disk (docking state, window pos/size, option state, etc).
	 */
	void LoadEditorSettings();

	/**
	 * Saves editor settings to disk (docking state, window pos/size, option state, etc).
	 */
	void SaveEditorSettings();

	/** Gets the text in the code view widget */
	FText GetCodeViewText() const;

	/** Copies all the HLSL Code View code to the clipboard */
	FReply CopyCodeViewTextToClipboard();


	/**
	 * Binds our UI commands to delegates
	 */
	void BindCommands();
	
	/** Command for the apply button */
	void OnApply();
	bool OnApplyEnabled() const;
	/** Command for the camera home button */
	void OnCameraHome();
	/** Command for the show unused connectors button */
	void OnShowConnectors();
	bool IsOnShowConnectorsChecked() const;
	/** Command for the Toggle Live Preview button */
	void ToggleLivePreview();
	bool IsToggleLivePreviewChecked() const;
	/** Command for the Toggle Real Time button */
	void ToggleRealTimeExpressions();
	bool IsToggleRealTimeExpressionsChecked() const;
	/** Command for the Refresh all previews button */
	void OnAlwaysRefreshAllPreviews();
	bool IsOnAlwaysRefreshAllPreviews() const;
	/** Command for the stats button */
	void ToggleStats();
	bool IsToggleStatsChecked() const;
	void ToggleReleaseStats();
	bool IsToggleReleaseStatsChecked() const;
	void ToggleBuiltinStats();
	bool IsToggleBuiltinStatsChecked() const;

	/** Mobile stats button. */
	void ToggleMobileStats();
	bool IsToggleMobileStatsChecked() const;
	/** Command for using currently selected texture */
	void OnUseCurrentTexture();
	/** Command for converting nodes to objects */
	void OnConvertObjects();
	/** Command for converting nodes to textures */
	void OnConvertTextures();
	/** Command for previewing a selected node */
	void OnPreviewNode();
	/** Command for toggling real time preview of selected node */
	void OnToggleRealtimePreview();
	/** Command to select nodes downstream of selected node */
	void OnSelectDownstreamNodes();
	/** Command to select nodes upstream of selected node */
	void OnSelectUpstreamNodes();
	/** Command to force a refresh of all previews (triggered by space bar) */
	void OnForceRefreshPreviews();
	/** Create comment node on graph */
	void OnCreateComment();
	/** Create ComponentMask node on graph */
	void OnCreateComponentMaskNode();
	/** Bring up the search tab */
	void OnFindInMaterial();

	/** Will promote selected pin to a parameter of the pin type */
	void OnPromoteToParameter();

	/** Used to know if we can promote selected pin to a parameter of the pin type */
	bool OnCanPromoteToParameter();

	/** Will  return the UClass to create from the Pin Type */
	UClass* GetOnPromoteToParameterClass(UEdGraphPin* TargetPin);

	/** Open documentation for the selected node class */
	void OnGoToDocumentation();
	/** Can we open documentation for the selected node */
	bool CanGoToDocumentation();

	/** Util to try and get doc link for the currently selected node */
	FString GetDocLinkForSelectedNode();

	/** Callback from the Asset Registry when an asset is renamed. */
	void RenameAssetFromRegistry(const FAssetData& InAddedAssetData, const FString& InNewName);

	/** Callback to tell the Material Editor that a materials usage flags have been changed */
	void OnMaterialUsageFlagsChanged(class UMaterial* MaterialThatChanged, int32 FlagThatChanged);

	/** Callback when an asset is imported */
	void OnAssetPostImport(UFactory* InFactory, UObject* InObject);

	void OnVectorParameterDefaultChanged(class UMaterialExpression*, FName ParameterName, const FLinearColor& Value);
	void OnScalarParameterDefaultChanged(class UMaterialExpression*, FName ParameterName, float Value);

	void SetVectorParameterDefaultOnDependentMaterials(FName ParameterName, const FLinearColor& Value, bool bOverride);
	void SetScalarParameterDefaultOnDependentMaterials(FName ParameterName, float, bool bOverride);

	// FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

	// FNotifyHook interface
	virtual void NotifyPreChange(UProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged) override;

	/** Flags the material as dirty */
	void SetMaterialDirty() {bMaterialDirty = true;}

	/** Toggles the collapsed flag of a Material Expression and updates preview */
	void ToggleCollapsed(UMaterialExpression* MaterialExpression);

	/**
	 * Refreshes material expression previews.  Refreshes all previews if bAlwaysRefreshAllPreviews is true.
	 * Otherwise, refreshes only those previews that have a bRealtimePreview of true.
	 */
	void RefreshExpressionPreviews();

	/**
	 * Refreshes the preview for the specified material expression.  Does nothing if the specified expression
	 * has a bRealtimePreview of false.
	 *
	 * @param	MaterialExpression		The material expression to update.
	 */
	void RefreshExpressionPreview(UMaterialExpression* MaterialExpression, bool bRecompile);

	/**
	 * Returns the expression preview for the specified material expression.
	 */
	FMatExpressionPreview* GetExpressionPreview(UMaterialExpression* MaterialExpression, bool& bNewlyCreated);

	/** Pointer to the object that the current color picker is working on. Can be NULL and stale. */
	TWeakObjectPtr<UObject> ColorPickerObject;
	TWeakObjectPtr<UProperty> ColorPickerProperty;

	/** Called before the color picker commits a change. */
	void PreColorPickerCommit(FLinearColor LinearColor);

	/** Called whenever the color picker is used and accepted. */
	void OnColorPickerCommitted(FLinearColor LinearColor);

	/** Create new graph editor widget */
	TSharedRef<class SGraphEditor> CreateGraphEditorWidget();

	/**
	 * Deletes any disconnected material expressions.
	 */
	void CleanUnusedExpressions();

	/**
	 * Displays a warning message to the user if the expressions to remove would cause any issues
	 *
	 * @param NodesToRemove The expression nodes we wish to remove
	 *
	 * @return Whether the user agrees to remove these expressions
	 */
	bool CheckExpressionRemovalWarnings(const TArray<UEdGraphNode*>& NodesToRemove);

	/** Removes the selected expression from the favorites list. */
	void RemoveSelectedExpressionFromFavorites();
	/** Adds the selected expression to the favorites list. */
	void AddSelectedExpressionToFavorites();

private:
	TSharedRef<SDockTab> SpawnTab_Preview(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_MaterialProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_HLSLCode(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Palette(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Stats(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Find(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);

	void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
private:
	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockableTab> > SpawnedToolPanels;

	/** Property View */
	TSharedPtr<class IDetailsView> MaterialDetailsView;

	/** New Graph Editor */
	TSharedPtr<class SGraphEditor> GraphEditor;

	/** Preview Viewport widget */
	TSharedPtr<class SMaterialEditor3DPreviewViewport> PreviewViewport;

	/** Preview viewport widget used for UI materials */
	TSharedPtr<class SMaterialEditorUIPreviewViewport> PreviewUIViewport;

	/** Widget to hold utility components for the HLSL Code View */
	TSharedPtr<SWidget> CodeViewUtility;

	/** Widget for the HLSL Code View */
	TSharedPtr<class SScrollBox> CodeView;
	/** Cached Code for the widget */
	FString HLSLCode;

	/** Tracks whether the code tab is open, so we don't have to update it when closed. */
	TWeakPtr<SDockTab> CodeTab;

	/** Palette of Material Expressions and functions */
	TSharedPtr<class SMaterialPalette> Palette;

	/** Stats log, with the log listing that it reflects */
	TSharedPtr<class SWidget> Stats;
	TSharedPtr<class IMessageLogListing> StatsListing;

	/** Find results log as well as the search filter */
	TSharedPtr<class SFindInMaterial> FindResults;

	/** The current transaction. */
	FScopedTransaction* ScopedTransaction;

	/** If true, always refresh all expression previews.  This overrides UMaterialExpression::bRealtimePreview. */
	bool bAlwaysRefreshAllPreviews;

	/** Material expression previews. */
	TIndirectArray<class FMatExpressionPreview> ExpressionPreviews;

	/** Information about material to show when stats are enabled */
	TArray<TSharedPtr<FMaterialInfo>> MaterialInfoList;

	TArray<FName> OverriddenVectorParametersToRevert;
	TArray<FName> OverriddenScalarParametersToRevert;

	/** If true, don't render connectors that are not connected to anything. */
	bool bHideUnusedConnectors;

	/** If true, the preview material is compiled on every edit of the material. If false, only on Apply. */
	bool bLivePreview;

	/** Just storing this choice for now, not sure what difference it will make to Graph Editor */
	bool bIsRealtime;

	/** If true, show material stats like number of shader instructions. */
	bool bShowStats;

	/** If true, show stats for an empty material. Helps artists to judge the cost of their changes to the graph. */
	bool bShowBuiltinStats;

	/** If true, show material stats and errors for mobile. */
	bool bShowMobileStats;

	/** Command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/**	The tab ids for the material editor */
	static const FName PreviewTabId;		
	static const FName GraphCanvasTabId;	
	static const FName PropertiesTabId;	
	static const FName HLSLCodeTabId;	
	static const FName PaletteTabId;
	static const FName StatsTabId;
	static const FName FindTabId;
	static const FName PreviewSettingsTabId;
};
