// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 *	This will hold all of our enums and types and such that we need to
 *	use in multiple files where the enum can't be mapped to a specific file.
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "EngineTypes.generated.h"

class AActor;
class UDecalComponent;
class UPhysicalMaterial;
class UPrimitiveComponent;
class USceneComponent;

/**
 * Default number of components to expect in TInlineAllocators used with AActor component arrays.
 * Used by engine code to try to avoid allocations in AActor::GetComponents(), among others.
 */
enum { NumInlinedActorComponents = 24 };

UENUM()
enum EAspectRatioAxisConstraint
{
	AspectRatio_MaintainYFOV UMETA(DisplayName="Maintain Y-Axis FOV"),
	AspectRatio_MaintainXFOV UMETA(DisplayName="Maintain X-Axis FOV"),
	AspectRatio_MajorAxisFOV UMETA(DisplayName="Maintain Major Axis FOV"),
	AspectRatio_MAX,
};


/** The type of metric we want about the actor **/
UENUM()
enum EActorMetricsType
{
	METRICS_VERTS,
	METRICS_TRIS,
	METRICS_SECTIONS,
	METRICS_MAX,
};


/** Return values for UEngine::Browse. */
namespace EBrowseReturnVal
{
	enum Type
	{
		/** Successfully browsed to a new map. */
		Success,
		/** Immediately failed to browse. */
		Failure,
		/** A connection is pending. */
		Pending,
	};
}

/** Rules for attaching components - needs to be kept synced to EDetachmentRule */
UENUM()
enum class EAttachmentRule : uint8
{
	/** Keeps current relative transform as the relative transform to the new parent. */
	KeepRelative,

	/** Automatically calculates the relative transform such that the attached component maintains the same world transform. */
	KeepWorld,

	/** Snaps transform to the attach point */
	SnapToTarget,
};

/** Rules for attaching components */
struct ENGINE_API FAttachmentTransformRules
{
	/** Various preset attachment rules. Note that these default rules do NOT by default weld simulated bodies */
	static FAttachmentTransformRules KeepRelativeTransform;
	static FAttachmentTransformRules KeepWorldTransform;
	static FAttachmentTransformRules SnapToTargetNotIncludingScale;
	static FAttachmentTransformRules SnapToTargetIncludingScale;

	FAttachmentTransformRules(EAttachmentRule InRule, bool bInWeldSimulatedBodies)
		: LocationRule(InRule)
		, RotationRule(InRule)
		, ScaleRule(InRule)
		, bWeldSimulatedBodies(bInWeldSimulatedBodies)
	{}

	FAttachmentTransformRules(EAttachmentRule InLocationRule, EAttachmentRule InRotationRule, EAttachmentRule InScaleRule, bool bInWeldSimulatedBodies)
		: LocationRule(InLocationRule)
		, RotationRule(InRotationRule)
		, ScaleRule(InScaleRule)
		, bWeldSimulatedBodies(bInWeldSimulatedBodies)
	{}

	/** The rule to apply to location when attaching */
	EAttachmentRule LocationRule;

	/** The rule to apply to rotation when attaching */
	EAttachmentRule RotationRule;

	/** The rule to apply to scale when attaching */
	EAttachmentRule ScaleRule;

	/** Whether to weld simulated bodies together when attaching */
	bool bWeldSimulatedBodies;
};

/** Rules for detaching components - needs to be kept synced to EAttachmentRule */
UENUM()
enum class EDetachmentRule : uint8
{
	/** Keeps current relative transform. */
	KeepRelative,

	/** Automatically calculates the relative transform such that the detached component maintains the same world transform. */
	KeepWorld,
};

/** Rules for detaching components */
struct ENGINE_API FDetachmentTransformRules
{
	/** Various preset detachment rules */
	static FDetachmentTransformRules KeepRelativeTransform;
	static FDetachmentTransformRules KeepWorldTransform;

	FDetachmentTransformRules(EDetachmentRule InRule, bool bInCallModify)
		: LocationRule(InRule)
		, RotationRule(InRule)
		, ScaleRule(InRule)
		, bCallModify(bInCallModify)
	{}

	FDetachmentTransformRules(EDetachmentRule InLocationRule, EDetachmentRule InRotationRule, EDetachmentRule InScaleRule, bool bInCallModify)
		: LocationRule(InLocationRule)
		, RotationRule(InRotationRule)
		, ScaleRule(InScaleRule)
		, bCallModify(bInCallModify)
	{}

	FDetachmentTransformRules(const FAttachmentTransformRules& AttachmentRules, bool bInCallModify)
		: LocationRule(AttachmentRules.LocationRule == EAttachmentRule::KeepRelative ? EDetachmentRule::KeepRelative : EDetachmentRule::KeepWorld)
		, RotationRule(AttachmentRules.RotationRule == EAttachmentRule::KeepRelative ? EDetachmentRule::KeepRelative : EDetachmentRule::KeepWorld)
		, ScaleRule(AttachmentRules.ScaleRule == EAttachmentRule::KeepRelative ? EDetachmentRule::KeepRelative : EDetachmentRule::KeepWorld)
		, bCallModify(bInCallModify)
	{}

	/** The rule to apply to location when detaching */
	EDetachmentRule LocationRule;

	/** The rule to apply to rotation when detaching */
	EDetachmentRule RotationRule;

	/** The rule to apply to scale when detaching */
	EDetachmentRule ScaleRule;

	/** Whether to call Modify() on the components concerned when detaching */
	bool bCallModify;
};

UENUM()
namespace EAttachLocation
{
	enum Type
	{
		/** Keeps current relative transform as the relative transform to the new parent. */
		KeepRelativeOffset,
		
		/** Automatically calculates the relative transform such that the attached component maintains the same world transform. */
		KeepWorldPosition,

		/** Snaps location and rotation to the attach point. Calculates the relative scale so that the final world scale of the component remains the same. */
		SnapToTarget					UMETA(DisplayName = "Snap to Target, Keep World Scale"),
		
		/** Snaps entire transform to target, including scale. */
		SnapToTargetIncludingScale		UMETA(DisplayName = "Snap to Target, Including Scale"),
	};
}


/**
 * A priority for sorting scene elements by depth.
 * Elements with higher priority occlude elements with lower priority, disregarding distance.
 */
UENUM()
enum ESceneDepthPriorityGroup
{
	/** World scene DPG. */
	SDPG_World,
	/** Foreground scene DPG. */
	SDPG_Foreground,
	SDPG_MAX,
};


UENUM()
enum EIndirectLightingCacheQuality
{
	/** The indirect lighting cache will be disabled for this object, so no GI from stationary lights on movable objects. */
	ILCQ_Off,
	/** A single indirect lighting sample computed at the bounds origin will be interpolated which fades over time to newer results. */
	ILCQ_Point,
	/** The object will get a 5x5x5 stable volume of interpolated indirect lighting, which allows gradients of lighting intensity across the receiving object. */
	ILCQ_Volume
};

UENUM()
enum EOcclusionCombineMode
{
	/** Take the minimum occlusion value.  This is effective for avoiding over-occlusion from multiple methods, but can result in indoors looking too flat. */
	OCM_Minimum,
	/** 
	 * Multiply together occlusion values from Distance Field Ambient Occlusion and Screen Space Ambient Occlusion.  
	 * This gives a good sense of depth everywhere, but can cause over-occlusion. 
	 * SSAO should be tweaked to be less strong compared to Minimum.
	 */
	OCM_Multiply,
	OCM_MAX,
};

/** Note: This is mirrored in Lightmass, be sure to update the blend mode structure and logic there if this changes. */
// Note: Check UMaterialInstance::Serialize if changed!!
UENUM(BlueprintType)
enum EBlendMode
{
	BLEND_Opaque UMETA(DisplayName="Opaque"),
	BLEND_Masked UMETA(DisplayName="Masked"),
	BLEND_Translucent UMETA(DisplayName="Translucent"),
	BLEND_Additive UMETA(DisplayName="Additive"),
	BLEND_Modulate UMETA(DisplayName="Modulate"),
	BLEND_AlphaComposite UMETA(DisplayName ="AlphaComposite (Premultiplied Alpha)"),
	BLEND_MAX,
};


UENUM()
enum ESamplerSourceMode
{
	/** Get the sampler from the texture.  Every unique texture will consume a sampler slot, which are limited in number. */
	SSM_FromTextureAsset UMETA(DisplayName="From texture asset"),
	/** Shared sampler source that does not consume a sampler slot.  Uses wrap addressing and gets filter mode from the world texture group. */
	SSM_Wrap_WorldGroupSettings UMETA(DisplayName="Shared: Wrap"),
	/** Shared sampler source that does not consume a sampler slot.  Uses clamp addressing and gets filter mode from the world texture group. */
	SSM_Clamp_WorldGroupSettings UMETA(DisplayName="Shared: Clamp")
};


UENUM()
enum ETranslucencyLightingMode
{
	/** 
	 * Lighting will be calculated for a volume, without directionality.  Use this on particle effects like smoke and dust.
	 * This is the cheapest per-pixel lighting method, however the material normal is not taken into account.
	 */
	TLM_VolumetricNonDirectional UMETA(DisplayName="Volumetric NonDirectional"),

	 /** 
	 * Lighting will be calculated for a volume, with directionality so that the normal of the material is taken into account. 
	 * Note that the default particle tangent space is facing the camera, so enable bGenerateSphericalParticleNormals to get a more useful tangent space.
	 */
	TLM_VolumetricDirectional UMETA(DisplayName="Volumetric Directional"),

	/** 
	 * Same as Volumetric Non Directional, but lighting is only evaluated at vertices so the pixel shader cost is significantly less.
	 * Note that lighting still comes from a volume texture, so it is limited in range.  Directional lights become unshadowed in the distance.
	 */
	TLM_VolumetricPerVertexNonDirectional UMETA(DisplayName="Volumetric PerVertex NonDirectional"),

	 /** 
	 * Same as Volumetric Directional, but lighting is only evaluated at vertices so the pixel shader cost is significantly less.
	 * Note that lighting still comes from a volume texture, so it is limited in range.  Directional lights become unshadowed in the distance.
	 */
	TLM_VolumetricPerVertexDirectional UMETA(DisplayName="Volumetric PerVertex Directional"),

	/** 
	 * Lighting will be calculated for a surface. The light in accumulated in a volume so the result is blurry, 
	 * limited distance but the per pixel cost is very low. Use this on translucent surfaces like glass and water.
	 * Only diffuse lighting is supported.
	 */
	TLM_Surface UMETA(DisplayName="Surface TranslucencyVolume"),

	/** 
	 * Lighting will be calculated for a surface. Use this on translucent surfaces like glass and water.
	 * This is implemented with forward shading so specular highlights from local lights are supported, however many deferred-only features are not.
	 * This is the most expensive translucency lighting method as each light's contribution is computed per-pixel.
	 */
	TLM_SurfacePerPixelLighting UMETA(DisplayName="Surface ForwardShading"),

	TLM_MAX,
};

/** Determines how the refraction offset should be computed for the material. */
UENUM()
enum ERefractionMode
{
	/** 
	 * Refraction is computed based on the camera vector entering a medium whose index of refraction is defined by the Refraction material input.  
	 * The new medium's surface is defined by the material's normal.  With this mode, a flat plane seen from the side will have a constant refraction offset.
	 * This is a physical model of refraction but causes reading outside the scene color texture so is a poor fit for large refractive surfaces like water.
	 */
	RM_IndexOfRefraction UMETA(DisplayName="Index Of Refraction"),

	/** 
	 * The refraction offset into Scene Color is computed based on the difference between the per-pixel normal and the per-vertex normal.  
	 * With this mode, a material whose normal is the default (0, 0, 1) will never cause any refraction.  This mode is only valid with tangent space normals.
	 * The refraction material input scales the offset, although a value of 1.0 maps to no refraction, and a value of 2 maps to a scale of 1.0 on the offset.
	 * This is a non-physical model of refraction but is useful on large refractive surfaces like water, since offsets have to stay small to avoid reading outside scene color.
	 */
	RM_PixelNormalOffset UMETA(DisplayName="Pixel Normal Offset")
};

/**
 * Enumerates available options for the translucency sort policy.
 */
UENUM()
namespace ETranslucentSortPolicy
{
	enum Type
	{
		/** Sort based on distance from camera centerpoint to bounding sphere centerpoint. (Default, best for 3D games.) */
		SortByDistance = 0,

		/** Sort based on the post-projection Z distance to the camera. */
		SortByProjectedZ = 1,

		/** Sort based on the projection onto a fixed axis. (Best for 2D games.) */
		SortAlongAxis = 2,
	};
}

UENUM()
enum ESceneCaptureSource 
{ 
	SCS_SceneColorHDR UMETA(DisplayName="SceneColor (HDR) in RGB, Inv Opacity in A"),
	SCS_SceneColorHDRNoAlpha UMETA(DisplayName="SceneColor (HDR) in RGB, 0 in A"),
	SCS_FinalColorLDR UMETA(DisplayName="Final Color (LDR) in RGB"),
	SCS_SceneColorSceneDepth UMETA(DisplayName="SceneColor (HDR) in RGB, SceneDepth in A"),
	SCS_SceneDepth UMETA(DisplayName="SceneDepth in R"),
	SCS_DeviceDepth UMETA(DisplayName = "DeviceDepth in RGB"),
	SCS_Normal UMETA(DisplayName="Normal in RGB (Deferred Renderer only)"),
	SCS_BaseColor UMETA(DisplayName="BaseColor in RGB (Deferred Renderer only)")
};

UENUM()
enum ESceneCaptureCompositeMode
{ 
	SCCM_Overwrite UMETA(DisplayName="Overwrite"),
	SCCM_Additive UMETA(DisplayName="Additive"),
	SCCM_Composite UMETA(DisplayName="Composite")
};

#define NUM_LIGHTING_CHANNELS 3

USTRUCT(BlueprintType)
struct FLightingChannels
{
	GENERATED_USTRUCT_BODY()

	FLightingChannels() :
		bChannel0(true),
		bChannel1(false),
		bChannel2(false)
	{}

