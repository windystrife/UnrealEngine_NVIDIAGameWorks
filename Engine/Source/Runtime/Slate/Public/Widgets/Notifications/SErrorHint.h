// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Animation/CurveSequence.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Notifications/SErrorText.h"

class SLATE_API SErrorHint
	: public SCompoundWidget
	, public IErrorReportingWidget
{
public:

	SLATE_BEGIN_ARGS( SErrorHint )
		: _ErrorText()
		{}
		SLATE_ARGUMENT(FText, ErrorText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:

	// IErrorReportingWidget interface

	virtual void SetError( const FText& InErrorText ) override;
	virtual void SetError( const FString& InErrorText ) override;
	virtual bool HasError() const override;
	virtual TSharedRef<SWidget> AsWidget() override;

private:

	TAttribute<EVisibility> CustomVisibility;
	EVisibility MyVisibility() const;

	FVector2D GetDesiredSizeScale() const;
	FCurveSequence ExpandAnimation;

	TSharedPtr<SWidget> ImageWidget;
	FText ErrorText;
	FText GetErrorText() const;
};
