// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "UObject/LazyObjectPtr.h"
#include "LandscapeInfo.generated.h"

class ALandscape;
class ALandscapeProxy;
class ALandscapeStreamingProxy;
class ULandscapeComponent;
class ULandscapeLayerInfoObject;
class ULevel;
class UMaterialInstanceConstant;
struct FLandscapeEditorLayerSettings;

/** Structure storing Collision for LandscapeComponent Add */
#if WITH_EDITORONLY_DATA
struct FLandscapeAddCollision
{
	FVector Corners[4];

	FLandscapeAddCollision()
	{
		Corners[0] = Corners[1] = Corners[2] = Corners[3] = FVector::ZeroVector;
	}
};
#endif // WITH_EDITORONLY_DATA

USTRUCT()
struct FLandscapeInfoLayerSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	ULandscapeLayerInfoObject* LayerInfoObj;

	UPROPERTY()
	FName LayerName;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	UMaterialInstanceConstant* ThumbnailMIC;

	UPROPERTY()
	ALandscapeProxy* Owner;

	UPROPERTY(transient)
	int32 DebugColorChannel;

	UPROPERTY(transient)
	uint32 bValid:1;
#endif // WITH_EDITORONLY_DATA

	FLandscapeInfoLayerSettings()
		: LayerInfoObj(nullptr)
		, LayerName(NAME_None)
#if WITH_EDITORONLY_DATA
		, ThumbnailMIC(nullptr)
		, Owner(nullptr)
		, DebugColorChannel(0)
		, bValid(false)
#endif // WITH_EDITORONLY_DATA
	{
	}

	LANDSCAPE_API FLandscapeInfoLayerSettings(ULandscapeLayerInfoObject* InLayerInfo, ALandscapeProxy* InProxy);

	FLandscapeInfoLayerSettings(FName InPlaceholderLayerName, ALandscapeProxy* InProxy)
		: LayerInfoObj(nullptr)
		, LayerName(InPlaceholderLayerName)
#if WITH_EDITORONLY_DATA
		, ThumbnailMIC(nullptr)
		, Owner(InProxy)
		, DebugColorChannel(0)
		, bValid(false)
#endif
	{
	}

	LANDSCAPE_API FName GetLayerName() const;

#if WITH_EDITORONLY_DATA
	LANDSCAPE_API FLandscapeEditorLayerSettings& GetEditorSettings() const;
#endif
};

UCLASS(Transient)
class ULandscapeInfo : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TLazyObjectPtr<ALandscape> LandscapeActor;

	UPROPERTY()
	FGuid LandscapeGuid;

	UPROPERTY()
	int32 ComponentSizeQuads;
	
	UPROPERTY()
	int32 SubsectionSizeQuads;

	UPROPERTY()
	int32 ComponentNumSubsections;
	
	UPROPERTY()
	FVector DrawScale;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FLandscapeInfoLayerSettings> Layers;
#endif // WITH_EDITORONLY_DATA

public:
	/** Map of the offsets (in component space) to the component. Valid in editor only. */
	TMap<FIntPoint, ULandscapeComponent*> XYtoComponentMap;

#if WITH_EDITORONLY_DATA
	/** Lookup map used by the "add component" tool. Only available near valid LandscapeComponents.
	    only for use by the "add component" tool. Todo - move into the tool? */
	TMap<FIntPoint, FLandscapeAddCollision> XYtoAddCollisionMap;
#endif

	UPROPERTY()
	TSet<ALandscapeStreamingProxy*> Proxies;

private:
	TSet<ULandscapeComponent*> SelectedComponents;

	TSet<ULandscapeComponent*> SelectedRegionComponents;

public:
	TMap<FIntPoint,float> SelectedRegion;

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

