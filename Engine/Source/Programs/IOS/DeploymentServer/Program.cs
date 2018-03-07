/**
 * Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Runtime.Remoting;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting.Channels.Ipc;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.IO;
using Microsoft.Win32;
using iPhonePackager;
using Manzana;

namespace DeploymentServer
{
	class Program
    {
		static int ExitCode = 0;

        static int Main(string[] args)
        {
            if ((args.Length == 2) && (args[0].Equals("-iphonepackager")))
            {
				try
				{
					// We were run as a 'child' process, quit when our 'parent' process exits
					// There is no parent-child relationship WRT windows, it's self-imposed.
					int ParentPID = int.Parse(args[1]);

					DeploymentProxy.Deployer = new DeploymentImplementation();
					IpcServerChannel Channel = new IpcServerChannel("iPhonePackager");
					ChannelServices.RegisterChannel(Channel, false);
					RemotingConfiguration.RegisterWellKnownServiceType(typeof(DeploymentProxy), "DeploymentServer_PID" + ParentPID.ToString(), WellKnownObjectMode.Singleton);

					Process ParentProcess = Process.GetProcessById(ParentPID);
					while (!ParentProcess.HasExited)
					{
						CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
					}
				}
				catch (System.Exception Ex)
				{
					Console.WriteLine(Ex.Message);
				}
            }
            else
            {
				// Parse the command
				if (ParseCommand(args))
				{
					Deployer = new DeploymentImplementation();
					bool bCommandComplete = false;
					System.Threading.Thread enumerateLoop = new System.Threading.Thread(delegate()
					{
						RunCommand();
						bCommandComplete = true;
					});
					enumerateLoop.Start();
					while (!bCommandComplete)
					{
						CoreFoundationRunLoop.RunLoopRunInMode(CoreFoundationRunLoop.kCFRunLoopDefaultMode(), 1.0, 0);
					}
				}
                Console.WriteLine("Exiting.");
            }

			Environment.ExitCode = Program.ExitCode;
			return Program.ExitCode;
        }

		private static string Command = "";
		private static List<string> FileList = new List<string>();
		private static string Bundle = "";
		private static string Manifest = "";
		private static string ipaPath = "";
		private static string Device = "";
		private static bool ParseCommand(string[] Arguments)
		{
			if (Arguments.Length >= 1)
			{
				Command = Arguments[0].ToLowerInvariant();
				for (int ArgIndex = 1; ArgIndex < Arguments.Length; ArgIndex++)
				{
					string Arg = Arguments[ArgIndex].ToLowerInvariant();
					if (Arg.StartsWith("-"))
					{
						switch (Arg)
						{
							case "-file":
								if (Arguments.Length > ArgIndex + 1)
								{
									FileList.Add(Arguments[++ArgIndex]);
								}
								else
								{
									return false;
								}
								break;

							case "-bundle":
								if (Arguments.Length > ArgIndex + 1)
								{
									Bundle = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-manifest":
								if (Arguments.Length > ArgIndex + 1)
								{
									Manifest = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-ipa":
								if (Arguments.Length > ArgIndex + 1)
								{
									ipaPath = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;

							case "-device":
								if (Arguments.Length > ArgIndex + 1)
								{
									Device = Arguments[++ArgIndex];
								}
								else
								{
									return false;
								}
								break;
						}
					}
				}
			}
			return true;
		}

		private static DeploymentImplementation Deployer = null;
		private static void RunCommand()
		{
			Deployer.DeviceId = Device;

			bool bResult = true;
			switch (Command)
			{
				case "backup":
					bResult = Deployer.BackupFiles(Bundle, FileList.ToArray());
					break;

				case "deploy":
					bResult = Deployer.InstallFilesOnDevice(Bundle, Manifest);
					break;

				case "install":
					bResult = Deployer.InstallIPAOnDevice(ipaPath);
					break;

				case "enumerate":
					Deployer.EnumerateConnectedDevices();
					break;

				case "listdevices":
					Deployer.ListDevices();
					break;

				case "listentodevice":
					Deployer.ListenToDevice(Device);
					break;
			}

			Program.ExitCode = bResult ? 0 : 1;
		}
    }
}