	/** Default channel for all primitives and lights. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Channels)
	uint8 bChannel0:1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Channels)
	uint8 bChannel1:1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Channels)
	uint8 bChannel2:1;
};

inline uint8 GetLightingChannelMaskForStruct(FLightingChannels Value)
{
	// Note: this is packed into 3 bits of a stencil channel
	return (uint8)((Value.bChannel0 ? 1 : 0) | (Value.bChannel1 << 1) | (Value.bChannel2 << 2));
}

inline uint8 GetDefaultLightingChannelMask()
{
	return 1;
}

// Returns the index of the first lighting channel set, or -1 if no channels are set.
inline int32 GetFirstLightingChannelFromMask(uint8 Mask)
{
	return Mask ? FPlatformMath::CountTrailingZeros(Mask) : -1;
}

/*
* Enumerates available GBufferFormats.
*/
UENUM()
namespace EGBufferFormat
{
	// When this enum is updated please update CVarGBufferFormat comments 
	enum Type
	{
		Force8BitsPerChannel = 0 UMETA(DisplayName = "Force 8 Bits Per Channel", ToolTip = "Forces all GBuffers to 8 bits per channel. Intended as profiling for best performance."),
		Default = 1 UMETA(ToolTip = "See GBuffer allocation function for layout details."),
		HighPrecisionNormals = 3 UMETA(ToolTip = "Same as Default except normals are encoded at 16 bits per channel."),
		Force16BitsPerChannel = 5 UMETA(DisplayName = "Force 16 Bits Per Channel", ToolTip = "Forces all GBuffers to 16 bits per channel. Intended as profiling for best quality."),
	};
}

/** Controls the way that the width scale property affects animation trails. */
UENUM()
enum ETrailWidthMode
{
	ETrailWidthMode_FromCentre UMETA(DisplayName = "From Centre"),
	ETrailWidthMode_FromFirst UMETA(DisplayName = "From First Socket"),
	ETrailWidthMode_FromSecond UMETA(DisplayName = "From Second Socket"),
};

UENUM()
namespace EParticleCollisionMode
{
	enum Type
	{
		SceneDepth UMETA(DisplayName="Scene Depth"),
		DistanceField UMETA(DisplayName="Distance Field")
	};
}


// Note: Check UMaterialInstance::Serialize if changed!

UENUM()
enum EMaterialShadingModel
{
	MSM_Unlit				UMETA(DisplayName="Unlit"),
	MSM_DefaultLit			UMETA(DisplayName="Default Lit"),
	MSM_Subsurface			UMETA(DisplayName="Subsurface"),
	MSM_PreintegratedSkin	UMETA(DisplayName="Preintegrated Skin"),
	MSM_ClearCoat			UMETA(DisplayName="Clear Coat"),
	MSM_SubsurfaceProfile	UMETA(DisplayName="Subsurface Profile"),
	MSM_TwoSidedFoliage		UMETA(DisplayName="Two Sided Foliage"),
	MSM_Hair				UMETA(DisplayName="Hair"),
	MSM_Cloth				UMETA(DisplayName="Cloth"),
	MSM_Eye					UMETA(DisplayName="Eye"),
	MSM_MAX,
};


/** This is used by the drawing passes to determine tessellation policy, so changes here need to be supported in native code. */
UENUM()
enum EMaterialTessellationMode
{
	/** Tessellation disabled. */
	MTM_NoTessellation UMETA(DisplayName="No Tessellation"),
	/** Simple tessellation. */
	MTM_FlatTessellation UMETA(DisplayName="Flat Tessellation"),
	/** Simple spline based tessellation. */
	MTM_PNTriangles UMETA(DisplayName="PN Triangles"),
	MTM_MAX,
};


UENUM(BlueprintType)
enum EMaterialSamplerType
{
	SAMPLERTYPE_Color UMETA(DisplayName="Color"),
	SAMPLERTYPE_Grayscale UMETA(DisplayName="Grayscale"),
	SAMPLERTYPE_Alpha UMETA(DisplayName="Alpha"),
	SAMPLERTYPE_Normal UMETA(DisplayName="Normal"),
	SAMPLERTYPE_Masks UMETA(DisplayName="Masks"),
	SAMPLERTYPE_DistanceFieldFont UMETA(DisplayName="Distance Field Font"),
	SAMPLERTYPE_LinearColor UMETA(DisplayName = "Linear Color"),
	SAMPLERTYPE_LinearGrayscale UMETA(DisplayName = "Linear Grayscale"),
	SAMPLERTYPE_External UMETA(DisplayName = "External"),
	SAMPLERTYPE_MAX,
};


/**	Lighting build quality enumeration */
UENUM()
enum ELightingBuildQuality
{
	Quality_Preview,
	Quality_Medium,
	Quality_High,
	Quality_Production,
	Quality_MAX,
};


UENUM()
enum ETriangleSortOption
{
	TRISORT_None,
	TRISORT_CenterRadialDistance,
	TRISORT_Random,
	TRISORT_MergeContiguous,
	TRISORT_Custom,
	TRISORT_CustomLeftRight,
	TRISORT_MAX,
};


/** Enum to specify which axis to use for the forward vector when using TRISORT_CustomLeftRight sort mode. */
UENUM()
enum ETriangleSortAxis
{
	TSA_X_Axis,
	TSA_Y_Axis,
	TSA_Z_Axis,
	TSA_MAX,
};


/** Movement modes for Characters. */
UENUM(BlueprintType)
enum EMovementMode
{
	/** None (movement is disabled). */
	MOVE_None		UMETA(DisplayName="None"),

	/** Walking on a surface. */
	MOVE_Walking	UMETA(DisplayName="Walking"),

	/** 
	 * Simplified walking on navigation data (e.g. navmesh). 
	 * If bGenerateOverlapEvents is true, then we will perform sweeps with each navmesh move.
	 * If bGenerateOverlapEvents is false then movement is cheaper but characters can overlap other objects without some extra process to repel/resolve their collisions.
	 */
	MOVE_NavWalking	UMETA(DisplayName="Navmesh Walking"),

	/** Falling under the effects of gravity, such as after jumping or walking off the edge of a surface. */
	MOVE_Falling	UMETA(DisplayName="Falling"),

	/** Swimming through a fluid volume, under the effects of gravity and buoyancy. */
	MOVE_Swimming	UMETA(DisplayName="Swimming"),

	/** Flying, ignoring the effects of gravity. Affected by the current physics volume's fluid friction. */
	MOVE_Flying		UMETA(DisplayName="Flying"),

	/** User-defined custom movement mode, including many possible sub-modes. */
	MOVE_Custom		UMETA(DisplayName="Custom"),

	MOVE_MAX		UMETA(Hidden),
};


/** Smoothing approach used by network interpolation for Characters. */
UENUM(BlueprintType)
enum class ENetworkSmoothingMode : uint8
{
	/** No smoothing, only change position as network position updates are received. */
	Disabled		UMETA(DisplayName="Disabled"),

	/** Linear interpolation from source to target. */
	Linear			UMETA(DisplayName="Linear"),

	/** Exponential. Faster as you are further from target. */
	Exponential		UMETA(DisplayName="Exponential"),

	/** Special linear interpolation designed specifically for replays. Not intended as a selectable mode in-editor. */
	Replay			UMETA(Hidden, DisplayName="Replay"),
};

/** This filter allows us to refine queries (channel, object) with an additional level of ignore by tagging entire classes of objects (e.g. "Red team", "Blue team")
    If(QueryIgnoreMask & ShapeFilter != 0) filter out */
typedef uint8 FMaskFilter;

// Number of bits used currently from FMaskFilter.
enum { NumExtraFilterBits = 6 };

// NOTE!!Some of these values are used to index into FCollisionResponseContainers and must be kept in sync.
// @see FCollisionResponseContainer::SetResponse().

// @NOTE!!!! This DisplayName [DISPLAYNAME] SHOULD MATCH suffix of ECC_DISPLAYNAME
// Otherwise it will mess up collision profile loading
// If you change this, please also change FCollisionResponseContainers
//
// If you add any more TraceQuery="1", you also should change UCollsionProfile::LoadProfileConfig
// Metadata doesn't work outside of editor, so you'll need to add manually

// @NOTE : when you add more here for predefined engine channel
// please change the max in the CollisionProfile
// search ECC_Destructible

// in order to use this custom channels
// we recommend to define in your local file
// - i.e. #define COLLISION_WEAPON		ECC_GameTraceChannel1
// and make sure you customize these it in INI file by
// 
// in DefaultEngine.ini
//
// [/Script/Engine.CollisionProfile]
// GameTraceChannel1="Weapon"
// 
// also in the INI file, you can override collision profiles that are defined by simply redefining
// note that Weapon isn't defined in the BaseEngine.ini file, but "Trigger" is defined in Engine
// +Profiles=(Name="Trigger",CollisionEnabled=QueryOnly,ObjectTypeName=WorldDynamic, DefaultResponse=ECR_Overlap, CustomResponses=((Channel=Visibility, Response=ECR_Ignore), (Channel=Weapon, Response=ECR_Ignore)))


/** 
 * Enum indicating different type of objects for rigid-body collision purposes. 
 */
UENUM(BlueprintType)
enum ECollisionChannel
{

	ECC_WorldStatic UMETA(DisplayName="WorldStatic"),
	ECC_WorldDynamic UMETA(DisplayName="WorldDynamic"),
	ECC_Pawn UMETA(DisplayName="Pawn"),
	ECC_Visibility UMETA(DisplayName="Visibility" , TraceQuery="1"),
	ECC_Camera UMETA(DisplayName="Camera" , TraceQuery="1"),
	ECC_PhysicsBody UMETA(DisplayName="PhysicsBody"),
	ECC_Vehicle UMETA(DisplayName="Vehicle"),
	ECC_Flex UMETA(DisplayName = "Flex"),
	ECC_Destructible UMETA(DisplayName="Destructible"),

	/** Reserved for gizmo collision */
	ECC_EngineTraceChannel1 UMETA(Hidden),

	ECC_EngineTraceChannel2 UMETA(Hidden),
	ECC_EngineTraceChannel3 UMETA(Hidden),
	ECC_EngineTraceChannel4 UMETA(Hidden), 
	ECC_EngineTraceChannel5 UMETA(Hidden),

	ECC_GameTraceChannel1 UMETA(Hidden),
	ECC_GameTraceChannel2 UMETA(Hidden),
	ECC_GameTraceChannel3 UMETA(Hidden),
	ECC_GameTraceChannel4 UMETA(Hidden),
	ECC_GameTraceChannel5 UMETA(Hidden),
	ECC_GameTraceChannel6 UMETA(Hidden),
	ECC_GameTraceChannel7 UMETA(Hidden),
	ECC_GameTraceChannel8 UMETA(Hidden),
	ECC_GameTraceChannel9 UMETA(Hidden),
	ECC_GameTraceChannel10 UMETA(Hidden),
	ECC_GameTraceChannel11 UMETA(Hidden),
	ECC_GameTraceChannel12 UMETA(Hidden),
	ECC_GameTraceChannel13 UMETA(Hidden),
	ECC_GameTraceChannel14 UMETA(Hidden),
	ECC_GameTraceChannel15 UMETA(Hidden),
	ECC_GameTraceChannel16 UMETA(Hidden),
	ECC_GameTraceChannel17 UMETA(Hidden),
	ECC_GameTraceChannel18 UMETA(Hidden),
	
	/** Add new serializeable channels above here (i.e. entries that exist in FCollisionResponseContainer) */
	/** Add only nonserialized/transient flags below */

	// NOTE!!!! THESE ARE BEING DEPRECATED BUT STILL THERE FOR BLUEPRINT. PLEASE DO NOT USE THEM IN CODE

	ECC_OverlapAll_Deprecated UMETA(Hidden),
	ECC_MAX,
};

DECLARE_DELEGATE_OneParam(FOnConstraintBroken, int32 /*ConstraintIndex*/);


#define COLLISION_GIZMO ECC_EngineTraceChannel1


/** @note, if you change this, change GetCollisionChannelFromOverlapFilter() to match */
UENUM(BlueprintType)
enum EOverlapFilterOption
{
	// returns both overlaps with both dynamic and static components
	OverlapFilter_All UMETA(DisplayName="AllObjects"),
	// returns only overlaps with dynamic actors (far fewer results in practice, much more efficient)
	OverlapFilter_DynamicOnly UMETA(DisplayName="AllDynamicObjects"),
	// returns only overlaps with static actors (fewer results, more efficient)
	OverlapFilter_StaticOnly UMETA(DisplayName="AllStaticObjects"),
};


UENUM(BlueprintType)
enum EObjectTypeQuery
{
	ObjectTypeQuery1 UMETA(Hidden), 
	ObjectTypeQuery2 UMETA(Hidden), 
	ObjectTypeQuery3 UMETA(Hidden), 
	ObjectTypeQuery4 UMETA(Hidden), 
	ObjectTypeQuery5 UMETA(Hidden), 
	ObjectTypeQuery6 UMETA(Hidden), 
	ObjectTypeQuery7 UMETA(Hidden), 
	ObjectTypeQuery8 UMETA(Hidden), 
	ObjectTypeQuery9 UMETA(Hidden), 
	ObjectTypeQuery10 UMETA(Hidden), 
	ObjectTypeQuery11 UMETA(Hidden), 
	ObjectTypeQuery12 UMETA(Hidden), 
	ObjectTypeQuery13 UMETA(Hidden), 
	ObjectTypeQuery14 UMETA(Hidden), 
	ObjectTypeQuery15 UMETA(Hidden), 
	ObjectTypeQuery16 UMETA(Hidden), 
	ObjectTypeQuery17 UMETA(Hidden), 
	ObjectTypeQuery18 UMETA(Hidden), 
	ObjectTypeQuery19 UMETA(Hidden), 
	ObjectTypeQuery20 UMETA(Hidden), 
	ObjectTypeQuery21 UMETA(Hidden), 
	ObjectTypeQuery22 UMETA(Hidden), 
	ObjectTypeQuery23 UMETA(Hidden), 
	ObjectTypeQuery24 UMETA(Hidden), 
	ObjectTypeQuery25 UMETA(Hidden), 
	ObjectTypeQuery26 UMETA(Hidden), 
	ObjectTypeQuery27 UMETA(Hidden), 
	ObjectTypeQuery28 UMETA(Hidden), 
	ObjectTypeQuery29 UMETA(Hidden), 
	ObjectTypeQuery30 UMETA(Hidden), 
	ObjectTypeQuery31 UMETA(Hidden), 
	ObjectTypeQuery32 UMETA(Hidden),

	ObjectTypeQuery_MAX	UMETA(Hidden)
};


UENUM(BlueprintType)
enum ETraceTypeQuery
{
	TraceTypeQuery1 UMETA(Hidden), 
	TraceTypeQuery2 UMETA(Hidden), 
	TraceTypeQuery3 UMETA(Hidden), 
	TraceTypeQuery4 UMETA(Hidden), 
	TraceTypeQuery5 UMETA(Hidden), 
	TraceTypeQuery6 UMETA(Hidden), 
	TraceTypeQuery7 UMETA(Hidden), 
	TraceTypeQuery8 UMETA(Hidden), 
	TraceTypeQuery9 UMETA(Hidden), 
	TraceTypeQuery10 UMETA(Hidden), 
	TraceTypeQuery11 UMETA(Hidden), 
	TraceTypeQuery12 UMETA(Hidden), 
	TraceTypeQuery13 UMETA(Hidden), 
	TraceTypeQuery14 UMETA(Hidden), 
	TraceTypeQuery15 UMETA(Hidden), 
	TraceTypeQuery16 UMETA(Hidden), 
	TraceTypeQuery17 UMETA(Hidden), 
	TraceTypeQuery18 UMETA(Hidden), 
	TraceTypeQuery19 UMETA(Hidden), 
	TraceTypeQuery20 UMETA(Hidden), 
	TraceTypeQuery21 UMETA(Hidden), 
	TraceTypeQuery22 UMETA(Hidden), 
	TraceTypeQuery23 UMETA(Hidden), 
	TraceTypeQuery24 UMETA(Hidden), 
	TraceTypeQuery25 UMETA(Hidden), 
	TraceTypeQuery26 UMETA(Hidden), 
	TraceTypeQuery27 UMETA(Hidden), 
	TraceTypeQuery28 UMETA(Hidden), 
	TraceTypeQuery29 UMETA(Hidden), 
	TraceTypeQuery30 UMETA(Hidden), 
	TraceTypeQuery31 UMETA(Hidden), 
	TraceTypeQuery32 UMETA(Hidden),

	TraceTypeQuery_MAX	UMETA(Hidden)
};


/** Enum indicating which physics scene to use. */
UENUM()
enum EPhysicsSceneType
{
	/** The synchronous scene, which must finish before Unreal simulation code is run. */
	PST_Sync,
	/** The cloth scene, which may run while Unreal simulation code runs. */
	PST_Cloth,
	/** The asynchronous scene, which may run while Unreal simulation code runs. */
	PST_Async,
	PST_MAX,
};


/** Enum indicating how each type should respond */
UENUM(BlueprintType)
enum ECollisionResponse
{
	ECR_Ignore UMETA(DisplayName="Ignore"),
	ECR_Overlap UMETA(DisplayName="Overlap"),
	ECR_Block UMETA(DisplayName="Block"),
	ECR_MAX,
};


UENUM()
enum EFilterInterpolationType
{
	BSIT_Average UMETA(DisplayName = "Averaged Interpolation"),
	BSIT_Linear UMETA(DisplayName = "Linear Interpolation"),
	BSIT_Cubic UMETA(DisplayName = "Cubic Interpolation"),
	BSIT_MAX
};


UENUM()
enum EInputConsumeOptions
{
	/** This component consumes all input and no components lower in the stack are processed. */
	ICO_ConsumeAll=0,
	/** This component consumes all events for a keys it has bound (whether or not they are handled successfully).  Components lower in the stack will not receive events from these keys. */
	ICO_ConsumeBoundKeys,
	/** All input events will be available to components lower in the stack. */
	ICO_ConsumeNone,
	ICO_MAX
};


namespace EWorldType
{
	enum Type
	{
		/** An untyped world, in most cases this will be the vestigial worlds of streamed in sub-levels */
		None,

