// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "AI/Navigation/NavAreas/NavArea.h"

class UBrushComponent;
class UPrimitiveComponent;

template<typename InElementType> class TNavStatArray;

struct ENGINE_API FNavigationModifier
{
	FNavigationModifier() : bHasMetaAreas(false) {}
	FORCEINLINE bool HasMetaAreas() const { return !!bHasMetaAreas; }

protected:
	/** set to true if any of areas used by this modifier is a meta nav area (UNavAreaMeta subclass)*/
	int32 bHasMetaAreas : 1;
};

namespace ENavigationShapeType
{
	enum Type
	{
		Unknown,
		Cylinder,
		Box,
		Convex,
	};
}

namespace ENavigationAreaMode
{
	enum Type
	{
		// apply area modifier on all voxels in bounds 
		Apply,
		
		// apply area modifier only on those voxels in bounds that are matching replace area Id
		Replace,

		// apply area modifier on all voxels in bounds, performed during low area prepass (see: ARecastNavMesh.bMarkLowHeightAreas)
		// (ReplaceInLowPass: mark ONLY "low" voxels that will be removed after prepass, ApplyInLowPass: mark all voxels, including "low" ones)
		ApplyInLowPass,

		// apply area modifier only on those voxels in bounds that are matching replace area Id, performed during low area prepass (see: ARecastNavMesh.bMarkLowHeightAreas)
		// (ReplaceInLowPass: mark ONLY "low" voxels that will be removed after prepass, ApplyInLowPass: mark all voxels, including "low" ones)
		ReplaceInLowPass,
	};
}

namespace ENavigationCoordSystem
{
	enum Type
	{
		Unreal,
		Recast,
	};
}

/** Area modifier: cylinder shape */
struct FCylinderNavAreaData
{
	FVector Origin;
	float Radius;
	float Height;
};

/** Area modifier: box shape (AABB) */
struct FBoxNavAreaData
{
	FVector Origin;
	FVector Extent;
};

struct FConvexNavAreaData
{
	TArray<FVector> Points;
	float MinZ;
	float MaxZ;
};

/** Area modifier: base */
struct ENGINE_API FAreaNavModifier : public FNavigationModifier
{
	/** transient value used for navigation modifiers sorting. If < 0 then not set*/
	float Cost;
	float FixedCost;

	FAreaNavModifier() : Cost(0.0f), FixedCost(0.0f), Bounds(ForceInitToZero), ShapeType(ENavigationShapeType::Unknown), ApplyMode(ENavigationAreaMode::Apply), bIncludeAgentHeight(false) {}
	FAreaNavModifier(float Radius, float Height, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> AreaClass);
	FAreaNavModifier(const FVector& Extent, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> AreaClass);
	FAreaNavModifier(const FBox& Box, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> AreaClass);
	FAreaNavModifier(const TArray<FVector>& Points, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> AreaClass);
	FAreaNavModifier(const TArray<FVector>& Points, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> AreaClass);
	FAreaNavModifier(const TNavStatArray<FVector>& Points, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavArea> AreaClass);
	FAreaNavModifier(const UBrushComponent* BrushComponent, const TSubclassOf<UNavArea> AreaClass);

	FORCEINLINE const FBox& GetBounds() const { return Bounds; }
	FORCEINLINE ENavigationShapeType::Type GetShapeType() const { return ShapeType; }
	FORCEINLINE ENavigationAreaMode::Type GetApplyMode() const { return ApplyMode; }
	FORCEINLINE bool ShouldIncludeAgentHeight() const { return bIncludeAgentHeight; }
	FORCEINLINE void SetIncludeAgentHeight(bool bInclude) { bIncludeAgentHeight = bInclude; }
	FORCEINLINE const TSubclassOf<UNavArea> GetAreaClass() const { return TSubclassOf<UNavArea>(AreaClassOb.Get()); }
	FORCEINLINE const TSubclassOf<UNavArea> GetAreaClassToReplace() const { return TSubclassOf<UNavArea>(ReplaceAreaClassOb.Get()); }

