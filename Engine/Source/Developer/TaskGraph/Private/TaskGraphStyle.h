// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

/**
 * Declares the Editor's visual style.
 */
class FTaskGraphStyle
{
public:
	
	class FStyle : public FSlateStyleSet
	{
	public:
		FStyle();

		void Initialize();
	};

	static void Initialize();

	static void Shutdown();

	static TSharedPtr<class ISlateStyle> Get();

	static TSharedPtr< FTaskGraphStyle::FStyle > StyleInstance;
};