		/** The game world */
		Game,

		/** A world being edited in the editor */
		Editor,

		/** A Play In Editor world */
		PIE,

		/** A preview world for an editor tool */
		EditorPreview,

		/** A preview world for a game */
		GamePreview,

		/** An editor world that was loaded but not currently being edited in the level editor */
		Inactive
	};

	DEPRECATED(4.14, "EWorldType::Preview is deprecated. Please use either EWorldType::EditorPreview or EWorldType::GamePreview")
	const EWorldType::Type Preview = EWorldType::EditorPreview;
}


enum class EFlushLevelStreamingType
{
	None,			
	Full,			// Allow multiple load requests
	Visibility,		// Flush visibility only, do not allow load requests, flushes async loading as well
};


USTRUCT()
struct FResponseChannel
{
	GENERATED_USTRUCT_BODY()

	/** This should match DisplayName of ECollisionChannel 
	 *	Meta data of custom channels can be used as well
	 */
	 UPROPERTY(EditAnywhere, Category = FResponseChannel)
	FName Channel;

	UPROPERTY(EditAnywhere, Category = FResponseChannel)
	TEnumAsByte<enum ECollisionResponse> Response;

	FResponseChannel()
		: Response(ECR_Block) {}

	FResponseChannel( FName InChannel, ECollisionResponse InResponse )
		: Channel(InChannel)
		, Response(InResponse) {}

	bool operator==(const FResponseChannel& Other) const
	{
		return Channel == Other.Channel && Response == Other.Response;
	}
};


/**
 *	Container for indicating a set of collision channels that this object will collide with.
 */
USTRUCT(BlueprintType)
struct ENGINE_API FCollisionResponseContainer
{
	GENERATED_USTRUCT_BODY()

#if !CPP      //noexport property

	///////////////////////////////////////
	// Reserved Engine Trace Channels
	// 
	// Note - 	If you change this (add/remove/modify) 
	// 			you should make sure it matches with ECollisionChannel (including DisplayName)
	// 			They has to be mirrored if serialized
	///////////////////////////////////////
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer, meta=(DisplayName="WorldStatic"))
	TEnumAsByte<enum ECollisionResponse> WorldStatic;    // 0

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer, meta=(DisplayName="WorldDynamic"))
	TEnumAsByte<enum ECollisionResponse> WorldDynamic;    // 1.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer, meta=(DisplayName="Pawn"))
	TEnumAsByte<enum ECollisionResponse> Pawn;    		// 2

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer, meta=(DisplayName="Visibility"))
	TEnumAsByte<enum ECollisionResponse> Visibility;    // 3

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer, meta=(DisplayName="Camera"))
	TEnumAsByte<enum ECollisionResponse> Camera;    // 4

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer, meta=(DisplayName="PhysicsBody"))
	TEnumAsByte<enum ECollisionResponse> PhysicsBody;    // 5

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer, meta=(DisplayName="Vehicle"))
	TEnumAsByte<enum ECollisionResponse> Vehicle;    // 6

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CollisionResponseContainer, meta = (DisplayName="Flex"))
	TEnumAsByte<enum ECollisionResponse> Flex;    // 7

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer, meta=(DisplayName="Destructible"))
	TEnumAsByte<enum ECollisionResponse> Destructible;    // 8

	///////////////////////////////////////
	// Unspecified Engine Trace Channels
	///////////////////////////////////////

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> EngineTraceChannel1;    // 9

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> EngineTraceChannel2;    // 10

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> EngineTraceChannel3;    // 11

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> EngineTraceChannel4;    // 12

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> EngineTraceChannel5;    // 13

	///////////////////////////////////////
	// in order to use this custom channels
	// we recommend to define in your local file
	// - i.e. #define COLLISION_WEAPON		ECC_GameTraceChannel1
	// and make sure you customize these it in INI file by
	// 
	// in DefaultEngine.ini
	//	
	// [/Script/Engine.CollisionProfile]
	// GameTraceChannel1="Weapon"
	// 
	// also in the INI file, you can override collision profiles that are defined by simply redefining
	// note that Weapon isn't defined in the BaseEngine.ini file, but "Trigger" is defined in Engine
	// +Profiles=(Name="Trigger",CollisionEnabled=QueryOnly,ObjectTypeName=WorldDynamic, DefaultResponse=ECR_Overlap, CustomResponses=((Channel=Visibility, Response=ECR_Ignore), (Channel=Weapon, Response=ECR_Ignore)))
	///////////////////////////////////////
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel1;    // 14

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel2;    // 15

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel3;    // 16

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel4;    // 17

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel5;    // 18

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel6;    // 19

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel7;    // 20

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel8;    // 21

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel9;    // 22

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel10;    // 23

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel11;    // 24

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel12;    // 25

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel13;    // 26

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel14;    // 27

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel15;    // 28

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel16;    // 28

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel17;    // 30

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=CollisionResponseContainer)
	TEnumAsByte<enum ECollisionResponse> GameTraceChannel18;    // 31

#endif

	union
	{
		struct
		{
			//Reserved Engine Trace Channels
			uint8 WorldStatic;			// 0
			uint8 WorldDynamic;			// 1
			uint8 Pawn;					// 2
			uint8 Visibility;			// 3
			uint8 Camera;				// 4
			uint8 PhysicsBody;			// 5
			uint8 Vehicle;				// 6
			uint8 Flex;					// 7
			uint8 Destructible;			// 8

			// Unspecified Engine Trace Channels
			uint8 EngineTraceChannel1;   // 9
			uint8 EngineTraceChannel2;   // 10
			uint8 EngineTraceChannel3;   // 11
			uint8 EngineTraceChannel4;   // 12
			uint8 EngineTraceChannel5;   // 13			

			// Unspecified Game Trace Channels
			uint8 GameTraceChannel1;     // 14
			uint8 GameTraceChannel2;     // 15
			uint8 GameTraceChannel3;     // 16
			uint8 GameTraceChannel4;     // 17
			uint8 GameTraceChannel5;     // 18
			uint8 GameTraceChannel6;     // 19
			uint8 GameTraceChannel7;     // 20
			uint8 GameTraceChannel8;     // 21
			uint8 GameTraceChannel9;     // 22
			uint8 GameTraceChannel10;    // 23
			uint8 GameTraceChannel11;    // 24 
			uint8 GameTraceChannel12;    // 25
			uint8 GameTraceChannel13;    // 26
			uint8 GameTraceChannel14;    // 27
			uint8 GameTraceChannel15;    // 28
			uint8 GameTraceChannel16;    // 29 
			uint8 GameTraceChannel17;    // 30
			uint8 GameTraceChannel18;    // 31
		};
		uint8 EnumArray[32];
	};

	/** This constructor will set all channels to ECR_Block */
	FCollisionResponseContainer();
	FCollisionResponseContainer(ECollisionResponse DefaultResponse);

	/** Set the response of a particular channel in the structure. */
	void SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse);

	/** Set all channels to the specified response */
	void SetAllChannels(ECollisionResponse NewResponse);

	/** Replace the channels matching the old response with the new response */
	void ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse);

	/** Returns the response set on the specified channel */
	FORCEINLINE_DEBUGGABLE ECollisionResponse GetResponse(ECollisionChannel Channel) const { return (ECollisionResponse)EnumArray[Channel]; }

	/** Set all channels from ChannelResponse Array **/
	void UpdateResponsesFromArray(TArray<FResponseChannel> & ChannelResponses);
	int32 FillArrayFromResponses(TArray<FResponseChannel> & ChannelResponses);

	/** Take two response containers and create a new container where each element is the 'min' of the two inputs (ie Ignore and Block results in Ignore) */
	static FCollisionResponseContainer CreateMinContainer(const FCollisionResponseContainer& A, const FCollisionResponseContainer& B);

	static const struct FCollisionResponseContainer & GetDefaultResponseContainer() { return DefaultResponseContainer; }

private:

	/** static variable for default data to be used without reconstructing everytime **/
	static FCollisionResponseContainer DefaultResponseContainer;

	friend class UCollisionProfile;
};


/** Enum for controlling the falloff of strength of a radial impulse as a function of distance from Origin. */
UENUM()
enum ERadialImpulseFalloff
{
	/** Impulse is a constant strength, up to the limit of its range. */
	RIF_Constant,
	/** Impulse should get linearly weaker the further from origin. */
	RIF_Linear,
	RIF_MAX,
};


/** Presets of values used in considering when put this body to sleep. */
UENUM()
enum class ESleepFamily : uint8
{
	/** Engine defaults. */
	Normal,
	/** A family of values with a lower sleep threshold; good for slower pendulum-like physics. */
	Sensitive,
	/** Specify your own sleep threshold multiplier */
	Custom,
};


/** Enum used to indicate what type of timeline signature a function matches. */
UENUM()
enum ETimelineSigType
{
	ETS_EventSignature,
	ETS_FloatSignature,
	ETS_VectorSignature,
	ETS_LinearColorSignature,
	ETS_InvalidSignature,
	ETS_MAX,
};


/** Enum used to describe what type of collision is enabled on a body. */
UENUM(BlueprintType)
namespace ECollisionEnabled 
{ 
	enum Type 
	{ 
		/** Will not create any representation in the physics engine. Cannot be used for spatial queries (raycasts, sweeps, overlaps) or simulation (rigid body, constraints). Best performance possible (especially for moving objects) */
		NoCollision UMETA(DisplayName="No Collision"), 
		/** Only used for spatial queries (raycasts, sweeps, and overlaps). Cannot be used for simulation (rigid body, constraints). Useful for character movement and things that do not need physical simulation. Performance gains by keeping data out of simulation tree. */
		QueryOnly UMETA(DisplayName="Query Only (No Physics Collision)"),
		/** Only used only for physics simulation (rigid body, constraints). Cannot be used for spatial queries (raycasts, sweeps, overlaps). Useful for jiggly bits on characters that do not need per bone detection. Performance gains by keeping data out of query tree */
		PhysicsOnly UMETA(DisplayName="Physics Only (No Query Collision)"),
		/** Can be used for both spatial queries (raycasts, sweeps, overlaps) and simulation (rigid body, constraints). */
		QueryAndPhysics UMETA(DisplayName="Collision Enabled (Query and Physics)") 
	}; 
} 

FORCEINLINE bool CollisionEnabledHasPhysics(ECollisionEnabled::Type CollisionEnabled)
{
	return (CollisionEnabled == ECollisionEnabled::PhysicsOnly) ||
			(CollisionEnabled == ECollisionEnabled::QueryAndPhysics);
}

FORCEINLINE bool CollisionEnabledHasQuery(ECollisionEnabled::Type CollisionEnabled)
{
	return (CollisionEnabled == ECollisionEnabled::QueryOnly) ||
			(CollisionEnabled == ECollisionEnabled::QueryAndPhysics);
}

/** Describes the physical state of a rigid body. */
USTRUCT()
struct FRigidBodyState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector_NetQuantize100 Position;

	UPROPERTY()
	FQuat Quaternion;

	UPROPERTY()
	FVector_NetQuantize100 LinVel;

	UPROPERTY()
	FVector_NetQuantize100 AngVel;

	UPROPERTY()
	uint8 Flags;

	FRigidBodyState()
		: Position(ForceInit)
		, Quaternion(ForceInit)
		, LinVel(ForceInit)
		, AngVel(ForceInit)
		, Flags(0)
	{ }
};


namespace ERigidBodyFlags
{
	enum Type
	{
		None				= 0x00,
		Sleeping			= 0x01,
		NeedsUpdate			= 0x02,
	};
}


/** Rigid body error correction data */
USTRUCT()
struct FRigidBodyErrorCorrection
{
	GENERATED_USTRUCT_BODY()

	/** max squared position difference to perform velocity adjustment */
	UPROPERTY()
	float LinearDeltaThresholdSq;

	/** strength of snapping to desired linear velocity */
	UPROPERTY()
	float LinearInterpAlpha;

	/** inverted duration after which linear velocity adjustment will fix error */
	UPROPERTY()
	float LinearRecipFixTime;

	/** max squared angle difference (in radians) to perform velocity adjustment */
	UPROPERTY()
	float AngularDeltaThreshold;

	/** strength of snapping to desired angular velocity */
	UPROPERTY()
	float AngularInterpAlpha;

	/** inverted duration after which angular velocity adjustment will fix error */
	UPROPERTY()
	float AngularRecipFixTime;

	/** min squared body speed to perform velocity adjustment */
	UPROPERTY()
	float BodySpeedThresholdSq;

	FRigidBodyErrorCorrection()
		: LinearDeltaThresholdSq(5.0f)
		, LinearInterpAlpha(0.2f)
		, LinearRecipFixTime(1.0f)
		, AngularDeltaThreshold(0.2f * PI)
		, AngularInterpAlpha(0.1f)
		, AngularRecipFixTime(1.0f)
		, BodySpeedThresholdSq(0.2f)
	{ }
};


/**
 * Information about one contact between a pair of rigid bodies.
 */
USTRUCT()
struct FRigidBodyContactInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector ContactPosition;

	UPROPERTY()
	FVector ContactNormal;

	UPROPERTY()
	float ContactPenetration;

	UPROPERTY()
	class UPhysicalMaterial* PhysMaterial[2];


	FRigidBodyContactInfo()
		: ContactPosition(ForceInit)
		, ContactNormal(ForceInit)
		, ContactPenetration(0)
	{
		for (int32 ElementIndex = 0; ElementIndex < 2; ElementIndex++)
		{
			PhysMaterial[ElementIndex] = nullptr;
		}
	}

	FRigidBodyContactInfo(	const FVector& InContactPosition, 
							const FVector& InContactNormal, 
							float InPenetration, 
							UPhysicalMaterial* InPhysMat0, 
							UPhysicalMaterial* InPhysMat1 )
		: ContactPosition(InContactPosition)
		, ContactNormal(InContactNormal)
		, ContactPenetration(InPenetration)
	{
		PhysMaterial[0] = InPhysMat0;
		PhysMaterial[1] = InPhysMat1;
	}

	/** Swap the order of info in this info  */
	void SwapOrder();
};


/**
 * Information about an overall collision, including contacts.
 */
USTRUCT()
struct FCollisionImpactData
{
	GENERATED_USTRUCT_BODY()

	/** all the contact points in the collision*/
	UPROPERTY()
	TArray<struct FRigidBodyContactInfo> ContactInfos;

	/** the total impulse applied as the two objects push against each other*/
	UPROPERTY()
	FVector TotalNormalImpulse;

	/** the total counterimpulse applied of the two objects sliding against each other*/
	UPROPERTY()
	FVector TotalFrictionImpulse;

	FCollisionImpactData()
	: TotalNormalImpulse(ForceInit)
	, TotalFrictionImpulse(ForceInit)
	{}

	/** Iterate over ContactInfos array and swap order of information */
	void SwapContactOrders();
};


/** Struct used to hold effects for destructible damage events */
USTRUCT(BlueprintType)
struct FFractureEffect
{
	GENERATED_USTRUCT_BODY()

	/** Particle system effect to play at fracture location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FractureEffect)
	class UParticleSystem* ParticleSystem;

	/** Sound cue to play at fracture location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FractureEffect)
	class USoundBase* Sound;

	FFractureEffect()
		: ParticleSystem(nullptr)
		, Sound(nullptr)
	{ }
};


/**	Struct for handling positions relative to a base actor, which is potentially moving */
USTRUCT(BlueprintType)
struct ENGINE_API FBasedPosition
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BasedPosition)
	class AActor* Base;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BasedPosition)
	FVector Position;

	UPROPERTY()
	mutable FVector CachedBaseLocation;

	UPROPERTY()
	mutable FRotator CachedBaseRotation;

	UPROPERTY()
	mutable FVector CachedTransPosition;

	FBasedPosition();
	explicit FBasedPosition( class AActor *InBase, const FVector& InPosition );
	// Retrieve world location of this position
	FVector operator*() const;
	void Set( class AActor* InBase, const FVector& InPosition );
	void Clear();

	friend FArchive& operator<<( FArchive& Ar, FBasedPosition& T );
};


