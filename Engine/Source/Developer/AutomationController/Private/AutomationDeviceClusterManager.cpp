// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationDeviceClusterManager.h"
#include "IAutomationControllerManager.h"

void FAutomationDeviceClusterManager::Reset()
{
	Clusters.Empty();
}


void FAutomationDeviceClusterManager::AddDeviceFromMessage(const FMessageAddress& MessageAddress, const FAutomationWorkerFindWorkersResponse& Message, const uint32 GroupFlags)
{
	int32 TestClusterIndex;
	int32 TestDeviceIndex;
	//if we don't already know about this device
	if (!FindDevice(MessageAddress, TestClusterIndex, TestDeviceIndex))
	{
		FDeviceState NewDevice(MessageAddress, Message);
		FString GroupName = GetGroupNameForDevice(NewDevice, GroupFlags);
		//ensure the proper cluster exists
		int32 ClusterIndex = 0;
		for (; ClusterIndex < Clusters.Num(); ++ClusterIndex)
		{
			if (Clusters[ClusterIndex].ClusterName == GroupName)
			{
				//found the cluster, now just append the device
				Clusters[ClusterIndex].Devices.Add(NewDevice);
				break;
			}
		}
		// if we didn't find the device type yet, add a new cluster and add this device
		if (ClusterIndex == Clusters.Num())
		{
			FDeviceCluster NewCluster;
			NewCluster.ClusterName = GroupName;
			NewCluster.DeviceTypeName = Message.Platform;
			NewCluster.Devices.Add(NewDevice);
			Clusters.Add(NewCluster);
		}
	}
}


void FAutomationDeviceClusterManager::Remove(const FMessageAddress& MessageAddress)
{
	for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
	{
		for (int32 DeviceIndex = Clusters[ClusterIndex].Devices.Num()-1; DeviceIndex >= 0; --DeviceIndex)
		{
			if (MessageAddress == Clusters[ClusterIndex].Devices[DeviceIndex].DeviceMessageAddress)
			{
				Clusters[ClusterIndex].Devices.RemoveAt(DeviceIndex);
			}
		}
	}
}


FString FAutomationDeviceClusterManager::GetGroupNameForDevice(const FDeviceState& DeviceState, const uint32 DeviceGroupFlags)
{
	FString OutGroupName;

	if( (DeviceGroupFlags & (1 << EAutomationDeviceGroupTypes::MachineName)) > 0 )
	{
		OutGroupName += DeviceState.DeviceName + TEXT("-");
	}

	if( (DeviceGroupFlags & (1 << EAutomationDeviceGroupTypes::Platform)) > 0 )
	{
		OutGroupName += DeviceState.PlatformName + TEXT("-");
	}

	if( (DeviceGroupFlags & (1 << EAutomationDeviceGroupTypes::OSVersion)) > 0 )
	{
		OutGroupName += DeviceState.OSVersionName + TEXT("-");
	}

	if( (DeviceGroupFlags & (1 << EAutomationDeviceGroupTypes::Model)) > 0 )
	{
		OutGroupName += DeviceState.ModelName + TEXT("-");
	}

	if( (DeviceGroupFlags & (1 << EAutomationDeviceGroupTypes::GPU)) > 0 )
	{
		OutGroupName += DeviceState.GPUName + TEXT("-");
	}

	if( (DeviceGroupFlags & (1 << EAutomationDeviceGroupTypes::CPUModel)) > 0 )
	{
		OutGroupName += DeviceState.CPUModelName + TEXT("-");
	}

	if( (DeviceGroupFlags & (1 << EAutomationDeviceGroupTypes::RamInGB)) > 0 )
	{
		OutGroupName += FString::Printf(TEXT("%uGB Ram-"),DeviceState.RAMInGB);
	}

	if( (DeviceGroupFlags & (1 << EAutomationDeviceGroupTypes::RenderMode)) > 0 )
	{
		OutGroupName += DeviceState.RenderModeName + TEXT("-");
	}

	if( OutGroupName.Len() > 0 )
	{
		//Get rid of the trailing '-'
		OutGroupName = OutGroupName.LeftChop(1);
	}

	return OutGroupName;
}


