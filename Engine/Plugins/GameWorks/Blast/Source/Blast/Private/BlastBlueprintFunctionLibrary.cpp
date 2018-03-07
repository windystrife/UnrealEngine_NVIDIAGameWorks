#include "BlastBlueprintFunctionLibrary.h"

#include "Components/PrimitiveComponent.h"

bool UBlastBlueprintFunctionLibrary::IsValidToApplyForces(class UPrimitiveComponent* Component, FName BoneName)
{
	if (Component && Component->Mobility == EComponentMobility::Movable)
	{
		FBodyInstance* BI = Component->GetBodyInstance(BoneName);
		return BI && BI->bSimulatePhysics;
	}
	return false;
}
