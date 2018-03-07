// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SGammaUIPanel : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SGammaUIPanel ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	float OnGetGamma() const;
	void OnGammaChanged( float NewValue );
};
