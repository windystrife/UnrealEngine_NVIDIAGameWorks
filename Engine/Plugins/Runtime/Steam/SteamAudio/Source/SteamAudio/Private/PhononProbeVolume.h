//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "PhononCommon.h"
#include "GameFramework/Volume.h"
#include "PhononProbeVolume.generated.h"

UENUM()
enum class EPhononProbePlacementStrategy : uint8
{
	// Places a single probe at the centroid of the volume.
	CENTROID = 0 UMETA(DisplayName = "Centroid"),
	// Places uniformly spaced probes along the floor at a specified height.
	UNIFORM_FLOOR = 1 UMETA(DisplayName = "Uniform Floor")
};

UENUM()
enum class EPhononProbeMobility : uint8
{
	// Static probes remain fixed at runtime.
	STATIC = 0 UMETA(DisplayName = "Static"),
	// Dynamic probes inherit this volume's offset at runtime.
	DYNAMIC = 1 UMETA(DisplayName = "Dynamic")
};

USTRUCT()
struct FBakedDataInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 Size;
};

inline bool operator==(const FBakedDataInfo& lhs, const FBakedDataInfo& rhs)
{
	return lhs.Name == rhs.Name && lhs.Size == rhs.Size;
}

inline bool operator<(const FBakedDataInfo& lhs, const FBakedDataInfo& rhs)
{
	return lhs.Name < rhs.Name;
}

/**
 * Phonon Probe volumes generate a set of probes at which acoustic information will be sampled
 * at bake time.
 */
UCLASS(HideCategories = (Actor, Advanced, Attachment, Collision), meta = (BlueprintSpawnableComponent))
class STEAMAUDIO_API APhononProbeVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;

	void PlaceProbes(IPLhandle PhononScene, IPLProbePlacementProgressCallback ProbePlacementCallback, TArray<IPLSphere>& ProbeSpheres);
#endif

	void UpdateProbeBoxData(IPLhandle ProbeBox);

	uint8* GetProbeBoxData();

	int32 GetProbeBoxDataSize() const;

	uint8* GetProbeBatchData();

	int32 GetProbeBatchDataSize() const;

	int32 GetDataSizeForSource(const FName& UniqueIdentifier) const;

	class UPhononProbeComponent* GetPhononProbeComponent() const { return PhononProbeComponent; }

	// Method by which probes are placed within the volume.
	UPROPERTY(EditAnywhere, Category = ProbeGeneration)
	EPhononProbePlacementStrategy PlacementStrategy;

	// How far apart to place probes.
	UPROPERTY(EditAnywhere, Category = ProbeGeneration, meta = (ClampMin = "0.0", ClampMax = "5000.0", UIMin = "0.0", UIMax = "5000.0"))
	float HorizontalSpacing;

	// How high above the floor to place probes.
	UPROPERTY(EditAnywhere, Category = ProbeGeneration, meta = (ClampMin = "0.0", ClampMax = "5000.0", UIMin = "0.0", UIMax = "5000.0"))
	float HeightAboveFloor;

	// Number of probes contained in this probe volume.
	UPROPERTY(VisibleAnywhere, Category = ProbeVolumeStatistics, meta = (DisplayName = "Probe Points"))
	int32 NumProbes;

	// Size of probe data in bytes.
	UPROPERTY(VisibleAnywhere, Category = ProbeVolumeStatistics, meta = (DisplayName = "Probe Data Size"))
	int32 ProbeBoxDataSize;

	UPROPERTY(VisibleAnywhere, Category = ProbeVolumeStatistics, meta = (DisplayName = "Detailed Statistics"))
	TArray<FBakedDataInfo> BakedDataInfo;

	UPROPERTY()
	class UPhononProbeComponent* PhononProbeComponent;

private:

	UPROPERTY()
	TArray<uint8> ProbeBoxData;

	UPROPERTY()
	TArray<uint8> ProbeBatchData;
};