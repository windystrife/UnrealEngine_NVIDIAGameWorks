/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace iPhonePackager
{
    [Serializable]
    public struct ConnectedDeviceInfo
    {
        public string DeviceName;
        public string UDID;
        public string DeviceType;

        public ConnectedDeviceInfo(string DeviceName, string UDID, string DeviceType)
        {
            this.DeviceName = DeviceName;
            this.UDID = UDID;
            this.DeviceType = DeviceType;
        }
    }

    public interface DeployTimeReportingInterface
    {
        void Log(string Line);
        void Warning(string Line);
        void Error(string Line);
        void SetProgressIndex(int Progress);
        int GetTransferProgressDivider();
    }

    public interface DeploymentInterface
    {
		string DeviceId { get; set; }
        void SetReportingInterface(DeployTimeReportingInterface InReporter);
        bool UninstallIPAOnDevice(string ApplicationIdentifier);
        ConnectedDeviceInfo [] EnumerateConnectedDevices();
        bool InstallIPAOnDevice(string IPAPath);
		bool InstallFilesOnDevice(string BundleIdentifier, string ManifestFile);
		bool BackupDocumentsDirectory(string BundleIdentifier, string DestinationDocumentsDirectory);
		bool BackupFiles(string BundleIdentifier, string[] Files);
		bool ListApplications();
	}
}
