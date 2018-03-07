// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureInstanceView.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"

class UPrimitiveComponent;
class UTexture2D;
struct FStreamingViewInfo;
struct FTextureStreamingSettings;

#define MAX_TEXTURE_SIZE (float(1 << (MAX_TEXTURE_MIP_COUNT - 1)))

// Main Thread Job Requirement : find all instance of a component and update it's bound.
// Threaded Job Requirement : get the list of instance texture easily from the list of 

// A constant view on the relationship between textures, components and bounds. 
// Has everything needed for the worker task to compute the required size per texture.
class FTextureInstanceView : public FRefCountedObject
{
public:

	// The bounds are into their own arrays to allow SIMD-friendly processing
	struct FBounds4
	{
		FORCEINLINE FBounds4();

		/** X coordinates for the bounds origin of 4 texture instances */
		FVector4 OriginX;
		/** Y coordinates for the bounds origin of 4 texture instances */
		FVector4 OriginY;
		/** Z coordinates for the bounds origin of 4 texture instances */
		FVector4 OriginZ;

		/** X coordinates for used to compute the distance condition between min and max */
		FVector4 RangeOriginX;
		/** Y coordinates for used to compute the distance condition between min and max */
		FVector4 RangeOriginY;
		/** Z coordinates for used to compute the distance condition between min and max */
		FVector4 RangeOriginZ;

		/** X size of the bounds box extent of 4 texture instances */
		FVector4 ExtentX;
		/** Y size of the bounds box extent of 4 texture instances */
		FVector4 ExtentY;
		/** Z size of the bounds box extent of 4 texture instances */
		FVector4 ExtentZ;

		/** Sphere radii for the bounding sphere of 4 texture instances */
		FVector4 Radius;

		/** The relative box the bound was computed with */
		FUintVector4 PackedRelativeBox;

		/** Minimal distance (between the bounding sphere origin and the view origin) for which this entry is valid */
		FVector4 MinDistanceSq;
		/** Minimal range distance (between the bounding sphere origin and the view origin) for which this entry is valid */
		FVector4 MinRangeSq;
		/** Maximal range distance (between the bounding sphere origin and the view origin) for which this entry is valid */
		FVector4 MaxRangeSq;

		/** Last visibility time for this bound, used for priority */
		FVector4 LastRenderTime; //(FApp::GetCurrentTime() - Component->LastRenderTime);


		void Set(int32 Index, const FBoxSphereBounds& Bounds, uint32 InPackedRelativeBox, float LastRenderTime, const FVector& RangeOrigin, float MinDistance, float MinRange, float MaxRange);
		void UnpackBounds(int32 Index, const FBoxSphereBounds& Bounds);
		void FullUpdate(int32 Index, const FBoxSphereBounds& Bounds, float LastRenderTime);
		FORCEINLINE void UpdateLastRenderTime(int32 Index, float LastRenderTime);

		// Clears entry between 0 and 4
		FORCEINLINE void Clear(int32 Index);

		FORCEINLINE void OffsetBounds(int32 Index, const FVector& Offset);
	};

	struct FElement
	{
		FORCEINLINE FElement();

		const UPrimitiveComponent* Component; // Which component this relates too
		const UTexture2D* Texture;	// Texture, never dereferenced.

		int32 BoundsIndex;		// The Index associated to this component (static component can have several bounds).
		float TexelFactor;		// The texture scale to be applied to this instance.
		bool bForceLoad;		// The texture needs to be force loaded.

		int32 PrevTextureLink;	// The previous element which uses the same texture as this Element. The first element referred by TextureMap will have INDEX_NONE.
		int32 NextTextureLink;	// The next element which uses the same texture as this Element. Last element will have INDEX_NONE

		// Components are always updated as a whole, so individual elements can not be removed. Removing the need for PrevComponentLink.
		int32 NextComponentLink;	// The next element that uses the same component as this Element. The first one is referred by ComponentMap and the last one will have INDEX_NONE.
	};

	/**
 	 * FCompiledElement is a stripped down version of element and is stored in an array instead of using a linked list.
     * It is only used when the data is not expected to change and reduce that cache cost of iterating on all elements.
     **/
	struct FCompiledElement
	{
		FCompiledElement() {}
		FCompiledElement(int32 InBoundsIndex, float InTexelFactor, bool InForceLoad) : BoundsIndex(InBoundsIndex), TexelFactor(InTexelFactor), bForceLoad(InForceLoad) {}

		int32 BoundsIndex;
		float TexelFactor;
		bool bForceLoad;

		FORCEINLINE bool operator==(const FCompiledElement& Rhs) const { return BoundsIndex == Rhs.BoundsIndex && TexelFactor == Rhs.TexelFactor && bForceLoad == Rhs.bForceLoad; }
	};

	struct FTextureDesc
	{
		FTextureDesc(int32 InHeadLink, int32 InLODGroup) : HeadLink(InHeadLink), LODGroup(InLODGroup) {}

		// The index of head element using the texture.
		int32 HeadLink;
		// The LODGroup of the texture, used to performe some tasks async.
		const int32 LODGroup;
	};


	// Iterator processing all elements refering to a texture.
	class FTextureLinkConstIterator
	{
	public:
		FTextureLinkConstIterator(const FTextureInstanceView& InState, const UTexture2D* InTexture);

