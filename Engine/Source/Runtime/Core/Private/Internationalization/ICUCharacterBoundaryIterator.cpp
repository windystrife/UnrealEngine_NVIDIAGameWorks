// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/IBreakIterator.h"
#include "Internationalization/BreakIterator.h"

#if UE_ENABLE_ICU
#include "Internationalization/ICUBreakIterator.h"

TSharedRef<IBreakIterator> FBreakIterator::CreateCharacterBoundaryIterator()
{
	return MakeShareable(new FICUBreakIterator(FICUBreakIteratorManager::Get().CreateCharacterBoundaryIterator()));
}

#endif
