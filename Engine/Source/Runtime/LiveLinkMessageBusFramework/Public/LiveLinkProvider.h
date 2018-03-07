// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SharedPointer.h"
#include "LiveLinkTypes.h"

struct ILiveLinkProvider
{
public:
	LIVELINKMESSAGEBUSFRAMEWORK_API static TSharedPtr<ILiveLinkProvider> CreateLiveLinkProvider(const FString& ProviderName);

	// Update hierarchy for named subject
	virtual void UpdateSubject(const FName& SubjectName, const TArray<FName>& BoneNames, const TArray<int32>& BoneParents) = 0;

	// Remove named subject
	virtual void ClearSubject(const FName& SubjectName) = 0;

	// Update subject with transform data
	virtual void UpdateSubjectFrame(const FName& SubjectName, const TArray<FTransform>& BoneTransforms, const TArray<FLiveLinkCurveElement>& CurveData, double Time, int32 FrameNum) = 0;

	virtual ~ILiveLinkProvider() {}
};