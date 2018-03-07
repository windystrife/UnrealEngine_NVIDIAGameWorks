// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/FlexContainer.h"

#include "PhysXSupport.h"

DEFINE_LOG_CATEGORY(LogFlex);

void FlexErrorFunc(NvFlexErrorSeverity, const char* msg, const char* file, int line)
{
	UE_LOG(LogFlex, Warning, TEXT("Flex Error: %s, %s:%d"), ANSI_TO_TCHAR(msg), ANSI_TO_TCHAR(file), line);
}

// UFlexContainer
UFlexContainer::UFlexContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxParticles = 8192;
	DebugDraw = false;

	Gravity = FVector(0.0f, 0.0f, -980.0f);
	Wind = FVector(0.0f, 0.0f, 0.0f);
	Radius = 15.0f;
	Viscosity = 0.0f;
	ShapeFriction = 0.2f;
	ParticleFriction = 0.1f;
	Drag = 0.0f;
	Lift = 0.0f;
	Damping = 0.0f;
	NumSubsteps = 1;
	MinFrameRate = 60;
	TimeStepSmoothingFactor = 0.99f;
	NumIterations = 3;
	RestDistance = 0.5f;
	Dissipation = 0.0f;
	ComplexCollision = false;
	ObjectType = ECC_Flex;
	ResponseToChannels = FCollisionResponseContainer::GetDefaultResponseContainer();
	CollisionMarginParticles = 0.0f;
	CollisionMarginShapes = 0.0f;
	CollisionDistance = 5.0f;
	PlasticThreshold = 0.0f;
	PlasticCreep = 0.0f;
	Fluid = false;
	SleepThreshold = 0.0f;
	ShockPropagation = 0.0f;
	Restitution = 0.0f;
	MaxVelocity = 5000;
	MaxContainerBound = 1e12;
	RelaxationMode = EFlexSolverRelaxationMode::Local;
	RelaxationFactor = 1.0f;
	SolidPressure = 1.0f;
	AnisotropyScale = 0.0f;
	AnisotropyMin = 0.1f;
	AnisotropyMax = 2.0f;
	PositionSmoothing = 0.0f;
	Adhesion = 0.0f;
	Cohesion = 0.025f;
	SurfaceTension = 0.0f;
	VorticityConfinement = 0.0f;
	bUseMergedBounds = true;
	FixedTimeStep = true;
}


