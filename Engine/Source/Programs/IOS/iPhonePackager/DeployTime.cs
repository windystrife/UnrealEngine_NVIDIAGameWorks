/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;
using System.Linq;
using System.Runtime.Remoting.Channels.Ipc;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting;

namespace iPhonePackager
{
	// Implementation of DeploymentServer -> Application interface
	[Serializable]
	class DeployTimeReporter : DeployTimeReportingInterface
	{
		public void Log(string Line)
		{
			Program.Log(Line);
		}

		public void Error(string Line)
		{
			Program.Error(Line);
		}

		public void Warning(string Line)
		{
			Program.Warning(Line);
		}

		public void SetProgressIndex(int Progress)
		{
			Program.ProgressIndex = Progress;
		}

		public int GetTransferProgressDivider()
		{
			return (Program.BGWorker != null) ? 1000 : 25;
		}
	}

	class DeploymentHelper
	{
		static DeploymentInterface DeployTimeInstance;

		public static Process DeploymentServerProcess = null;

		public static void InstallIPAOnConnectedDevices(string IPAPath)
		{
			// Read the mobile provision to check for issues
			FileOperations.ReadOnlyZipFileSystem Zip = new FileOperations.ReadOnlyZipFileSystem(IPAPath);
			MobileProvision Provision = null;
			try
			{
				MobileProvisionParser.ParseFile(Zip.ReadAllBytes("embedded.mobileprovision"));
			}
			catch (System.Exception ex)
			{
				Program.Warning(String.Format("Couldn't find an embedded mobile provision ({0})", ex.Message));
				Provision = null;
			}
			Zip.Close();

			if (Provision != null)
			{
				var DeviceList = DeploymentHelper.Get().EnumerateConnectedDevices();

				foreach (var DeviceInfo in DeviceList)
				{
					string UDID = DeviceInfo.UDID;
					string DeviceName = DeviceInfo.DeviceName;

					// Check the IPA's mobile provision against the connected device to make sure this device is authorized
					// We'll still try installing anyways, but this message is more friendly than the failure we get back from MobileDeviceInterface
					if (UDID != String.Empty)
					{
						if (!Provision.ContainsUDID(UDID))
						{
							Program.Warning(String.Format("Embedded provision in IPA does not include the UDID {0} of device '{1}'.  The installation is likely to fail.", UDID, DeviceName));
						}
					}
					else
					{
						Program.Warning(String.Format("Unable to query device for UDID, and therefore unable to verify IPA embedded mobile provision contains this device."));
					}
				}
			}

			DeploymentHelper.Get().InstallIPAOnDevice(IPAPath);
		}