		FORCEINLINE operator bool() const { return CurrElementIndex != INDEX_NONE; }
		FORCEINLINE void operator++() { CurrElementIndex = State.Elements[CurrElementIndex].NextTextureLink; }

		void OutputToLog(float MaxNormalizedSize, float MaxNormalizedSize_VisibleOnly, const TCHAR* Prefix) const;

		FORCEINLINE int32 GetBoundsIndex() const { return State.Elements[CurrElementIndex].BoundsIndex; }
		FORCEINLINE float GetTexelFactor() const { return State.Elements[CurrElementIndex].TexelFactor; }
		FORCEINLINE bool GetForceLoad() const { return State.Elements[CurrElementIndex].bForceLoad; }

		FBoxSphereBounds GetBounds() const;

	protected:

		FORCEINLINE const UPrimitiveComponent* GetComponent() const { return State.Elements[CurrElementIndex].Component; }

		const FTextureInstanceView& State;
		int32 CurrElementIndex;
	};

	class FTextureLinkIterator : public FTextureLinkConstIterator
	{
	public:
		FTextureLinkIterator(FTextureInstanceView& InState, const UTexture2D* InTexture) : FTextureLinkConstIterator(InState, InTexture) {}

		FORCEINLINE void ClampTexelFactor(float CMin, float CMax)
		{ 
			float& TexelFactor = const_cast<FTextureInstanceView&>(State).Elements[CurrElementIndex].TexelFactor;
			TexelFactor = FMath::Clamp<float>(TexelFactor, CMin, CMax);
		}
	};

	class FTextureIterator
	{
	public:
		FTextureIterator(const FTextureInstanceView& InState) : MapIt(InState.TextureMap) {}

		operator bool() const { return (bool)MapIt; }
		void operator++() { ++MapIt; }

		const UTexture2D* operator*() const { return MapIt.Key(); }
		int32 GetLODGroup() const { return MapIt.Value().LODGroup; }

	private:

		TMap<const UTexture2D*, FTextureDesc>::TConstIterator MapIt;
	};

	FORCEINLINE int32 NumBounds4() const { return Bounds4.Num(); }
	FORCEINLINE const FBounds4& GetBounds4(int32 Bounds4Index ) const {  return Bounds4[Bounds4Index]; }

	FORCEINLINE FTextureLinkIterator GetElementIterator(const UTexture2D* InTexture ) {  return FTextureLinkIterator(*this, InTexture); }
	FORCEINLINE FTextureLinkConstIterator GetElementIterator(const UTexture2D* InTexture ) const {  return FTextureLinkConstIterator(*this, InTexture); }
	FORCEINLINE FTextureIterator GetTextureIterator( ) const {  return FTextureIterator(*this); }

	// Whether or not this state has compiled elements.
	bool HasCompiledElements() const { return CompiledTextureMap.Num() != 0; }
	// If this has compiled elements, return the array relate to a given texture.
	const TArray<FCompiledElement>* GetCompiledElements(const UTexture2D* Texture) const { return CompiledTextureMap.Find(Texture); }

	static TRefCountPtr<const FTextureInstanceView> CreateView(const FTextureInstanceView* RefView);
	static TRefCountPtr<FTextureInstanceView> CreateViewWithUninitializedBounds(const FTextureInstanceView* RefView);
	static void SwapData(FTextureInstanceView* Lfs, FTextureInstanceView* Rhs);

protected:

	TArray<FBounds4> Bounds4;

	TArray<FElement> Elements;

	TMap<const UTexture2D*, FTextureDesc> TextureMap;

	// CompiledTextureMap is used to iterate more quickly on each elements by avoiding the linked list indirections.
	TMap<const UTexture2D*, TArray<FCompiledElement> > CompiledTextureMap;
};

// Data used to compute visibility
class FTextureInstanceAsyncView
{
public:

	FTextureInstanceAsyncView() {}

	FTextureInstanceAsyncView(const FTextureInstanceView* InView) : View(InView) {}

	void UpdateBoundSizes_Async(const TArray<FStreamingViewInfo>& ViewInfos, float LastUpdateTime, const FTextureStreamingSettings& Settings);

	// MaxSize : Biggest texture size for all instances.
	// MaxSize_VisibleOnly : Biggest texture size for visble instances only.
	void GetTexelSize(const UTexture2D* InTexture, float& MaxSize, float& MaxSize_VisibleOnly, const TCHAR* LogPrefix) const;

	bool HasTextureReferences(const UTexture2D* InTexture) const;

	// Release the data now as this is expensive.
	void OnTaskDone() { BoundsViewInfo.Empty(); }

private:

	TRefCountPtr<const FTextureInstanceView> View;

	struct FBoundsViewInfo
	{
		/** The biggest normalized size (ScreenSize / Distance) accross all view.*/
		float MaxNormalizedSize;
		/*
		 * The biggest normalized size accross all view for visible instances only.
		 * Visible instances are the one that are in range and also that have been seen recently.
		 */
		float MaxNormalizedSize_VisibleOnly;
	};

	// Normalized Texel Factors for each bounds and view. This is the data built by ComputeBoundsViewInfos
	// @TODO : store data for different views continuously to improve reads.
	TArray<FBoundsViewInfo> BoundsViewInfo;

	void ProcessElement(const FBoundsViewInfo& BoundsVieWInfo, float TexelFactor, bool bForceLoad, float& MaxSize, float& MaxSize_VisibleOnly) const;
};
