// @third party code - BEGIN HairWorks
#pragma once

#include "Object.h"
#include "HairWorksMaterial.generated.h"

namespace nvidia{namespace HairWorks{
	struct InstanceDescriptor;
	struct Pin;
}}

UENUM()
enum class EHairWorksStrandBlendMode : uint8
{
	/** Overwrite with strand texture. */
	Overwrite,
	/** Multiply strand texture to base color (root/tip). */
	Multiply,
	/** Add strand color on top of base color. */
	Add,
	/** Add/subtract strand color to/from base color. */
	Modulate,
};

UENUM()
enum class EHairWorksColorizeMode: uint8
{
	None,
	Lod,
	Tangents,
	Normal,
};

USTRUCT(BlueprintType)
struct ENGINE_API FHairWorksPin
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physical|Pin")
	FName Bone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Pin")
	bool bDynamicPin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Pin")
	bool bTetherPin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Pin", meta = (ClampMin = "0", ClampMax = "1"))
	float Stiffness = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Pin", meta = (ClampMin = "0", ClampMax = "1"))
	float InfluenceFallOff = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Pin")
	FVector4 InfluenceFallOffCurve = FVector4(1, 1, 1, 1);
};

class UTexture2D;

/**
* HairWorksMaterial represents physical and graphics attributes of a hair.
*/
UCLASS(BlueprintType)
class ENGINE_API UHairWorksMaterial: public UObject
{
	GENERATED_UCLASS_BODY()

#pragma region Visualization
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bHair = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bGuideCurves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bSkinnedGuideCurves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bControlPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bGrowthMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bBones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bBoundingBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bCollisionCapsules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bHairInteraction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bPinConstraints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bShadingNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	bool bShadingNormalCenter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Visualization)
	EHairWorksColorizeMode ColorizeOptions = EHairWorksColorizeMode::None;
#pragma endregion

#pragma region General
	/** Whether to enable this hair. When disabled, hair will not cause any computation/rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	bool bEnable = true;

	/** How many vertices are generated per each control hair segments in spline curves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta = (ClampMin = "1", ClampMax = "4"))
	int32 SplineMultiplier;
#pragma endregion

#pragma region Physical
	/** Whether to turn on / off simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|General")
	bool bSimulate = true;

	/** Whether to simulate in world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|General")
	bool bSimulateInWorldSpace = false;

	/** Mass scale for this hair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|General", meta = (ClampMin = "-50", ClampMax = "50"))
	float MassScale = 10;

	/** Damping to slow down hair motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|General", meta = (ClampMin = "0", ClampMax = "1"))
	float Damping = 0;

	/** Inertia control. (0: no inertia, 1 : full inertia) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|General", meta = (ClampMin = "0", ClampMax = "1"))
	float InertiaScale = 1;

	/** Speed limit where everything gets locked (for teleport etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|General", meta = (ClampMin = "0", ClampMax = "1000"))
	float InertiaLimit = 1000;
#pragma endregion

#pragma region Wind
	/** Vector for main wind direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Wind", meta = (DisplayName = "Direction"))
	FRotator WindDirection;

	/** Main wind strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Wind", meta = (DisplayName = "Strength"))
	float Wind = 0;

	/** Noise of wind strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Wind", meta = (DisplayName = "Noise", ClampMin = "0", ClampMax = "1"))
	float WindNoise = 0;
#pragma endregion

#pragma region Stiffness
	/** How close hairs try to stay within skinned position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Global", ClampMin = "0", ClampMax = "1"))
	float StiffnessGlobal = 0.5;

	/** Control map for stiffness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Global Map"))
	UTexture2D* StiffnessGlobalMap;

	/** Curve values for stiffness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Global Curve"))
	FVector4 StiffnessGlobalCurve = FVector4(1, 1, 1, 1);

	/** How strongly hairs move toward the stiffness target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Strength", ClampMin = "0", ClampMax = "1"))
	float StiffnessStrength = 1;

	/** Curve values for stiffness strength */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Strength Curve"))
	FVector4 StiffnessStrengthCurve = FVector4(1, 1, 1, 1);

	/** How fast hair stiffness generated motion decays over time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Damping", ClampMin = "0", ClampMax = "1"))
	float StiffnessDamping = 0;

	/** Curve values for stiffness damping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Damping Curve"))
	FVector4 StiffnessDampingCurve = FVector4(1, 1, 1, 1);

	/** Attenuation of stiffness away from the root (stiffer at root, weaker toward tip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Root", ClampMin = "0", ClampMax = "1"))
	float StiffnessRoot = 0;

	/** Control map for stiffness root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Root Map"))
	UTexture2D* StiffnessRootMap;

	/** Attenuation of stiffness away from the tip (stiffer at tip, weaker toward root). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Tip", ClampMin = "0", ClampMax = "1"))
	float StiffnessTip = 0;

	/** Stiffness for bending, useful for long hair */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Bend", ClampMin = "0", ClampMax = "1"))
	float StiffnessBend = 0;

	/** Curve values for stiffness bend */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Stiffness", meta = (DisplayName = "Bend Curve"))
	FVector4 StiffnessBendCurve = FVector4(1, 1, 1, 1);
