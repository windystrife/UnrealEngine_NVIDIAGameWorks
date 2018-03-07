// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/TextRange.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/IRunRenderer.h"

class SLATE_API ILayoutBlock
{
public:

	virtual ~ILayoutBlock() {}

	virtual TSharedRef< class IRun > GetRun() const = 0;
	virtual FTextRange GetTextRange() const = 0;
	virtual FVector2D GetSize() const = 0;
	virtual FLayoutBlockTextContext GetTextContext() const = 0;
	virtual TSharedPtr< class IRunRenderer > GetRenderer() const = 0;

	virtual void SetLocationOffset( const FVector2D& InLocationOffset ) = 0;
	virtual FVector2D GetLocationOffset() const = 0;
};
