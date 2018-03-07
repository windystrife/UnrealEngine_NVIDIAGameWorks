// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MatineeTransaction.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpTrack.h"
#include "MatineeOptions.h"
#include "K2Node_MatineeController.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpTrackInst.h"

/*-----------------------------------------------------------------------------
	FMatineeTransaction
-----------------------------------------------------------------------------*/
void FMatineeTransaction::SaveObject( UObject* Object )
{
	check(Object);

	if( Object->IsA( AMatineeActor::StaticClass() ) ||  
		Object->IsA( UInterpData::StaticClass() ) ||
		Object->IsA( UInterpGroup::StaticClass() ) ||
		Object->IsA( UInterpTrack::StaticClass() ) ||
		Object->IsA( UInterpGroupInst::StaticClass() ) ||
		Object->IsA( UInterpTrackInst::StaticClass() ) ||
		Object->IsA( UMatineeOptions::StaticClass() ) ||
		Object->IsA( UK2Node_MatineeController::StaticClass() ) )
	{
		// Save the object.
		new( Records )FObjectRecord( this, Object, NULL, 0, 0, 0, 0, NULL, NULL, NULL );
	}
}

void FMatineeTransaction::SaveArray( UObject* Object, FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_DC DefaultConstructor, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
{
	// Never want this.
}
