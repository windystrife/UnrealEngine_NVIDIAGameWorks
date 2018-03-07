// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

//@todo: perforce doesn't currently support VS2015
#if USE_P4_API
PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include <p4/clientapi.h>
#include <p4/i18napi.h>
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
#endif

//@todo: perforce doesn't currently support VS2015
#if !USE_P4_API
class ClientApi
{};
class ClientUser
{};
class StrBuf
{};
class StrDict
{};
class Error
{};
class StrPtr
{};
class KeepAlive
{};
#endif

