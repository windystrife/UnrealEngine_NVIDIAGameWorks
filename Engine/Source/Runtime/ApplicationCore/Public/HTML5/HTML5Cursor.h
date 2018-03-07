// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ICursor.h"
#include "Math/Vector2D.h"

class FHTML5Cursor : public ICursor
{
public:

	FHTML5Cursor();

	virtual ~FHTML5Cursor();

	virtual FVector2D GetPosition() const override;

	virtual void SetPosition( const int32 X, const int32 Y ) override;

	virtual void SetType( const EMouseCursor::Type InNewCursor ) override;

	virtual EMouseCursor::Type GetType() const override
	{
		return CurrentType;
	}

	virtual void GetSize( int32& Width, int32& Height ) const override;

	virtual void Show( bool bShow ) override;

	virtual void Lock( const RECT* const Bounds ) override;

	virtual void SetTypeShape(EMouseCursor::Type InCursorType, void* CursorHandle) override { }

private:
	EMouseCursor::Type CurrentType;
	FVector2D Position;	
	bool CursorStatus; 
	bool LockStatus; 

	friend class FHTML5Application;
};