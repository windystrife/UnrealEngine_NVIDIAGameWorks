// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

 /* Dependencies
 *****************************************************************************/

#include "CoreMinimal.h"
#include "ModuleManager.h"
#include "SlateStyle.h"
#include "OnlineSubsystem.h"

 /* Interfaces
 *****************************************************************************/

#include "ILoginFlowModule.h"



#define FACTORY(ReturnType, Type, ...) \
class Type##Factory \
{ \
public: \
	static ReturnType Create(__VA_ARGS__); \
}; 

#define IFACTORY(ReturnType, Type, ...) \
class Type##Factory \
{ \
public: \
	virtual ReturnType Create(__VA_ARGS__) = 0; \
};

