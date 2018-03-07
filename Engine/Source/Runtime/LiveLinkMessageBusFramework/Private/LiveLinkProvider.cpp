// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkProvider.h"
#include "IMessageContext.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "LiveLinkMessages.h"
#include "LiveLinkTypes.h"

#include "HAL/PlatformProcess.h"

// Subject that the application has told us about
struct FTrackedSubject
{
	// Ref skeleton to go with transform data
	FLiveLinkRefSkeleton RefSkeleton;

	// Bone transform data
	TArray<FTransform> Transforms;

	// Curve data
	TArray<FLiveLinkCurveElement> Curves;

	// Incrementing time (application time) for interpolation purposes
	double Time;

	// Frame number of current data
	int32 FrameNum;
};

// Address that we have had a connection request from
struct FTrackedAddress
{
	FTrackedAddress(FMessageAddress InAddress)
		: Address(InAddress)
		, LastHeartbeatTime(FPlatformTime::Seconds())
	{}

	FMessageAddress Address;
	double			LastHeartbeatTime;
};

struct FLiveLinkProvider : public ILiveLinkProvider
{
private:
	// How long we give connections before we decide they are dead
	static const double CONNECTION_TIMEOUT;

	const FString ProviderName;
	
	const FString MachineName;

	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	// Array of our current connections
	TArray<FTrackedAddress> ConnectedAddresses;

	// Cache of our current subject state
	TMap<FName, FTrackedSubject> Subjects;
	
