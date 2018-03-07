// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "ArrayView.h"

#define LOCTEXT_NAMESPACE "SequencerHelpers"

class FSequencer;
class IKeyArea;
class UMovieSceneSection;

class SequencerHelpers
{
public:
	/**
	 * Gets the key areas from the requested node
	 */
	static void GetAllKeyAreas(TSharedPtr<FSequencerDisplayNode> DisplayNode, TSet<TSharedPtr<IKeyArea>>& KeyAreas);

	/**
	 * Get the section index that relates to the specified time
	 * @return the index corresponding to the highest overlapping section, or nearest section where no section overlaps the current time
	 */
	static int32 GetSectionFromTime(TArrayView<UMovieSceneSection* const> InSections, float Time);

	/**
	 * Get descendant nodes
	 */
	static void GetDescendantNodes(TSharedRef<FSequencerDisplayNode> DisplayNode, TSet<TSharedRef<FSequencerDisplayNode> >& Nodes);

	/**
	 * Gets all sections from the requested node
	 */
	static void GetAllSections(TSharedRef<FSequencerDisplayNode> DisplayNode, TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections);

	/**
	 * Find object binding node from the given display node
	 */
	static bool FindObjectBindingNode(TSharedRef<FSequencerDisplayNode> DisplayNode, TSharedRef<FSequencerDisplayNode>& ObjectBindingNode);

	/**
	 * Convert time to frame
	 */
	static int32 TimeToFrame(float Time, float FrameRate);

	/**
	 * Convert frame to time
	 */
	static float FrameToTime(int32 Frame, float FrameRate);

	/** Snaps a time value in seconds to the frame rate. */
	static float SnapTimeToInterval(float InTime, float InFrameRate);

	/**
	 * Validate that the nodes with selected keys or sections actually are true
	 */

	static void ValidateNodesWithSelectedKeysOrSections(FSequencer& Sequencer);

	/**
	 * Update the nodes with selected sections from the hovered node
	 */
	static void UpdateHoveredNodeFromSelectedSections(FSequencer& Sequencer);

	/**
	 * Update the nodes with selected keys from the hovered node
	 */
	static void UpdateHoveredNodeFromSelectedKeys(FSequencer& Sequencer);

	/**
	 * Perform default selection for the specified mouse event, based on the current hotspot
	 */
	static void PerformDefaultSelection(FSequencer& Sequencer, const FPointerEvent& MouseEvent);
	
	/**
	 * Attempt to summon a context menu for the current hotspot
	 */
	static TSharedPtr<SWidget> SummonContextMenu(FSequencer& Sequencer, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
};


class SequencerSnapValues
{
public:
	SequencerSnapValues() {}

