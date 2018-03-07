// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Layout/Visibility.h"
#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class SEditableTextBox;


/**
 * Implements a details view customization for the UImgMediaSource class.
 */
class FImgMediaSourceCustomization
	: public IDetailCustomization
{
public:

	/** Virtual destructor. */
	~FImgMediaSourceCustomization() { }

public:

	//~ IDetailCustomization interface

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:

	/**
	 * Creates an instance of this class.
	 *
	 * @return The new instance.
	 */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FImgMediaSourceCustomization());
	}

protected:

	/**
	 * Get the path to the currently selected image sequence.
	 *
	 * @return Sequence path string.
	 */
	FString GetSequencePath() const;

private:

	/** Callback for picking a path in the source directory picker. */
	void HandleSequencePathPickerPathPicked(const FString& PickedPath);

	/** Callback for getting the visibility of warning icon for invalid SequencePath paths. */
	EVisibility HandleSequencePathWarningIconVisibility() const;

private:

	/** Text block widget showing the found proxy directories. */
	TSharedPtr<SEditableTextBox> ProxiesTextBlock;

	/** Pointer to the SequencePath.Path property handle. */
	TSharedPtr<IPropertyHandle> SequencePathProperty;
};