#pragma endregion

#pragma region Collision
	/** Radius of backstop collision(normalized along hair length). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Collision", meta = (ClampMin = "0", ClampMax = "1"))
	float Backstop = 0;

	/** Friction when capsule collision is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Collision", meta = (ClampMin = "0", ClampMax = "10"))
	float Friction = 0;

	/** Whether to use the sphere/capsule collision or not for hair/body collision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Collision")
	bool bCapsuleCollision = false;

	/** How strong the hair interaction force is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Collision", meta = (DisplayName = "Interaction", ClampMin = "0", ClampMax = "1"))
	float StiffnessInteraction = 0;

	/** Curve values for interaction stiffness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Collision", meta = (DisplayName = "Interaction Curve"))
	FVector4 StiffnessInteractionCurve = FVector4(1, 1, 1, 1);
#pragma endregion

#pragma region Pin
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "Physical|Pin")
	TArray<FHairWorksPin> Pins;

	///** Stiffness for pin constraints. */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical|Pin", meta = (DisplayName = "Stiffness", ClampMin = "0", ClampMax = "1"))
	//float PinStiffness = 1;
#pragma endregion

#pragma region Volume
	/** Hair density per face (1.0 = 64 hairs per face). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Volume", meta = (ClampMin = "0", ClampMax = "10"))
	float Density = 1;

	/** Control map for density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Volume")
	UTexture2D* DensityMap;

	/** Whether to use per-pixel sampling or per-vertex sampling for density map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Volume")
	bool bUsePixelDensity = false;

	/** Length control for growing hair effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Volume", meta = (ClampMin = "0", ClampMax = "1"))
	float LengthScale = 1;

	/** Control map for length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Volume")
	UTexture2D* LengthScaleMap;

	/** Length variation noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Volume", meta = (ClampMin = "0", ClampMax = "1"))
	float LengthNoise = 0;
#pragma endregion

#pragma region Strand Width
	/** Hair width (thickness). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|StrandWidth", meta = (DisplayName = "Width", ClampMin = "0", ClampMax = "10"))
	float WidthScale = 1;

	/** Control map for hair width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|StrandWidth", meta = (DisplayName = "Width Map"))
	UTexture2D* WidthScaleMap;

	/** Scale factor for top side of the strand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|StrandWidth", meta = (DisplayName = "Root Scale", ClampMin = "0", ClampMax = "1"))
	float WidthRootScale = 1;

	/** Scale factor for bottom side of the strand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|StrandWidth", meta = (DisplayName = "Tip Scale", ClampMin = "0", ClampMax = "1"))
	float WidthTipScale = 0.1;

	/** Noise factor for hair width noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|StrandWidth", meta = (DisplayName = "Noise", ClampMin = "0", ClampMax = "1"))
	float WidthNoise = 0;
#pragma endregion

#pragma region Clumping
	/** How clumped each hair face is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Clumping", meta = (DisplayName = "Scale", ClampMin = "0", ClampMax = "1"))
	float ClumpingScale = 0;

	/** Control map for clumping scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Clumping", meta = (DisplayName = "Scale Map"))
	UTexture2D* ClumpingScaleMap;

	/** Exponential factor to control roundness of clump shape (0 = linear cone, clump scale *= power(t, roundness), where t is normalized distance from the root). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Clumping", meta = (DisplayName = "Roundness", ClampMin = "0.01", ClampMax = "2"))
	float ClumpingRoundness = 1;

	/** Control map for clumping roundness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Clumping", meta = (DisplayName = "Roundness Map"))
	UTexture2D* ClumpingRoundnessMap;

	/** Probability of each hair gets clumped (0 = all hairs get clumped, 1 = clump scale is randomly distributed from 0 to 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Clumping", meta = (DisplayName = "Noise", ClampMin = "0", ClampMax = "1"))
	float ClumpingNoise;
#pragma endregion

#pragma region Waviness
	/** Size of waves for hair waviness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Scale", ClampMin = "0", ClampMax = "5"))
	float WavinessScale = 0;

	/** Control map for waviness scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Scale Map"))
	UTexture2D* WavinessScaleMap;

	/** Noise factor for the wave scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Scale Noise", ClampMin = "0", ClampMax = "1"))
	float WavinessScaleNoise = 0.5;

	/** Waviness at strand level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Scale Strand", ClampMin = "0", ClampMax = "1"))
	float WavinessScaleStrand;

	/** Waviness at clump level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Scale Clump", ClampMin = "0", ClampMax = "1"))
	float WavinessScaleClump;

	/** Wave frequency (1.0 = one sine wave along hair length). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Frequency", ClampMin = "1", ClampMax = "10"))
	float WavinessFreq = 3;

	/** Control map for waviness frequency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Frequency Map"))
	UTexture2D* WavinessFreqMap;

	/** Noise factor for the wave frequency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Frequency Noise", ClampMin = "0", ClampMax = "1"))
	float WavinessFreqNoise = 0.5;

	/** For some distance from the root, we attenuate waviness so that root itself does not move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Waviness", meta = (DisplayName = "Root Straighten", ClampMin = "0", ClampMax = "1"))
	float WavinessRootStraigthen = 0;
#pragma endregion

#pragma region Color
	/** Color of hair root (when hair textures are not used). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Color")
	FLinearColor RootColor = FLinearColor(1, 1, 1, 1);

	/** Color map for root color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Color")
	UTexture2D* RootColorMap;

	/** Color of hair tip (when hair textures are not used). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Color")
	FLinearColor TipColor = FLinearColor(1, 1, 1, 1);

	/** Color map for tip color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Color")
	UTexture2D* TipColorMap;

	/** Blend factor between root and tip color in addition to hair length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Color", meta = (DisplayName = "Root/Tip Color Weight", ClampMin = "0", ClampMax = "1"))
	float RootTipColorWeight = 0.5;

	/** Falloff factor for root/tip color interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Color", meta = (DisplayName = "Root/Tip Color Falloff", ClampMin = "0", ClampMax = "1"))
	float RootTipColorFalloff = 1;

	/** Falloff factor for alpha transition from root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Color", meta = (DisplayName = "Root Alpha Falloff", ClampMin = "0", ClampMax = "1"))
	float RootAlphaFalloff = 0;
#pragma endregion

#pragma region Strand
	/** Texture along hair strand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Strand")
	UTexture2D* PerStrandTexture;

	/** When the strand texture is used, the blend mode determines how colors should combine between the strand texture and other color textures(root, tip, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Strand")
	EHairWorksStrandBlendMode StrandBlendMode = EHairWorksStrandBlendMode::Overwrite;

	/** Scale strand texture before blend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Strand", meta = (ClampMin = "0", ClampMax = "1"))
	float StrandBlendScale;
#pragma endregion

#pragma region Diffuse
	/** Blend factor between Kajiya hair lighting vs normal skin lighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Diffuse", meta = (ClampMin = "0", ClampMax = "1"))
	float DiffuseBlend = 0.5;

	/** Blend factor between mesh normal vs hair normal. Use higher value for longer (surface like) hair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Diffuse", meta = (ClampMin = "0", ClampMax = "1"))
	float HairNormalWeight;

	/** Name for the bone which we use as model center for diffuse shading purpose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Diffuse")
	FName HairNormalCenter;
#pragma endregion

#pragma region Specular
	/** Specular color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Specular", meta = (DisplayName = "Color"))
	FLinearColor SpecularColor = FLinearColor(1, 1, 1, 1);

	/** Color map for specular color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Specular", meta = (DisplayName = "Color Map"))
	UTexture2D* SpecularColorMap;

	/** Primary specular factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Specular", meta = (ClampMin = "0", ClampMax = "1"))
	float PrimaryScale = 0.1;

	/** Primary specular power exponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Specular", meta = (ClampMin = "1", ClampMax = "1000"))
	float PrimaryShininess = 100;

	/** Shift factor to make specular highlight move with noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Specular", meta = (ClampMin = "0", ClampMax = "1"))
	float PrimaryBreakup = 0;

	/** Secondary specular factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Specular", meta = (ClampMin = "0", ClampMax = "1"))
	float SecondaryScale = 0.05;

	/** Secondary specular power exponent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Specular", meta = (ClampMin = "1", ClampMax = "1000"))
	float SecondaryShininess = 20;

	/** Secondary highlight shift offset along tangents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Specular", meta = (ClampMin = "-1", ClampMax = "1"))
	float SecondaryOffset = 0.1;
#pragma endregion

#pragma region Glint
	/** Strength of the glint noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Glint", meta = (DisplayName = "Strength", ClampMin = "0", ClampMax = "1"))
	float GlintStrength;

	/** Number of glint sparklers along each hair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Glint", meta = (DisplayName = "Size", ClampMin = "1", ClampMax = "1024"))
	float GlintSize;

	/** Glint power exponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Glint", meta = (DisplayName = "Power Exponent", ClampMin = "1", ClampMax = "16"))
	float GlintPowerExponent;
#pragma endregion

#pragma region Shadow
	/** Distance through hair volume beyond which hairs get completely shadowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Shadow", meta = (ClampMin = "0", ClampMax = "1"))
	float ShadowAttenuation = 0.8;

	/** Density scale factor to reduce hair density for shadow map rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Shadow", meta = (ClampMin = "0", ClampMax = "1"))
	float ShadowDensityScale = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Shadow")
	bool bCastShadows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Graphics|Shadow")
	bool bReceiveShadows = true;
#pragma endregion

#pragma region Culling
	/** When this is on, density for hairs outside view are set to 0. Use this option when fur is in a closeup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|Culling")
	bool bViewFrustumCulling = true;

	/** When this is on, density for hairs growing from back-facing faces will be set to 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|Culling")
	bool bBackfaceCulling = false;

	/** Threshold to determine back face, note that this value should be slightly smaller 0 to avoid hairs at the silhouette from disappearing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|Culling", meta = (ClampMin = "-1", ClampMax = "1"))
	float BackfaceCullingThreshold = -0.2;
#pragma endregion

#pragma region Distance LOD
	/** Whether to enable LOD for far away object (distance LOD) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DistanceLOD", meta = (DisplayName = "Enable"))
	bool bDistanceLodEnable = false;

	/** Distance (in scene unit) to camera where fur will start fading out (by reducing density). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DistanceLOD", meta = (DisplayName = "Start Distance", ClampMin = "0"))
	float DistanceLodStart = 5;

	/** Distance (in scene unit) to camera where fur will completely disappear (and stop simulating). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DistanceLOD", meta = (DisplayName = "End Distance", ClampMin = "0"))
	float DistanceLodEnd = 10;

	/** Distance (in scene unit) to camera where fur will fade with alpha from 1 (this distance) to 0 (DistanceLODEnd). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DistanceLOD", meta = (ClampMin = "0", ClampMax = "1000"))
	float FadeStartDistance = 1000;

	/** Hair width that can change when close up density is triggered by closeup LOD mechanism. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DistanceLOD", meta = (DisplayName = "Base Width Scale", ClampMin = "0", ClampMax = "10"))
	float DistanceLodBaseWidthScale = 1;

	/** Density when distance LOD is in action. hairDensity gets scaled based on LOD factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DistanceLOD", meta = (DisplayName = "Base Density Scale", ClampMin = "0", ClampMax = "10"))
	float DistanceLodBaseDensityScale = 0;
#pragma endregion

#pragma region Detail LOD
	/** Whether to enable LOD for close object (detail LOD). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DetailLOD", meta = (DisplayName = "Enable"))
	bool bDetailLodEnable = false;

	/** Distance (in scene unit) to camera where fur will start getting denser toward closeup density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DetailLOD", meta = (DisplayName = "Start Distance", ClampMin = "0"))
	float DetailLodStart = 2;

	/** Distance (in scene unit) to camera where fur will get full closeup density value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DetailLOD", meta = (DisplayName = "End Distance", ClampMin = "0"))
	float DetailLodEnd = 1;

	/** Hair width that can change when close up density is triggered by closeup LOD mechanism. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DetailLOD", meta = (DisplayName = "Base Width Scale", ClampMin = "0", ClampMax = "10"))
	float DetailLodBaseWidthScale = 10;

	/** Density scale when closeup LOD is in action.  hairDensity gets scaled based on LOD factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelOfDetail|DetailLOD", meta = (DisplayName = "Base Density Scale", ClampMin = "0", ClampMax = "10"))
	float DetailLodBaseDensityScale = 1;
#pragma endregion

	//~ Begin UObject interface.
	virtual void PostLoad()override;
	//~ End UObject interface.

	/** Get and Set hair instance parameters*/
	void GetHairInstanceParameters(nvidia::HairWorks::InstanceDescriptor& HairDescriptor, TArray<UTexture2D*>& HairTexture)const;
	void SetHairInstanceParameters(const nvidia::HairWorks::InstanceDescriptor& HairDescriptor, const TArray<UTexture2D*>& HairTexture);

protected:
	/** Read or write attributes from or to GFSDK_HairInstanceDescriptor. */
	void SyncHairDescriptor(nvidia::HairWorks::InstanceDescriptor& HairDescriptor, TArray<UTexture2D*>& HairTextures, bool bFromDescriptor);

	template<typename TParameter, typename TProperty>
	void SyncHairParameter(TParameter& Parameter, TProperty& Property, bool bFromDescriptor)
	{
		if(bFromDescriptor)
			Property = Parameter;
		else
			Parameter = Property;
	}
};
// @third party code - END HairWorks