/** Struct for caching Quat<->Rotator conversions. */
struct ENGINE_API FRotationConversionCache
{
	FRotationConversionCache()
		: CachedQuat(FQuat::Identity)
		, CachedRotator(FRotator::ZeroRotator)
	{
	}

	/** Convert a FRotator to FQuat. Uses the cached conversion if possible, and updates it if there was no match. */
	FORCEINLINE_DEBUGGABLE FQuat RotatorToQuat(const FRotator& InRotator) const
	{
		if (CachedRotator != InRotator)
		{
			CachedRotator = InRotator.GetNormalized();
			CachedQuat = CachedRotator.Quaternion();
		}
		return CachedQuat;
	}

	/** Convert a FRotator to FQuat. Uses the cached conversion if possible, but does *NOT* update the cache if there was no match. */
	FORCEINLINE_DEBUGGABLE FQuat RotatorToQuat_ReadOnly(const FRotator& InRotator) const
	{
		if (CachedRotator == InRotator)
		{
			return CachedQuat;
		}
		return InRotator.Quaternion();
	}

	/** Convert a FQuat to FRotator. Uses the cached conversion if possible, and updates it if there was no match. */
	FORCEINLINE_DEBUGGABLE FRotator QuatToRotator(const FQuat& InQuat) const
	{
		if (CachedQuat != InQuat)
		{
			CachedQuat = InQuat.GetNormalized();
			CachedRotator = CachedQuat.Rotator();
		}
		return CachedRotator;
	}

	/** Convert a FQuat to FRotator. Uses the cached conversion if possible, but does *NOT* update the cache if there was no match. */
	FORCEINLINE_DEBUGGABLE FRotator QuatToRotator_ReadOnly(const FQuat& InQuat) const
	{
		if (CachedQuat == InQuat)
		{
			return CachedRotator;
		}
		return InQuat.Rotator();
	}

	/** Version of QuatToRotator when the Quat is known to already be normalized. */
	FORCEINLINE_DEBUGGABLE FRotator NormalizedQuatToRotator(const FQuat& InNormalizedQuat) const
	{
		if (CachedQuat != InNormalizedQuat)
		{
			CachedQuat = InNormalizedQuat;
			CachedRotator = InNormalizedQuat.Rotator();
		}
		return CachedRotator;
	}

	/** Version of QuatToRotator when the Quat is known to already be normalized. Does *NOT* update the cache if there was no match. */
	FORCEINLINE_DEBUGGABLE FRotator NormalizedQuatToRotator_ReadOnly(const FQuat& InNormalizedQuat) const
	{
		if (CachedQuat == InNormalizedQuat)
		{
			return CachedRotator;
		}
		return InNormalizedQuat.Rotator();
	}

	/** Return the cached Quat. */
	FORCEINLINE_DEBUGGABLE FQuat GetCachedQuat() const
	{
		return CachedQuat;
	}

	/** Return the cached Rotator. */
	FORCEINLINE_DEBUGGABLE FRotator GetCachedRotator() const
	{
		return CachedRotator;
	}

private:
	mutable FQuat		CachedQuat;		// FQuat matching CachedRotator such that CachedQuat.Rotator() == CachedRotator.
	mutable FRotator	CachedRotator;	// FRotator matching CachedQuat such that CachedRotator.Quaternion() == CachedQuat.
};



/** A line of subtitle text and the time at which it should be displayed. */
USTRUCT()
struct FSubtitleCue
{
	GENERATED_USTRUCT_BODY()

	/** The text to appear in the subtitle. */
	UPROPERTY(EditAnywhere, Category=SubtitleCue)
	FText Text;

	/** The time at which the subtitle is to be displayed, in seconds relative to the beginning of the line. */
	UPROPERTY(EditAnywhere, Category=SubtitleCue)
	float Time;

	FSubtitleCue()
		: Time(0)
	{ }
};


/**	A subtitle localized to a specific language. */
USTRUCT()
struct FLocalizedSubtitle
{
	GENERATED_USTRUCT_BODY()

	/** The 3-letter language for this subtitle */
	UPROPERTY()
	FString LanguageExt;

	/**
	 * Subtitle cues.  If empty, use SoundNodeWave's SpokenText as the subtitle.  Will often be empty,
	 * as the contents of the subtitle is commonly identical to what is spoken.
	 */
	UPROPERTY()
	TArray<FSubtitleCue> Subtitles;

	/** true if this sound is considered to contain mature content. */
	UPROPERTY()
	uint32 bMature:1;

	/** true if the subtitles have been split manually. */
	UPROPERTY()
	uint32 bManualWordWrap:1;

	/** true if the subtitles should be displayed one line at a time. */
	UPROPERTY()
	uint32 bSingleLine:1;

	FLocalizedSubtitle()
		: bMature(false)
		, bManualWordWrap(false)
		, bSingleLine(false)
	{ }
};


/**	Per-light settings for Lightmass */
USTRUCT()
struct FLightmassLightSettings
{
	GENERATED_USTRUCT_BODY()

	/** 0 will be completely desaturated, 1 will be unchanged */
	UPROPERTY(EditAnywhere, Category=Lightmass, meta=(UIMin = "0.0", UIMax = "4.0"))
	float IndirectLightingSaturation;

	/** Controls the falloff of shadow penumbras */
	UPROPERTY(EditAnywhere, Category=Lightmass, meta=(UIMin = "0.1", UIMax = "4.0"))
	float ShadowExponent;

	/** 
	 * Whether to use area shadows for stationary light precomputed shadowmaps.  
	 * Area shadows get softer the further they are from shadow casters, but require higher lightmap resolution to get the same quality where the shadow is sharp.
	 */
	UPROPERTY(EditAnywhere, Category=Lightmass)
	bool bUseAreaShadowsForStationaryLight;

	FLightmassLightSettings()
		: IndirectLightingSaturation(1.0f)
		, ShadowExponent(2.0f)
		, bUseAreaShadowsForStationaryLight(false)
	{ }
};


/**	Point/spot settings for Lightmass */
USTRUCT()
struct FLightmassPointLightSettings : public FLightmassLightSettings
{
	GENERATED_USTRUCT_BODY()
};


/**	Directional light settings for Lightmass */
USTRUCT()
struct FLightmassDirectionalLightSettings : public FLightmassLightSettings
{
	GENERATED_USTRUCT_BODY()

	/** Angle that the directional light's emissive surface extends relative to a receiver, affects penumbra sizes. */
	UPROPERTY(EditAnywhere, Category=Lightmass, meta=(UIMin = ".0001", UIMax = "5"))
	float LightSourceAngle;

	FLightmassDirectionalLightSettings()
		: LightSourceAngle(1.0f)
	{
	}
};


/**	Per-object settings for Lightmass */
USTRUCT()
struct FLightmassPrimitiveSettings
{
	GENERATED_USTRUCT_BODY()

	/** If true, this object will be lit as if it receives light from both sides of its polygons. */
	UPROPERTY(EditAnywhere, Category=Lightmass)
	uint32 bUseTwoSidedLighting:1;

	/** If true, this object will only shadow indirect lighting.  					*/
	UPROPERTY(EditAnywhere, Category=Lightmass)
	uint32 bShadowIndirectOnly:1;

	/** If true, allow using the emissive for static lighting.						*/
	UPROPERTY(EditAnywhere, Category=Lightmass)
	uint32 bUseEmissiveForStaticLighting:1;

	/** 
	 * Typically the triangle normal is used for hemisphere gathering which prevents incorrect self-shadowing from artist-tweaked vertex normals. 
	 * However in the case of foliage whose vertex normal has been setup to match the underlying terrain, gathering in the direction of the vertex normal is desired.
	 */
	UPROPERTY(EditAnywhere, Category=Lightmass)
	uint32 bUseVertexNormalForHemisphereGather:1;

	/** Direct lighting falloff exponent for mesh area lights created from emissive areas on this primitive. */
	UPROPERTY()
	float EmissiveLightFalloffExponent;

	/**
	 * Direct lighting influence radius.
	 * The default is 0, which means the influence radius should be automatically generated based on the emissive light brightness.
	 * Values greater than 0 override the automatic method.
	 */
	UPROPERTY()
	float EmissiveLightExplicitInfluenceRadius;

	/** Scales the emissive contribution of all materials applied to this object.	*/
	UPROPERTY(EditAnywhere, Category=Lightmass)
	float EmissiveBoost;

	/** Scales the diffuse contribution of all materials applied to this object.	*/
	UPROPERTY(EditAnywhere, Category=Lightmass)
	float DiffuseBoost;

	/** Fraction of samples taken that must be occluded in order to reach full occlusion. */
	UPROPERTY(EditAnywhere, Category=Lightmass)
	float FullyOccludedSamplesFraction;

	FLightmassPrimitiveSettings()
	{
		bUseTwoSidedLighting = false;
		bShadowIndirectOnly = false;
		bUseEmissiveForStaticLighting = false;
		bUseVertexNormalForHemisphereGather = false;
		EmissiveLightFalloffExponent = 8.0f;
		EmissiveLightExplicitInfluenceRadius = 0.0f;
		EmissiveBoost = 1.0f;
		DiffuseBoost = 1.0f;
		FullyOccludedSamplesFraction = 1.0f;
	}

	friend bool operator==(const FLightmassPrimitiveSettings& A, const FLightmassPrimitiveSettings& B)
	{
		//@todo UE4. Do we want a little 'leeway' in joining 
		if ((A.bUseTwoSidedLighting != B.bUseTwoSidedLighting) ||
			(A.bShadowIndirectOnly != B.bShadowIndirectOnly) || 
			(A.bUseEmissiveForStaticLighting != B.bUseEmissiveForStaticLighting) || 
			(A.bUseVertexNormalForHemisphereGather != B.bUseVertexNormalForHemisphereGather) || 
			(fabsf(A.EmissiveLightFalloffExponent - B.EmissiveLightFalloffExponent) > SMALL_NUMBER) ||
			(fabsf(A.EmissiveLightExplicitInfluenceRadius - B.EmissiveLightExplicitInfluenceRadius) > SMALL_NUMBER) ||
			(fabsf(A.EmissiveBoost - B.EmissiveBoost) > SMALL_NUMBER) ||
			(fabsf(A.DiffuseBoost - B.DiffuseBoost) > SMALL_NUMBER) ||
			(fabsf(A.FullyOccludedSamplesFraction - B.FullyOccludedSamplesFraction) > SMALL_NUMBER))
		{
			return false;
		}
		return true;
	}

	// Functions.
	friend FArchive& operator<<(FArchive& Ar, FLightmassPrimitiveSettings& Settings);
};


/**	Debug options for Lightmass */
USTRUCT()
struct FLightmassDebugOptions
{
	GENERATED_USTRUCT_BODY()

	/**
	 *	If false, UnrealLightmass.exe is launched automatically (default)
	 *	If true, it must be launched manually (e.g. through a debugger) with the -debug command line parameter.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bDebugMode:1;

	/**	If true, all participating Lightmass agents will report back detailed stats to the log.	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bStatsEnabled:1;

	/**	If true, BSP surfaces split across model components are joined into 1 mapping	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bGatherBSPSurfacesAcrossComponents:1;

	/**	The tolerance level used when gathering BSP surfaces.	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	float CoplanarTolerance;

	/**
	 *	If true, Lightmass will import mappings immediately as they complete.
	 *	It will not process them, however.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bUseImmediateImport:1;

	/**
	 *	If true, Lightmass will process appropriate mappings as they are imported.
	 *	NOTE: Requires ImmediateMode be enabled to actually work.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bImmediateProcessMappings:1;

	/**	If true, Lightmass will sort mappings by texel cost. */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bSortMappings:1;

	/**	If true, the generate coefficients will be dumped to binary files. */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bDumpBinaryFiles:1;

	/**
	 *	If true, Lightmass will write out BMPs for each generated material property
	 *	sample to <GAME>\ScreenShots\Materials.
	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bDebugMaterials:1;

	/**	If true, Lightmass will pad the calculated mappings to reduce/eliminate seams. */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bPadMappings:1;

	/**
	 *	If true, will fill padding of mappings with a color rather than the sampled edges.
	 *	Means nothing if bPadMappings is not enabled...
	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bDebugPaddings:1;

	/**
	 * If true, only the mapping containing a debug texel will be calculated, all others
	 * will be set to white
	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bOnlyCalcDebugTexelMappings:1;

	/** If true, color lightmaps a random color */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bUseRandomColors:1;

	/** If true, a green border will be placed around the edges of mappings */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bColorBordersGreen:1;

	/**
	 * If true, Lightmass will overwrite lightmap data with a shade of red relating to
	 * how long it took to calculate the mapping (Red = Time / ExecutionTimeDivisor)
	 */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	uint32 bColorByExecutionTime:1;

	/** The amount of time that will be count as full red when bColorByExecutionTime is enabled */
	UPROPERTY(EditAnywhere, Category=LightmassDebugOptions)
	float ExecutionTimeDivisor;

	ENGINE_API FLightmassDebugOptions();
};


/**
 *	Debug options for Swarm
 */
USTRUCT()
struct FSwarmDebugOptions
{
	GENERATED_USTRUCT_BODY()

	/**
	 *	If true, Swarm will distribute jobs.
	 *	If false, only the local machine will execute the jobs.
	 */
	UPROPERTY(EditAnywhere, Category=SwarmDebugOptions)
	uint32 bDistributionEnabled:1;

	/**
	 *	If true, Swarm will force content to re-export rather than using the cached version.
	 *	If false, Swarm will attempt to use the cached version.
	 */
	UPROPERTY(EditAnywhere, Category=SwarmDebugOptions)
	uint32 bForceContentExport:1;

	UPROPERTY()
	uint32 bInitialized:1;

	FSwarmDebugOptions()
		: bDistributionEnabled(true)
		, bForceContentExport(false)
	{
	}

	//@todo UE4. For some reason, the global instance is not initializing to the default settings...
	// Be sure to update this function to properly set the desired initial values!!!!
	void Touch();
};


UENUM()
enum ELightMapPaddingType
{
	LMPT_NormalPadding,
	LMPT_PrePadding,
	LMPT_NoPadding
};


/** Bit-field flags that affects storage (e.g. packing, streaming) and other info about a shadowmap. */
UENUM()
enum EShadowMapFlags
{
	/** No flags. */
	SMF_None			= 0,
	/** Shadowmap should be placed in a streaming texture. */
	SMF_Streamed		= 0x00000001
};


/** Reference to a specific material in a PrimitiveComponent. */
USTRUCT()
struct FPrimitiveMaterialRef
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	class UPrimitiveComponent* Primitive;

	UPROPERTY()
	class UDecalComponent* Decal;

	UPROPERTY()
	int32 ElementIndex;

	FPrimitiveMaterialRef()
		: Primitive(nullptr)
		, Decal(nullptr)
		, ElementIndex(0)
	{ }

	FPrimitiveMaterialRef(UPrimitiveComponent* InPrimitive, int32 InElementIndex)
		: Primitive(InPrimitive)
		, Decal(nullptr)
		, ElementIndex(InElementIndex)
	{ 	}

	FPrimitiveMaterialRef(UDecalComponent* InDecal, int32 InElementIndex)
		: Primitive(nullptr)
		, Decal(InDecal)
		, ElementIndex(InElementIndex)
	{ 	}
};


/**
 * Structure containing information about one hit of a trace, such as point of impact and surface normal at that point.
 */
USTRUCT(BlueprintType, meta = (HasNativeBreak = "Engine.GameplayStatics.BreakHitResult", HasNativeMake = "Engine.GameplayStatics.MakeHitResult"))
struct ENGINE_API FHitResult
{
	GENERATED_USTRUCT_BODY()

	/** Indicates if this hit was a result of blocking collision. If false, there was no hit or it was an overlap/touch instead. */
	UPROPERTY()
	uint32 bBlockingHit:1;

