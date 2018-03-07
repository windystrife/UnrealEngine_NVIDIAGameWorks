// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ConstructorHelpers.h"


// Macro to create a composure material and set it to <DestMemberName>.
#define COMPOSURE_GET_MATERIAL(MaterialType,DestMemberName,MaterialDirName,MaterialFileName) \
	static ConstructorHelpers::FObjectFinder<U##MaterialType> G##DestMemberName##Material( \
		TEXT(#MaterialType "'/Composure/Materials/" MaterialDirName MaterialFileName "." MaterialFileName "'")); \
	DestMemberName = G##DestMemberName##Material.Object


// Macro to create a composure dynamic material instance and set it to <DestMemberName>.
#define COMPOSURE_CREATE_DYMAMIC_MATERIAL(MaterialType,DestMemberName,MaterialDirName,MaterialFileName) \
	static ConstructorHelpers::FObjectFinder<U##MaterialType> G##DestMemberName##Material( \
		TEXT(#MaterialType "'/Composure/Materials/" MaterialDirName MaterialFileName "." MaterialFileName "'")); \
	DestMemberName = UMaterialInstanceDynamic::Create(G##DestMemberName##Material.Object, this, TEXT(#DestMemberName))


DECLARE_LOG_CATEGORY_EXTERN(Composure, Log, All);
