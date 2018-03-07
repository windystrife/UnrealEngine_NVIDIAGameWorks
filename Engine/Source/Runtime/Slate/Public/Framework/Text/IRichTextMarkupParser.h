// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateGlobals.h"
#include "Framework/Text/ITextDecorator.h"

#if WITH_FANCY_TEXT

/**
 */
class SLATE_API IRichTextMarkupParser
{
public:

	/**
	 * Virtual destructor
	 */
	virtual ~IRichTextMarkupParser() {}

	/**
	 * Processes the provided Input string producing a set of FTextLineParseResults and a output string stripped of any markup.
	 */
	virtual void Process(TArray<FTextLineParseResults>& Results, const FString& Input, FString& Output) = 0;

};


#endif //WITH_FANCY_TEXT
