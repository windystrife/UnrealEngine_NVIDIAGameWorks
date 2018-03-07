// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EngineDefines.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "SVectorInputBox.h"
#include "IBlastMeshEditor.h"
#include "BlastFractureSettings.generated.h"

class UMaterialInterface;
class UBlastMesh;

namespace Nv
{
namespace Blast
{
class FractureTool;
class VoronoiSitesGenerator;
class Mesh;
}
}

USTRUCT()
struct FBlastVector
{
	GENERATED_USTRUCT_BODY()

	FBlastVector() {}
	FBlastVector(EBlastViewportControlMode InDefaultControlMode, const FVector& Vector)
		: DefaultControlMode(InDefaultControlMode), V(Vector)
	{
	}

	const FBlastVector& operator= (const FVector& Vector)
	{
		V = Vector;
		return *this;
	}

	operator FVector() const
	{
		return V;
	}

	void Activate()
	{
		OnVisualModification.Broadcast(this);
		IsActive = true;
	}

	FVector V;
	EBlastViewportControlMode DefaultControlMode = EBlastViewportControlMode::Point;
	FBlastVector* DefaultBlastVectorActivation = nullptr;
	bool IsActive = false;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnVisualModificationDelegate, const FBlastVector*);
	static FOnVisualModificationDelegate OnVisualModification;
};

class FBlastVectorCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:

	FReply OnClicked();
	const struct FSlateBrush* GetVisibilityBrush() const;

	TOptional<float> OnGetValue(int axis) const;
	void OnValueCommitted(float NewValue, ETextCommit::Type CommitType, int axis);

	TSharedPtr<class SButton> Button;
	TSharedPtr<IPropertyHandle> PropertyHandle;
};

/** Parameters to describe the application of U,V coordinates on a particular slice within a destructible. */
USTRUCT()
struct FBlastFractureMaterial
{
	GENERATED_USTRUCT_BODY()

	/**
	 * The UV scale (geometric distance/unit texture distance) for interior materials.
	 * Default = (100.0f,100.0f).
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	FVector2D	UVScale;

	/**
	 * A UV origin offset for interior materials.
	 * Default = (0.0f,0.0f).
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	FVector2D	UVOffset;

	/**
	 * Object-space vector specifying surface tangent direction.  If this vector is (0.0f,0.0f,0.0f), then an arbitrary direction will be chosen.
	 * Default = (0.0f,0.0f,0.0f).
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	FVector		Tangent;

	/**
	 * Angle from tangent direction for the u coordinate axis.
	 * Default = 0.0f.
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	float		UAngle;

	/**
	 * The element index to use for the newly-created triangles.
	 * If a negative index is given, a new element will be created for interior triangles.
	 * Default = -1
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	int32		InteriorElementIndex;

	FBlastFractureMaterial()
		: UVScale(100.0f, 100.0f)
		, UVOffset(0.0f, 0.0f)
		, Tangent(0.0f, 0.0f, 0.0f)
		, UAngle(0.0f)
		, InteriorElementIndex(-1)
	{ }

	//~ Begin FFractureMaterial Interface
//	void	FillNxFractureMaterialDesc(nvidia::apex::FractureMaterialDesc& PFractureMaterialDesc);
	//~ End FFractureMaterial Interface
};

/** Per-chunk authoring data. */
USTRUCT()
struct FBlastChunkParameters
{
	GENERATED_USTRUCT_BODY()

	/**
	Defines the chunk to be environmentally supported, if the appropriate NxDestructibleParametersFlag flags
	are set in NxDestructibleParameters.
	Default = false.
	*/
	UPROPERTY(VisibleAnywhere, Category = DestructibleChunkParameters)
	bool bIsSupportChunk;

	/**
	Defines the chunk to be unfractureable.  If this is true, then none of its children will be fractureable.
	Default = false.
	*/
	//UPROPERTY(EditAnywhere, Category = DestructibleChunkParameters)
	//bool bDoNotFracture;

	/**
	Defines the chunk to be undamageable.  This means this chunk will not fracture, but its children might.
	Default = false.
	*/
	//UPROPERTY(EditAnywhere, Category = DestructibleChunkParameters)
	//bool bDoNotDamage;

