// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMeshPainter.h"
#include "SPaintModeWidget.h"
#include "MeshPaintTypes.h"
#include "Engine/StaticMesh.h"

/** struct used to store the color data copied from mesh instance to mesh instance */
struct FPerLODVertexColorData
{
	TArray< FColor > ColorsByIndex;
	TMap<FVector, FColor> ColorsByPosition;
};

/** struct used to store the color data copied from mesh component to mesh component */
struct FPerComponentVertexColorData
{
	FPerComponentVertexColorData(const UStaticMesh* InStaticMesh, int32 InComponentIndex)
		: OriginalMesh(InStaticMesh)
		, ComponentIndex(InComponentIndex)
	{
	}

	/** We match up components by the mesh they use */
	TWeakObjectPtr<const UStaticMesh> OriginalMesh;

	/** We also match by component index */
	int32 ComponentIndex;

	/** Vertex colors by LOD */
	TArray<FPerLODVertexColorData> PerLODVertexColorData;
};

/** Struct to hold MeshPaint settings on a per mesh basis */
struct FInstanceTexturePaintSettings
{
	UTexture2D* SelectedTexture;
	int32 SelectedUVChannel;

	FInstanceTexturePaintSettings()
		: SelectedTexture(nullptr)
		, SelectedUVChannel(0)
	{}
	FInstanceTexturePaintSettings(UTexture2D* InSelectedTexture, int32 InSelectedUVSet)
		: SelectedTexture(InSelectedTexture)
		, SelectedUVChannel(InSelectedUVSet)
	{}

	void operator=(const FInstanceTexturePaintSettings& SrcSettings)
	{
		SelectedTexture = SrcSettings.SelectedTexture;
		SelectedUVChannel = SrcSettings.SelectedUVChannel;
	}
};

class UPaintModeSettings;
class IMeshPaintGeometryAdapter;
class FMeshPaintParameters;
struct FAssetData;
class SWidget;

struct FPaintableTexture;
struct FPaintTexture2DData;
struct FTexturePaintMeshSectionInfo;
struct FPerVertexPaintActionArgs;

/** Painter class used by the level viewport mesh painting mode */
class FPaintModePainter : public IMeshPainter
{
	friend class FTexturePaintSettingsCustomization;
	friend class FVertexPaintSettingsCustomization;
	friend class SPaintModeWidget;
protected:
	FPaintModePainter();
	~FPaintModePainter();
	void Init();

	void RegisterTexturePaintCommands();
	void RegisterVertexPaintCommands();
public:
	static FPaintModePainter* Get();
	TSharedPtr<FUICommandList> GetUICommandList();
	
	/** Begin IMeshPainter overrides */
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;		
	virtual void Refresh() override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void FinishPainting() override;	
	virtual void Reset() override;
	virtual TSharedPtr<IMeshPaintGeometryAdapter> GetMeshAdapterForComponent(const UMeshComponent* Component) override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual void RegisterCommands(TSharedRef<FUICommandList> CommandList) override;
	virtual void UnregisterCommands(TSharedRef<FUICommandList> CommandList) override;
	virtual const FHitResult GetHitResult(const FVector& Origin, const FVector& Direction) override;
	virtual void ActorSelected(AActor* Actor) override;
	virtual void ActorDeselected(AActor* Actor) override;
	virtual UPaintBrushSettings* GetBrushSettings() override;
	virtual UMeshPaintSettings* GetPainterSettings() override;
	virtual TSharedPtr<SWidget> GetWidget() override;
	/** End IMeshPainter overrides */
	
protected:
	/** Override from IMeshPainter used for applying actual painting */
	virtual bool PaintInternal(const FVector& InCameraOrigin, const FVector& InRayOrigin, const FVector& InRayDirection, EMeshPaintAction PaintAction, float PaintStrength) override;

	/** Per vertex action function used for painting vertex colors */
	void ApplyVertexColor(FPerVertexPaintActionArgs& Args, int32 VertexIndex, FMeshPaintParameters Parameters);

