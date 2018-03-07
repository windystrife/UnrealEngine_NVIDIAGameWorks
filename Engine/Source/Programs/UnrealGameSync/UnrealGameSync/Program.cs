// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	static partial class Program
	{
		/// <summary>
		/// SQL connection string used to connect to the database for telemetry and review data. The 'Program' class is a partial class, to allow an
		/// opportunistically included C# source file in NotForLicensees/ProgramSettings.cs to override this value in a static constructor.
		/// </summary>
		public static readonly string SqlConnectionString = null;

		public static string SyncVersion = null;

		[STAThread]
		static void Main(string[] Args)
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			bool bFirstInstance;
			using(Mutex InstanceMutex = new Mutex(true, "UnrealGameSyncRunning", out bFirstInstance))
			{
				using(EventWaitHandle ActivateEvent = new EventWaitHandle(false, EventResetMode.AutoReset, "ActivateUnrealGameSync"))
				{
					if(bFirstInstance)
					{
						InnerMain(InstanceMutex, ActivateEvent, Args);
					}
					else
					{
						ActivateEvent.Set();
					}
				}
			}
		}
		
		static void InnerMain(Mutex InstanceMutex, EventWaitHandle ActivateEvent, string[] Args)
		{
			List<string> RemainingArgs = new List<string>(Args);

			string UpdatePath;
			ParseArgument(RemainingArgs, "-updatepath=", out UpdatePath);

			string UpdateSpawn;
			ParseArgument(RemainingArgs, "-updatespawn=", out UpdateSpawn);

			bool bRestoreState;
			ParseOption(RemainingArgs, "-restorestate", out bRestoreState);

			bool bUnstable;
			ParseOption(RemainingArgs, "-unstable", out bUnstable);

            string ProjectFileName;
            ParseArgument(RemainingArgs, "-project=", out ProjectFileName);

			string UpdateConfigFile = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "AutoUpdate.ini");
			MergeUpdateSettings(UpdateConfigFile, ref UpdatePath, ref UpdateSpawn);

			string SyncVersionFile = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "SyncVersion.txt");
			if(File.Exists(SyncVersionFile))
			{
				try
				{
					SyncVersion = File.ReadAllText(SyncVersionFile).Trim();
				}
				catch(Exception)
				{
					SyncVersion = null;
				}
			}

			string DataFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnrealGameSync");
			Directory.CreateDirectory(DataFolder);

			using(TelemetryWriter Telemetry = new TelemetryWriter(SqlConnectionString, Path.Combine(DataFolder, "Telemetry.log")))
			{
				try
				{
					using(UpdateMonitor UpdateMonitor = new UpdateMonitor(new PerforceConnection(null, null, null), UpdatePath))
					{
						using(BoundedLogWriter Log = new BoundedLogWriter(Path.Combine(DataFolder, "UnrealGameSync.log")))
						{
							Log.WriteLine("Application version: {0}", Assembly.GetExecutingAssembly().GetName().Version);
							Log.WriteLine("Started at {0}", DateTime.Now.ToString());

							UserSettings Settings = new UserSettings(Path.Combine(DataFolder, "UnrealGameSync.ini"));
							if(!String.IsNullOrEmpty(ProjectFileName))
							{
								string FullProjectFileName = Path.GetFullPath(ProjectFileName);
								if(!Settings.OpenProjectFileNames.Any(x => x.Equals(FullProjectFileName, StringComparison.InvariantCultureIgnoreCase)))
								{
									Settings.OpenProjectFileNames = Settings.OpenProjectFileNames.Concat(new string[]{ FullProjectFileName }).ToArray();
								}
							}

							MainWindow Window = new MainWindow(UpdateMonitor, SqlConnectionString, DataFolder, ActivateEvent, bRestoreState, UpdateSpawn ?? Assembly.GetExecutingAssembly().Location, ProjectFileName, bUnstable, Log, Settings);
							if(bUnstable)
							{
								Window.Text += String.Format(" (UNSTABLE BUILD {0})", Assembly.GetExecutingAssembly().GetName().Version);
							}
							Application.Run(Window);
						}

						if(UpdateMonitor.IsUpdateAvailable && UpdateSpawn != null)
						{
							InstanceMutex.Close();
							Utility.SpawnProcess(UpdateSpawn, "-restorestate" + (bUnstable? " -unstable" : ""));
						}
					}
				}
				catch(Exception Ex)
				{
					TelemetryWriter.Enqueue(TelemetryErrorType.Crash, Ex.ToString(), null, DateTime.Now);
					MessageBox.Show(String.Format("UnrealGameSync has crashed.\n\n{0}", Ex.ToString()));
				}
			}
		}

		static void MergeUpdateSettings(string UpdateConfigFile, ref string UpdatePath, ref string UpdateSpawn)
		{
			try
			{
				ConfigFile UpdateConfig = new ConfigFile();
				if(File.Exists(UpdateConfigFile))
				{
					UpdateConfig.Load(UpdateConfigFile);
				}

				if(UpdatePath == null)
				{
					UpdatePath = UpdateConfig.GetValue("Update.Path", null);
				}
				else
				{
					UpdateConfig.SetValue("Update.Path", UpdatePath);
				}

				if(UpdateSpawn == null)
				{
					UpdateSpawn = UpdateConfig.GetValue("Update.Spawn", null);
				}
				else
				{
					UpdateConfig.SetValue("Update.Spawn", UpdateSpawn);
				}

				UpdateConfig.Save(UpdateConfigFile);
			}
			catch(Exception)
			{
			}
		}

		static bool ParseOption(List<string> RemainingArgs, string Option, out bool Value)
		{
			for(int Idx = 0; Idx < RemainingArgs.Count; Idx++)
			{
				if(RemainingArgs[Idx].Equals(Option, StringComparison.InvariantCultureIgnoreCase))
				{
					Value = true;
					RemainingArgs.RemoveAt(Idx);
					return true;
				}
			}

			Value = false;
			return false;
		}

		static bool ParseArgument(List<string> RemainingArgs, string Prefix, out string Value)
		{
			for(int Idx = 0; Idx < RemainingArgs.Count; Idx++)
			{
				if(RemainingArgs[Idx].StartsWith(Prefix, StringComparison.InvariantCultureIgnoreCase))
				{
					Value = RemainingArgs[Idx].Substring(Prefix.Length);
					RemainingArgs.RemoveAt(Idx);
					return true;
				}
			}

			Value = null;
			return false;
		}
	}
}
