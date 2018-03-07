// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Framework/Text/BaseTextLayoutMarshaller.h"

class FTextLayout;

/**
 * Get/set the raw text to/from a text layout as plain text
 */
class SLATE_API FPlainTextLayoutMarshaller : public FBaseTextLayoutMarshaller
{
public:

	static TSharedRef< FPlainTextLayoutMarshaller > Create();

	virtual ~FPlainTextLayoutMarshaller();
	
	void SetIsPassword(const TAttribute<bool>& InIsPassword);

	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;

protected:

	FPlainTextLayoutMarshaller();

	/** This this marshaller displaying a password? */
	TAttribute<bool> bIsPassword;

};
