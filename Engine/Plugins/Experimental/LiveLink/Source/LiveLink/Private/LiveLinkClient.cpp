// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClient.h"
#include "ScopeLock.h"
#include "UObjectHash.h"
#include "LiveLinkSourceFactory.h"
#include "Guid.h"

const double VALIDATE_SOURCES_TIME = 3.0; //How long should we wait between validation checks
const int32 MIN_FRAMES_TO_REMOVE = 5;

FLiveLinkCurveIntegrationData FLiveLinkCurveKey::UpdateCurveKey(const TArray<FLiveLinkCurveElement>& CurveElements)
{
	FLiveLinkCurveIntegrationData IntegrationData;

	int32 CurrentSize = CurveNames.Num();

	IntegrationData.CurveValues.AddDefaulted(CurrentSize);
	
	for(const FLiveLinkCurveElement& Elem : CurveElements)
	{
		int32 CurveIndex = CurveNames.IndexOfByKey(Elem.CurveName);
		if (CurveIndex == INDEX_NONE)
		{
			CurveIndex = CurveNames.Add(Elem.CurveName);
			IntegrationData.CurveValues.AddDefaulted();
		}
		IntegrationData.CurveValues[CurveIndex].SetValue(Elem.CurveValue);
	}
	IntegrationData.NumNewCurves = CurveNames.Num() - CurrentSize;

	return IntegrationData;
}

void BlendItem(const FTransform& A, const FTransform& B, FTransform& Output, float BlendWeight)
{
	const ScalarRegister ABlendWeight(1.0f - BlendWeight);
	const ScalarRegister BBlendWeight(BlendWeight);

	Output = A * ABlendWeight;
	Output.AccumulateWithShortestRotation(B, BBlendWeight);
	Output.NormalizeRotation();
}

void BlendItem(const FOptionalCurveElement& A, const FOptionalCurveElement& B, FOptionalCurveElement& Output, float BlendWeight)
{
	Output.Value = (A.Value * (1.0f - BlendWeight)) + (B.Value * BlendWeight);
	Output.bValid = A.bValid || B.bValid;
}

template<class Type>
void Blend(const TArray<Type>& A, const TArray<Type>& B, TArray<Type>& Output, float BlendWeight)
{
	check(A.Num() == B.Num());
	Output.SetNum(A.Num(), false);

	for (int32 BlendIndex = 0; BlendIndex < A.Num(); ++BlendIndex)
	{
		BlendItem(A[BlendIndex], B[BlendIndex], Output[BlendIndex], BlendWeight);
	}
}

void FLiveLinkSubject::AddFrame(const TArray<FTransform>& Transforms, const TArray<FLiveLinkCurveElement>& CurveElements, const FLiveLinkTimeCode& TimeCode, FGuid FrameSource)
{
	LastModifier = FrameSource;

	FLiveLinkFrame* NewFrame = nullptr;

	if(CachedConnectionSettings.bUseInterpolation)
	{
		if (TimeCode.Time < LastReadTime)
		{
			//Gone back in time
			Frames.Reset();
			LastReadTime = 0;
			SubjectTimeOffset = TimeCode.Offset;
		}

		if (Frames.Num() == 0)
		{
			Frames.AddDefaulted();
			NewFrame = &Frames[0];
			LastReadFrame = 0;
		}
		else
		{
			if (LastReadFrame > MIN_FRAMES_TO_REMOVE)
			{
				check(Frames.Num() > LastReadFrame);
				Frames.RemoveAt(0, LastReadFrame, false);
				LastReadFrame = 0;
			}

			int32 FrameIndex = Frames.Num() - 1;

			for (; FrameIndex >= 0; --FrameIndex)
			{
				if (Frames[FrameIndex].TimeCode.Time < TimeCode.Time)
				{
					break;
				}
			}

			int32 NewFrameIndex = Frames.Insert(FLiveLinkFrame(), FrameIndex + 1);
			NewFrame = &Frames[NewFrameIndex];
		}
	}
	else
	{
		//No interpolation
		if (Frames.Num() > 1)
		{
			Frames.Reset();
		}

		if (Frames.Num() == 0)
		{
			Frames.AddDefaulted();
		}

		NewFrame = &Frames[0];

		LastReadTime = 0;
		LastReadFrame = 0;

		SubjectTimeOffset = TimeCode.Offset;
	}

	FLiveLinkCurveIntegrationData IntegrationData = CurveKeyData.UpdateCurveKey(CurveElements);

	check(NewFrame);
	NewFrame->Transforms = Transforms;
	NewFrame->Curves = MoveTemp(IntegrationData.CurveValues);
	NewFrame->TimeCode = TimeCode;

	// update existing curves
	if (IntegrationData.NumNewCurves > 0)
	{
		for (FLiveLinkFrame& Frame : Frames)
		{
			Frame.ExtendCurveData(IntegrationData.NumNewCurves);
		}
	}
}

