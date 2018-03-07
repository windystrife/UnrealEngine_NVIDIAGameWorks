// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkRefSkeleton.h"
#include "Features/IModularFeature.h"
#include "LiveLinkTypes.h"
#include "Guid.h"

class ILiveLinkSource;

class LIVELINKINTERFACE_API ILiveLinkClient : public IModularFeature
{
public:
	virtual ~ILiveLinkClient() {}

	static FName ModularFeatureName;

	// Helper functions for making time codes for new frames
	virtual FLiveLinkTimeCode MakeTimeCode(double InTime, int32 InFrameNum) const = 0;
	virtual FLiveLinkTimeCode MakeTimeCodeFromTimeOnly(double InTime) const = 0;

	// Add a new live link source to the client
	virtual void AddSource(TSharedPtr<ILiveLinkSource> InSource) = 0;

	virtual void PushSubjectSkeleton(FName SubjectName, const FLiveLinkRefSkeleton& RefSkeleton) = 0;
	virtual void PushSubjectData(FGuid SourceGuid, FName SubjectName, const TArray<FTransform>& Transforms, const TArray<FLiveLinkCurveElement>& CurveElements, const FLiveLinkTimeCode& TimeCode) = 0;
	virtual void ClearSubject(FName SubjectName) = 0;

	virtual const FLiveLinkSubjectFrame* GetSubjectData(FName SubjectName) = 0;
};