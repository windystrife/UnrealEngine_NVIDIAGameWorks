//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononGeometryComponent.h"
#include "PhononScene.h"
#include "Engine/StaticMeshActor.h"
#include "StaticMeshResources.h"

UPhononGeometryComponent::UPhononGeometryComponent()
	: ExportAllChildren(false)
	, NumVertices(0)
	, NumTriangles(0)
{
}

void UPhononGeometryComponent::UpdateStatistics()
{
#if WITH_EDITOR
	if (ExportAllChildren)
	{
		NumTriangles = SteamAudio::GetNumTrianglesAtRoot(GetOwner());
	}
	else
	{
		NumTriangles = SteamAudio::GetNumTrianglesForStaticMesh(Cast<AStaticMeshActor>(GetOwner()));
	}

	NumVertices = NumTriangles * 3;
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UPhononGeometryComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPhononGeometryComponent, ExportAllChildren)))
	{
		UpdateStatistics();
	}
}
#endif

void UPhononGeometryComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

#if WITH_EDITOR
	UpdateStatistics();
#endif // WITH_EDITOR
}
