// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.Deployment.WindowsInstaller;
using System.IO;
using Microsoft.Win32;
using System.Diagnostics;
using System.Windows.Forms;

namespace InstallerCustomAction
{
	public class CustomActions
	{
		[CustomAction]
		public static ActionResult RemoveClickOnceInstalls(Session Session)
		{
			string ShortcutPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "UnrealGameSync", "UnrealGameSync.appref-ms");
			if(!File.Exists(ShortcutPath))
			{
				Session.Log("Couldn't find {0} - skipping uninstall", ShortcutPath);
				return ActionResult.Success;
			}

			Record Record = new Record(1); 
			Record.FormatString = "The previous version of UnrealGameSync cannot be removed automatically.\n\nPlease uninstall from the following window.";
			MessageResult Result = Session.Message(InstallMessage.User | (InstallMessage)MessageBoxButtons.OKCancel, Record);
			if(Result == MessageResult.Cancel)
			{
				return ActionResult.UserExit;
			}

			Process NewProcess = Process.Start("rundll32.exe", "dfshim.dll,ShArpMaintain UnrealGameSync.application, Culture=neutral, PublicKeyToken=0000000000000000, processorArchitecture=msil");
			NewProcess.WaitForExit();
			
			if(File.Exists(ShortcutPath))
			{
				Record IgnoreRecord = new Record(1); 
				IgnoreRecord.FormatString = "UnrealSync is still installed. Install newer version anyway?";

				if(Session.Message(InstallMessage.User | (InstallMessage)MessageBoxButtons.YesNo, IgnoreRecord) == MessageResult.No)
				{
					return ActionResult.UserExit;
				}
			}

			return ActionResult.Success;
		}
	}
}
