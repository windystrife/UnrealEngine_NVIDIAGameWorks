// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TargetDeviceProxy.h"

#include "FileMessageAttachment.h"
#include "HAL/PlatformProcess.h"
#include "MessageEndpointBuilder.h"

#include "TargetDeviceServiceMessages.h"


/* FTargetDeviceProxy structors
 *****************************************************************************/

FTargetDeviceProxy::FTargetDeviceProxy(const FString& InName)
	: Connected(false)
	, LastUpdateTime(0)
	, Name(InName)
	, SupportsMultiLaunch(false)
	, SupportsPowerOff(false)
	, SupportsPowerOn(false)
	, SupportsReboot(false)
{

	InitializeMessaging();
}


FTargetDeviceProxy::FTargetDeviceProxy(const FString& InName, const FTargetDeviceServicePong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
	: Connected(false)
	, Name(InName)
	, SupportsMultiLaunch(false)
	, SupportsPowerOff(false)
	, SupportsPowerOn(false)
	, SupportsReboot(false)
{
	UpdateFromMessage(Message, Context);
	InitializeMessaging();
}


/* FTargetDeviceProxy interface
*****************************************************************************/

void FTargetDeviceProxy::UpdateFromMessage( const FTargetDeviceServicePong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	if (Message.Name != Name)
	{
		return;
	}

	MessageAddress = Context->GetSender();

	Connected = Message.Connected;
	HostName = Message.HostName;
	HostUser = Message.HostUser;
	Make = Message.Make;
	Model = Message.Model;
	Name = Message.Name;
	DeviceUser = Message.DeviceUser;
	DeviceUserPassword = Message.DeviceUserPassword;
	Shared = Message.Shared;
	SupportsMultiLaunch = Message.SupportsMultiLaunch;
	SupportsPowerOff = Message.SupportsPowerOff;
	SupportsPowerOn = Message.SupportsPowerOn;
	SupportsReboot = Message.SupportsReboot;
	SupportsVariants = Message.SupportsVariants;
	DefaultVariant = Message.DefaultVariant;

	// Update the map of flavors.
	for (int Index = 0; Index < Message.Variants.Num(); Index++)
	{
		const FTargetDeviceVariant& MsgVariant = Message.Variants[Index];

		FTargetDeviceProxyVariant & Variant = TargetDeviceVariants.Add(MsgVariant.VariantName);
		Variant.DeviceID = MsgVariant.DeviceID;
		Variant.VariantName = MsgVariant.VariantName;
		Variant.TargetPlatformName = MsgVariant.TargetPlatformName;
		Variant.TargetPlatformId = MsgVariant.TargetPlatformId;
		Variant.VanillaPlatformId = MsgVariant.VanillaPlatformId;
		Variant.PlatformDisplayName = FText::FromString(MsgVariant.PlatformDisplayName);
	}

	LastUpdateTime = FDateTime::UtcNow();
}


/* ITargetDeviceProxy interface
 *****************************************************************************/

int32 FTargetDeviceProxy::GetNumVariants() const
{
	return TargetDeviceVariants.Num();
}


int32 FTargetDeviceProxy::GetVariants(TArray<FName>& OutVariants) const
{
	return TargetDeviceVariants.GetKeys(OutVariants);
}


FName FTargetDeviceProxy::GetTargetDeviceVariant(const FString& InDeviceId) const
{
	for (TMap<FName, FTargetDeviceProxyVariant>::TConstIterator ItVariant(TargetDeviceVariants); ItVariant; ++ItVariant)
	{
		const FTargetDeviceProxyVariant& Variant = ItVariant.Value();

		if (Variant.DeviceID == InDeviceId)
		{
			return ItVariant.Key();
		}
	}

	return NAME_None;
}


const FString& FTargetDeviceProxy::GetTargetDeviceId(FName InVariant) const
{
	if (InVariant == NAME_None)
	{
		return TargetDeviceVariants[DefaultVariant].DeviceID;
	}

	return TargetDeviceVariants[InVariant].DeviceID;
}


FString FTargetDeviceProxy::GetTargetPlatformName(FName InVariant) const
{
	if (InVariant == NAME_None)
	{
		return TargetDeviceVariants[DefaultVariant].TargetPlatformName;
	}

	return TargetDeviceVariants[InVariant].TargetPlatformName;
}


FName FTargetDeviceProxy::GetTargetPlatformId(FName InVariant) const
{
	if (InVariant == NAME_None)
	{
		return TargetDeviceVariants[DefaultVariant].TargetPlatformId;
	}

	return TargetDeviceVariants[InVariant].TargetPlatformId;
}


FName FTargetDeviceProxy::GetVanillaPlatformId(FName InVariant) const
{
	if (InVariant == NAME_None)
	{
		return TargetDeviceVariants[DefaultVariant].VanillaPlatformId;
	}

	return TargetDeviceVariants[InVariant].VanillaPlatformId;
}


FText FTargetDeviceProxy::GetPlatformDisplayName(FName InVariant) const
{
	if (InVariant == NAME_None)
	{
		return TargetDeviceVariants[DefaultVariant].PlatformDisplayName;
	}

	return TargetDeviceVariants[InVariant].PlatformDisplayName;
}


bool FTargetDeviceProxy::HasDeviceId(const FString& InDeviceId) const
{
	for (TMap<FName, FTargetDeviceProxyVariant>::TConstIterator ItVariant(TargetDeviceVariants); ItVariant; ++ItVariant)
	{
		const FTargetDeviceProxyVariant& Variant = ItVariant.Value();

		if (Variant.DeviceID == InDeviceId)
		{
			return true;
		}
	}

	return false;
}


bool FTargetDeviceProxy::HasTargetPlatform(FName InTargetPlatformId) const
{
	for (TMap<FName, FTargetDeviceProxyVariant>::TConstIterator ItVariant(TargetDeviceVariants); ItVariant; ++ItVariant)
	{
		const FTargetDeviceProxyVariant& Variant = ItVariant.Value();

		if (Variant.TargetPlatformId == InTargetPlatformId)
		{
			return true;
		}
	}

	return false;
}


bool FTargetDeviceProxy::HasVariant(FName InVariant) const
{
	if (InVariant == NAME_None)
	{
		return TargetDeviceVariants.Contains(DefaultVariant);
	}

	return TargetDeviceVariants.Contains(InVariant);
}


bool FTargetDeviceProxy::DeployApp(FName InVariant, const TMap<FString, FString>& Files, const FGuid& TransactionId)
{
	for (TMap<FString, FString>::TConstIterator It(Files); It; ++It)
	{
		TSharedRef<IMessageAttachment, ESPMode::ThreadSafe> FileAttachment = MakeShareable(new FFileMessageAttachment(It.Key()));
		FString SourcePath = It.Key();

		MessageEndpoint->Send(new FTargetDeviceServiceDeployFile(It.Value(), TransactionId), FileAttachment, MessageAddress);
	}

	MessageEndpoint->Send(new FTargetDeviceServiceDeployCommit(InVariant, TransactionId), MessageAddress);

	return true;
}


bool FTargetDeviceProxy::LaunchApp(FName InVariant, const FString& AppId, EBuildConfigurations::Type BuildConfiguration, const FString& Params)
{
	MessageEndpoint->Send(new FTargetDeviceServiceLaunchApp(InVariant, AppId, BuildConfiguration, Params), MessageAddress);

	return true;
}


void FTargetDeviceProxy::PowerOff(bool Force)
{
	MessageEndpoint->Send(new FTargetDeviceServicePowerOff(FPlatformProcess::UserName(false), Force), MessageAddress);
}


void FTargetDeviceProxy::PowerOn()
{
	MessageEndpoint->Send(new FTargetDeviceServicePowerOn(FPlatformProcess::UserName(false)), MessageAddress);
}


void FTargetDeviceProxy::Reboot()
{
	MessageEndpoint->Send(new FTargetDeviceServiceReboot(FPlatformProcess::UserName(false)), MessageAddress);
}


void FTargetDeviceProxy::Run(FName InVariant, const FString& ExecutablePath, const FString& Params)
{
	MessageEndpoint->Send(new FTargetDeviceServiceRunExecutable(InVariant, ExecutablePath, Params), MessageAddress);
}


/* FTargetDeviceProxy implementation
 *****************************************************************************/

void FTargetDeviceProxy::InitializeMessaging()
{
	MessageEndpoint = FMessageEndpoint::Builder(FName(*FString::Printf(TEXT("FTargetDeviceProxy (%s)"), *Name)))
		.Handling<FTargetDeviceServiceDeployFinished>(this, &FTargetDeviceProxy::HandleDeployFinishedMessage)
		.Handling<FTargetDeviceServiceLaunchFinished>(this, &FTargetDeviceProxy::HandleLaunchFinishedMessage);
}

/* FTargetDeviceProxy event handlers
*****************************************************************************/

void FTargetDeviceProxy::HandleDeployFinishedMessage(const FTargetDeviceServiceDeployFinished& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Message.Succeeded)
	{
		DeployCommittedDelegate.Broadcast(Message.TransactionId, Message.AppID);
	}
	else
	{
		DeployFailedDelegate.Broadcast(Message.TransactionId);
	}
}


void FTargetDeviceProxy::HandleLaunchFinishedMessage(const FTargetDeviceServiceLaunchFinished& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (Message.Succeeded)
	{
		LaunchSucceededDelegate.Broadcast(Message.AppID, Message.ProcessId);
	}
	else
	{
		LaunchFailedDelegate.Broadcast(Message.AppID);
	}
}
