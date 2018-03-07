// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Widgets/SWindow.h"

/** A dialog for collecting settings for importing post move settings from an external file. */
class SComposurePostMoveSettingsImportDialog : public SWindow
{
public:
	DECLARE_DELEGATE_ThreeParams(FOnImportSelected, FString, float, int32);
	DECLARE_DELEGATE(FOnImportCanceled);

public:
	SLATE_BEGIN_ARGS(SComposurePostMoveSettingsImportDialog) {}
		/** Event which is executed when the user selects import. */
		SLATE_EVENT(FOnImportSelected, OnImportSelected)
		/** Event which is executed when the user cancels the import. */
		SLATE_EVENT(FOnImportCanceled, OnImportCanceled)
	SLATE_END_ARGS()

	/** 
	 * Constructs a new import dialog. 
	 *
	 * @param InArgs Slate arguments. 
	 * @param InFrameSize The size of the frame in pixels that the import settings are relative to.
	 * @param InFrameInterval The fixed frame interval for the movie scene which owns the track the data will be imported into. 
	 * @param InStartFrame The target start frame in the movie scene which will be used to import the data.
	 */
	void Construct(const FArguments& InArgs, float InFrameInterval, int32 InStartFrame);

private:
	FString GetFilePath() const;
	void FilePathPicked(const FString& PickedPath);

	float GetFrameInterval() const;
	void FrameIntervalChanged(float Value);

	int32 GetStartFrame() const;
	void StartFrameChanged(int32 Value);

	FReply OnCancelPressed();
	FReply OnImportPressed();

private:
	FString FilePath;
	float FrameInterval;
	int32 StartFrame;

	FOnImportSelected OnImportSelected;
	FOnImportCanceled OnImportCanceled;
};