	/** navigation area applied by this modifier */
	void SetAreaClass(const TSubclassOf<UNavArea> AreaClass);

	/** operation mode, ReplaceInLowPass will always automatically use UNavArea_LowHeight as ReplaceAreaClass! */
	void SetApplyMode(ENavigationAreaMode::Type InApplyMode);
	
	/** additional class for used by some ApplyModes, setting it will automatically change ApplyMode to keep backwards compatibility! */
	void SetAreaClassToReplace(const TSubclassOf<UNavArea> AreaClass);

	void GetCylinder(FCylinderNavAreaData& Data) const;
	void GetBox(FBoxNavAreaData& Data) const;
	void GetConvex(FConvexNavAreaData& Data) const;

protected:
	/** this should take a value of a game specific navigation modifier	*/
	TWeakObjectPtr<UClass> AreaClassOb;
	TWeakObjectPtr<UClass> ReplaceAreaClassOb;
	FBox Bounds;
	
	TArray<FVector> Points;
	TEnumAsByte<ENavigationShapeType::Type> ShapeType;
	TEnumAsByte<ENavigationAreaMode::Type> ApplyMode;

	/** if set, area shape will be extended by agent's height to cover area underneath like regular colliding geometry */
	uint8 bIncludeAgentHeight : 1;

	void Init(const TSubclassOf<UNavArea> InAreaClass);
	void SetConvex(const FVector* InPoints, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld);
	void SetBox(const FBox& Box, const FTransform& LocalToWorld);
};

/**
 *	This modifier allows defining ad-hoc navigation links defining 
 *	connections in an straightforward way.
 */
struct ENGINE_API FSimpleLinkNavModifier : public FNavigationModifier
{
	/** use Set/Append/Add function to update links, they will take care of meta areas */
	TArray<FNavigationLink> Links;
	TArray<FNavigationSegmentLink> SegmentLinks;
	FTransform LocalToWorld;
	int32 UserId;

	FSimpleLinkNavModifier() 
		: bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
	}

	FSimpleLinkNavModifier(const FNavigationLink& InLink, const FTransform& InLocalToWorld) 
		: LocalToWorld(InLocalToWorld)
		, bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
		UserId = InLink.UserId;
		AddLink(InLink);
	}	

	FSimpleLinkNavModifier(const TArray<FNavigationLink>& InLinks, const FTransform& InLocalToWorld) 
		: LocalToWorld(InLocalToWorld)
		, bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
		if (InLinks.Num() > 0)
		{
			UserId = InLinks[0].UserId;
			SetLinks(InLinks);
		}
	}

	FSimpleLinkNavModifier(const FNavigationSegmentLink& InLink, const FTransform& InLocalToWorld) 
		: LocalToWorld(InLocalToWorld)
		, bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
		UserId = InLink.UserId;
		AddSegmentLink(InLink);
	}	

	FSimpleLinkNavModifier(const TArray<FNavigationSegmentLink>& InSegmentLinks, const FTransform& InLocalToWorld) 
		: LocalToWorld(InLocalToWorld)
		, bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
		if (InSegmentLinks.Num() > 0)
		{
			UserId = InSegmentLinks[0].UserId;
			SetSegmentLinks(InSegmentLinks);
		}
	}

	FORCEINLINE bool HasFallDownLinks() const { return !!bHasFallDownLinks; }
	void SetLinks(const TArray<FNavigationLink>& InLinks);
	void SetSegmentLinks(const TArray<FNavigationSegmentLink>& InLinks);
	void AppendLinks(const TArray<FNavigationLink>& InLinks);
	void AppendSegmentLinks(const TArray<FNavigationSegmentLink>& InLinks);
	void AddLink(const FNavigationLink& InLink);
	void AddSegmentLink(const FNavigationSegmentLink& InLink);
	void UpdateFlags();

protected:
	/** set to true if any of links stored is a "fall down" link, i.e. requires vertical snapping to geometry */
	int32 bHasFallDownLinks : 1;
	int32 bHasMetaAreasPoint : 1;
	int32 bHasMetaAreasSegment : 1;
};

