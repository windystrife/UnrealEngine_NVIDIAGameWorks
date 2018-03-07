// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"

#ifndef SOURCE_CONTROL_WITH_SLATE
	#error SOURCE_CONTROL_WITH_SLATE not defined
#endif // SOURCE_CONTROL_WITH_SLATE

#if SOURCE_CONTROL_WITH_SLATE
	#include "SlateBasics.h"
	#include "EditorStyle.h"
#endif // SOURCE_CONTROL_WITH_SLATE

#if WITH_EDITOR
#include "PackageTools.h"
#include "MessageLog.h"
#endif

#if WITH_UNREAL_DEVELOPER_TOOLS
#include "MessageLogModule.h"
#endif