		public static bool ExecuteDeployCommand(string Command, string GamePath, string RPCCommand)
		{
			switch (Command.ToLowerInvariant())
			{
				case "backup":
					{
						string ApplicationIdentifier = RPCCommand;
						if (ApplicationIdentifier == null)
						{
							ApplicationIdentifier = Utilities.GetStringFromPList("CFBundleIdentifier");
						}

						if (Config.FilesForBackup.Count > 0)
						{
							if (!DeploymentHelper.Get().BackupFiles(ApplicationIdentifier, Config.FilesForBackup.ToArray()))
							{
								Program.Error("Failed to transfer manifest file from device to PC");
								Program.ReturnCode = (int)ErrorCodes.Error_DeviceBackupFailed;
							}
						}
						else if (!DeploymentHelper.Get().BackupDocumentsDirectory(ApplicationIdentifier, Config.GetRootBackedUpDocumentsDirectory()))
						{
							Program.Error("Failed to transfer documents directory from device to PC");
							Program.ReturnCode = (int)ErrorCodes.Error_DeviceBackupFailed;
						}
					}
					break;
				case "uninstall":
					{
						string ApplicationIdentifier = RPCCommand;
						if (ApplicationIdentifier == null)
						{
							ApplicationIdentifier = Utilities.GetStringFromPList("CFBundleIdentifier");
						}

						if (!DeploymentHelper.Get().UninstallIPAOnDevice(ApplicationIdentifier))
						{
							Program.Error("Failed to uninstall IPA on device");
							Program.ReturnCode = (int)ErrorCodes.Error_AppUninstallFailed;
						}
					}
					break;
				case "deploy":
				case "install":
					{
						string IPAPath = GamePath;
						string AdditionalCommandline = Program.AdditionalCommandline;

						if (!String.IsNullOrEmpty(AdditionalCommandline) && !Config.bIterate)
						{
							// Read the mobile provision to check for issues
							FileOperations.ReadOnlyZipFileSystem Zip = new FileOperations.ReadOnlyZipFileSystem(IPAPath);
							try
							{
								// Compare the commandline embedded to prevent us from any unnecessary writing.
								byte[] CommandlineBytes = Zip.ReadAllBytes("ue4commandline.txt");
								string ExistingCommandline = Encoding.UTF8.GetString(CommandlineBytes, 0, CommandlineBytes.Length);
								if (ExistingCommandline != AdditionalCommandline)
								{
									// Ensure we have a temp dir to stage our temporary ipa
									if( !Directory.Exists( Config.PCStagingRootDir ) )
									{
										Directory.CreateDirectory(Config.PCStagingRootDir);
									}

									string TmpFilePath = Path.Combine(Path.GetDirectoryName(Config.PCStagingRootDir), Path.GetFileNameWithoutExtension(IPAPath) + ".tmp.ipa");
									if( File.Exists( TmpFilePath ) )
									{
										File.Delete(TmpFilePath);
									}

									File.Copy(IPAPath, TmpFilePath);
								
									// Get the project name:
									string ProjectFile = ExistingCommandline.Split(' ').FirstOrDefault();

									// Write out the new commandline.
									FileOperations.ZipFileSystem WritableZip = new FileOperations.ZipFileSystem(TmpFilePath);
									byte[] NewCommandline = Encoding.UTF8.GetBytes(ProjectFile + " " + AdditionalCommandline);
									WritableZip.WriteAllBytes("ue4commandline.txt", NewCommandline);

									// We need to residn the application after the commandline file has changed.
									CodeSignatureBuilder CodeSigner = new CodeSignatureBuilder();
									CodeSigner.FileSystem = WritableZip;

									CodeSigner.PrepareForSigning();
									CodeSigner.PerformSigning();

									WritableZip.Close();

									// Set the deploying ipa path to our new ipa
									IPAPath = TmpFilePath;
								}
							}
							catch (System.Exception ex)
							{
								Program.Warning(String.Format("Failed to override the commandline.txt file: ({0})", ex.Message));
							}
							Zip.Close();
						}

						if (Config.bIterate)
						{
							string ApplicationIdentifier = RPCCommand;
							if (String.IsNullOrEmpty(ApplicationIdentifier))
							{
								ApplicationIdentifier = Utilities.GetStringFromPList("CFBundleIdentifier");
							}

							DeploymentHelper.Get().DeviceId = Config.DeviceId;
							if (!DeploymentHelper.Get().InstallFilesOnDevice(ApplicationIdentifier, Config.DeltaManifest))
							{
								Program.Error("Failed to install Files on device");
								Program.ReturnCode = (int)ErrorCodes.Error_FilesInstallFailed;
							}
						}
						else if (File.Exists(IPAPath))
						{
							DeploymentHelper.Get().DeviceId = Config.DeviceId;
							if (!DeploymentHelper.Get().InstallIPAOnDevice(IPAPath))
							{
								Program.Error("Failed to install IPA on device");
								Program.ReturnCode = (int)ErrorCodes.Error_AppInstallFailed;
							}
						}
						else
						{
							Program.Error(String.Format("Failed to find IPA file: '{0}'", IPAPath));
							Program.ReturnCode = (int)ErrorCodes.Error_AppNotFound;
						}
					}
					break;

				default:
					return false;
			}

			return true;
		}

		static DeployTimeReporter Reporter = new DeployTimeReporter();

		public static DeploymentInterface Get()
		{
			if (DeployTimeInstance == null)
			{
				if (DeploymentServerProcess == null)
				{
					DeploymentServerProcess = CreateDeploymentServerProcess();
				}

				DeployTimeInstance = (DeploymentInterface)Activator.GetObject(
				  typeof(DeploymentInterface),
				  @"ipc://iPhonePackager/DeploymentServer_PID" + Process.GetCurrentProcess().Id.ToString());
			}

			if (DeployTimeInstance == null)
			{
				Program.Error("Failed to connect to deployment server");
				throw new Exception("Failed to connect to deployment server");
			}

			DeployTimeInstance.SetReportingInterface(Reporter);

			return DeployTimeInstance;
		}

		static Process CreateDeploymentServerProcess()
		{
			Process NewProcess = new Process();
			if (Environment.OSVersion.Platform == PlatformID.MacOSX || Environment.OSVersion.Platform == PlatformID.Unix)
			{
				NewProcess.StartInfo.WorkingDirectory = Path.GetFullPath(".");
				NewProcess.StartInfo.FileName = "../../../Build/BatchFiles/Mac/RunMono.sh";
				NewProcess.StartInfo.Arguments = "\"" + NewProcess.StartInfo.WorkingDirectory + "/DeploymentServer.exe\" -iphonepackager " + Process.GetCurrentProcess().Id.ToString();
			}
			else
			{
				NewProcess.StartInfo.WorkingDirectory = Path.GetFullPath(".");
				NewProcess.StartInfo.FileName = NewProcess.StartInfo.WorkingDirectory + "\\DeploymentServer.exe";
				NewProcess.StartInfo.Arguments = "-iphonepackager " + Process.GetCurrentProcess().Id.ToString();
			}
			NewProcess.StartInfo.UseShellExecute = false;

			try
			{
				NewProcess.Start();
				System.Threading.Thread.Sleep(500);
			}
			catch (System.Exception ex)
			{
				Program.Error("Failed to create deployment server process ({0})", ex.Message);
			}

			return NewProcess;
		}
	}
}
