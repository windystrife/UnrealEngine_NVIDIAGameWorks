// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnumKeyArea.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "CurveKeyEditors/SEnumCurveKeyEditor.h"


/* IKeyArea interface
 *****************************************************************************/

bool FEnumKeyArea::CanCreateKeyEditor()
{
	return true;
}


TSharedRef<SWidget> FEnumKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNew(SEnumCurveKeyEditor)
		.Sequencer(Sequencer)
		.OwningSection(OwningSection)
		.Curve(&Curve)
		.Enum(Enum)
		.ExternalValue(ExternalValue);
};