	//Message bus message handlers
	void HandlePingMessage(const FLiveLinkPingMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleConnectMessage(const FLiveLinkConnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleHeartbeat(const FLiveLinkHeartbeatMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	// End message bus message handlers

	// Validate our current connections
	void ValidateConnections()
	{
		double CutOffTime = FPlatformTime::Seconds() - CONNECTION_TIMEOUT;

		ConnectedAddresses.RemoveAll([=](const FTrackedAddress& Address) { return Address.LastHeartbeatTime < CutOffTime; });
	}

	// Get the cached data for the named subject
	FTrackedSubject& GetTrackedSubject(const FName& SubjectName)
	{
		return Subjects.FindOrAdd(SubjectName);
	}

	// Clear a existing track subject
	void ClearTrackedSubject(const FName& SubjectName)
	{
		Subjects.Remove(SubjectName);
	}

	// Send hierarchy data for named subject to supplied address
	void SendSubjectToAddress(FName SubjectName, const FTrackedSubject& Subject, FMessageAddress Address)
	{
		FLiveLinkSubjectDataMessage* SubjectData = new FLiveLinkSubjectDataMessage;
		SubjectData->RefSkeleton = Subject.RefSkeleton;
		SubjectData->SubjectName = SubjectName;

		MessageEndpoint->Send(SubjectData, Address);
	}

	// Send hierarchy data for named subject to supplied address
	void SendSubjectToAddress(FName SubjectName, FMessageAddress Address)
	{
		SendSubjectToAddress(SubjectName, GetTrackedSubject(SubjectName), Address);
	}

	// Send frame data for named subject to supplied address
	void SendSubjectFrameToAddress(FName SubjectName, const FTrackedSubject& Subject, FMessageAddress Address)
	{
		FLiveLinkSubjectFrameMessage* SubjectFrame = new FLiveLinkSubjectFrameMessage;
		SubjectFrame->Transforms = Subject.Transforms;
		SubjectFrame->SubjectName = SubjectName;
		SubjectFrame->Curves = Subject.Curves;
		SubjectFrame->Time = Subject.Time;
		SubjectFrame->FrameNum = Subject.FrameNum;

		MessageEndpoint->Send(SubjectFrame, Address);
	}

	// Send frame data for named subject to supplied address
	void SendSubjectFrameToAddress(FName SubjectName, FMessageAddress Address)
	{
		SendSubjectFrameToAddress(SubjectName, GetTrackedSubject(SubjectName), Address);
	}

	void SendClearSubjectToAddress(FName SubjectName, FMessageAddress Address)
	{
		FLiveLinkClearSubject* ClearSubject = new FLiveLinkClearSubject(SubjectName);
		MessageEndpoint->Send(ClearSubject, Address);
	}

	// Send hierarchy data for named subject to current connections
	void SendSubjectToConnections(FName SubjectName)
	{
		ValidateConnections();

		for (const FTrackedAddress& Address : ConnectedAddresses)
		{
			SendSubjectToAddress(SubjectName, Address.Address);
		}
	}

	// Send frame data for named subject to current connections
	void SendSubjectFrameToConnections(FName SubjectName)
	{
		ValidateConnections();

		for (const FTrackedAddress& Address : ConnectedAddresses)
		{
			SendSubjectFrameToAddress(SubjectName, Address.Address);
		}
	}

	void SendClearSubjectToConnections(FName SubjectName)
	{
		ValidateConnections();

		for (const FTrackedAddress& Address : ConnectedAddresses)
		{
			SendClearSubjectToAddress(SubjectName, Address.Address);
		}
	}

public:
	FLiveLinkProvider(const FString& InProviderName)
		: ProviderName(InProviderName)
		, MachineName(FPlatformProcess::ComputerName())
	{
		MessageEndpoint = FMessageEndpoint::Builder(*InProviderName)
			.ReceivingOnAnyThread()
			.Handling<FLiveLinkPingMessage>(this, &FLiveLinkProvider::HandlePingMessage)
			.Handling<FLiveLinkConnectMessage>(this, &FLiveLinkProvider::HandleConnectMessage)
			.Handling<FLiveLinkHeartbeatMessage>(this, &FLiveLinkProvider::HandleHeartbeat);

		if (MessageEndpoint.IsValid())
		{
			MessageEndpoint->Subscribe<FLiveLinkPingMessage>();
		}
	}

	virtual void UpdateSubject(const FName& SubjectName, const TArray<FName>& BoneNames, const TArray<int32>& BoneParents)
	{
		FTrackedSubject& Subject = GetTrackedSubject(SubjectName);
		Subject.RefSkeleton.SetBoneNames(BoneNames);
		Subject.RefSkeleton.SetBoneParents(BoneParents);
		Subject.Transforms.Empty();
		
		SendSubjectToConnections(SubjectName);
	}

	virtual void ClearSubject(const FName& SubjectName)
	{
		ClearTrackedSubject(SubjectName);
		SendClearSubjectToConnections(SubjectName);
	}

	virtual void UpdateSubjectFrame(const FName& SubjectName, const TArray<FTransform>& BoneTransforms, const TArray<FLiveLinkCurveElement>& CurveData, double Time, int32 FrameNum)
	{
		FTrackedSubject& Subject = GetTrackedSubject(SubjectName);

		Subject.Transforms = BoneTransforms;
		Subject.Curves = CurveData;
		Subject.Time = Time;
		Subject.FrameNum = FrameNum;

		SendSubjectFrameToConnections(SubjectName);
	}

	virtual ~FLiveLinkProvider()
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Destroyed"));
	}
};

const double FLiveLinkProvider::CONNECTION_TIMEOUT = 10.f;

void FLiveLinkProvider::HandlePingMessage(const FLiveLinkPingMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	MessageEndpoint->Send(new FLiveLinkPongMessage(ProviderName, MachineName, Message.PollRequest), Context->GetSender());
}

void FLiveLinkProvider::HandleConnectMessage(const FLiveLinkConnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	const FMessageAddress& ConnectionAddress = Context->GetSender();

	if (!ConnectedAddresses.ContainsByPredicate([=](const FTrackedAddress& Address) { return Address.Address == ConnectionAddress; }))
	{
		ConnectedAddresses.Add(FTrackedAddress(ConnectionAddress));
		for (const auto& Subject : Subjects)
		{
			SendSubjectToAddress(Subject.Key, Subject.Value, ConnectionAddress);
			FPlatformProcess::Sleep(0.1); //HACK: Try to help these go in order, editor needs extra buffering support to make sure this isn't needed in future.
			SendSubjectFrameToAddress(Subject.Key, Subject.Value, ConnectionAddress);
		}
	}
}

void FLiveLinkProvider::HandleHeartbeat(const FLiveLinkHeartbeatMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FTrackedAddress* TrackedAddress = ConnectedAddresses.FindByPredicate([=](const FTrackedAddress& ConAddress) { return ConAddress.Address == Context->GetSender(); });
	if (TrackedAddress)
	{
		TrackedAddress->LastHeartbeatTime = FPlatformTime::Seconds();

		// Respond so editor gets heartbeat too
		MessageEndpoint->Send(new FLiveLinkHeartbeatMessage(), Context->GetSender());
	}
}

TSharedPtr<ILiveLinkProvider> ILiveLinkProvider::CreateLiveLinkProvider(const FString& ProviderName)
{
	return MakeShareable(new FLiveLinkProvider(ProviderName));
}