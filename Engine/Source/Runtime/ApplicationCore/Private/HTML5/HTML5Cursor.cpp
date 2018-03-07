// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5Cursor.h"

FHTML5Cursor::FHTML5Cursor()
{
	// Set the default cursor
	SetType( EMouseCursor::Default );

	CursorStatus = true; 
	LockStatus = false; 
}


FHTML5Cursor::~FHTML5Cursor()
{
}

FVector2D FHTML5Cursor::GetPosition() const
{
	return Position;
}

void FHTML5Cursor::SetPosition( const int32 X, const int32 Y )
{
	Position.Set(X, Y);
}

void FHTML5Cursor::SetType( const EMouseCursor::Type InNewCursor )
{
	CurrentType = InNewCursor;

	// TODO: use .css to correctly change cursor types in javascript. 
	if ( InNewCursor == EMouseCursor::None)
		CursorStatus = false; 
	else 
		CursorStatus  = true; 

}

void FHTML5Cursor::GetSize( int32& Width, int32& Height ) const
{
	Width = 16;
	Height = 16;
}

void FHTML5Cursor::Show( bool bShow )
{
	CursorStatus = bShow;
}

void FHTML5Cursor::Lock( const RECT* const Bounds )
{
	if(Bounds == NULL)
	{
		LockStatus = false;
	}
	else
	{
		LockStatus = true; 
	}
}