	/** Per triangle action function used for retrieving triangle eligible for texture painting */
	void GatherTextureTriangles(IMeshPaintGeometryAdapter* Adapter, int32 TriangleIndex, const int32 VertexIndices[3], TArray<FTexturePaintTriangleInfo>* TriangleInfo, TArray<FTexturePaintMeshSectionInfo>* SectionInfos, int32 UVChannelIndex);
protected:
	/** Functions to determine whether or not an instance vertex color action is valid */
	bool DoesRequireVertexColorsFixup() const;
	bool CanRemoveInstanceColors() const;
	bool CanPasteInstanceVertexColors() const;
	bool CanCopyInstanceVertexColors() const;

	/** Functions to determine whether or not an vertex color action is valid */
	bool CanSaveMeshPackages() const;
	bool SelectionContainsValidAdapters() const;
	bool CanPropagateVertexColors() const;
	bool CanSaveModifiedTextures() const;

	/** Checks whether or not the given asset should not be shown in the list of textures to paint on */
	bool ShouldFilterTextureAsset(const FAssetData& AssetData) const;

	/** Callback for when the user changes the texture to paint on */
	void PaintTextureChanged(const FAssetData& AssetData);
	
	/** Returns the maximum LOD index for the mesh we paint */
	int32 GetMaxLODIndexToPaint() const;

	/** Callback for when the user changes whether or not vertex color should be painted to a specific LOD or LOD 0 and propagated */
	void LODPaintStateChanged(const bool bLODPaintingEnabled);

	void PaintLODChanged();

	/** Returns the maximum UV index for the mesh we paint */
	int32 GetMaxUVIndexToPaint() const;
	
	/** Retrieves per-instnace texture paint settings instance for the given component */
	FInstanceTexturePaintSettings& AddOrRetrieveInstanceTexturePaintSettings(UMeshComponent* Component);
	
	/** Functions for retrieving and resetting cached paint data */
	void CacheSelectionData();
	void CacheTexturePaintData();
	void ResetPaintingState();
	/** Copy and pasting functionality from and to a StaticMeshComponent (currently only supports per instance)*/
	void CopyVertexColors();
	void PasteVertexColors();
	void FixVertexColors();
	void RemoveVertexColors();
	
	/** Checks whether or not the current selection contains components which reference the same (static/skeletal)-mesh */
	bool ContainsDuplicateMeshes(TArray<UMeshComponent*>& Components) const;
	
	/** Returns the instance of ComponentClass found in the current Editor selection */
	template<typename ComponentClass>
	TArray<ComponentClass*> GetSelectedComponents() const;
protected:
	/** Starts painting a texture */
	void StartPaintingTexture(UMeshComponent* InMeshComponent, const IMeshPaintGeometryAdapter& GeometryInfo);

	/** Paints on a texture */
	void PaintTexture(const FMeshPaintParameters& InParams,
		TArray<FTexturePaintTriangleInfo>& InInfluencedTriangles,
		const IMeshPaintGeometryAdapter& GeometryInfo);

	/** Finishes painting a texture */
	void FinishPaintingTexture();

	/**
	* Used to get a reference to data entry associated with the texture.  Will create a new entry if one is not found.
	*
	* @param	inTexture 		The texture we want to retrieve data for.
	* @return					Returns a reference to the paint data associated with the texture.  This reference
	*								is only valid until the next change to any key in the map.  Will return NULL only when inTexture is NULL.
	*/
	FPaintTexture2DData* GetPaintTargetData(const UTexture2D* inTexture);

	/**
	* Used to add an entry to to our paint target data.
	*
	* @param	inTexture 		The texture we want to create data for.
	* @return					Returns a reference to the newly created entry.  If an entry for the input texture already exists it will be returned instead.
	*								Will return NULL only when inTexture is NULL.   This reference is only valid until the next change to any key in the map.
	*
	*/
	FPaintTexture2DData* AddPaintTargetData(UTexture2D* inTexture);

	/**
	* Used to get the original texture that was overridden with a render target texture.
	*
	* @param	inTexture 		The render target that was used to override the original texture.
	* @return					Returns a reference to texture that was overridden with the input render target texture.  Returns NULL if we don't find anything.
	*
	*/
	UTexture2D* GetOriginalTextureFromRenderTarget(UTextureRenderTarget2D* inTexture);
	
	/**
	* Used to commit all paint changes to corresponding target textures.
	* @param	bShouldTriggerUIRefresh		Flag to trigger a UI Refresh if any textures have been modified (defaults to true)
	*/
	void CommitAllPaintedTextures();

	/** Clears all texture overrides, removing any pending texture paint changes */
	void ClearAllTextureOverrides();