#if WITH_EDITOR
	// @todo document 
	// all below.
	LANDSCAPE_API void GetComponentsInRegion(int32 X1, int32 Y1, int32 X2, int32 Y2, TSet<ULandscapeComponent*>& OutComponents, bool bOverlap = true) const;
	LANDSCAPE_API bool GetLandscapeExtent(int32& MinX, int32& MinY, int32& MaxX, int32& MaxY) const;
	LANDSCAPE_API void ExportHeightmap(const FString& Filename);
	LANDSCAPE_API void ExportLayer(ULandscapeLayerInfoObject* LayerInfo, const FString& Filename);
	LANDSCAPE_API bool ApplySplines(bool bOnlySelected);
	bool ApplySplinesInternal(bool bOnlySelected, ALandscapeProxy* Landscape);

	LANDSCAPE_API bool GetSelectedExtent(int32& MinX, int32& MinY, int32& MaxX, int32& MaxY) const;
	FVector GetLandscapeCenterPos(float& LengthZ, int32 MinX = MAX_int32, int32 MinY = MAX_int32, int32 MaxX = MIN_int32, int32 MaxY = MIN_int32);
	LANDSCAPE_API bool IsValidPosition(int32 X, int32 Y);
	LANDSCAPE_API void DeleteLayer(ULandscapeLayerInfoObject* LayerInfo, const FName& LayerName);
	LANDSCAPE_API void ReplaceLayer(ULandscapeLayerInfoObject* FromLayerInfo, ULandscapeLayerInfoObject* ToLayerInfo);

	LANDSCAPE_API void UpdateDebugColorMaterial();

	LANDSCAPE_API TSet<ULandscapeComponent*> GetSelectedComponents() const;
	LANDSCAPE_API TSet<ULandscapeComponent*> GetSelectedRegionComponents() const;
	LANDSCAPE_API void UpdateSelectedComponents(TSet<ULandscapeComponent*>& NewComponents, bool bIsComponentwise = true);
	LANDSCAPE_API void SortSelectedComponents();
	LANDSCAPE_API void ClearSelectedRegion(bool bIsComponentwise = true);

	// only for use by the "add component" tool. Todo - move into the tool?
	LANDSCAPE_API void UpdateAllAddCollisions();
	LANDSCAPE_API void UpdateAddCollision(FIntPoint LandscapeKey);

	LANDSCAPE_API FLandscapeEditorLayerSettings& GetLayerEditorSettings(ULandscapeLayerInfoObject* LayerInfo) const;
	LANDSCAPE_API void CreateLayerEditorSettingsFor(ULandscapeLayerInfoObject* LayerInfo);

	LANDSCAPE_API ULandscapeLayerInfoObject* GetLayerInfoByName(FName LayerName, ALandscapeProxy* Owner = nullptr) const;
	LANDSCAPE_API int32 GetLayerInfoIndex(FName LayerName, ALandscapeProxy* Owner = nullptr) const;
	LANDSCAPE_API int32 GetLayerInfoIndex(ULandscapeLayerInfoObject* LayerInfo, ALandscapeProxy* Owner = nullptr) const;
	LANDSCAPE_API bool UpdateLayerInfoMap(ALandscapeProxy* Proxy = nullptr, bool bInvalidate = false);

	/**
	 *  Returns the landscape proxy of this landscape info in the given level (if it exists)
	 *  @param  Level  Level to look in
	 *	@return        Landscape or landscape proxy found in the given level, or null if none
	 */
	LANDSCAPE_API ALandscapeProxy* GetLandscapeProxyForLevel(ULevel* Level) const;

	/**
	 *  Returns landscape which is spawned in the current level that was previously added to this landscape info object
	 *  @param	bRegistered		Whether to consider only registered(visible) landscapes
	 *	@return					Landscape or landscape proxy found in the current level 
	 */
	LANDSCAPE_API ALandscapeProxy* GetCurrentLevelLandscapeProxy(bool bRegistered) const;
	
	/** 
	 *	returns shared landscape or landscape proxy, mostly for transformations
	 *	@todo: should be removed
	 */
	LANDSCAPE_API ALandscapeProxy* GetLandscapeProxy() const;

	/**
	 * Runs the given function on the root landscape actor and all streaming proxies
	 * Most easily used with a lambda as follows:
	 * ForAllLandscapeProxies([](ALandscapeProxy* Proxy)
	 * {
	 *     // Code
	 * });
	 */
	LANDSCAPE_API void ForAllLandscapeProxies(TFunctionRef<void(ALandscapeProxy*)> Fn) const;

	/** Associates passed actor with this info object
 	 *  @param	Proxy		Landscape actor to register
	 *  @param  bMapCheck	Whether to warn about landscape errors
	 */
	LANDSCAPE_API void RegisterActor(ALandscapeProxy* Proxy, bool bMapCheck = false);
	
	/** Deassociates passed actor with this info object*/
	LANDSCAPE_API void UnregisterActor(ALandscapeProxy* Proxy);

	/** Associates passed landscape component with this info object
	 *  @param	Component	Landscape component to register
	 *  @param  bMapCheck	Whether to warn about landscape errors
	 */
	LANDSCAPE_API void RegisterActorComponent(ULandscapeComponent* Component, bool bMapCheck = false);
	
	/** Deassociates passed landscape component with this info object*/
	LANDSCAPE_API void UnregisterActorComponent(ULandscapeComponent* Component);

	/** Resets all actors, proxies, components registrations */
	LANDSCAPE_API void Reset();

	/** Recreate all LandscapeInfo objects in given world
	 *  @param  bMapCheck	Whether to warn about landscape errors
	 */
	LANDSCAPE_API static void RecreateLandscapeInfo(UWorld* InWorld, bool bMapCheck);

	/** 
	 *  Fixes up proxies relative position to landscape actor
	 *  basically makes sure that each LandscapeProxy RootComponent transform reflects LandscapeSectionOffset value
	 *  requires LandscapeActor to be loaded
	 *  Does not work in World composition mode!
	 */
	LANDSCAPE_API void FixupProxiesTransform();
	
	// Update per-component layer whitelists to include the currently painted layers
	LANDSCAPE_API void UpdateComponentLayerWhitelist();

	LANDSCAPE_API void RecreateCollisionComponents();

	LANDSCAPE_API void RemoveXYOffsets();

	/** Postpones landscape textures baking, usually used during landscape painting to avoid hitches */
	LANDSCAPE_API void PostponeTextureBaking();
#endif
};