void FAutomationDeviceClusterManager::ReGroupDevices( const uint32 GroupFlags )
{
	//Get all the devices
	TArray< FDeviceState > AllDevices;
	for(int32 i=0; i<GetNumClusters(); ++i)
	{
		AllDevices += Clusters[i].Devices;
	}

	//Clear out the clusters
	Reset();

	//Generate new group names based off the active flags and readd the devices
	for(int32 i=0; i<AllDevices.Num(); ++i)
	{
		const FDeviceState* DeviceIt = &AllDevices[i];
		FString GroupName = GetGroupNameForDevice(*DeviceIt, GroupFlags);
		//ensure the proper cluster exists
		int32 ClusterIndex = 0;
		for (; ClusterIndex < Clusters.Num(); ++ClusterIndex)
		{
			if (Clusters[ClusterIndex].ClusterName == GroupName)
			{
				//found the cluster, now just append the device
				Clusters[ClusterIndex].Devices.Add(*DeviceIt);
				break;
			}
		}
		// if we didn't find the device type yet, add a new cluster and add this device
		if (ClusterIndex == Clusters.Num())
		{
			FDeviceCluster NewCluster;
			NewCluster.ClusterName = GroupName;
			NewCluster.DeviceTypeName = DeviceIt->PlatformName;
			NewCluster.Devices.Add(*DeviceIt);
			Clusters.Add(NewCluster);
		}
	}
}


int32 FAutomationDeviceClusterManager::GetNumClusters() const
{
	return Clusters.Num();
}


int32 FAutomationDeviceClusterManager::GetTotalNumDevices() const
{
	int Total = 0;
	for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
	{
		Total += Clusters[ClusterIndex].Devices.Num();
	}
	return Total;
}


int32 FAutomationDeviceClusterManager::GetNumDevicesInCluster(const int32 ClusterIndex) const
{
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	return Clusters[ClusterIndex].Devices.Num();
}


int32 FAutomationDeviceClusterManager::GetNumActiveDevicesInCluster(const int32 ClusterIndex) const
{
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	int32 ActiveDevices = 0;
	for ( int32 Index = 0; Index < Clusters[ ClusterIndex ].Devices.Num(); Index++ )
	{
		if ( Clusters[ ClusterIndex ].Devices[ Index ].IsDeviceAvailable )
		{
			ActiveDevices++;
		}
	}
	return ActiveDevices;
}


FString FAutomationDeviceClusterManager::GetClusterGroupName(const int32 ClusterIndex) const
{
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	return Clusters[ClusterIndex].ClusterName;
}


FString FAutomationDeviceClusterManager::GetClusterDeviceType(const int32 ClusterIndex) const
{
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	return Clusters[ClusterIndex].DeviceTypeName;
}


FString FAutomationDeviceClusterManager::GetClusterDeviceName(const int32 ClusterIndex, const int32 DeviceIndex) const
{
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	check((DeviceIndex >= 0) && (DeviceIndex < Clusters[ClusterIndex].Devices.Num()));
	return Clusters[ClusterIndex].Devices[DeviceIndex].GameInstanceName;
}


bool FAutomationDeviceClusterManager::FindDevice(const FMessageAddress& MessageAddress, int32& OutClusterIndex, int32& OutDeviceIndex)
{
	OutClusterIndex = INDEX_NONE;
	OutDeviceIndex = INDEX_NONE;
	for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
	{
		for (int32 DeviceIndex = 0; DeviceIndex < Clusters[ClusterIndex].Devices.Num(); ++DeviceIndex)
		{
			//if network addresses match
			if (MessageAddress == Clusters[ClusterIndex].Devices[DeviceIndex].DeviceMessageAddress)
			{
				OutClusterIndex = ClusterIndex;
				OutDeviceIndex = DeviceIndex;
				return true;
			}
		}
	}
	return false;
}


