// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "SlateWidgetStyleContainerInterface.generated.h"

UINTERFACE()
class SLATECORE_API USlateWidgetStyleContainerInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ISlateWidgetStyleContainerInterface
{
	GENERATED_IINTERFACE_BODY()

public:

	virtual const struct FSlateWidgetStyle* const GetStyle() const = 0;
};