	/**
	 * Whether the trace started in penetration, i.e. with an initial blocking overlap.
	 * In the case of penetration, if PenetrationDepth > 0.f, then it will represent the distance along the Normal vector that will result in
	 * minimal contact between the swept shape and the object that was hit. In this case, ImpactNormal will be the normal opposed to movement at that location
	 * (ie, Normal may not equal ImpactNormal). ImpactPoint will be the same as Location, since there is no single impact point to report.
	 */
	UPROPERTY()
	uint32 bStartPenetrating:1;

	/**
	 * 'Time' of impact along trace direction (ranging from 0.0 to 1.0) if there is a hit, indicating time between TraceStart and TraceEnd.
	 * For swept movement (but not queries) this may be pulled back slightly from the actual time of impact, to prevent precision problems with adjacent geometry.
	 */
	UPROPERTY()
	float Time;
	 
	/** The distance from the TraceStart to the Location in world space. This value is 0 if there was an initial overlap (trace started inside another colliding object). */
	UPROPERTY()
	float Distance; 
	
	/**
	 * The location in world space where the moving shape would end up against the impacted object, if there is a hit. Equal to the point of impact for line tests.
	 * Example: for a sphere trace test, this is the point where the center of the sphere would be located when it touched the other object.
	 * For swept movement (but not queries) this may not equal the final location of the shape since hits are pulled back slightly to prevent precision issues from overlapping another surface.
	 */
	UPROPERTY()
	FVector_NetQuantize Location;

	/**
	 * Location in world space of the actual contact of the trace shape (box, sphere, ray, etc) with the impacted object.
	 * Example: for a sphere trace test, this is the point where the surface of the sphere touches the other object.
	 * @note: In the case of initial overlap (bStartPenetrating=true), ImpactPoint will be the same as Location because there is no meaningful single impact point to report.
	 */
	UPROPERTY()
	FVector_NetQuantize ImpactPoint;

	/**
	 * Normal of the hit in world space, for the object that was swept. Equal to ImpactNormal for line tests.
	 * This is computed for capsules and spheres, otherwise it will be the same as ImpactNormal.
	 * Example: for a sphere trace test, this is a normalized vector pointing in towards the center of the sphere at the point of impact.
	 */
	UPROPERTY()
	FVector_NetQuantizeNormal Normal;

	/**
	 * Normal of the hit in world space, for the object that was hit by the sweep, if any.
	 * For example if a box hits a flat plane, this is a normalized vector pointing out from the plane.
	 * In the case of impact with a corner or edge of a surface, usually the "most opposing" normal (opposed to the query direction) is chosen.
	 */
	UPROPERTY()
	FVector_NetQuantizeNormal ImpactNormal;

	/**
	 * Start location of the trace.
	 * For example if a sphere is swept against the world, this is the starting location of the center of the sphere.
	 */
	UPROPERTY()
	FVector_NetQuantize TraceStart;

	/**
	 * End location of the trace; this is NOT where the impact occurred (if any), but the furthest point in the attempted sweep.
	 * For example if a sphere is swept against the world, this would be the center of the sphere if there was no blocking hit.
	 */
	UPROPERTY()
	FVector_NetQuantize TraceEnd;

	/**
	  * If this test started in penetration (bStartPenetrating is true) and a depenetration vector can be computed,
	  * this value is the distance along Normal that will result in moving out of penetration.
	  * If the distance cannot be computed, this distance will be zero.
	  */
	UPROPERTY()
	float PenetrationDepth;

	/** Extra data about item that was hit (hit primitive specific). */
	UPROPERTY()
	int32 Item;

	/**
	 * Physical material that was hit.
	 * @note Must set bReturnPhysicalMaterial on the swept PrimitiveComponent or in the query params for this to be returned.
	 */
	UPROPERTY()
	TWeakObjectPtr<class UPhysicalMaterial> PhysMaterial;

	/** Actor hit by the trace. */
	UPROPERTY()
	TWeakObjectPtr<class AActor> Actor;

	/** PrimitiveComponent hit by the trace. */
	UPROPERTY()
	TWeakObjectPtr<class UPrimitiveComponent> Component;

	/** Name of bone we hit (for skeletal meshes). */
	UPROPERTY()
	FName BoneName;

	/** Face index we hit (for complex hits with triangle meshes). */
	UPROPERTY()
	int32 FaceIndex;

//#nv begin #Blast Pass our bone that was hit
	/** Name of the other bone which took part in hit event (for skeletal meshes). */
	UPROPERTY()
	FName OtherBoneName;
//nv end

	FHitResult()
	{
		Init();
	}
	
	explicit FHitResult(float InTime)
	{
		Init();
		Time = InTime;
	}

	explicit FHitResult(EForceInit InInit)
	{
		Init();
	}

	explicit FHitResult(ENoInit NoInit)
	{
	}

	explicit FHitResult(FVector Start, FVector End)
	{
		Init(Start, End);
	}

	/** Initialize empty hit result with given time. */
	FORCEINLINE void Init()
	{
		FMemory::Memzero(this, sizeof(FHitResult));
		Time = 1.f;
	}

	/** Initialize empty hit result with given time, TraceStart, and TraceEnd */
	FORCEINLINE void Init(FVector Start, FVector End)
	{
		FMemory::Memzero(this, sizeof(FHitResult));
		Time = 1.f;
		TraceStart = Start;
		TraceEnd = End;
	}

	/** Ctor for easily creating "fake" hits from limited data. */
	FHitResult(class AActor* InActor, class UPrimitiveComponent* InComponent, FVector const& HitLoc, FVector const& HitNorm);
 
	/** Reset hit result while optionally saving TraceStart and TraceEnd. */
	FORCEINLINE void Reset(float InTime = 1.f, bool bPreserveTraceData = true)
	{
		const FVector SavedTraceStart = TraceStart;
		const FVector SavedTraceEnd = TraceEnd;
		Init();
		Time = InTime;
		if (bPreserveTraceData)
		{
			TraceStart = SavedTraceStart;
			TraceEnd = SavedTraceEnd;
		}
	}

	/** Utility to return the Actor that owns the Component that was hit. */
	FORCEINLINE AActor* GetActor() const
	{
		return Actor.Get();
	}

	/** Utility to return the Component that was hit. */
	FORCEINLINE UPrimitiveComponent* GetComponent() const
	{
		return Component.Get();
	}

	/** Optimized serialize function */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Return true if there was a blocking hit that was not caused by starting in penetration. */
	FORCEINLINE bool IsValidBlockingHit() const
	{
		return bBlockingHit && !bStartPenetrating;
	}

	/** Static utility function that returns the first 'blocking' hit in an array of results. */
	static FHitResult* GetFirstBlockingHit(TArray<FHitResult>& InHits)
	{
		for(int32 HitIdx=0; HitIdx<InHits.Num(); HitIdx++)
		{
			if(InHits[HitIdx].bBlockingHit)
			{
				return &InHits[HitIdx];
			}
		}
		return nullptr;
	}

	/** Static utility function that returns the number of blocking hits in array. */
	static int32 GetNumBlockingHits(const TArray<FHitResult>& InHits)
	{
		int32 NumBlocks = 0;
		for(int32 HitIdx=0; HitIdx<InHits.Num(); HitIdx++)
		{
			if(InHits[HitIdx].bBlockingHit)
			{
				NumBlocks++;
			}
		}
		return NumBlocks;
	}

	/** Static utility function that returns the number of overlapping hits in array. */
	static int32 GetNumOverlapHits(const TArray<FHitResult>& InHits)
	{
		return (InHits.Num() - GetNumBlockingHits(InHits));
	}

	/**
	 * Get a copy of the HitResult with relevant information reversed.
	 * For example when receiving a hit from another object, we reverse the normals.
	 */
	static FHitResult GetReversedHit(const FHitResult& Hit)
	{
		FHitResult Result(Hit);
		Result.Normal = -Result.Normal;
		Result.ImpactNormal = -Result.ImpactNormal;
		return Result;
	}

	FString ToString() const;
};

// All members of FHitResult are PODs.
template<> struct TIsPODType<FHitResult> { enum { Value = true }; };

template<>
struct TStructOpsTypeTraits<FHitResult> : public TStructOpsTypeTraitsBase2<FHitResult>
{
	enum
	{
		WithNetSerializer = true,
	};
};


/** Whether to teleport physics body or not */
enum class ETeleportType
{
	/** Do not teleport physics body. This means velocity will reflect the movement between initial and final position, and collisions along the way will occur */
	None,
	/** Teleport physics body so that velocity remains the same and no collision occurs */
	TeleportPhysics
};

FORCEINLINE ETeleportType TeleportFlagToEnum(bool bTeleport) { return bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None; }
FORCEINLINE bool TeleportEnumToFlag(ETeleportType Teleport) { return ETeleportType::TeleportPhysics == Teleport; }


/** Structure containing information about one hit of an overlap test */
USTRUCT()
struct ENGINE_API FOverlapResult
{
	GENERATED_USTRUCT_BODY()

	/** Actor that the check hit. */
	UPROPERTY()
	TWeakObjectPtr<class AActor> Actor;

	/** PrimitiveComponent that the check hit. */
	UPROPERTY()
	TWeakObjectPtr<class UPrimitiveComponent> Component;

	/** This is the index of the overlapping item. 
		For DestructibleComponents, this is the ChunkInfo index. 
		For SkeletalMeshComponents this is the Body index or INDEX_NONE for single body */
	int32 ItemIndex;

	/** Utility to return the Actor that owns the Component that was hit */
	AActor* GetActor() const;

	/** Utility to return the Component that was hit */
	UPrimitiveComponent* GetComponent() const;

	/** Indicates if this hit was requesting a block - if false, was requesting a touch instead */
	UPROPERTY()
	uint32 bBlockingHit:1;

	FOverlapResult()
	{
		FMemory::Memzero(this, sizeof(FOverlapResult));
	}
};

// All members of FOverlapResult are PODs.
template<> struct TIsPODType<FOverlapResult> { enum { Value = true }; };

/** Structure containing information about minimum translation direction (MTD) */
USTRUCT()
struct ENGINE_API FMTDResult
{
	GENERATED_USTRUCT_BODY()

	/** Normalized direction of the minimum translation required to fix penetration. */
	UPROPERTY()
	FVector Direction;

	/** Distance required to move along the MTD vector (Direction). */
	UPROPERTY()
	float Distance;

	FMTDResult()
	{
		FMemory::Memzero(this, sizeof(FMTDResult));
	}
};


/** Struct used for passing information from Matinee to an Actor for blending animations during a sequence. */
USTRUCT()
struct FAnimSlotInfo
{
	GENERATED_USTRUCT_BODY()

	/** Name of slot that we want to play the animtion in. */
	UPROPERTY()
	FName SlotName;

	/** Strength of each Channel within this Slot. Channel indexs are determined by track order in Matinee. */
	UPROPERTY()
	TArray<float> ChannelWeights;
};


/** Used to indicate each slot name and how many channels they have. */
USTRUCT()
struct FAnimSlotDesc
{
	GENERATED_USTRUCT_BODY()

	/** Name of the slot. */
	UPROPERTY()
	FName SlotName;

	/** Number of channels that are available in this slot. */
	UPROPERTY()
	int32 NumChannels;


	FAnimSlotDesc()
		: NumChannels(0)
	{ }

};

/** Enum for controlling buckets for update rate optimizations if we need to stagger
 *  Multiple actor populations separately.
 */
UENUM()
enum class EUpdateRateShiftBucket : uint8
{
	ShiftBucket0 = 0,
	ShiftBucket1,
	ShiftBucket2,
	ShiftBucket3,
	ShiftBucket4,
	ShiftBucket5,
	ShiftBucketMax
};

/** Container for Animation Update Rate parameters.
 * They are shared for all components of an Actor, so they can be updated in sync. */
USTRUCT()
struct FAnimUpdateRateParameters
{
	GENERATED_USTRUCT_BODY()

public:
	enum EOptimizeMode
	{
		TrailMode,
		LookAheadMode,
	};

	/** Cache which Update Rate Optimization mode we are using */
	EOptimizeMode OptimizeMode;

	/** How often animation will be updated/ticked. 1 = every frame, 2 = every 2 frames, etc. */
	UPROPERTY()
	int32 UpdateRate;

	/** How often animation will be evaluated. 1 = every frame, 2 = every 2 frames, etc.
	 *  has to be a multiple of UpdateRate. */
	UPROPERTY()
	int32 EvaluationRate;

	/** When skipping a frame, should it be interpolated or frozen? */
	UPROPERTY()
	uint32 bInterpolateSkippedFrames : 1;

	/** Whether or not to use the defined LOD/Frameskip map instead of separate distance factor thresholds */
	UPROPERTY()
	uint32 bShouldUseLodMap : 1;

	/** If set, LOD/Frameskip map will be queried with mesh's MinLodModel instead of current LOD (PredictedLODLevel) */
	UPROPERTY()
	uint32 bShouldUseMinLod : 1;

	/** (This frame) animation update should be skipped. */
	UPROPERTY()
	uint32 bSkipUpdate : 1;

	/** (This frame) animation evaluation should be skipped. */
	UPROPERTY()
	uint32 bSkipEvaluation : 1;

	UPROPERTY(Transient)
	/** Track time we have lost via skipping */
	float TickedPoseOffestTime;

	UPROPERTY(Transient)
	/** Total time of the last series of skipped updates */
	float AdditionalTime;

	/** The delta time of the last tick */
	float ThisTickDelta;

	/** Rate of animation evaluation when non rendered (off screen and dedicated servers).
	 * a value of 4 means evaluated 1 frame, then 3 frames skipped */
	UPROPERTY()
	int32 BaseNonRenderedUpdateRate;

	/** Array of MaxDistanceFactor to use for AnimUpdateRate when mesh is visible (rendered).
	 * MaxDistanceFactor is size on screen, as used by LODs
	 * Example:
	 *		BaseVisibleDistanceFactorThesholds.Add(0.4f)
	 *		BaseVisibleDistanceFactorThesholds.Add(0.2f)
	 * means:
	 *		0 frame skip, MaxDistanceFactor > 0.4f
	 *		1 frame skip, MaxDistanceFactor > 0.2f
	 *		2 frame skip, MaxDistanceFactor > 0.0f
	 */
	UPROPERTY()
	TArray<float> BaseVisibleDistanceFactorThesholds;

	/** Map of LOD levels to frame skip amounts. if bShouldUseLodMap is set these values will be used for
	 * the frameskip amounts and the distance factor thresholds will be ignored. The flag and these values
	 * should be configured using the customization callback when parameters are created for a component.
	 *
	 * Note that this is # of frames to skip, so if you have 20, that means every 21th frame, it will update, and evaluate. 
	 */
	UPROPERTY()
	TMap<int32, int32> LODToFrameSkipMap;

	/** Max Evaluation Rate allowed for interpolation to be enabled. Beyond, interpolation will be turned off. */
	UPROPERTY()
	int32 MaxEvalRateForInterpolation;

	/** The bucket to use when deciding which counter to use to calculate shift values */
	UPROPERTY()
	EUpdateRateShiftBucket ShiftBucket;

public:

	/** Default constructor. */
	FAnimUpdateRateParameters()
		: OptimizeMode(TrailMode)
		, UpdateRate(1)
		, EvaluationRate(1)
		, bInterpolateSkippedFrames(false)
		, bShouldUseLodMap(false)
		, bShouldUseMinLod(false)
		, bSkipUpdate(false)
		, bSkipEvaluation(false)
		, TickedPoseOffestTime(0.f)
		, AdditionalTime(0.f)
		, ThisTickDelta(0.f)
		, BaseNonRenderedUpdateRate(4)
		, MaxEvalRateForInterpolation(4)
		, ShiftBucket(EUpdateRateShiftBucket::ShiftBucket0)
	{ 
		BaseVisibleDistanceFactorThesholds.Add(0.24f);
		BaseVisibleDistanceFactorThesholds.Add(0.12f);
	}