void FLiveLinkSubject::BuildInterpolatedFrame(const double InSeconds, FLiveLinkSubjectFrame& OutFrame)
{
	OutFrame.RefSkeleton = RefSkeleton;
	OutFrame.CurveKeyData = CurveKeyData;

	OutFrame.Transforms.Reset();
	OutFrame.Curves.Reset();

	if (!CachedConnectionSettings.bUseInterpolation)
	{
		OutFrame.Transforms = Frames.Last().Transforms;
		OutFrame.Curves = Frames.Last().Curves;
		LastReadTime = Frames.Last().TimeCode.Time;
		LastReadFrame = Frames.Num()-1;
	}
	else
	{
		LastReadTime = (InSeconds - SubjectTimeOffset) - CachedConnectionSettings.InterpolationOffset;

		bool bBuiltFrame = false;

		for (int32 FrameIndex = Frames.Num() - 1; FrameIndex >= 0; --FrameIndex)
		{
			if (Frames[FrameIndex].TimeCode.Time < LastReadTime)
			{
				//Found Start frame

				if (FrameIndex == Frames.Num() - 1)
				{
					LastReadFrame = FrameIndex;
					OutFrame.Transforms = Frames[FrameIndex].Transforms;
					OutFrame.Curves = Frames[FrameIndex].Curves;
					bBuiltFrame = true;
					break;
				}
				else
				{
					LastReadFrame = FrameIndex;
					const FLiveLinkFrame& PreFrame = Frames[FrameIndex];
					const FLiveLinkFrame& PostFrame = Frames[FrameIndex + 1];

					// Calc blend weight (Amount through frame gap / frame gap) 
					const float BlendWeight = (LastReadTime - PreFrame.TimeCode.Time) / (PostFrame.TimeCode.Time - PreFrame.TimeCode.Time);

					Blend(PreFrame.Transforms, PostFrame.Transforms, OutFrame.Transforms, BlendWeight);
					Blend(PreFrame.Curves, PostFrame.Curves, OutFrame.Curves, BlendWeight);

					bBuiltFrame = true;
					break;
				}
			}
		}

		if (!bBuiltFrame)
		{
			LastReadFrame = 0;
			// Failed to find an interp point so just take earliest frame
			OutFrame.Transforms = Frames[0].Transforms;
			OutFrame.Curves = Frames[0].Curves;
		}
	}
}

FLiveLinkClient::~FLiveLinkClient()
{
	TArray<int> ToRemove;
	ToRemove.Reserve(Sources.Num());

	while (Sources.Num() > 0)
	{
		ToRemove.Reset();

		for (int32 Idx = 0; Idx < Sources.Num(); ++Idx)
		{
			if (Sources[Idx]->RequestSourceShutdown())
			{
				ToRemove.Add(Idx);
			}
		}

		for (int32 Idx = ToRemove.Num() - 1; Idx >= 0; --Idx)
		{
			Sources.RemoveAtSwap(ToRemove[Idx],1,false);
		}
	}
}

void FLiveLinkClient::Tick(float DeltaTime)
{
	if (LastValidationCheck < FPlatformTime::Seconds() - VALIDATE_SOURCES_TIME)
	{
		ValidateSources();
	}

	BuildThisTicksSubjectSnapshot();
}

