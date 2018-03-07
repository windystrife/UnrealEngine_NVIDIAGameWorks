// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "SurveyPage.h"

class SSurveyPage : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SSurveyPage )
	{ }
	SLATE_END_ARGS()


	/** Widget constructor */
	void Construct( const FArguments& Args, const TSharedRef< FSurveyPage >& Page );
};
