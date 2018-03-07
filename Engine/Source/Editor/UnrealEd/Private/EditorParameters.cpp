// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "MaterialEditor/DEditorParameterValue.h"
#include "MaterialEditor/DEditorFontParameterValue.h"
#include "MaterialEditor/DEditorScalarParameterValue.h"
#include "MaterialEditor/DEditorStaticComponentMaskParameterValue.h"
#include "MaterialEditor/DEditorStaticSwitchParameterValue.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "MaterialEditor/DEditorVectorParameterValue.h"

UDEditorParameterValue::UDEditorParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDEditorFontParameterValue::UDEditorFontParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDEditorScalarParameterValue::UDEditorScalarParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDEditorStaticComponentMaskParameterValue::UDEditorStaticComponentMaskParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDEditorStaticSwitchParameterValue::UDEditorStaticSwitchParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDEditorTextureParameterValue::UDEditorTextureParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UDEditorVectorParameterValue::UDEditorVectorParameterValue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