void FLiveLinkClient::ValidateSources()
{
	bool bSourcesChanged = false;
	for (int32 SourceIdx = Sources.Num() - 1; SourceIdx >= 0; --SourceIdx)
	{
		if (!Sources[SourceIdx]->IsSourceStillValid())
		{
			RemoveSourceInternal(SourceIdx);

			bSourcesChanged = true;
		}
	}

	for (int32 SourceIdx = SourcesToRemove.Num()-1; SourceIdx >= 0; --SourceIdx)
	{
		if (SourcesToRemove[SourceIdx]->RequestSourceShutdown())
		{
			SourcesToRemove.RemoveAtSwap(SourceIdx, 1, false);
		}
	}

	LastValidationCheck = FPlatformTime::Seconds();

	if (bSourcesChanged)
	{
		OnLiveLinkSourcesChanged.Broadcast();
	}
}

void FLiveLinkClient::BuildThisTicksSubjectSnapshot()
{
	TArray<FName> OldSubjectSnapshotNames;
	ActiveSubjectSnapshots.GenerateKeyArray(OldSubjectSnapshotNames);
	
	const double CurrentInterpTime = FPlatformTime::Seconds();	// Set this up once, every subject
																// uses the same time

	{
		FScopeLock Lock(&SubjectDataAccessCriticalSection);

		for (TPair<FName, FLiveLinkSubject>& SubjectPair : LiveSubjectData)
		{
			const FName SubjectName = SubjectPair.Key;
			OldSubjectSnapshotNames.RemoveSingleSwap(SubjectName, false);

			FLiveLinkSubject& SourceSubject = SubjectPair.Value;

			FLiveLinkConnectionSettings* SubjectConnectionSettings = GetConnectionSettingsForEntry(SourceSubject.LastModifier);
			if (SubjectConnectionSettings)
			{
				SourceSubject.CachedConnectionSettings = *SubjectConnectionSettings;
			}

			if (SourceSubject.Frames.Num() > 0)
			{
				FLiveLinkSubjectFrame& SnapshotSubject = ActiveSubjectSnapshots.FindOrAdd(SubjectName);

				SourceSubject.BuildInterpolatedFrame(CurrentInterpTime, SnapshotSubject);
			}
		}
	}

	for (FName SubjectName : OldSubjectSnapshotNames)
	{
		ActiveSubjectSnapshots.Remove(SubjectName);
	}
}

void FLiveLinkClient::AddSource(TSharedPtr<ILiveLinkSource> InSource)
{
	Sources.Add(InSource);
	SourceGuids.Add(FGuid::NewGuid());
	ConnectionSettings.AddDefaulted();

	InSource->ReceiveClient(this, SourceGuids.Last());
}

void FLiveLinkClient::RemoveSourceInternal(int32 SourceIdx)
{
	Sources.RemoveAtSwap(SourceIdx, 1, false);
	SourceGuids.RemoveAtSwap(SourceIdx, 1, false);
	ConnectionSettings.RemoveAtSwap(SourceIdx, 1, false);
}

void FLiveLinkClient::RemoveSource(FGuid InEntryGuid)
{
	LastValidationCheck = 0.0; //Force validation check next frame
	int32 SourceIdx = GetSourceIndexForGUID(InEntryGuid);
	if (SourceIdx != INDEX_NONE)
	{
		SourcesToRemove.Add(Sources[SourceIdx]);
		RemoveSourceInternal(SourceIdx);
		OnLiveLinkSourcesChanged.Broadcast();
	}
}

void FLiveLinkClient::RemoveAllSources()
{
	LastValidationCheck = 0.0; //Force validation check next frame
	SourcesToRemove = Sources;
	Sources.Reset();
	SourceGuids.Reset();
	ConnectionSettings.Reset();
	OnLiveLinkSourcesChanged.Broadcast();
}

FLiveLinkTimeCode FLiveLinkClient::MakeTimeCode(double InTime, int32 InFrameNum) const
{
	FLiveLinkTimeCode TC = MakeTimeCodeFromTimeOnly(InTime);
	TC.FrameNum = InFrameNum;
	return TC;
}