FMessageAddress FAutomationDeviceClusterManager::GetDeviceMessageAddress(const int32 ClusterIndex, const int32 DeviceIndex) const
{
	//verify cluster/device index
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	check((DeviceIndex >= 0) && (DeviceIndex < Clusters[ClusterIndex].Devices.Num()));
	return Clusters[ClusterIndex].Devices[DeviceIndex].DeviceMessageAddress;
}


TArray<FMessageAddress> FAutomationDeviceClusterManager::GetDevicesReservedForTest(const int32 ClusterIndex, TSharedPtr <IAutomationReport> Report)
{
	TArray<FMessageAddress> DeviceAddresses;

	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	for (int32 DeviceIndex = 0; DeviceIndex < Clusters[ClusterIndex].Devices.Num(); ++DeviceIndex)
	{
		if (Clusters[ClusterIndex].Devices[DeviceIndex].Report == Report)
		{
			DeviceAddresses.Add(Clusters[ClusterIndex].Devices[DeviceIndex].DeviceMessageAddress);
		}
	}
	return DeviceAddresses;
}


TSharedPtr <IAutomationReport> FAutomationDeviceClusterManager::GetTest(const int32 ClusterIndex, const int32 DeviceIndex) const
{
	//verify cluster/device index
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	check((DeviceIndex >= 0) && (DeviceIndex < Clusters[ClusterIndex].Devices.Num()));
	return Clusters[ClusterIndex].Devices[DeviceIndex].Report;
}


void FAutomationDeviceClusterManager::SetTest(const int32 ClusterIndex, const int32 DeviceIndex, TSharedPtr <IAutomationReport> NewReport)
{
	//verify cluster/device index
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	check((DeviceIndex >= 0) && (DeviceIndex < Clusters[ClusterIndex].Devices.Num()));
	Clusters[ClusterIndex].Devices[DeviceIndex].Report = NewReport;
}


void FAutomationDeviceClusterManager::ResetAllDevicesRunningTest( const int32 ClusterIndex, IAutomationReportPtr InTest )
{	
	TArray<FMessageAddress> DeviceAddresses;

	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	for (int32 DeviceIndex = 0; DeviceIndex < Clusters[ClusterIndex].Devices.Num(); ++DeviceIndex)
	{
		if( Clusters[ClusterIndex].Devices[DeviceIndex].Report == InTest )
		{
			Clusters[ClusterIndex].Devices[DeviceIndex].Report = NULL;
		}
	}
}


void FAutomationDeviceClusterManager::DisableDevice( const int32 ClusterIndex, const int32 DeviceIndex )
{
	//verify cluster/device index
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	check((DeviceIndex >= 0) && (DeviceIndex < Clusters[ClusterIndex].Devices.Num()));
	Clusters[ClusterIndex].Devices[DeviceIndex].IsDeviceAvailable = false;
}


bool FAutomationDeviceClusterManager::DeviceEnabled( const int32 ClusterIndex, const int32 DeviceIndex )
{
	//verify cluster/device index
	check((ClusterIndex >= 0) && (ClusterIndex < Clusters.Num()));
	check((DeviceIndex >= 0) && (DeviceIndex < Clusters[ClusterIndex].Devices.Num()));
	return Clusters[ClusterIndex].Devices[DeviceIndex].IsDeviceAvailable;
}


bool FAutomationDeviceClusterManager::HasActiveDevice()
{
	bool IsDeviceAvailable = false;
	for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
	{
		for (int32 DeviceIndex = 0; DeviceIndex < Clusters[ClusterIndex].Devices.Num(); ++DeviceIndex)
		{
			//if network addresses match
			if ( Clusters[ClusterIndex].Devices[DeviceIndex].IsDeviceAvailable )
			{
				IsDeviceAvailable = true;
				break;
			}
		}
	}
	return IsDeviceAvailable;
}
