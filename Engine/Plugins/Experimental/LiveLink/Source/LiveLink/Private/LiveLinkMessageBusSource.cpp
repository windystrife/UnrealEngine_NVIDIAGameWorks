// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMessageBusSource.h"
#include "LiveLinkMessages.h"
#include "ILiveLinkClient.h"

#include "MessageEndpointBuilder.h"

void FLiveLinkMessageBusSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;

	MessageEndpoint = FMessageEndpoint::Builder(TEXT("LiveLinkMessageBusSource"))
					  .Handling<FLiveLinkSubjectDataMessage>(this, &FLiveLinkMessageBusSource::HandleSubjectData)
					  .Handling<FLiveLinkSubjectFrameMessage>(this, &FLiveLinkMessageBusSource::HandleSubjectFrame)
					  .Handling<FLiveLinkHeartbeatMessage>(this, &FLiveLinkMessageBusSource::HandleHeartbeat)
					  .Handling<FLiveLinkClearSubject>(this, &FLiveLinkMessageBusSource::HandleClearSubject);


	MessageEndpoint->Send(new FLiveLinkConnectMessage(), ConnectionAddress);
}

const double LL_CONNECTION_TIMEOUT = 15.0;
const double LL_HALF_CONNECTION_TIMEOUT = LL_CONNECTION_TIMEOUT / 2.0;

bool FLiveLinkMessageBusSource::IsSourceStillValid()
{
	const double CurrentTime = FPlatformTime::Seconds();

	if (HeartbeatLastSent > (CurrentTime - LL_HALF_CONNECTION_TIMEOUT) &&
		ConnectionLastActive < (CurrentTime - LL_CONNECTION_TIMEOUT) )
	{
		//We have recently tried to heartbeat and not received anything back
		return false;
	}
	MessageEndpoint->Send(new FLiveLinkHeartbeatMessage(), ConnectionAddress);
	HeartbeatLastSent = CurrentTime;
	
	//Don't know that connection is dead yet
	return true;
}

void FLiveLinkMessageBusSource::HandleHeartbeat(const FLiveLinkHeartbeatMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	ConnectionLastActive = FPlatformTime::Seconds();
}

void FLiveLinkMessageBusSource::HandleClearSubject(const FLiveLinkClearSubject& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	ConnectionLastActive = FPlatformTime::Seconds();
	Client->ClearSubject(Message.SubjectName);
}

bool FLiveLinkMessageBusSource::RequestSourceShutdown()
{
	FMessageEndpoint::SafeRelease(MessageEndpoint);
	return true;
}

void FLiveLinkMessageBusSource::HandleSubjectData(const FLiveLinkSubjectDataMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	ConnectionLastActive = FPlatformTime::Seconds();

	//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("HandleSubjectData %s\n"), *Message.SubjectName);
	/*for (const FString& Name : Message.BoneNames)
	{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\tName: %s\n"), *Name);
	}*/
	/*FScopeLock Lock(&GBoneDataCS);

	if (BoneID != (Message.BoneID - 1))
	{
	UE_LOG(LogTemp, Warning, TEXT("BONE ID SKIP Was On:%i Now:%i"), BoneID, Message.BoneID);
	}
	if (BoneNames.Num() == Message.BoneNames.Num() || BoneNames.Num() == 0)
	{
	BoneNames.Reset();
	for (const FString& Name : Message.BoneNames)
	{
	BoneNames.Add(FName(*Name));
	}
	//BoneTransforms.Reset();
	BoneID = Message.BoneID;
	}
	else
	{
	UE_LOG(LogTemp, Warning, TEXT("INVALID BONE NAMES RECIEVED %i != existing %i"), Message.BoneNames.Num(), BoneNames.Num());
	}*/

	Client->PushSubjectSkeleton(Message.SubjectName, Message.RefSkeleton);
}

void FLiveLinkMessageBusSource::HandleSubjectFrame(const FLiveLinkSubjectFrameMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	ConnectionLastActive = FPlatformTime::Seconds();

	//FPlatformMisc::LowLevelOutputDebugString(TEXT("HandleSubjectFrame\n"));
	/*if (BoneID != Message.BoneID)
	{
	UE_LOG(LogTemp, Warning, TEXT("BONE ID MISMATCH Exp:%i Got:%i"), BoneID, Message.BoneID);
	}*/
	/*for (const FTransform& T : Message.Transforms)
	{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\tTransform: %s\n"), *T.ToString());
	}*/

	FLiveLinkTimeCode TC = Client->MakeTimeCode(Message.Time, Message.FrameNum);
	Client->PushSubjectData(SourceGuid, Message.SubjectName, Message.Transforms, Message.Curves, TC);
}