	/**
	Defines the chunk to be uncrumbleable.  This means this chunk will not be broken down into fluid mesh particles
	no matter how much damage it takes.  Note: this only applies to chunks with no children.  For a chunk with
	children, then:
	1) The chunk may be broken down into its children, and then its children may be crumbled, if the doNotCrumble flag
	is not set on them.
	2) If the Destructible module's chunk depth offset LOD may be set such that this chunk effectively has no children.
	In this case, the doNotCrumble flag will apply to it.
	Default = false.
	*/
	//UPROPERTY(EditAnywhere, Category = DestructibleChunkParameters)
	//bool bDoNotCrumble;

	FBlastChunkParameters()
		: bIsSupportChunk(false)
		//, bDoNotFracture(false)
		//, bDoNotDamage(false)
		//, bDoNotCrumble(false)
	{
	}
};

UENUM()
enum class EBlastFractureMethod : uint8
{
	/** Voronoi method with randomly generated sites. */
	VoronoiUniform,
	/** Clustered Voronoi method with randomly generated clasters and sites. */
	VoronoiClustered,
	/** Radial pattern*/
	VoronoiRadial,
	/** Fracture in sphere */
	VoronoiInSphere,
	/** Remove Voronoi sites in sphere */
	VoronoiRemoveInSphere,
	/** Slicing method for grid like chunk cutting. */
	UniformSlicing,
	/** Cutout method for fracturing with bitmap pattern */
	Cutout,
	/** Split chunk with cut */
	Cut
};

UCLASS()
class UBlastFixChunkHierarchyProperties : public UObject
{
	GENERATED_UCLASS_BODY()

	/**
		If number of children of some chunk less then Threshold then it would be considered as already optimized and skipped.
	*/
	UPROPERTY(EditAnywhere, Category = General, Transient)
	uint32 Threshold = 20;

	/**
		Maximum number of children for processed chunks.
	*/
	UPROPERTY(EditAnywhere, Category = General, Transient)
	uint32 TargetedClusterSize = 10;
};

UCLASS()
class UBlastRebuildCollisionMeshProperties : public UObject
{
	GENERATED_UCLASS_BODY()

	/**
		Maximum number of convex hull generated for one chunk. If equal to 1 convex decomposition is disabled.
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = General, Transient)
	uint32 MaximumNumberOfHulls = 32;

	/**
		Voxel grid resolution used for chunk convex decomposition.
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = General, Transient)
	uint32 VoxelGridResolution = 1000000;

	/**
		Value between 0 and 1, controls how accurate hull generation is
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = General, Transient)
	float Concavity = 0.0025f;

	/**
		Reuild collision mesh only for selected chunks. If not set rebuild collision mesh for all chunks.
	*/
	UPROPERTY(EditAnywhere, Category = General, Transient)
	bool IsOnlyForSelectedChunks = true;
};

DECLARE_DELEGATE(FOnStaticMeshSelected);

UCLASS()
class UBlastStaticMeshHolder : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Import, Transient)
	class UStaticMesh* StaticMesh = nullptr;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& e) override;

	FOnStaticMeshSelected OnStaticMeshSelected;
};

class FBlastFractureSettingsComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** Execute tool command. */
	static class FReply ExecuteToolCommand(class IDetailLayoutBuilder* DetailBuilder, UFunction* MethodToExecute);

	/** IDetailCustomization interface. */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

