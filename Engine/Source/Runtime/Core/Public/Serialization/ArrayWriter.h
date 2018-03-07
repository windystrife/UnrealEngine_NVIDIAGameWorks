// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Serialization/BufferArchive.h"

/**
 * Archive objects that are also a TArray. Since FBufferArchive is already the 
 * writer version, we just typedef to the old, outdated name
 */
typedef FBufferArchive FArrayWriter;