	static const TArray<SNumericDropDown<float>::FNamedValue>& GetSnapValues()
	{
		static TArray<SNumericDropDown<float>::FNamedValue> SnapValues;
		static bool SnapValuesInitialized = false;

		if (!SnapValuesInitialized)
		{
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.001f, LOCTEXT( "Snap_OneThousandth", "0.001" ), LOCTEXT( "SnapDescription_OneThousandth", "Set snap to 1/1000th" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.01f, LOCTEXT( "Snap_OneHundredth", "0.01" ), LOCTEXT( "SnapDescription_OneHundredth", "Set snap to 1/100th" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.1f, LOCTEXT( "Snap_OneTenth", "0.1" ), LOCTEXT( "SnapDescription_OneTenth", "Set snap to 1/10th" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 1.0f, LOCTEXT( "Snap_One", "1" ), LOCTEXT( "SnapDescription_One", "Set snap to 1" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 10.0f, LOCTEXT( "Snap_Ten", "10" ), LOCTEXT( "SnapDescription_Ten", "Set snap to 10" ) ) );
			SnapValues.Add( SNumericDropDown<float>::FNamedValue( 100.0f, LOCTEXT( "Snap_OneHundred", "100" ), LOCTEXT( "SnapDescription_OneHundred", "Set snap to 100" ) ) );

			SnapValuesInitialized = true;
		}

		return SnapValues;
	}

	static const TArray<SNumericDropDown<float>::FNamedValue>& GetSecondsSnapValues()
	{
		static TArray<SNumericDropDown<float>::FNamedValue> SecondsSnapValues;
		static bool SecondsSnapValuesInitialized = false;

		if (!SecondsSnapValuesInitialized)
		{
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.001f, LOCTEXT( "Snap_OneThousandthSeconds", "0.001s" ), LOCTEXT( "SnapDescription_OneThousandthSeconds", "Set snap to 1/1000th of a second" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.01f, LOCTEXT( "Snap_OneHundredthSeconds", "0.01s" ), LOCTEXT( "SnapDescription_OneHundredthSeconds", "Set snap to 1/100th of a second" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 0.1f, LOCTEXT( "Snap_OneTenthSeconds", "0.1s" ), LOCTEXT( "SnapDescription_OneTenthSeconds", "Set snap to 1/10th of a second" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1.0f, LOCTEXT( "Snap_OneSeconds", "1s" ), LOCTEXT( "SnapDescription_OneSeconds", "Set snap to 1 second" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 10.0f, LOCTEXT( "Snap_TenSeconds", "10s" ), LOCTEXT( "SnapDescription_TenSeconds", "Set snap to 10 seconds" ) ) );
			SecondsSnapValues.Add( SNumericDropDown<float>::FNamedValue( 100.0f, LOCTEXT( "Snap_OneHundredSeconds", "100s" ), LOCTEXT( "SnapDescription_OneHundredSeconds", "Set snap to 100 seconds" ) ) );

			SecondsSnapValuesInitialized = true;
		}

		return SecondsSnapValues;
	}

	static const TArray<SNumericDropDown<float>::FNamedValue>& GetFrameRateSnapValues()
	{
		static TArray<SNumericDropDown<float>::FNamedValue> FrameRateSnapValues;
		static bool FrameRateSnapValuesInitialized = false;

		if (!FrameRateSnapValuesInitialized)
		{
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 15.0f, LOCTEXT( "Snap_15Fps", "15 fps" ), LOCTEXT( "SnapDescription_15Fps", "Set snap to 15 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 24.0f, LOCTEXT( "Snap_24Fps", "24 fps (film)" ), LOCTEXT( "SnapDescription_24Fps", "Set snap to 24 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 25.0f, LOCTEXT( "Snap_25Fps", "25 fps (PAL/25)" ), LOCTEXT( "SnapDescription_25Fps", "Set snap to 25 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 29.97f, LOCTEXT( "Snap_29.97Fps", "29.97 fps (NTSC/30)" ), LOCTEXT( "SnapDescription_29.97Fps", "Set snap to 29.97 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 30.0f, LOCTEXT( "Snap_30Fps", "30 fps" ), LOCTEXT( "SnapDescription_30Fps", "Set snap to 30 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 48.0f, LOCTEXT( "Snap_48Fps", "48 fps" ), LOCTEXT( "SnapDescription_48Fps", "Set snap to 48 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 50.0f, LOCTEXT( "Snap_50Fps", "50 fps (PAL/50)" ), LOCTEXT( "SnapDescription_50Fps", "Set snap to 50 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 59.94f, LOCTEXT( "Snap_59.94Fps", "59.94 fps (NTSC/60)" ), LOCTEXT( "SnapDescription_59.94Fps", "Set snap to 59.94 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 60.0f, LOCTEXT( "Snap_60Fps", "60 fps" ), LOCTEXT( "SnapDescription_60Fps", "Set snap to 60 fps" ) ) );
			FrameRateSnapValues.Add( SNumericDropDown<float>::FNamedValue( 1 / 120.0f, LOCTEXT( "Snap_120Fps", "120 fps" ), LOCTEXT( "SnapDescription_120Fps", "Set snap to 120 fps" ) ) );

			FrameRateSnapValuesInitialized = true;
		}

		return FrameRateSnapValues;
	}

	static const TArray<SNumericDropDown<float>::FNamedValue>& GetTimeSnapValues()
	{
		static TArray<SNumericDropDown<float>::FNamedValue> TimeSnapValues;
		static bool TimeSnapValuesInitialized = false;
		if (!TimeSnapValuesInitialized)
		{
			TimeSnapValues.Append(GetSecondsSnapValues());
			TimeSnapValues.Append(GetFrameRateSnapValues());

			TimeSnapValuesInitialized = true;
		}
		return TimeSnapValues;
	}

	static bool IsTimeSnapIntervalFrameRate(float InFrameRate)
	{
		for (auto FrameRateSnapValue : GetFrameRateSnapValues())
		{
			if (FMath::IsNearlyEqual(InFrameRate, FrameRateSnapValue.GetValue()))
			{
				return true;
			}
		}
		return false;
	}
};


#undef LOCTEXT_NAMESPACE
