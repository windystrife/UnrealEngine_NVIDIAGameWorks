// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/ICursor.h"
#include "Framework/Application/IInputProcessor.h"

class FSlateApplication;
struct FAnalogInputEvent;
struct FKeyEvent;
struct FPointerEvent;

namespace AnalogCursorMode
{
	enum Type
	{
		Accelerated,
		Direct,
	};
}

enum class EAnalogStick : uint8
{
	Left,
	Right,
	Max,
};

/**
 * A class that simulates a cursor driven by an analog stick.
 */
class SLATE_API FAnalogCursor : public IInputProcessor
{
public:
	FAnalogCursor();

	/** Dtor */
	virtual ~FAnalogCursor()
	{}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	void SetAcceleration(float NewAcceleration);
	void SetMaxSpeed(float NewMaxSpeed);
	void SetStickySlowdown(float NewStickySlowdown);
	void SetDeadZone(float NewDeadZone);
	void SetMode(AnalogCursorMode::Type NewMode);

protected:

	/** Getter */
	FORCEINLINE const FVector2D& GetAnalogValues( EAnalogStick Stick = EAnalogStick::Left ) const
	{
		return AnalogValues[ static_cast< uint8 >( Stick ) ];
	}

	/** Set the cached analog stick declinations to 0 */
	void ClearAnalogValues();

	/** Handles updating the cursor position and processing a Mouse Move Event */
	virtual void UpdateCursorPosition(FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor, const FVector2D& NewPosition);

	/** Current speed of the cursor */
	FVector2D CurrentSpeed;

	/** Current sub-pixel offset */
	FVector2D CurrentOffset;

	/** Current settings */
	float Acceleration;
	float MaxSpeed;
	float StickySlowdown;
	float DeadZone;
	AnalogCursorMode::Type Mode;

private:

	FORCEINLINE FVector2D& GetAnalogValue( EAnalogStick Stick )
	{
		return AnalogValues[ static_cast< uint8 >( Stick ) ];
	}

	/** Input from the gamepad */
	FVector2D AnalogValues[ static_cast<uint8>( EAnalogStick::Max ) ];
};

