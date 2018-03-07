// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ImmediatePhysics
{

struct FActorHandle;

struct FJoint
{
	FActorHandle* DynamicActor;
	FActorHandle* OtherActor;
};

}