	/** Set parameters and verify inputs for Trail Mode (original behaviour - skip frames, track skipped time and then catch up afterwards).
	 * @param : UpdateShiftRate. Shift our update frames so that updates across all skinned components are staggered
	 * @param : NewUpdateRate. How often animation will be updated/ticked. 1 = every frame, 2 = every 2 frames, etc.
	 * @param : NewEvaluationRate. How often animation will be evaluated. 1 = every frame, 2 = every 2 frames, etc.
	 * @param : bNewInterpSkippedFrames. When skipping a frame, should it be interpolated or frozen?
	 */
	void SetTrailMode(float DeltaTime, uint8 UpdateRateShift, int32 NewUpdateRate, int32 NewEvaluationRate, bool bNewInterpSkippedFrames);

	void SetLookAheadMode(float DeltaTime, uint8 UpdateRateShift, float LookAheadAmount);

	float GetInterpolationAlpha() const;

	float GetRootMotionInterp() const;

	bool DoEvaluationRateOptimizations() const
	{
		return OptimizeMode == LookAheadMode || EvaluationRate > 1;
	}

	/* Getter for bSkipUpdate */
	bool ShouldSkipUpdate() const
	{
		return bSkipUpdate;
	}

	/* Getter for bSkipEvaluation */
	bool ShouldSkipEvaluation() const
	{
		return bSkipEvaluation;
	}

	/* Getter for bInterpolateSkippedFrames */
	bool ShouldInterpolateSkippedFrames() const
	{
		return bInterpolateSkippedFrames;
	}

	/** Called when we are ticking a pose to make sure we accumulate all needed time */
	float GetTimeAdjustment()
	{
		return AdditionalTime;
	}

	FColor GetUpdateRateDebugColor() const
	{
		if (OptimizeMode == TrailMode)
		{
			switch (UpdateRate)
			{
			case 1: return FColor::Red;
			case 2: return FColor::Green;
			case 3: return FColor::Blue;
			}
			return FColor::Black;
		}
		else
		{
			if (bSkipUpdate)
			{
				return FColor::Yellow;
			}
			return FColor::Green;
		}
	}
};


/**
 * Point Of View type.
 */
USTRUCT(BlueprintType)
struct FPOV
{
	GENERATED_USTRUCT_BODY()

	/** Location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=POV)
	FVector Location;

	/** Rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=POV)
	FRotator Rotation;

	/** FOV angle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=POV)
	float FOV;

	FPOV() 
	: Location(ForceInit),Rotation(ForceInit), FOV(90.0f)
	{}

	FPOV(FVector InLocation, FRotator InRotation, float InFOV)
	: Location(InLocation), Rotation(InRotation), FOV(InFOV) 
	{}

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar, FPOV& POV)
	{
		return Ar << POV.Location << POV.Rotation << POV.FOV;
	}
};



/**
 * Settings applied when building a mesh.
 */
USTRUCT()
struct FMeshBuildSettings
{
	GENERATED_USTRUCT_BODY()

	/** If true, degenerate triangles will be removed. */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	bool bUseMikkTSpace;

	/** If true, normals in the raw mesh are ignored and recomputed. */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	bool bRecomputeNormals;

	/** If true, tangents in the raw mesh are ignored and recomputed. */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	bool bRecomputeTangents;

	/** If true, degenerate triangles will be removed. */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	bool bRemoveDegenerates;
	
	/** Required for PNT tessellation but can be slow. Recommend disabling for larger meshes. */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	bool bBuildAdjacencyBuffer;

	/** Required to optimize mesh in mirrored transform. Double index buffer size. */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	bool bBuildReversedIndexBuffer;

	/** If true, Tangents will be stored at 16 bit vs 8 bit precision. */
	UPROPERTY(EditAnywhere, Category = BuildSettings)
	bool bUseHighPrecisionTangentBasis;

	/** If true, UVs will be stored at full floating point precision. */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	bool bUseFullPrecisionUVs;

	UPROPERTY(EditAnywhere, Category=BuildSettings)
	bool bGenerateLightmapUVs;

	UPROPERTY(EditAnywhere, Category=BuildSettings)
	int32 MinLightmapResolution;

	UPROPERTY(EditAnywhere, Category=BuildSettings, meta=(DisplayName="Source Lightmap Index"))
	int32 SrcLightmapIndex;

	UPROPERTY(EditAnywhere, Category=BuildSettings, meta=(DisplayName="Destination Lightmap Index"))
	int32 DstLightmapIndex;

	UPROPERTY()
	float BuildScale_DEPRECATED;

	/** The local scale applied when building the mesh */
	UPROPERTY(EditAnywhere, Category=BuildSettings, meta=(DisplayName="Build Scale"))
	FVector BuildScale3D;

	/** 
	 * Scale to apply to the mesh when allocating the distance field volume texture.
	 * The default scale is 1, which is assuming that the mesh will be placed unscaled in the world.
	 */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	float DistanceFieldResolutionScale;

	/** 
	 * Whether to generate the distance field treating every triangle hit as a front face.  
	 * When enabled prevents the distance field from being discarded due to the mesh being open, but also lowers Distance Field AO quality.
	 */
	UPROPERTY(EditAnywhere, Category=BuildSettings, meta=(DisplayName="Two-Sided Distance Field Generation"))
	bool bGenerateDistanceFieldAsIfTwoSided;

	UPROPERTY()
	float DistanceFieldBias_DEPRECATED;

	UPROPERTY(EditAnywhere, Category=BuildSettings)
	class UStaticMesh* DistanceFieldReplacementMesh;

	/** Default settings. */
	FMeshBuildSettings()
		: bUseMikkTSpace(true)
		, bRecomputeNormals(true)
		, bRecomputeTangents(true)
		, bRemoveDegenerates(true)
		, bBuildAdjacencyBuffer(true)
		, bBuildReversedIndexBuffer(true)
		, bUseHighPrecisionTangentBasis(false)
		, bUseFullPrecisionUVs(false)
		, bGenerateLightmapUVs(true)
		, MinLightmapResolution(64)
		, SrcLightmapIndex(0)
		, DstLightmapIndex(1)
		, BuildScale_DEPRECATED(1.0f)
		, BuildScale3D(1.0f, 1.0f, 1.0f)
		, DistanceFieldResolutionScale(1.0f)
		, bGenerateDistanceFieldAsIfTwoSided(false)
		, DistanceFieldBias_DEPRECATED(0.0f)
		, DistanceFieldReplacementMesh(NULL)
	{ }

	/** Equality operator. */
	bool operator==(const FMeshBuildSettings& Other) const
	{
		return bRecomputeNormals == Other.bRecomputeNormals
			&& bRecomputeTangents == Other.bRecomputeTangents
			&& bUseMikkTSpace == Other.bUseMikkTSpace
			&& bRemoveDegenerates == Other.bRemoveDegenerates
			&& bBuildAdjacencyBuffer == Other.bBuildAdjacencyBuffer
			&& bBuildReversedIndexBuffer == Other.bBuildReversedIndexBuffer
			&& bUseHighPrecisionTangentBasis == Other.bUseHighPrecisionTangentBasis
			&& bUseFullPrecisionUVs == Other.bUseFullPrecisionUVs
			&& bGenerateLightmapUVs == Other.bGenerateLightmapUVs
			&& MinLightmapResolution == Other.MinLightmapResolution
			&& SrcLightmapIndex == Other.SrcLightmapIndex
			&& DstLightmapIndex == Other.DstLightmapIndex
			&& BuildScale3D == Other.BuildScale3D
			&& DistanceFieldResolutionScale == Other.DistanceFieldResolutionScale
			&& bGenerateDistanceFieldAsIfTwoSided == Other.bGenerateDistanceFieldAsIfTwoSided
			&& DistanceFieldReplacementMesh == Other.DistanceFieldReplacementMesh;
	}

	/** Inequality. */
	bool operator!=(const FMeshBuildSettings& Other) const
	{
		return !(*this == Other);
	}
};



USTRUCT(BlueprintType)
struct ENGINE_API FDamageEvent
{
	GENERATED_USTRUCT_BODY()

public:

	/** Default constructor (no initialization). */
	FDamageEvent() { }

	FDamageEvent(FDamageEvent const& InDamageEvent)
		: DamageTypeClass(InDamageEvent.DamageTypeClass)
	{ }
	
	virtual ~FDamageEvent() { }

	explicit FDamageEvent(TSubclassOf<class UDamageType> InDamageTypeClass)
		: DamageTypeClass(InDamageTypeClass)
	{ }

	/** Optional DamageType for this event.  If nullptr, UDamageType will be assumed. */
	UPROPERTY()
	TSubclassOf<class UDamageType> DamageTypeClass;

	/** ID for this class. NOTE this must be unique for all damage events. */
	static const int32 ClassID = 0;

	virtual int32 GetTypeID() const { return FDamageEvent::ClassID; }
	virtual bool IsOfType(int32 InID) const { return FDamageEvent::ClassID == InID; };

	/** This is for compatibility with old-style functions which want a unified set of hit data regardless of type of hit.  Ideally this will go away over time. */
	virtual void GetBestHitInfo(AActor const* HitActor, AActor const* HitInstigator, struct FHitResult& OutHitInfo, FVector& OutImpulseDir) const;
};


USTRUCT()
struct ENGINE_API FPointDamageEvent : public FDamageEvent
{
	GENERATED_USTRUCT_BODY()

	// skipping ImpulseMag for now
	UPROPERTY()
	float Damage;
	
	/** Direction the shot came from. Should be normalized. */
	UPROPERTY()
	FVector_NetQuantizeNormal ShotDirection;
	
	UPROPERTY()
	struct FHitResult HitInfo;

	FPointDamageEvent() : HitInfo() {}
	FPointDamageEvent(float InDamage, struct FHitResult const& InHitInfo, FVector const& InShotDirection, TSubclassOf<class UDamageType> InDamageTypeClass)
		: FDamageEvent(InDamageTypeClass), Damage(InDamage), ShotDirection(InShotDirection), HitInfo(InHitInfo)
	{}
	
	/** ID for this class. NOTE this must be unique for all damage events. */
	static const int32 ClassID = 1;
	
	virtual int32 GetTypeID() const override { return FPointDamageEvent::ClassID; };
	virtual bool IsOfType(int32 InID) const override { return (FPointDamageEvent::ClassID == InID) || FDamageEvent::IsOfType(InID); };

	/** Simple API for common cases where we are happy to assume a single hit is expected, even though damage event may have multiple hits. */
	virtual void GetBestHitInfo(AActor const* HitActor, AActor const* HitInstigator, struct FHitResult& OutHitInfo, FVector& OutImpulseDir) const override;
};


USTRUCT(BlueprintType)
struct ENGINE_API FRadialDamageParams
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RadialDamageParams)
	float BaseDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RadialDamageParams)
	float MinimumDamage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RadialDamageParams)
	float InnerRadius;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RadialDamageParams)
	float OuterRadius;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RadialDamageParams)
	float DamageFalloff;

// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RadiusDamageParams)
// 	float BaseImpulseMag;

	FRadialDamageParams()
		: BaseDamage(0.f), MinimumDamage(0.f), InnerRadius(0.f), OuterRadius(0.f), DamageFalloff(1.f)
	{}
	FRadialDamageParams(float InBaseDamage, float InInnerRadius, float InOuterRadius, float InDamageFalloff)
		: BaseDamage(InBaseDamage), MinimumDamage(0.f), InnerRadius(InInnerRadius), OuterRadius(InOuterRadius), DamageFalloff(InDamageFalloff)
	{}
	FRadialDamageParams(float InBaseDamage, float InMinimumDamage, float InInnerRadius, float InOuterRadius, float InDamageFalloff)
		: BaseDamage(InBaseDamage), MinimumDamage(InMinimumDamage), InnerRadius(InInnerRadius), OuterRadius(InOuterRadius), DamageFalloff(InDamageFalloff)
	{}
	FRadialDamageParams(float InBaseDamage, float InRadius)
		: BaseDamage(InBaseDamage), MinimumDamage(0.f), InnerRadius(0.f), OuterRadius(InRadius), DamageFalloff(1.f)
	{}

	float GetDamageScale(float DistanceFromEpicenter) const;

	/** Return outermost radius of the damage area. Protects against malformed data. */
	float GetMaxRadius() const { return FMath::Max( FMath::Max(InnerRadius, OuterRadius), 0.f ); }
};


USTRUCT()
struct ENGINE_API FRadialDamageEvent : public FDamageEvent
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FRadialDamageParams Params;
	
	UPROPERTY()
	FVector Origin;

	// @fixme, will not replicate properly?  component pointer
	UPROPERTY()
	TArray<struct FHitResult> ComponentHits;

	/** ID for this class. NOTE this must be unique for all damage events. */
	static const int32 ClassID = 2;

	virtual int32 GetTypeID() const override { return FRadialDamageEvent::ClassID; };
	virtual bool IsOfType(int32 InID) const override { return (FRadialDamageEvent::ClassID == InID) || FDamageEvent::IsOfType(InID); };

	/** Simple API for common cases where we are happy to assume a single hit is expected, even though damage event may have multiple hits. */
	virtual void GetBestHitInfo(AActor const* HitActor, AActor const* HitInstigator, struct FHitResult& OutHitInfo, FVector& OutImpulseDir) const override;
};


UENUM()
enum ENetRole
{
	/** No role at all. */
	ROLE_None,
	/** Locally simulated proxy of this actor. */
	ROLE_SimulatedProxy,
	/** Locally autonomous proxy of this actor. */
	ROLE_AutonomousProxy,
	/** Authoritative control over the actor. */
	ROLE_Authority,
	ROLE_MAX,
};


UENUM()
enum ENetDormancy
{
	/** This actor can never go network dormant. */
	DORM_Never,
	/** This actor can go dormant, but is not currently dormant. Game code will tell it when it go dormant. */
	DORM_Awake,
	/** This actor wants to go fully dormant for all connections. */
	DORM_DormantAll,
	/** This actor may want to go dormant for some connections, GetNetDormancy() will be called to find out which. */
	DORM_DormantPartial,
	/** This actor is initially dormant for all connection if it was placed in map. */
	DORM_Initial,
	DORN_MAX,
};


UENUM()
namespace EAutoReceiveInput
{
	enum Type
	{
		Disabled,
		Player0,
		Player1,
		Player2,
		Player3,
		Player4,
		Player5,
		Player6,
		Player7,
	};
}


UENUM()
enum class EAutoPossessAI : uint8
{
	/** Feature is disabled (do not automatically possess AI). */
	Disabled,
	/** Only possess by an AI Controller if Pawn is placed in the world. */
	PlacedInWorld,
	/** Only possess by an AI Controller if Pawn is spawned after the world has loaded. */
	Spawned,
	/** Pawn is automatically possessed by an AI Controller whenever it is created. */
	PlacedInWorldOrSpawned,
};


UENUM(BlueprintType)
namespace EEndPlayReason
{
	enum Type
	{
		/** When the Actor or Component is explicitly destroyed. */
		Destroyed,
		/** When the world is being unloaded for a level transition. */
		LevelTransition,
		/** When the world is being unloaded because PIE is ending. */
		EndPlayInEditor,
		/** When the level it is a member of is streamed out. */
		RemovedFromWorld,
		/** When the application is being exited. */
		Quit,
	};

}

DECLARE_DYNAMIC_DELEGATE(FTimerDynamicDelegate);

// Unique handle that can be used to distinguish timers that have identical delegates.
USTRUCT(BlueprintType)
struct FTimerHandle
{
	GENERATED_BODY()

	friend class FTimerManager;

	FTimerHandle()
	: Handle(0)
	{
	}

	bool IsValid() const
	{
		return Handle != 0;
	}

	void Invalidate()
	{
		Handle = 0;
	}

	DEPRECATED(4.12, "This function is deprecated to avoid problems with timer wraparound. Please call FTimerManager::ValidateHandle.")
	void MakeValid();

	bool operator==(const FTimerHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FTimerHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%ull"), Handle);
	}

private:
	UPROPERTY(Transient)
	uint64 Handle;
};

