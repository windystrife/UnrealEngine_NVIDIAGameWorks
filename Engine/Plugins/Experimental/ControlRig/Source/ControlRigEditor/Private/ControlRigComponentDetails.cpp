// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigComponentDetails.h"
#include "UObject/Class.h"
#include "ControlRigComponent.h"

#define LOCTEXT_NAMESPACE "ControlRigComponentDetails"

TSharedRef<IDetailCustomization> FControlRigComponentDetails::MakeInstance()
{
	return MakeShareable( new FControlRigComponentDetails );
}

void FControlRigComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
}

#undef LOCTEXT_NAMESPACE