UCLASS()
class UBlastFractureSettingsNoise : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Amplitude of cutting surface noise. If it is 0 - noise is disabled. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Noise, meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float									Amplitude;

	/** Frequencey of cutting surface noise. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Noise, meta = (ClampMin = "0", UIMin = "0"))
	float									Frequency;

	/** Octave number in surface noise. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Noise, meta = (ClampMin = "0", UIMin = "0"))
	int32									OctaveNumber;

	/**
	Cutting surface resolution.
	Note: large surface resolution may lead to significant increase of authoring time
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Noise, meta = (ClampMin = "0", UIMin = "0"))
	int32									SurfaceResolution;

	void Setup(float InAmplitude, float InFrequency, int32 InOctaveNumber, int32 InSurfaceResolution);
	void Setup(const UBlastFractureSettingsNoise& Other);
};

UCLASS()
class UBlastFractureSettingsVoronoi : public UObject
{
	GENERATED_UCLASS_BODY()

	/** If this set fracture will remove all previously generated Voronoi sites. */
	UPROPERTY(EditAnywhere, Category = VoronoiFracture)
	uint32									ForceReset : 1;

	/** Cells scale along X, Y, Z axis. */
	UPROPERTY(EditAnywhere, Category = VoronoiFracture)
	FVector									CellAnisotropy;

	/** Cells rotation around X, Y, Z axis.  Has no effect without cells anisotropy. */
	UPROPERTY(EditAnywhere, Category = VoronoiFracture)
	FQuat									CellRotation;

	void Setup(bool InForceReset, const FVector& InAnisotropy, const FQuat& InRotation);
	void Setup(const UBlastFractureSettingsVoronoi& Other);
};

UCLASS()
class UBlastFractureSettingsVoronoiUniform : public UBlastFractureSettingsVoronoi
{
	GENERATED_UCLASS_BODY()

	/** The number of voronoi cell sites. */
	UPROPERTY(EditAnywhere, Category = VoronoiUniformFracture, meta = (DefaultValue = "10", ClampMin = "2", UIMin = "2"))
	int32									CellCount;
};

UCLASS()
class UBlastFractureSettingsVoronoiClustered : public UBlastFractureSettingsVoronoi
{
	GENERATED_UCLASS_BODY()

	/** The number of voronoi cell sites. */
	UPROPERTY(EditAnywhere, Category = VoronoiUniformFracture, meta = (DefaultValue = "10", ClampMin = "2", UIMin = "2"))
	int32									CellCount;

	/** The number of voronoi cluster counts. */
	UPROPERTY(EditAnywhere, Category = VoronoiClusteredFracture, meta = (DefaultValue = "2", ClampMin = "2", UIMin = "2"))
	int32									ClusterCount;

	/** The Voronoi cluster radius. */
	UPROPERTY(EditAnywhere, Category = VoronoiClusteredFracture, meta = (DefaultValue = "1", ClampMin = "0", UIMin = "0"))
	float									ClusterRadius;
};


UCLASS()
class UBlastFractureSettingsRadial : public UBlastFractureSettingsVoronoi
{
	GENERATED_UCLASS_BODY()
		
	/** The center of generated pattern. */
	UPROPERTY(EditAnywhere, Category = RadialFracture)
	FBlastVector							Origin;

	/** The normal to plane in which sites are generated. */
	UPROPERTY(EditAnywhere, Category = RadialFracture)
	FBlastVector							Normal = FBlastVector(EBlastViewportControlMode::Normal, FVector(0.f, 0.f, 1.f));

	/** The pattern radius. */
	UPROPERTY(EditAnywhere, Category = RadialFracture, meta = (DefaultValue = "1", ClampMin = "0", UIMin = "0"))
	float									Radius;

	/**	The number of angular steps. */
	UPROPERTY(EditAnywhere, Category = RadialFracture, meta = (DefaultValue = "2", ClampMin = "2", UIMin = "2"))
	int32									AngularSteps;

	/** The number of radial steps. */
	UPROPERTY(EditAnywhere, Category = RadialFracture, meta = (DefaultValue = "2", ClampMin = "1", UIMin = "1"))
	int32									RadialSteps;
		
