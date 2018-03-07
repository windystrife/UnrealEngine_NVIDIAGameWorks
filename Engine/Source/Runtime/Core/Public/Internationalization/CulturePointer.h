// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"

class FCulture;

typedef TSharedPtr<FCulture, ESPMode::ThreadSafe> FCulturePtr;
typedef TSharedRef<FCulture, ESPMode::ThreadSafe> FCultureRef;
