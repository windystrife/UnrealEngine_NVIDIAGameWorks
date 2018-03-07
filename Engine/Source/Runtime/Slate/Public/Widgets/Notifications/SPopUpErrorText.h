// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SComboButton.h"

class SLATE_API SPopupErrorText : public SComboButton, public IErrorReportingWidget
{
public:
	SLATE_BEGIN_ARGS(SPopupErrorText)
		: _ShowInNewWindow( false )
		, _Font()
	{}
		/** The popup appears in a new window instead of in the same window that this widget is in */
		SLATE_ARGUMENT( bool, ShowInNewWindow )
 		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
	SLATE_END_ARGS()

	virtual void Construct( const FArguments& InArgs );

	// IErrorReportingWidget interface

	virtual void SetError( const FText& InErrorText ) override;
	virtual void SetError( const FString& InErrorText ) override;

	virtual bool HasError() const override;

	virtual TSharedRef<SWidget> AsWidget() override;

	// IErrorReportingWidget interface

private:
	TSharedPtr<SErrorText> HasErrorSymbol;
	TSharedPtr<SErrorText> ErrorText;
};