struct ENGINE_API FCustomLinkNavModifier : public FNavigationModifier
{
	FTransform LocalToWorld;

	void Set(TSubclassOf<UNavLinkDefinition> LinkDefinitionClass, const FTransform& InLocalToWorld);
	FORCEINLINE const TSubclassOf<UNavLinkDefinition> GetNavLinkClass() const { return TSubclassOf<UNavLinkDefinition>(LinkDefinitionClassOb.Get()); }

protected:
	TWeakObjectPtr<UClass> LinkDefinitionClassOb;
};

struct ENGINE_API FCompositeNavModifier : public FNavigationModifier
{
	FCompositeNavModifier() : bHasPotentialLinks(false), bAdjustHeight(false) {}

	void Shrink();
	void Reset();
	void Empty();

	FORCEINLINE bool IsEmpty() const 
	{ 
		return (Areas.Num() == 0) && (SimpleLinks.Num() == 0) && (CustomLinks.Num() == 0);
	}

	void Add(const FAreaNavModifier& Area)
	{
		Areas.Add(Area);
		bHasMetaAreas |= Area.HasMetaAreas(); 
		bAdjustHeight |= Area.ShouldIncludeAgentHeight();
	}

	void Add(const FSimpleLinkNavModifier& Link)
	{
		SimpleLinks.Add(Link);
		bHasMetaAreas |= Link.HasMetaAreas(); 
	}

	void Add(const FCustomLinkNavModifier& Link)
	{
		CustomLinks.Add(Link); 
		bHasMetaAreas |= Link.HasMetaAreas(); 
	}

	void Add(const FCompositeNavModifier& Modifiers)
	{
		Areas.Append(Modifiers.Areas); 
		SimpleLinks.Append(Modifiers.SimpleLinks); 
		CustomLinks.Append(Modifiers.CustomLinks); 
		bHasMetaAreas |= Modifiers.bHasMetaAreas; 
		bAdjustHeight |= Modifiers.HasAgentHeightAdjust();
	}

	void CreateAreaModifiers(const UPrimitiveComponent* PrimComp, const TSubclassOf<UNavArea> AreaClass);

	FORCEINLINE const TArray<FAreaNavModifier>& GetAreas() const { return Areas; }
	FORCEINLINE const TArray<FSimpleLinkNavModifier>& GetSimpleLinks() const { return SimpleLinks; }
	FORCEINLINE const TArray<FCustomLinkNavModifier>& GetCustomLinks() const { return CustomLinks; }
	
	FORCEINLINE bool HasLinks() const { return (SimpleLinks.Num() > 0) || (CustomLinks.Num() > 0); }
	FORCEINLINE bool HasPotentialLinks() const { return bHasPotentialLinks; }
	FORCEINLINE bool HasAgentHeightAdjust() const { return bAdjustHeight; }
	FORCEINLINE bool HasAreas() const { return Areas.Num() > 0; }

	FORCEINLINE void ReserveForAdditionalAreas(int32 AdditionalElementsCount) { Areas.Reserve(Areas.Num() + AdditionalElementsCount); }

	void MarkPotentialLinks() { bHasPotentialLinks = true; }

	/** returns a copy of Modifier */
	FCompositeNavModifier GetInstantiatedMetaModifier(const struct FNavAgentProperties* NavAgent, TWeakObjectPtr<UObject> WeakOwnerPtr) const;
	uint32 GetAllocatedSize() const;

	bool HasPerInstanceTransforms() const;
	// Should be called only on game thread
	void GetPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& PerInstanceTransforms) const;

public:
	// Gathers per instance data for navigation area modifiers in a specified area box
	FNavDataPerInstanceTransformDelegate NavDataPerInstanceTransformDelegate;

private:
	TArray<FAreaNavModifier> Areas;
	TArray<FSimpleLinkNavModifier> SimpleLinks;
	TArray<FCustomLinkNavModifier> CustomLinks;
	uint32 bHasPotentialLinks : 1;
	uint32 bAdjustHeight : 1;
};