	/** The angle offset at each radial step. */
	UPROPERTY(EditAnywhere, Category = RadialFracture, meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float									AngleOffset;

	/** The randomness of sites distribution. */
	UPROPERTY(EditAnywhere, Category = RadialFracture, meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float									Variability;
};

UCLASS()
class UBlastFractureSettingsInSphere : public UBlastFractureSettingsVoronoi
{
	GENERATED_UCLASS_BODY()

	/** The number of voronoi cell sites. */
	UPROPERTY(EditAnywhere, Category = VoronoiUniformFracture, meta = (DefaultValue = "10", ClampMin = "2", UIMin = "2"))
	int32									CellCount;

	/** The sphere radius. */
	UPROPERTY(EditAnywhere, Category = InSphereFracture, meta = (DefaultValue = "100", ClampMin = "0", UIMin = "0"))
	float									Radius;

	/** The sphere origin. */
	UPROPERTY(EditAnywhere, Category = InSphereFracture)
	FBlastVector							Origin;
};

UCLASS()
class UBlastFractureSettingsRemoveInSphere : public UBlastFractureSettingsVoronoi
{
	GENERATED_UCLASS_BODY()

	/** The sphere radius. */
	UPROPERTY(EditAnywhere, Category = RemoveInSphere, meta = (DefaultValue = "100", ClampMin = "0", UIMin = "0"))
	float									Radius;

	/** The sphere origin. */
	UPROPERTY(EditAnywhere, Category = RemoveInSphere)
	FBlastVector							Origin;

	/** The probability of removing site. */
	UPROPERTY(EditAnywhere, Category = RemoveInSphere, meta = (DefaultValue = "1", ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float									Probability;
};

UCLASS()
class UBlastFractureSettingsUniformSlicing : public UBlastFractureSettingsNoise
{
	GENERATED_UCLASS_BODY()
		
	/** The number of slices along X, Y, Z axis. */
	UPROPERTY(EditAnywhere, Category = UniformSlicingFracture)
	FIntVector								SlicesCount; //TODO Min/Max check

	/** The angle of slice will vary in range depending on AngleVariation. Note: the order of chunk cutting X, Y, Z. Resulting chunks depend on order. */
	UPROPERTY(EditAnywhere, Category = UniformSlicingFracture, meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float									AngleVariation;

	/** The slice offset will vary in range depending on OffsetVariation. Note: the order of chunk cutting X, Y, Z. Resulting chunks depend on order. */
	UPROPERTY(EditAnywhere, Category = UniformSlicingFracture, meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float									OffsetVariation;
};

UCLASS()
class UBlastFractureSettingsCutout : public UBlastFractureSettingsNoise
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = CutoutFracture)
	class UTexture2D*						Pattern;

	/** The bitmap pattern transform. Note: Pattern is flate and scele to normal is not applied. */
	//UPROPERTY(EditAnywhere, Category = CutoutFracture)
	//FTransform								Transform;

	/** The center of cutout plane. */
	UPROPERTY(EditAnywhere, Category = CutoutFracture)
	FBlastVector							Origin;

	/** The normal to cutout plane */
	UPROPERTY(EditAnywhere, Category = CutoutFracture)
	FBlastVector							Normal = FBlastVector(EBlastViewportControlMode::Normal, FVector(0.f, 0.f, 1.f));

	/** The size of cutout plane. */
	UPROPERTY(EditAnywhere, Category = CutoutFracture)
	FVector2D								Size;

	/** The rotation of cutout plane around normal in degree. */
	UPROPERTY(EditAnywhere, Category = CutoutFracture)
	float									RotationZ;

	/** Periodic boundary condition. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = CutoutFracture)
	bool									bPeriodic;

	/** Fill gaps in cutout pattern. Each partition will be expanded until the boundaries of other partitions. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = CutoutFracture)
	bool									bFillGaps;
};

UCLASS()
class UBlastFractureSettingsCut : public UBlastFractureSettingsNoise
{
	GENERATED_UCLASS_BODY()

	/** The point on plane. */
	UPROPERTY(EditAnywhere, Category = CutFracture)
	FBlastVector							Point;

	/** The normal to plane */
	UPROPERTY(EditAnywhere, Category = CutFracture)
	FBlastVector							Normal = FBlastVector(EBlastViewportControlMode::Normal, FVector(0.f, 0.f, 1.f));
};

DECLARE_DELEGATE(FOnFractureMethodChanged);
DECLARE_DELEGATE(FOnMaterialSelected);

UCLASS()
class UBlastFractureSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	/** The Fracture method. */
	UPROPERTY(EditAnywhere, Category = General)
	EBlastFractureMethod                    FractureMethod;

	EBlastFractureMethod                    PreviousFractureMethod = EBlastFractureMethod::VoronoiUniform;

	/** Stored interior material data.  Just need one as we only support Voronoi splitting. */
	//UPROPERTY(EditAnywhere, transient, Category = General)
	//FBlastFractureMaterial					FractureMaterialDesc;

	/** If set new chunks replace fractured chunk on its depth level otherwise will be added as children.
		This flag has no effect for root chunk, fractured chunks will be added as its children.
	*/
	UPROPERTY(EditAnywhere, Category = General)
	uint32									bReplaceFracturedChunk : 1;

	/** If set fracture tool will produce new chunk for each unconnected convex 
		otherwise chunks which contains few unconnected convexes is possible.
	*/
	UPROPERTY(EditAnywhere, Category = General, meta = (DefaultValue = "1"))
	uint32									bRemoveIslands : 1;

	/** If set specified Fracture seed will be used otherwise fracture seed will be generated randomly.
		 Set it for reproducing the same fracture and unset for fracture diversity. */
	UPROPERTY(EditAnywhere, Category = General, meta = (InlineEditConditionToggle))
	uint32									bUseFractureSeed : 1;

	/** Fracture seed for random number generator used in fracture tool. */
	UPROPERTY(EditAnywhere, Category = General, meta = (EditCondition = "bUseFractureSeed"))
	int32									FractureSeed;

	/** If set default support depth will be used for fractured chunks otherwise leaves (chunks without children)
		will be marked as support.
	*/
	UPROPERTY(EditAnywhere, Category = General, meta = (InlineEditConditionToggle))
	uint32									bDefaultSupportDepth : 1;

	/** Fractured chunks will be support chunks if its depth the same as DefaultSupportDepth or 
		if it has no children and its depth is less then DefaultSupportDepth. */
	UPROPERTY(EditAnywhere, Category = General, meta = (EditCondition = "bDefaultSupportDepth"))
	int32									DefaultSupportDepth;

	/** Load default fracture settings */
	UFUNCTION(meta = (FractureSettingsDefaults))
	void									LoadDefault();

	/** Save default fracture settings */
	UFUNCTION(meta = (FractureSettingsDefaults))
	void									SaveAsDefault();

	void									Reset();

	/** The material for internal faces of fractured chunks. External materials will be inherited from root chunk. */
	UPROPERTY(EditAnywhere, Category = General)
	UMaterialInterface*						InteriorMaterial;

	/** The existing slot to apply to the interior material. If none, then a new slot is created"*/
	UPROPERTY(EditAnywhere, Category = General)
	FName									InteriorMaterialSlotName;

	//These need to be tagged to be seen by the GC
	UPROPERTY(Instanced)
	UBlastFractureSettingsVoronoiUniform*	VoronoiUniformFracture;
	
	UPROPERTY(Instanced)
	UBlastFractureSettingsVoronoiClustered*	VoronoiClusteredFracture;
	
	UPROPERTY(Instanced)
	UBlastFractureSettingsRadial*			RadialFracture;

	UPROPERTY(Instanced)
	UBlastFractureSettingsInSphere*			InSphereFracture;
	
	UPROPERTY(Instanced)
	UBlastFractureSettingsRemoveInSphere*	RemoveInSphere;

	UPROPERTY(Instanced)
	UBlastFractureSettingsUniformSlicing*	UniformSlicingFracture;

	UPROPERTY(Instanced)
	UBlastFractureSettingsCutout*			CutoutFracture;

	UPROPERTY(Instanced)
	UBlastFractureSettingsCut*				CutFracture;

	/** APEX references materials by name, but we'll bypass that mechanism and use of UE materials instead. */
	//UPROPERTY(EditFixedSize, Category = Material)
	//TArray<UMaterialInterface*>				Materials;

	/** Per-chunk authoring parameters, which should be made writable when a chunk selection GUI is in place. */
	//UPROPERTY()
	//TArray<FBlastChunkParameters>			ChunkParameters;

	//~ Begin UObject Interface.
	virtual void							PostEditChangeProperty(struct FPropertyChangedEvent& e) override;
	//~ End  UObject Interface

	//TODO add buttons save default/reset to default: 1) for all settings (DONE) 2) for each setting

	TSharedPtr<struct FFractureSession>		FractureSession;

	//TArray<TSharedPtr<FString>>				FractureHistory;

	FOnFractureMethodChanged				OnFractureMethodChanged;

	FOnMaterialSelected						OnMaterialSelected;

	class FBlastMeshEditor*					BlastMeshEditor;
};

/** Config for UBlastFractureSettings. Need this for load/save default values and sripts */
UCLASS(config = EditorPerProjectUserSettings)
class UBlastFractureSettingsConfig
	: public UObject
{
public:
	GENERATED_UCLASS_BODY()

	UPROPERTY(config)
	EBlastFractureMethod                    FractureMethod;

	UPROPERTY(config)
	uint32									VoronoiForceReset : 1;

	UPROPERTY(config, meta = (ClampMin = "0.01", ClampMax = "1"))
	FVector									VoronoiCellAnisotropy;

	UPROPERTY(config)
	FQuat									VoronoiCellRotation;

	UPROPERTY(config, meta = (ClampMin = "2"))
	int32									VoronoiUniformCellCount;

	UPROPERTY(config, meta = (ClampMin = "2"))
	int32									VoronoiClusteredCellCount;

	UPROPERTY(config, meta = (ClampMin = "2"))
	int32									VoronoiClusteredClusterCount;

	UPROPERTY(config, meta = (ClampMin = "0"))
	float									VoronoiClusteredClusterRadius;

	//UPROPERTY(config)
	//FVector									RadialOrigin;

	//UPROPERTY(config)
	//FVector									RadialNormal;

	UPROPERTY(config, meta = (ClampMin = "0.01"))
	float									RadialRadius;

	UPROPERTY(config, meta = (ClampMin = "2"))
	int32									RadialAngularSteps;

	UPROPERTY(config, meta = (ClampMin = "1"))
	int32									RadialRadialSteps;

	UPROPERTY(config, meta = (ClampMin = "0", ClampMax = "1"))
	float									RadialAngleOffset;

	UPROPERTY(config, meta = (ClampMin = "0", ClampMax = "1"))
	float									RadialVariability;

	UPROPERTY(config, meta = (ClampMin = "2"))
	int32									InSphereCellCount;
	
	UPROPERTY(config, meta = (ClampMin = "0.01"))
	float									InSphereRadius;

	UPROPERTY(config, meta = (ClampMin = "0.01"))
	float									RemoveInSphereRadius;

	UPROPERTY(config, meta = (ClampMin = "0", ClampMax = "1"))
	float									RemoveInSphereProbability;

	UPROPERTY(config, meta = (ClampMin = "1"))
	FIntVector								UniformSlicingSlicesCount;

	UPROPERTY(config, meta = (ClampMin = "0", ClampMax = "1"))
	float									UniformSlicingAngleVariation;

	UPROPERTY(config, meta = (ClampMin = "0", ClampMax = "1"))
	float									UniformSlicingOffsetVariation;

	UPROPERTY(config)
	FVector2D								CutoutSize;

	UPROPERTY(config)
	float									CutoutRotationZ;

	UPROPERTY(config)
	bool									bCutoutPeriodic;

	UPROPERTY(config)
	bool									bCutoutFillGaps;

	UPROPERTY(config, meta = (ClampMin = "0"))
	float									NoiseAmplitude;

	UPROPERTY(config, meta = (ClampMin = "0"))
	float									NoiseFrequency;

	UPROPERTY(config, meta = (ClampMin = "0"))
	int32									NoiseOctaveNumber;

	UPROPERTY(config, meta = (ClampMin = "0"))
	int32									NoiseSurfaceResolution;

	UPROPERTY(config)
	uint32									bReplaceFracturedChunk : 1;

	UPROPERTY(config)
	uint32									bRemoveIslands : 1;

	UPROPERTY(config)
	int32									RandomSeed;

	UPROPERTY(config)
	int32									DefaultSupportDepth;

	UPROPERTY(config)
	TArray <FString>						FractureScriptNames;

	UPROPERTY(config)
	TArray <FString>						FractureScripts;

};

