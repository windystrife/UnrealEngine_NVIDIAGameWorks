// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnumOnlyHeader.generated.h"

UENUM()
enum ESomeEnum
{
	One,
	Two,
	Three
};

UENUM()
namespace ESomeNamespacedEnum
{
	enum Type
	{
		Four,
		Five,
		Six
	};
}

UENUM()
enum class ECppEnum : uint8
{
	Seven,
	Eight,
	Nine
};