	/** Sets all required texture overrides for the mesh component using the adapter */
	void SetAllTextureOverrides(const IMeshPaintGeometryAdapter& GeometryInfo, UMeshComponent* InMeshComponent);

	/** Set a specific texture override using a mesh adapter */
	void SetSpecificTextureOverrideForMesh(const IMeshPaintGeometryAdapter& GeometryInfo, UTexture2D* Texture);

	/** Used to tell the texture paint system that we will need to restore the rendertargets */
	void RestoreRenderTargets();

	/** Returns the number of texture that require a commit. */
	int32 GetNumberOfPendingPaintChanges() const;

private:
	/** Widget representing the state and settings for the painter */
	TSharedPtr<SPaintModeWidget> Widget;

	/** Painting settings */
	UPaintModeSettings* PaintSettings;
	/** Basic set of brush settings */
	UPaintBrushSettings* BrushSettings;

	/** Forces a specific LOD level to be rendered for the selected mesh components */
	void ApplyForcedLODIndex(int32 ForcedLODIndex);

	/** Updates the paint targets based on property changes on actors in the scene */
	void UpdatePaintTargets(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent);

	/** Vertex color action callbacks */
	void FillWithVertexColor();
	void PropagateVertexColorsToAsset();
	void ImportVertexColors();
	void SavePaintedAssets();
	void SaveModifiedTextures();
protected:	
	/** Texture paint state */
	/** Textures eligible for painting retrieved from the current selection */
	TArray<FPaintableTexture> PaintableTextures;
	/** Cached / stored instance texture paint settings for selected components */
	TMap<UMeshComponent*, FInstanceTexturePaintSettings> ComponentToTexturePaintSettingsMap;

	/** Temporary render target used to draw incremental paint to */
	UTextureRenderTarget2D* BrushRenderTargetTexture;

	/** Temporary render target used to store a mask of the affected paint region, updated every time we add incremental texture paint */
	UTextureRenderTarget2D* BrushMaskRenderTargetTexture;

	/** Temporary render target used to store generated mask for texture seams, we create this by projecting object triangles into texture space using the selected UV channel */
	UTextureRenderTarget2D* SeamMaskRenderTargetTexture;

	/** Stores data associated with our paint target textures */
	TMap< UTexture2D*, FPaintTexture2DData > PaintTargetData;

	/** Texture paint: Will hold a list of texture items that we can paint on */
	TArray<FTextureTargetListInfo> TexturePaintTargetList;
	
	/** Texture paint: The mesh components that we're currently painting */
	UMeshComponent* TexturePaintingCurrentMeshComponent;

	/** The original texture that we're painting */
	UTexture2D* PaintingTexture2D;

	/** True if we need to generate a texture seam mask used for texture dilation */
	bool bGenerateSeamMask;
	
	/** Used to store a flag that will tell the tick function to restore data to our rendertargets after they have been invalidated by a viewport resize. */
	bool bDoRestoreRenTargets;

	/** A map of the currently selected actors against info required for painting (selected material index etc)*/
	TMap<TWeakObjectPtr<AActor>, FMeshSelectedMaterialInfo> CurrentlySelectedActorsMaterialInfo;

	/** The currently selected actor, used to refer into the Map of Selected actor info */
	TWeakObjectPtr<AActor> ActorBeingEdited;
	// End texture paint state

	// Painter state
	/** Flag for updating cached data */
	bool bRefreshCachedData;
	/** Map of geometry adapters for each selected mesh component */
	TMap<UMeshComponent*, TSharedPtr<IMeshPaintGeometryAdapter>> ComponentToAdapterMap;
	// End painter state

	// Vertex paint state
	/** Current LOD index used for painting / forcing */
	int32 CachedLODIndex;
	/** Whether or not a specific LOD level should be forced */
	bool bCachedForceLOD;
	/** Flag whether or not the current selection contains per-LOD specific vertex colors */
	bool bSelectionContainsPerLODColors;
	/** Mesh components within the current selection which are eligible for painting */
	TArray<UMeshComponent*> PaintableComponents;
	/** Contains copied vertex color data */
	TArray<FPerComponentVertexColorData> CopiedColorsByComponent;
	// End vertex paint state

	/** UI command list object */
	TSharedPtr<FUICommandList> UICommandList;
};
