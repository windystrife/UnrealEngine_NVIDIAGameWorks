// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Timers;
using System.Runtime.CompilerServices;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Timer that exits the application of the execution of the command takes too long.
	/// </summary>
	public class WatchdogTimer : IDisposable, IProcess
	{
		private Timer CountownTimer;
		private System.Diagnostics.StackFrame TimerFrame;
		private object SyncObject = new object();
		private bool bTicking = false;

		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public WatchdogTimer(int Seconds)
		{
			TimerFrame = new System.Diagnostics.StackFrame(1);  
			CountownTimer = new Timer(Seconds * 1000.0);
			CountownTimer.Elapsed += Elapsed;
			CountownTimer.Start();
			bTicking = true;
			ProcessManager.AddProcess(this);
		}

		void Elapsed(object sender, ElapsedEventArgs e)
		{
			lock (SyncObject)
			{
				bTicking = false;
			}
			int Interval = (int)(CountownTimer.Interval * 0.001);
			Dispose();
			var Method = TimerFrame.GetMethod();
			Log.TraceError("BUILD FAILED: WatchdogTimer timed out after {0}s in {1}.{2}", Interval, Method.DeclaringType.Name, Method.Name);			
			Environment.Exit(1);
		}

		public void Dispose()
		{
			Log.TraceInformation("WatchdogTimer.Dispose()");
			lock (SyncObject)
			{
				if (CountownTimer != null)
				{
					CountownTimer.Stop();
					CountownTimer.Dispose();
					CountownTimer = null;
				}
				bTicking = false;
			}
			ProcessManager.RemoveProcess(this);
		}

		public void StopProcess(bool KillDescendants = true)
		{
			Dispose();			
		}

		public bool HasExited
		{
			get 
			{
				bool bResult = true;
 				lock (SyncObject)
				{
					bResult = bTicking;
				}
				return bResult;
			}
		}

		public string GetProcessName()
		{
			var Method = TimerFrame.GetMethod();
			return String.Format("WatchdogTimer_{0}.{1}", Method.DeclaringType.Name, Method.Name);
		}
	}
}