UENUM()
enum class EVectorQuantization : uint8
{
	/** Each vector component will be rounded to the nearest whole number. */
	RoundWholeNumber,
	/** Each vector component will be rounded, preserving one decimal place. */
	RoundOneDecimal,
	/** Each vector component will be rounded, preserving two decimal places. */
	RoundTwoDecimals
};

UENUM()
enum class ERotatorQuantization : uint8
{
	/** The rotator will be compressed to 8 bits per component. */
	ByteComponents,
	/** The rotator will be compressed to 16 bits per component. */
	ShortComponents
};

/** Replicated movement data of our RootComponent.
  * Struct used for efficient replication as velocity and location are generally replicated together (this saves a repindex) 
  * and velocity.Z is commonly zero (most position replications are for walking pawns). 
  */
USTRUCT()
struct ENGINE_API FRepMovement
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Transient)
	FVector LinearVelocity;

	UPROPERTY(Transient)
	FVector AngularVelocity;
	
	UPROPERTY(Transient)
	FVector Location;

	UPROPERTY(Transient)
	FRotator Rotation;

	/** If set, RootComponent should be sleeping. */
	UPROPERTY(Transient)
	uint8 bSimulatedPhysicSleep : 1;

	/** If set, additional physic data (angular velocity) will be replicated. */
	UPROPERTY(Transient)
	uint8 bRepPhysics : 1;

	/** Allows tuning the compression level for the replicated location vector. You should only need to change this from the default if you see visual artifacts. */
	UPROPERTY(EditDefaultsOnly, Category=Replication, AdvancedDisplay)
	EVectorQuantization LocationQuantizationLevel;

	/** Allows tuning the compression level for the replicated velocity vectors. You should only need to change this from the default if you see visual artifacts. */
	UPROPERTY(EditDefaultsOnly, Category=Replication, AdvancedDisplay)
	EVectorQuantization VelocityQuantizationLevel;

	/** Allows tuning the compression level for replicated rotation. You should only need to change this from the default if you see visual artifacts. */
	UPROPERTY(EditDefaultsOnly, Category=Replication, AdvancedDisplay)
	ERotatorQuantization RotationQuantizationLevel;

	FRepMovement();

	bool SerializeQuantizedVector(FArchive& Ar, FVector& Vector, EVectorQuantization QuantizationLevel)
	{
		// Since FRepMovement used to use FVector_NetQuantize100, we're allowing enough bits per component
		// regardless of the quantization level so that we can still support at least the same maximum magnitude
		// (2^30 / 100, or ~10 million).
		// This uses no inherent extra bandwidth since we're still using the same number of bits to store the
		// bits-per-component value. Of course, larger magnitudes will still use more bandwidth,
		// as has always been the case.
		switch(QuantizationLevel)
		{
			case EVectorQuantization::RoundTwoDecimals:
			{
				return SerializePackedVector<100, 30>(Vector, Ar);
			}

			case EVectorQuantization::RoundOneDecimal:
			{
				return SerializePackedVector<10, 27>(Vector, Ar);
			}

			default:
			{
				return SerializePackedVector<1, 24>(Vector, Ar);
			}
		}
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		// pack bitfield with flags
		uint8 Flags = (bSimulatedPhysicSleep << 0) | (bRepPhysics << 1);
		Ar.SerializeBits(&Flags, 2);
		bSimulatedPhysicSleep = ( Flags & ( 1 << 0 ) ) ? 1 : 0;
		bRepPhysics = ( Flags & ( 1 << 1 ) ) ? 1 : 0;

		bOutSuccess = true;

		// update location, rotation, linear velocity
		bOutSuccess &= SerializeQuantizedVector( Ar, Location, LocationQuantizationLevel );
		
		switch(RotationQuantizationLevel)
		{
			case ERotatorQuantization::ByteComponents:
			{
				Rotation.SerializeCompressed( Ar );
				break;
			}

			case ERotatorQuantization::ShortComponents:
			{
				Rotation.SerializeCompressedShort( Ar );
				break;
			}
		}
		
		bOutSuccess &= SerializeQuantizedVector( Ar, LinearVelocity, VelocityQuantizationLevel );

		// update angular velocity if required
		if ( bRepPhysics )
		{
			bOutSuccess &= SerializeQuantizedVector( Ar, AngularVelocity, VelocityQuantizationLevel );
		}

		return true;
	}

	void FillFrom(const struct FRigidBodyState& RBState, const AActor* const Actor = nullptr)
	{
		Location = RebaseOntoZeroOrigin(RBState.Position, Actor);
		Rotation = RBState.Quaternion.Rotator();
		LinearVelocity = RBState.LinVel;
		AngularVelocity = RBState.AngVel;
		bSimulatedPhysicSleep = (RBState.Flags & ERigidBodyFlags::Sleeping) != 0;
		bRepPhysics = true;
	}

	void CopyTo(struct FRigidBodyState& RBState, const AActor* const Actor = nullptr)
	{
		RBState.Position = RebaseOntoLocalOrigin(Location, Actor);
		RBState.Quaternion = Rotation.Quaternion();
		RBState.LinVel = LinearVelocity;
		RBState.AngVel = AngularVelocity;
		RBState.Flags = (bSimulatedPhysicSleep ? ERigidBodyFlags::Sleeping : ERigidBodyFlags::None) | ERigidBodyFlags::NeedsUpdate;
	}

	bool operator==(const FRepMovement& Other) const
	{
		if ( LinearVelocity != Other.LinearVelocity )
		{
			return false;
		}

		if ( AngularVelocity != Other.AngularVelocity )
		{
			return false;
		}

		if ( Location != Other.Location )
		{
			return false;
		}

		if ( Rotation != Other.Rotation )
		{
			return false;
		}

		if ( bSimulatedPhysicSleep != Other.bSimulatedPhysicSleep )
		{
			return false;
		}

		if ( bRepPhysics != Other.bRepPhysics )
		{
			return false;
		}

		return true;
	}

	bool operator!=(const FRepMovement& Other) const
	{
		return !(*this == Other);
	}

	static int32 EnableMultiplayerWorldOriginRebasing;

	/** Rebase zero-origin position onto local world origin value. */
	static FVector RebaseOntoLocalOrigin(const struct FVector& Location, const struct FIntVector& LocalOrigin);

	/** Rebase local-origin position onto zero world origin value. */
	static FVector RebaseOntoZeroOrigin(const struct FVector& Location, const struct FIntVector& LocalOrigin);

	/** Rebase zero-origin position onto an Actor's local world origin. */
	static FVector RebaseOntoLocalOrigin(const struct FVector& Location, const AActor* const WorldContextActor);

	/** Rebase an Actor's local-origin position onto zero world origin value. */
	static FVector RebaseOntoZeroOrigin(const struct FVector& Location, const AActor* const WorldContextActor);

	/** Rebase zero-origin position onto local world origin value based on an actor component's world. */
	static FVector RebaseOntoLocalOrigin(const struct FVector& Location, const class UActorComponent* const WorldContextActorComponent);

	/** Rebase local-origin position onto zero world origin value based on an actor component's world.*/
	static FVector RebaseOntoZeroOrigin(const struct FVector& Location, const class UActorComponent* const WorldContextActorComponent);
};


/** Handles attachment replication to clients. Movement replication will not happen while AttachParent is non-nullptr */
USTRUCT()
struct FRepAttachment
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	class AActor* AttachParent;

	UPROPERTY()
	FVector_NetQuantize100 LocationOffset;

	UPROPERTY()
	FVector_NetQuantize100 RelativeScale3D;

	UPROPERTY()
	FRotator RotationOffset;

	UPROPERTY()
	FName AttachSocket;

	UPROPERTY()
	class USceneComponent* AttachComponent;

	FRepAttachment()
		: AttachParent(nullptr)
		, LocationOffset(ForceInit)
		, RelativeScale3D(ForceInit)
		, RotationOffset(ForceInit)
		, AttachSocket(NAME_None)
		, AttachComponent(nullptr)
	{ }
};


/**
 * Controls behavior of WalkableSlopeOverride, determining how to affect walkability of surfaces for Characters.
 * @see FWalkableSlopeOverride
 * @see UCharacterMovementComponent::GetWalkableFloorAngle(), UCharacterMovementComponent::SetWalkableFloorAngle()
 */
UENUM(BlueprintType)
enum EWalkableSlopeBehavior
{
	/** Don't affect the walkable slope. Walkable slope angle will be ignored. */
	WalkableSlope_Default		UMETA(DisplayName="Unchanged"),

	/**
	 * Increase walkable slope.
	 * Makes it easier to walk up a surface, by allowing traversal over higher-than-usual angles.
	 * @see FWalkableSlopeOverride::WalkableSlopeAngle
	 */
	WalkableSlope_Increase		UMETA(DisplayName="Increase Walkable Slope"),

	/**
	 * Decrease walkable slope.
	 * Makes it harder to walk up a surface, by restricting traversal to lower-than-usual angles.
	 * @see FWalkableSlopeOverride::WalkableSlopeAngle
	 */
	WalkableSlope_Decrease		UMETA(DisplayName="Decrease Walkable Slope"),

	/**
	 * Make surface unwalkable.
	 * Note: WalkableSlopeAngle will be ignored.
	 */
	WalkableSlope_Unwalkable	UMETA(DisplayName="Unwalkable"),
	
	WalkableSlope_Max		UMETA(Hidden),
};


/** Struct allowing control over "walkable" normals, by allowing a restriction or relaxation of what steepness is normally walkable. */
USTRUCT(BlueprintType)
struct FWalkableSlopeOverride
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Behavior of this surface (whether we affect the walkable slope).
	 * @see GetWalkableSlopeBehavior(), SetWalkableSlopeBehavior()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=WalkableSlopeOverride)
	TEnumAsByte<EWalkableSlopeBehavior> WalkableSlopeBehavior;

	/**
	 * Override walkable slope angle (in degrees), applying the rules of the Walkable Slope Behavior.
	 * @see GetWalkableSlopeAngle(), SetWalkableSlopeAngle()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=WalkableSlopeOverride, meta=(ClampMin="0", ClampMax="90", UIMin="0", UIMax="90"))
	float WalkableSlopeAngle;

private:

	// Cached angle for which we computed a cosine.
	mutable float CachedSlopeAngle;
	// Cached cosine of angle.
	mutable float CachedSlopeCos;

public:

	FWalkableSlopeOverride()
		: WalkableSlopeBehavior(WalkableSlope_Default)
		, WalkableSlopeAngle(0.f)
		, CachedSlopeAngle(0.f)
		, CachedSlopeCos(1.f)
	{ }

	FWalkableSlopeOverride(EWalkableSlopeBehavior NewSlopeBehavior, float NewSlopeAngle)
		: WalkableSlopeBehavior(NewSlopeBehavior)
		, WalkableSlopeAngle(NewSlopeAngle)
		, CachedSlopeAngle(0.f)
		, CachedSlopeCos(1.f)
	{
	}

	// Gets the slope override behavior.
	FORCEINLINE EWalkableSlopeBehavior GetWalkableSlopeBehavior() const
	{
		return WalkableSlopeBehavior;
	}

	// Gets the slope angle used for the override behavior.
	FORCEINLINE float GetWalkableSlopeAngle() const
	{
		return WalkableSlopeAngle;
	}

	// Set the slope override behavior.
	FORCEINLINE void SetWalkableSlopeBehavior(EWalkableSlopeBehavior NewSlopeBehavior)
	{
		WalkableSlopeBehavior = NewSlopeBehavior;
	}

	// Set the slope angle used for the override behavior.
	FORCEINLINE void SetWalkableSlopeAngle(float NewSlopeAngle)
	{
		WalkableSlopeAngle = FMath::Clamp(NewSlopeAngle, 0.f, 90.f);
	}

	// Given a walkable floor normal Z value, either relax or restrict the value if we override such behavior.
	float ModifyWalkableFloorZ(float InWalkableFloorZ) const
	{
		switch(WalkableSlopeBehavior)
		{
			case WalkableSlope_Default:
			{
				return InWalkableFloorZ;
			}

			case WalkableSlope_Increase:
			{
				CheckCachedData();
				return FMath::Min(InWalkableFloorZ, CachedSlopeCos);
			}

			case WalkableSlope_Decrease:
			{
				CheckCachedData();
				return FMath::Max(InWalkableFloorZ, CachedSlopeCos);
			}

			case WalkableSlope_Unwalkable:
			{
				// Z component of a normal will always be less than this, so this will be unwalkable.
				return 2.0f;
			}

			default:
			{
				return InWalkableFloorZ;
			}
		}
	}

private:

	void CheckCachedData() const
	{
		if (CachedSlopeAngle != WalkableSlopeAngle)
		{
			const float AngleRads = FMath::DegreesToRadians(WalkableSlopeAngle);
			CachedSlopeCos = FMath::Clamp(FMath::Cos(AngleRads), 0.f, 1.f);
			CachedSlopeAngle = WalkableSlopeAngle;
		}
	}
};

template<> struct TIsPODType<FWalkableSlopeOverride> { enum { Value = true }; };

template<>
struct TStructOpsTypeTraits<FRepMovement> : public TStructOpsTypeTraitsBase2<FRepMovement>
{
	enum 
	{
		WithNetSerializer = true,
	};
};


/** Structure to hold and pass around transient flags used during replication. */
struct FReplicationFlags
{
	union
	{
		struct
		{
			/** True if replicating actor is owned by the player controller on the target machine. */
			uint32 bNetOwner:1;
			/** True if this is the initial network update for the replicating actor. */
			uint32 bNetInitial:1;
			/** True if this is actor is RemoteRole simulated. */
			uint32 bNetSimulated:1;
			/** True if this is actor's ReplicatedMovement.bRepPhysics flag is true. */
			uint32 bRepPhysics:1;
			/** True if this actor is replicating on a replay connection. */
			uint32 bReplay:1;
		};

		uint32	Value;
	};
	FReplicationFlags()
	{
		Value = 0;
	}
};


static_assert(sizeof(FReplicationFlags) == 4, "FReplicationFlags has invalid size.");

/** Struct used to specify the property name of the component to constrain */
USTRUCT()
struct FConstrainComponentPropName
{
	GENERATED_USTRUCT_BODY()

	/** Name of property */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FName	ComponentName;
};


/** 
 *	Struct that allows for different ways to reference a component. 
 *	If just an Actor is specified, will return RootComponent of that Actor.
 */
USTRUCT()
struct ENGINE_API FComponentReference
{
	GENERATED_USTRUCT_BODY()

	/** Pointer to a different Actor that owns the Component.  */
	UPROPERTY(EditInstanceOnly, Category=Component)
	AActor* OtherActor;

	/** Name of component property to use */
	UPROPERTY(EditAnywhere, Category=Component)
	FName	ComponentProperty;

	/** Allows direct setting of first component to constraint. */
	TWeakObjectPtr<class USceneComponent> OverrideComponent;

	/** Get the actual component pointer from this reference */
	class USceneComponent* GetComponent(AActor* OwningActor) const;
};


