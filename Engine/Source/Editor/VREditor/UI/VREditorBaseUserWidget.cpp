// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorBaseUserWidget.h"
#include "VREditorFloatingUI.h"

UVREditorBaseUserWidget::UVREditorBaseUserWidget( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	  Owner( nullptr )
{
}


void UVREditorBaseUserWidget::SetOwner( class AVREditorFloatingUI* NewOwner )
{
	Owner = NewOwner;
}

