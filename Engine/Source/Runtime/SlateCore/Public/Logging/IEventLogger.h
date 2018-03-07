// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;

namespace EEventLog
{
	/**
	 * Enumerates Slate events that can be logged.
	 */
	enum Type
	{
		MouseMove,
		MouseEnter,
		MouseLeave,
		MouseButtonDown,
		MouseButtonUp,
		MouseButtonDoubleClick,
		MouseWheel,
		DragDetected,
		DragEnter,
		DragLeave,
		DragOver,
		DragDrop,
		DropMessage,
		KeyDown,
		KeyUp,
		KeyChar,
		AnalogInput,
		UICommand,
		BeginTransaction,
		EndTransaction,
		TouchGesture,
		Other,
	};

	/**
	 * Converts a log event to a string.
	 *
	 * @param Event The event to convert.
	 * @return The event's string representation.
	 */
	inline FString ToString(EEventLog::Type Event)
	{
		switch (Event)
		{
		case MouseMove: return FString("MouseMove");
		case MouseEnter: return FString("MouseEnter");
		case MouseLeave: return FString("MouseLeave");
		case MouseButtonDown: return FString("MouseButtonDown");
		case MouseButtonUp: return FString("MouseButtonUp");
		case MouseButtonDoubleClick: return FString("MouseButtonDoubleClick");
		case MouseWheel: return FString("MouseWheel");
		case DragDetected: return FString("DragDetected");
		case DragEnter: return FString("DragEnter");
		case DragLeave: return FString("DragLeave");
		case DragOver: return FString("DragOver");
		case DragDrop: return FString("DragDrop");
		case DropMessage: return FString("DropMessage");
		case KeyDown: return FString("KeyDown");
		case KeyUp: return FString("KeyUp");
		case KeyChar: return FString("KeyChar");
		case AnalogInput: return FString("AnalogInput");
		case UICommand: return FString("UICommand");
		case BeginTransaction: return FString("BeginTransaction");
		case EndTransaction: return FString("EndTransaction");
		case TouchGesture: return FString("TouchGesture");
		case Other: return FString("Other");

		default:
			return FString("Unknown");
		}
	}
}


/**
 * Interface for Slate event loggers.
 */
class IEventLogger
{
public:

	/**
	 * Gets all collected events in a string.
	 *
	 * @return A string containing the logged events.
	 */
	virtual FString GetLog( ) const = 0;

	/**
	 * Logs an event.
	 *
	 * @param Event The event to log.
	 * @param AdditionalContent An optional string associated with the event.
	 * @param Widget The widget that generated the event.
	 */
	virtual void Log( EEventLog::Type Event, const FString& AdditionalContent, TSharedPtr<SWidget> Widget ) = 0;

	/**
	 * Saves all collected events to a file.
	 */
	virtual void SaveToFile( ) = 0;

public:

	/** Virtual destructor. */
	virtual ~IEventLogger( ) { }
};