FLiveLinkTimeCode FLiveLinkClient::MakeTimeCodeFromTimeOnly(double InTime) const
{
	FLiveLinkTimeCode TC;
	TC.Time = InTime;
	TC.Offset = FPlatformTime::Seconds() - InTime;
	return TC;
}


void FLiveLinkClient::PushSubjectSkeleton(FName SubjectName, const FLiveLinkRefSkeleton& RefSkeleton)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);
	
	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		Subject->Frames.Reset();
		Subject->RefSkeleton = RefSkeleton;
	}
	else
	{
		LiveSubjectData.Emplace(SubjectName, FLiveLinkSubject(RefSkeleton));
	}
}

void FLiveLinkClient::ClearSubject(FName SubjectName)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	LiveSubjectData.Remove(SubjectName);
}

void FLiveLinkClient::PushSubjectData(FGuid SourceGuid, FName SubjectName, const TArray<FTransform>& Transforms, const TArray<FLiveLinkCurveElement>& CurveElements, const FLiveLinkTimeCode& TimeCode)
{
	FScopeLock Lock(&SubjectDataAccessCriticalSection);

	if (FLiveLinkSubject* Subject = LiveSubjectData.Find(SubjectName))
	{
		Subject->AddFrame(Transforms, CurveElements, TimeCode, SourceGuid);
	}
}

const FLiveLinkSubjectFrame* FLiveLinkClient::GetSubjectData(FName SubjectName)
{
	if (FLiveLinkSubjectFrame* Subject = ActiveSubjectSnapshots.Find(SubjectName))
	{
		return Subject;
	}
	return nullptr;
}

int32 FLiveLinkClient::GetSourceIndexForGUID(FGuid InEntryGuid) const
{
	return SourceGuids.IndexOfByKey(InEntryGuid);
}

TSharedPtr<ILiveLinkSource> FLiveLinkClient::GetSourceForGUID(FGuid InEntryGuid) const
{
	int32 Idx = GetSourceIndexForGUID(InEntryGuid);
	return Idx != INDEX_NONE ? Sources[Idx] : nullptr;
}

FText FLiveLinkClient::GetSourceTypeForEntry(FGuid InEntryGuid) const
{
	TSharedPtr<ILiveLinkSource> Source = GetSourceForGUID(InEntryGuid);
	if (Source.IsValid())
	{
		return Source->GetSourceType();
	}
	return FText(NSLOCTEXT("TempLocTextLiveLink","InvalidSourceType", "Invalid Source Type"));
}

FText FLiveLinkClient::GetMachineNameForEntry(FGuid InEntryGuid) const
{
	TSharedPtr<ILiveLinkSource> Source = GetSourceForGUID(InEntryGuid);
	if (Source.IsValid())
	{
		return Source->GetSourceMachineName();
	}
	return FText(NSLOCTEXT("TempLocTextLiveLink","InvalidSourceMachineName", "Invalid Source Machine Name"));
}

FText FLiveLinkClient::GetEntryStatusForEntry(FGuid InEntryGuid) const
{
	TSharedPtr<ILiveLinkSource> Source = GetSourceForGUID(InEntryGuid);
	if (Source.IsValid())
	{
		return Source->GetSourceStatus();
	}
	return FText(NSLOCTEXT("TempLocTextLiveLink","InvalidSourceStatus", "Invalid Source Status"));
}

FLiveLinkConnectionSettings* FLiveLinkClient::GetConnectionSettingsForEntry(FGuid InEntryGuid)
{
	const int32 SourceIndex = GetSourceIndexForGUID(InEntryGuid);
	return (SourceIndex != INDEX_NONE) ? &ConnectionSettings[SourceIndex] : nullptr;
}

FDelegateHandle FLiveLinkClient::RegisterSourcesChangedHandle(const FLiveLinkSourcesChanged::FDelegate& SourcesChanged)
{
	return OnLiveLinkSourcesChanged.Add(SourcesChanged);
}

void FLiveLinkClient::UnregisterSourcesChangedHandle(FDelegateHandle Handle)
{
	OnLiveLinkSourcesChanged.Remove(Handle);
}
