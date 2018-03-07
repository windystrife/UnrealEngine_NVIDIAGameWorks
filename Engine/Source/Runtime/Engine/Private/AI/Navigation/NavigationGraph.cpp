// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "AI/Navigation/NavigationGraph.h"
#include "EngineUtils.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavGraphGenerator.h"
#include "AI/Navigation/NavNodeInterface.h"
#include "AI/Navigation/NavigationGraphNodeComponent.h"
#include "AI/Navigation/NavigationGraphNode.h"

//----------------------------------------------------------------------//
// FNavGraphNode
//----------------------------------------------------------------------//
FNavGraphNode::FNavGraphNode() 
{
	Edges.Reserve(InitialEdgesCount);
}

//----------------------------------------------------------------------//
// UNavigationGraphNodeComponent
//----------------------------------------------------------------------//
UNavigationGraphNodeComponent::UNavigationGraphNodeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNavigationGraphNodeComponent::BeginDestroy()
{
	Super::BeginDestroy();
	
	if (PrevNodeComponent != NULL)
	{
		PrevNodeComponent->NextNodeComponent = NextNodeComponent;
	}

	if (NextNodeComponent != NULL)
	{
		NextNodeComponent->PrevNodeComponent = PrevNodeComponent;
	}

	NextNodeComponent = NULL;
	PrevNodeComponent = NULL;
}

//----------------------------------------------------------------------//
// ANavigationGraphNode
//----------------------------------------------------------------------//
ANavigationGraphNode::ANavigationGraphNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//----------------------------------------------------------------------//
// ANavigationGraph
//----------------------------------------------------------------------//

ANavigationGraph::ANavigationGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		NavDataGenerator = MakeShareable(new FNavGraphGenerator(this));
	}
}

ANavigationData* ANavigationGraph::CreateNavigationInstances(UNavigationSystem* NavSys)
{
	if (NavSys == NULL || NavSys->GetWorld() == NULL)
	{
		return NULL;
	}

	// first check if there are any INavNodeInterface implementing actors in the world
	bool bCreateNavigation = false;
	FActorIterator It(NavSys->GetWorld());
	for(; It; ++It )
	{
		if (Cast<INavNodeInterface>(*It) != NULL)
		{
			bCreateNavigation = true;
			break;
		}
	}

	if (false && bCreateNavigation)
	{
		ANavigationGraph* Instance = NavSys->GetWorld()->SpawnActor<ANavigationGraph>();
	}

	return NULL;
}