/** Types of surfaces in the game. */
UENUM(BlueprintType)
enum EPhysicalSurface
{
	SurfaceType_Default UMETA(DisplayName="Default"),
	SurfaceType1 UMETA(Hidden),
	SurfaceType2 UMETA(Hidden),
	SurfaceType3 UMETA(Hidden),
	SurfaceType4 UMETA(Hidden),
	SurfaceType5 UMETA(Hidden),
	SurfaceType6 UMETA(Hidden),
	SurfaceType7 UMETA(Hidden),
	SurfaceType8 UMETA(Hidden),
	SurfaceType9 UMETA(Hidden),
	SurfaceType10 UMETA(Hidden),
	SurfaceType11 UMETA(Hidden),
	SurfaceType12 UMETA(Hidden),
	SurfaceType13 UMETA(Hidden),
	SurfaceType14 UMETA(Hidden),
	SurfaceType15 UMETA(Hidden),
	SurfaceType16 UMETA(Hidden),
	SurfaceType17 UMETA(Hidden),
	SurfaceType18 UMETA(Hidden),
	SurfaceType19 UMETA(Hidden),
	SurfaceType20 UMETA(Hidden),
	SurfaceType21 UMETA(Hidden),
	SurfaceType22 UMETA(Hidden),
	SurfaceType23 UMETA(Hidden),
	SurfaceType24 UMETA(Hidden),
	SurfaceType25 UMETA(Hidden),
	SurfaceType26 UMETA(Hidden),
	SurfaceType27 UMETA(Hidden),
	SurfaceType28 UMETA(Hidden),
	SurfaceType29 UMETA(Hidden),
	SurfaceType30 UMETA(Hidden),
	SurfaceType31 UMETA(Hidden),
	SurfaceType32 UMETA(Hidden),
	SurfaceType33 UMETA(Hidden),
	SurfaceType34 UMETA(Hidden),
	SurfaceType35 UMETA(Hidden),
	SurfaceType36 UMETA(Hidden),
	SurfaceType37 UMETA(Hidden),
	SurfaceType38 UMETA(Hidden),
	SurfaceType39 UMETA(Hidden),
	SurfaceType40 UMETA(Hidden),
	SurfaceType41 UMETA(Hidden),
	SurfaceType42 UMETA(Hidden),
	SurfaceType43 UMETA(Hidden),
	SurfaceType44 UMETA(Hidden),
	SurfaceType45 UMETA(Hidden),
	SurfaceType46 UMETA(Hidden),
	SurfaceType47 UMETA(Hidden),
	SurfaceType48 UMETA(Hidden),
	SurfaceType49 UMETA(Hidden),
	SurfaceType50 UMETA(Hidden),
	SurfaceType51 UMETA(Hidden),
	SurfaceType52 UMETA(Hidden),
	SurfaceType53 UMETA(Hidden),
	SurfaceType54 UMETA(Hidden),
	SurfaceType55 UMETA(Hidden),
	SurfaceType56 UMETA(Hidden),
	SurfaceType57 UMETA(Hidden),
	SurfaceType58 UMETA(Hidden),
	SurfaceType59 UMETA(Hidden),
	SurfaceType60 UMETA(Hidden),
	SurfaceType61 UMETA(Hidden),
	SurfaceType62 UMETA(Hidden),
	SurfaceType_Max UMETA(Hidden)
};


/** Describes how often this component is allowed to move. */
UENUM()
namespace EComponentMobility
{
	enum Type
	{
		/**
		 * Static objects cannot be moved or changed in game.
		 * - Allows baked lighting
		 * - Fastest rendering
		 */
		Static,

		/**
		 * A stationary light will only have its shadowing and bounced lighting from static geometry baked by Lightmass, all other lighting will be dynamic.
		 * - It can change color and intensity in game.
		 * - Can't move
		 * - Allows partial baked lighting
		 * - Dynamic shadows
		 */
		Stationary,

		/**
		 * Movable objects can be moved and changed in game.
		 * - Totally dynamic
		 * - Can cast dynamic shadows
		 * - Slowest rendering
		 */
		Movable
	};
}


UCLASS(abstract, config=Engine)
class ENGINE_API UEngineTypes : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Convert a trace type to a collision channel. */
	static ECollisionChannel ConvertToCollisionChannel(ETraceTypeQuery TraceType);

	/** Convert an object type to a collision channel. */
	static ECollisionChannel ConvertToCollisionChannel(EObjectTypeQuery ObjectType);

	/** Convert a collision channel to an object type. Note: performs a search of object types. */
	static EObjectTypeQuery ConvertToObjectType(ECollisionChannel CollisionChannel);

	/** Convert a collision channel to a trace type. Note: performs a search of trace types. */
	static ETraceTypeQuery ConvertToTraceType(ECollisionChannel CollisionChannel);
};


/** Type of a socket on a scene component. */
UENUM()
namespace EComponentSocketType
{
	enum Type
	{
		/** Not a valid socket or bone name. */
		Invalid,

		/** Skeletal bone. */
		Bone,

		/** Socket. */
		Socket,
	};
}


/** Info about a socket on a scene component */
struct FComponentSocketDescription
{
	/** Name of the socket */
	FName Name;

	/** Type of the socket */
	TEnumAsByte<EComponentSocketType::Type> Type;

	FComponentSocketDescription()
		: Name(NAME_None)
		, Type(EComponentSocketType::Invalid)
	{
	}

	FComponentSocketDescription(FName SocketName, EComponentSocketType::Type SocketType)
		: Name(SocketName)
		, Type(SocketType)
	{
	}
};

/** Dynamic delegate to use by components that want to route the broken-event into blueprints */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConstraintBrokenSignature, int32, ConstraintIndex);

// ANGULAR DOF
UENUM()
enum EAngularConstraintMotion
{
	/** No constraint against this axis. */ 
	ACM_Free		UMETA(DisplayName="Free"),
	/** Limited freedom along this axis. */ 
	ACM_Limited		UMETA(DisplayName="Limited"),
	/** Fully constraint against this axis. */
	ACM_Locked		UMETA(DisplayName="Locked"),

	ACM_MAX,
};

/** Enum to indicate which frame we want. */
UENUM()
namespace EConstraintFrame
{
	enum Type
	{
		Frame1,
		Frame2
	};
}


/**
 * Structure for file paths that are displayed in the UI.
 */
USTRUCT(BlueprintType)
struct FFilePath
{
	GENERATED_USTRUCT_BODY()

	/**
	 * The path to the file.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FilePath)
	FString FilePath;
};


/**
 * Structure for directory paths that are displayed in the UI.
 */
USTRUCT(BlueprintType)
struct FDirectoryPath
{
	GENERATED_USTRUCT_BODY()

	/**
	 * The path to the directory.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Path)
	FString Path;
};


/**
 * Reference to an editor collection of assets. This allows an editor-only picker UI
 */
USTRUCT(BlueprintType)
struct FCollectionReference
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Name of the collection
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CollectionReference)
	FName CollectionName;
};

/**
* This is used for redirecting old name to new name
* We use manually parsing array, but that makes harder to modify from property setting
* So adding this USTRUCT to support it properly
*/
USTRUCT()
struct ENGINE_API FRedirector
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName OldName;

	/** Types of objects that this physics objects will collide with. */
	UPROPERTY()
	FName NewName;

	FRedirector()
		: OldName(NAME_None)
		, NewName(NAME_None)
	{ }

	FRedirector(FName InOldName, FName InNewName)
		: OldName(InOldName)
		, NewName(InNewName)
	{ }
};


/** 
 * Structure for recording float values and displaying them as an Histogram through DrawDebugFloatHistory.
 */
USTRUCT(BlueprintType)
struct FDebugFloatHistory
{
	GENERATED_USTRUCT_BODY()

private:
	/** Samples */
	UPROPERTY(Transient)
	TArray<float> Samples;

public:
	/** Max Samples to record. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DebugFloatHistory")
	float MaxSamples;

	/** Min value to record. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebugFloatHistory")
	float MinValue;

	/** Max value to record. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebugFloatHistory")
	float MaxValue;

	/** Auto adjust Min/Max as new values are recorded? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DebugFloatHistory")
	bool bAutoAdjustMinMax;

	FDebugFloatHistory()
		: MaxSamples(100)
		, MinValue(0.f)
		, MaxValue(0.f)
		, bAutoAdjustMinMax(true)
	{ }

	FDebugFloatHistory(float const & InMaxSamples, float const & InMinValue, float const & InMaxValue, bool const & InbAutoAdjustMinMax)
		: MaxSamples(InMaxSamples)
		, MinValue(InMinValue)
		, MaxValue(InMaxValue)
		, bAutoAdjustMinMax(InbAutoAdjustMinMax)
	{ }

	/**
	 * Record a new Sample.
	 * if bAutoAdjustMinMax is true, this new value will potentially adjust those bounds.
	 * Otherwise value will be clamped before being recorded.
	 * If MaxSamples is exceeded, old values will be deleted.
	 * @param FloatValue new sample to record.
	 */
	void AddSample(float const & FloatValue)
	{
		if (bAutoAdjustMinMax)
		{
			// Adjust bounds and record value.
			MinValue = FMath::Min(MinValue, FloatValue);
			MaxValue = FMath::Max(MaxValue, FloatValue);
			Samples.Insert(FloatValue, 0);
		}
		else
		{
			// Record clamped value.
			Samples.Insert(FMath::Clamp(FloatValue, MinValue, MaxValue), 0);
		}

		// Do not exceed MaxSamples recorded.
		if( Samples.Num() > MaxSamples )
		{
			Samples.RemoveAt(MaxSamples, Samples.Num() - MaxSamples);
		}
	}

	/** Range between Min and Max values */
	float GetMinMaxRange() const
	{
		return (MaxValue - MinValue);
	}

	/** Min value. This could either be the min value recorded or min value allowed depending on 'bAutoAdjustMinMax'. */
	float GetMinValue() const
	{
		return MinValue;
	}

	/** Max value. This could be either the max value recorded or max value allowed depending on 'bAutoAdjustMinMax'. */
	float GetMaxValue() const
	{
		return MaxValue;
	}

	/** Number of Samples currently recorded */
	int GetNumSamples() const
	{
		return Samples.Num();
	}

	/** Read access to Samples array */
	TArray<float> const & GetSamples() const
	{
		return Samples;
	}
};


/** info for glow when using depth field rendering */
USTRUCT(BlueprintType)
struct FDepthFieldGlowInfo
{
	GENERATED_USTRUCT_BODY()

	/** whether to turn on the outline glow (depth field fonts only) */
	UPROPERTY(BlueprintReadWrite, Category="Glow")
	uint32 bEnableGlow:1;

	/** base color to use for the glow */
	UPROPERTY(BlueprintReadWrite, Category="Glow")
	FLinearColor GlowColor;

	/** if bEnableGlow, outline glow outer radius (0 to 1, 0.5 is edge of character silhouette)
	 * glow influence will be 0 at GlowOuterRadius.X and 1 at GlowOuterRadius.Y
	*/
	UPROPERTY(BlueprintReadWrite, Category="Glow")
	FVector2D GlowOuterRadius;

	/** if bEnableGlow, outline glow inner radius (0 to 1, 0.5 is edge of character silhouette)
	 * glow influence will be 1 at GlowInnerRadius.X and 0 at GlowInnerRadius.Y
	 */
	UPROPERTY(BlueprintReadWrite, Category="Glow")
	FVector2D GlowInnerRadius;


	FDepthFieldGlowInfo()
		: bEnableGlow(false)
		, GlowColor(ForceInit)
		, GlowOuterRadius(ForceInit)
		, GlowInnerRadius(ForceInit)
	{ }

	bool operator==(const FDepthFieldGlowInfo& Other) const
	{
		if (Other.bEnableGlow != bEnableGlow)
		{
			return false;
		}
		else if (!bEnableGlow)
		{
			// if the glow is disabled on both, the other values don't matter
			return true;
		}
		else
		{
			return (Other.GlowColor == GlowColor && Other.GlowOuterRadius == GlowOuterRadius && Other.GlowInnerRadius == GlowInnerRadius);
		}
	}

	bool operator!=(const FDepthFieldGlowInfo& Other) const
	{
		return !(*this == Other);
	}	
};


/** information used in font rendering */
USTRUCT(BlueprintType)
struct FFontRenderInfo
{
	GENERATED_USTRUCT_BODY()

	/** whether to clip text */
	UPROPERTY(BlueprintReadWrite, Category="FontInfo")
	uint32 bClipText:1;

	/** whether to turn on shadowing */
	UPROPERTY(BlueprintReadWrite, Category="FontInfo")
	uint32 bEnableShadow:1;

	/** depth field glow parameters (only usable if font was imported with a depth field) */
	UPROPERTY(BlueprintReadWrite, Category="FontInfo")
	struct FDepthFieldGlowInfo GlowInfo;

	FFontRenderInfo()
		: bClipText(false), bEnableShadow(false)
	{}
};


/** Simple 2d triangle with UVs */
USTRUCT(BlueprintType)
struct FCanvasUVTri
{
	GENERATED_USTRUCT_BODY()

	/** Position of first vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
	FVector2D V0_Pos;

	/** UV of first vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
		FVector2D V0_UV;

	/** Color of first vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
		FLinearColor V0_Color;

	/** Position of second vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
		FVector2D V1_Pos;

	/** UV of second vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
		FVector2D V1_UV;

	/** Color of second vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
		FLinearColor V1_Color;

	/** Position of third vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
		FVector2D V2_Pos;

	/** UV of third vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
		FVector2D V2_UV;

	/** Color of third vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CanvasUVTri)
		FLinearColor V2_Color;

	FCanvasUVTri()
		: V0_Pos(ForceInit)
		, V0_UV(ForceInit)
		, V0_Color(ForceInit)
		, V1_Pos(ForceInit)
		, V1_UV(ForceInit)
		, V1_Color(ForceInit)
		, V2_Pos(ForceInit)
		, V2_UV(ForceInit)
		, V2_Color(ForceInit)
	{ }
};


template <> struct TIsZeroConstructType<FCanvasUVTri> { enum { Value = true }; };

/** Defines available strategies for handling the case where an actor is spawned in such a way that it penetrates blocking collision. */
UENUM(BlueprintType)
enum class ESpawnActorCollisionHandlingMethod : uint8
{
	/** Fall back to default settings. */
	Undefined								UMETA(DisplayName = "Default"),
	/** Actor will spawn in desired location, regardless of collisions. */
	AlwaysSpawn								UMETA(DisplayName = "Always Spawn, Ignore Collisions"),
	/** Actor will try to find a nearby non-colliding location (based on shape components), but will always spawn even if one cannot be found. */
	AdjustIfPossibleButAlwaysSpawn			UMETA(DisplayName = "Try To Adjust Location, But Always Spawn"),
	/** Actor will try to find a nearby non-colliding location (based on shape components), but will NOT spawn unless one is found. */
	AdjustIfPossibleButDontSpawnIfColliding	UMETA(DisplayName = "Try To Adjust Location, Don't Spawn If Still Colliding"),
	/** Actor will fail to spawn. */
	DontSpawnIfColliding					UMETA(DisplayName = "Do Not Spawn"),
};

/** Defines the context of a user activity. Activities triggered in Blueprints will by type Game. Those created in code might choose to set another type. */
enum class EUserActivityContext : uint8
{
	Game,
	Editor,
	Other
};

/**
 * The description of a user activity
 */
USTRUCT(BlueprintType)
struct FUserActivity
{
	GENERATED_USTRUCT_BODY()

	/** Describes the user's activity */
	UPROPERTY(BlueprintReadWrite, Category = "Activity")
	FString ActionName;

	/** A game or editor activity? */
	EUserActivityContext Context;

	/** Default constructor. */
	FUserActivity()
		: Context(EUserActivityContext::Game)
	{ }

	/** Creates and initializes a new instance. */
	FUserActivity(const FString& InActionName)
		: ActionName(InActionName)
		, Context(EUserActivityContext::Game)
	{ }

	/** Creates and initializes a new instance with a context other than the default "Game". */
	FUserActivity(const FString& InActionName, EUserActivityContext InContext)
		: ActionName(InActionName)
		, Context(InContext)
	{ }
};

/** Which processors will have access to Mesh Vertex Buffers. */
UENUM()
enum class EMeshBufferAccess: uint8
{
    /** Access will be determined based on the assets used in the mesh and hardware / software capability. */
    Default,

    /** Force access on both CPU and GPU. */
    ForceCPUAndGPU
};

/** Indicates the type of a level collection, used in FLevelCollection. */
enum class ELevelCollectionType
{
	/**
	 * The dynamic levels that are used for normal gameplay and the source for any duplicated collections.
	 * Will contain a world's persistent level and any streaming levels that contain dynamic or replicated gameplay actors.
	 */
	DynamicSourceLevels,
	/** Gameplay relevant levels that have been duplicated from DynamicSourceLevels if requested by the game. */
	DynamicDuplicatedLevels,
	/**
	 * These levels are shared between the source levels and the duplicated levels, and should contain
	 * only static geometry and other visuals that are not replicated or affected by gameplay.
	 * These will not be duplicated in order to save memory.
	 */
	StaticLevels
};
