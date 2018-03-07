//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "Components/ActorComponent.h"
#include "PhononGeometryComponent.generated.h"

/**
 * Phonon Geometry components are used to tag an actor as containing geometry relevant to acoustics calculations.
 * Should be placed on Static Mesh actors.
 */
UCLASS(ClassGroup = (Audio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class UPhononGeometryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhononGeometryComponent();

	void OnComponentCreated();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Whether or not to export all actors attached to this actor.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool ExportAllChildren;

	// The number of vertices exported to Steam Audio.
	UPROPERTY(VisibleAnywhere, Category = GeometryStatistics, meta = (DisplayName = "Vertices"))
	uint32 NumVertices;

	// The number of triangles exported to Steam Audio.
	UPROPERTY(VisibleAnywhere, Category = GeometryStatistics, meta = (DisplayName = "Triangles"))
	uint32 NumTriangles;

private:
	void UpdateStatistics();
}; 