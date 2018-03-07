using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// SDK installation status
	/// </summary>
	enum SDKStatus
	{
		/// <summary>
		/// Desired SDK is installed and set up.
		/// </summary>
		Valid,


		/// <summary>
		/// Could not find the desired SDK, SDK setup failed, etc.		
		/// </summary>
		Invalid,
	};

	/// <summary>
	/// Output level for diagnostic messages when initializing the platform SDK
	/// </summary>
	enum SDKOutputLevel
	{
		/// <summary>
		/// No output
		/// </summary>
		Quiet,

		/// <summary>
		/// Minimal output
		/// </summary>
		Minimal,

		/// <summary>
		/// All messages
		/// </summary>
		Verbose,
	}

	/// <summary>
	/// SDK for a platform
	/// </summary>
	abstract class UEBuildPlatformSDK
	{
		// AutoSDKs handling portion

		#region protected AutoSDKs Utility

		/// <summary>
		/// Name of the file that holds currently install SDK version string
		/// </summary>
		protected const string CurrentlyInstalledSDKStringManifest = "CurrentlyInstalled.txt";

		/// <summary>
		/// name of the file that holds the last succesfully run SDK setup script version
		/// </summary>
		protected const string LastRunScriptVersionManifest = "CurrentlyInstalled.Version.txt";

		/// <summary>
		/// Name of the file that holds environment variables of current SDK
		/// </summary>
		protected const string SDKEnvironmentVarsFile = "OutputEnvVars.txt";

		protected const string SDKRootEnvVar = "UE_SDKS_ROOT";

		protected const string AutoSetupEnvVar = "AutoSDKSetup";

		/// <summary>
		/// Whether platform supports switching SDKs during runtime
		/// </summary>
		/// <returns>true if supports</returns>
		protected virtual bool PlatformSupportsAutoSDKs()
		{
			return false;
		}

		static private bool bCheckedAutoSDKRootEnvVar = false;
		static private bool bAutoSDKSystemEnabled = false;
		static private bool HasAutoSDKSystemEnabled()
		{
			if (!bCheckedAutoSDKRootEnvVar)
			{
				string SDKRoot = Environment.GetEnvironmentVariable(SDKRootEnvVar);
				if (SDKRoot != null)
				{
					bAutoSDKSystemEnabled = true;
				}
				bCheckedAutoSDKRootEnvVar = true;
			}
			return bAutoSDKSystemEnabled;
		}

		// Whether AutoSDK setup is safe. AutoSDKs will damage manual installs on some platforms.
		protected bool IsAutoSDKSafe()
		{
			return !IsAutoSDKDestructive() || !HasAnyManualInstall();
		}

		/// <summary>
		/// Returns SDK string as required by the platform
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected virtual string GetRequiredSDKString()
		{
			return "";
		}

		/// <summary>
		/// Gets the version number of the SDK setup script itself.  The version in the base should ALWAYS be the master revision from the last refactor.
		/// If you need to force a rebuild for a given platform, override this for the given platform.
		/// </summary>
		/// <returns>Setup script version</returns>
		protected virtual String GetRequiredScriptVersionString()
		{
			return "3.0";
		}

		/// <summary>
		/// Returns path to platform SDKs
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected string GetPathToPlatformAutoSDKs()
		{
			string SDKPath = "";
			string SDKRoot = Environment.GetEnvironmentVariable(SDKRootEnvVar);
			if (SDKRoot != null)
			{
				if (SDKRoot != "")
				{
					SDKPath = Path.Combine(SDKRoot, "Host" + BuildHostPlatform.Current.Platform, GetSDKTargetPlatformName());
				}
			}
			return SDKPath;
		}

		/// <summary>
		/// Because most ManualSDK determination depends on reading env vars, if this process is spawned by a process that ALREADY set up
		/// AutoSDKs then all the SDK env vars will exist, and we will spuriously detect a Manual SDK. (children inherit the environment of the parent process).
		/// Therefore we write out an env var to set in the command file (OutputEnvVars.txt) such that child processes can determine if their manual SDK detection
		/// is bogus.  Make it platform specific so that platforms can be in different states.
		/// </summary>
		protected string GetPlatformAutoSDKSetupEnvVar()
		{
			return GetSDKTargetPlatformName() + AutoSetupEnvVar;
		}

		/// <summary>
		/// Gets currently installed version
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="OutInstalledSDKVersionString">version string as currently installed</param>
		/// <returns>true if was able to read it</returns>
		protected bool GetCurrentlyInstalledSDKString(string PlatformSDKRoot, out string OutInstalledSDKVersionString)
		{
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(VersionFilename))
				{
					using (StreamReader Reader = new StreamReader(VersionFilename))
					{
						string Version = Reader.ReadLine();
						string Type = Reader.ReadLine();

						// don't allow ManualSDK installs to count as an AutoSDK install version.
						if (Type != null && Type == "AutoSDK")
						{
							if (Version != null)
							{
								OutInstalledSDKVersionString = Version;
								return true;
							}
						}
					}
				}
			}

			OutInstalledSDKVersionString = "";
			return false;
		}

		/// <summary>
		/// Gets the version of the last successfully run setup script.
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="OutLastRunScriptVersion">version string</param>
		/// <returns>true if was able to read it</returns>
		protected bool GetLastRunScriptVersionString(string PlatformSDKRoot, out string OutLastRunScriptVersion)
		{
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					using (StreamReader Reader = new StreamReader(VersionFilename))
					{
						string Version = Reader.ReadLine();
						if (Version != null)
						{
							OutLastRunScriptVersion = Version;
							return true;
						}
					}
				}
			}

			OutLastRunScriptVersion = "";
			return false;
		}

		/// <summary>
		/// Sets currently installed version
		/// </summary>
		/// <param name="InstalledSDKVersionString">SDK version string to set</param>
		/// <returns>true if was able to set it</returns>
		protected bool SetCurrentlyInstalledAutoSDKString(String InstalledSDKVersionString)
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				using (StreamWriter Writer = File.CreateText(VersionFilename))
				{
					Writer.WriteLine(InstalledSDKVersionString);
					Writer.WriteLine("AutoSDK");
					return true;
				}
			}

			return false;
		}

		protected void SetupManualSDK()
		{
			if (PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				String InstalledSDKVersionString = GetRequiredSDKString();
				String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
                if (!Directory.Exists(PlatformSDKRoot))
                {
                    Directory.CreateDirectory(PlatformSDKRoot);
                }

				{
					string VersionFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
					if (File.Exists(VersionFilename))
					{
						File.Delete(VersionFilename);
					}

					string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
					if (File.Exists(EnvVarFile))
					{
						File.Delete(EnvVarFile);
					}

					using (StreamWriter Writer = File.CreateText(VersionFilename))
					{
						Writer.WriteLine(InstalledSDKVersionString);
						Writer.WriteLine("ManualSDK");
					}
				}
			}
		}

		protected bool SetLastRunAutoSDKScriptVersion(string LastRunScriptVersion)
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				using (StreamWriter Writer = File.CreateText(VersionFilename))
				{
					Writer.WriteLine(LastRunScriptVersion);
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Returns Hook names as needed by the platform
		/// (e.g. can be overridden with custom executables or scripts)
		/// </summary>
		/// <param name="Hook">Hook type</param>
		protected virtual string GetHookExecutableName(SDKHookType Hook)
		{
			if (Hook == SDKHookType.Uninstall)
			{
				return "unsetup.bat";
			}

			return "setup.bat";
		}

		/// <summary>
		/// Runs install/uninstall hooks for SDK
		/// </summary>
		/// <param name="OutputLevel">The output level for diagnostic messages</param>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK root</param>
		/// <param name="SDKVersionString">version string to run for (can be empty!)</param>
		/// <param name="Hook">which one of hooks to run</param>
		/// <param name="bHookCanBeNonExistent">whether a non-existing hook means failure</param>
		/// <returns>true if succeeded</returns>
		protected virtual bool RunAutoSDKHooks(SDKOutputLevel OutputLevel, string PlatformSDKRoot, string SDKVersionString, SDKHookType Hook, bool bHookCanBeNonExistent = true)
		{
			if (!IsAutoSDKSafe())
			{
				Console.ForegroundColor = ConsoleColor.Red;
				LogAutoSDK(OutputLevel, GetSDKTargetPlatformName() + " attempted to run SDK hook which could have damaged manual SDK install!");
				Console.ResetColor();

				return false;
			}
			if (SDKVersionString != "")
			{
				string SDKDirectory = Path.Combine(PlatformSDKRoot, SDKVersionString);
				string HookExe = Path.Combine(SDKDirectory, GetHookExecutableName(Hook));

				if (File.Exists(HookExe))
				{
					LogAutoSDK(OutputLevel, "Running {0} hook {1}", Hook, HookExe);

					// run it
					Process HookProcess = new Process();
					HookProcess.StartInfo.WorkingDirectory = SDKDirectory;
					HookProcess.StartInfo.FileName = HookExe;
					HookProcess.StartInfo.Arguments = "";
					HookProcess.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;

					// seems to break the build machines?
					//HookProcess.StartInfo.UseShellExecute = false;
					//HookProcess.StartInfo.RedirectStandardOutput = true;
					//HookProcess.StartInfo.RedirectStandardError = true;					

					using (ScopedTimer HookTimer = new ScopedTimer("Time to run hook: ", (OutputLevel >= SDKOutputLevel.Minimal) ? LogEventType.Log : LogEventType.Verbose))
					{
						//installers may require administrator access to succeed. so run as an admmin.
						HookProcess.StartInfo.Verb = "runas";
						HookProcess.Start();
						HookProcess.WaitForExit();
					}

					//LogAutoSDK(HookProcess.StandardOutput.ReadToEnd());
					//LogAutoSDK(HookProcess.StandardError.ReadToEnd());
					if (HookProcess.ExitCode != 0)
					{
						LogAutoSDK(OutputLevel, "Hook exited uncleanly (returned {0}), considering it failed.", HookProcess.ExitCode);
						return false;
					}

					return true;
				}
				else
				{
					LogAutoSDK(OutputLevel, "File {0} does not exist", HookExe);
				}
			}
			else
			{
				LogAutoSDK(OutputLevel, "Version string is blank for {0}. Can't determine {1} hook.", PlatformSDKRoot, Hook.ToString());
			}

			return bHookCanBeNonExistent;
		}

		/// <summary>
		/// Loads environment variables from SDK
		/// If any commands are added or removed the handling needs to be duplicated in
		/// TargetPlatformManagerModule.cpp
		/// </summary>
		/// <param name="PlatformSDKRoot">absolute path to platform SDK</param>
		/// <param name="OutputLevel">The output level for diagnostic messages</param>
		/// <returns>true if succeeded</returns>
		protected bool SetupEnvironmentFromAutoSDK(string PlatformSDKRoot, SDKOutputLevel OutputLevel)
		{
			string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
			if (File.Exists(EnvVarFile))
			{
				using (StreamReader Reader = new StreamReader(EnvVarFile))
				{
					List<string> PathAdds = new List<string>();
					List<string> PathRemoves = new List<string>();

					List<string> EnvVarNames = new List<string>();
					List<string> EnvVarValues = new List<string>();

					bool bNeedsToWriteAutoSetupEnvVar = true;
					String PlatformSetupEnvVar = GetPlatformAutoSDKSetupEnvVar();
					for (; ; )
					{
						string VariableString = Reader.ReadLine();
						if (VariableString == null)
						{
							break;
						}

						string[] Parts = VariableString.Split('=');
						if (Parts.Length != 2)
						{
							LogAutoSDK(OutputLevel, "Incorrect environment variable declaration:");
							LogAutoSDK(OutputLevel, VariableString);
							return false;
						}

						if (String.Compare(Parts[0], "strippath", true) == 0)
						{
							PathRemoves.Add(Parts[1]);
						}
						else if (String.Compare(Parts[0], "addpath", true) == 0)
						{
							PathAdds.Add(Parts[1]);
						}
						else
						{
							if (String.Compare(Parts[0], PlatformSetupEnvVar) == 0)
							{
								bNeedsToWriteAutoSetupEnvVar = false;
							}
							// convenience for setup.bat writers.  Trim any accidental whitespace from var names/values.
							EnvVarNames.Add(Parts[0].Trim());
							EnvVarValues.Add(Parts[1].Trim());
						}
					}

					// don't actually set anything until we successfully validate and read all values in.
					// we don't want to set a few vars, return a failure, and then have a platform try to
					// build against a manually installed SDK with half-set env vars.
					for (int i = 0; i < EnvVarNames.Count; ++i)
					{
						string EnvVarName = EnvVarNames[i];
						string EnvVarValue = EnvVarValues[i];
						if (OutputLevel >= SDKOutputLevel.Verbose)
						{
							LogAutoSDK(OutputLevel, "Setting variable '{0}' to '{1}'", EnvVarName, EnvVarValue);
						}
						Environment.SetEnvironmentVariable(EnvVarName, EnvVarValue);
					}


                    // actually perform the PATH stripping / adding.
                    String OrigPathVar = Environment.GetEnvironmentVariable("PATH");
                    String PathDelimiter = UEBuildPlatform.GetPathVarDelimiter();
                    String[] PathVars = { };
                    if (!String.IsNullOrEmpty(OrigPathVar))
                    {
                        PathVars = OrigPathVar.Split(PathDelimiter.ToCharArray());
                    }
                    else
                    {
                        LogAutoSDK(OutputLevel, "Path environment variable is null during AutoSDK");
                    }

					List<String> ModifiedPathVars = new List<string>();
					ModifiedPathVars.AddRange(PathVars);

					// perform removes first, in case they overlap with any adds.
					foreach (String PathRemove in PathRemoves)
					{
						foreach (String PathVar in PathVars)
						{
							if (PathVar.IndexOf(PathRemove, StringComparison.OrdinalIgnoreCase) >= 0)
							{
								LogAutoSDK(OutputLevel, "Removing Path: '{0}'", PathVar);
								ModifiedPathVars.Remove(PathVar);
							}
						}
					}

					// remove all the of ADDs so that if this function is executed multiple times, the paths will be guaranteed to be in the same order after each run.
					// If we did not do this, a 'remove' that matched some, but not all, of our 'adds' would cause the order to change.
					foreach (String PathAdd in PathAdds)
					{
						foreach (String PathVar in PathVars)
						{
							if (String.Compare(PathAdd, PathVar, true) == 0)
							{
								LogAutoSDK(OutputLevel, "Removing Path: '{0}'", PathVar);
								ModifiedPathVars.Remove(PathVar);
							}
						}
					}

					// perform adds, but don't add duplicates
					foreach (String PathAdd in PathAdds)
					{
						if (!ModifiedPathVars.Contains(PathAdd))
						{
							LogAutoSDK(OutputLevel, "Adding Path: '{0}'", PathAdd);
							ModifiedPathVars.Add(PathAdd);
						}
					}

					String ModifiedPath = String.Join(PathDelimiter, ModifiedPathVars);
					Environment.SetEnvironmentVariable("PATH", ModifiedPath);

					Reader.Close();

					// write out env var command so any process using this commandfile will mark itself as having had autosdks set up.
					// avoids child processes spuriously detecting manualsdks.
					if (bNeedsToWriteAutoSetupEnvVar)
					{
						using (StreamWriter Writer = File.AppendText(EnvVarFile))
						{
							Writer.WriteLine("{0}=1", PlatformSetupEnvVar);
						}
						// set the var in the local environment in case this process spawns any others.
						Environment.SetEnvironmentVariable(PlatformSetupEnvVar, "1");
					}

					// make sure we know that we've modified the local environment, invalidating manual installs for this run.
					bLocalProcessSetupAutoSDK = true;

					return true;
				}
			}
			else
			{
				LogAutoSDK(OutputLevel, "Cannot set up environment for {1} because command file {1} does not exist.", PlatformSDKRoot, EnvVarFile);
			}

			return false;
		}

		protected void InvalidateCurrentlyInstalledAutoSDK()
		{
			String PlatformSDKRoot = GetPathToPlatformAutoSDKs();
			if (Directory.Exists(PlatformSDKRoot))
			{
				string SDKFilename = Path.Combine(PlatformSDKRoot, CurrentlyInstalledSDKStringManifest);
				if (File.Exists(SDKFilename))
				{
					File.Delete(SDKFilename);
				}

				string VersionFilename = Path.Combine(PlatformSDKRoot, LastRunScriptVersionManifest);
				if (File.Exists(VersionFilename))
				{
					File.Delete(VersionFilename);
				}

				string EnvVarFile = Path.Combine(PlatformSDKRoot, SDKEnvironmentVarsFile);
				if (File.Exists(EnvVarFile))
				{
					File.Delete(EnvVarFile);
				}
			}
		}

		/// <summary>
		/// Currently installed AutoSDK is written out to a text file in a known location.
		/// This function just compares the file's contents with the current requirements.
		/// </summary>
		protected SDKStatus HasRequiredAutoSDKInstalled()
		{
			if (PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				string AutoSDKRoot = GetPathToPlatformAutoSDKs();
				if (AutoSDKRoot != "")
				{
					// check script version so script fixes can be propagated without touching every build machine's CurrentlyInstalled file manually.
					bool bScriptVersionMatches = false;
					string CurrentScriptVersionString;
					if (GetLastRunScriptVersionString(AutoSDKRoot, out CurrentScriptVersionString) && CurrentScriptVersionString == GetRequiredScriptVersionString())
					{
						bScriptVersionMatches = true;
					}

					// check to make sure OutputEnvVars doesn't need regenerating
					string EnvVarFile = Path.Combine(AutoSDKRoot, SDKEnvironmentVarsFile);
					bool bEnvVarFileExists = File.Exists(EnvVarFile);

					string CurrentSDKString;
					if (bEnvVarFileExists && GetCurrentlyInstalledSDKString(AutoSDKRoot, out CurrentSDKString) && CurrentSDKString == GetRequiredSDKString() && bScriptVersionMatches)
					{
						return SDKStatus.Valid;
					}
					return SDKStatus.Invalid;
				}
			}
			return SDKStatus.Invalid;
		}

		// This tracks if we have already checked the sdk installation.
		private Int32 SDKCheckStatus = -1;

		// true if we've ever overridden the process's environment with AutoSDK data.  After that, manual installs cannot be considered valid ever again.
		private bool bLocalProcessSetupAutoSDK = false;

		protected bool HasSetupAutoSDK()
		{
			return bLocalProcessSetupAutoSDK || HasParentProcessSetupAutoSDK();
		}

		protected bool HasParentProcessSetupAutoSDK()
		{
			bool bParentProcessSetupAutoSDK = false;
			String AutoSDKSetupVarName = GetPlatformAutoSDKSetupEnvVar();
			String AutoSDKSetupVar = Environment.GetEnvironmentVariable(AutoSDKSetupVarName);
			if (!String.IsNullOrEmpty(AutoSDKSetupVar))
			{
				bParentProcessSetupAutoSDK = true;
			}
			return bParentProcessSetupAutoSDK;
		}

		protected SDKStatus HasRequiredManualSDK()
		{
			if (HasSetupAutoSDK())
			{
				return SDKStatus.Invalid;
			}

			// manual installs are always invalid if we have modified the process's environment for AutoSDKs
			return HasRequiredManualSDKInternal();
		}

		// for platforms with destructive AutoSDK.  Report if any manual sdk is installed that may be damaged by an autosdk.
		protected virtual bool HasAnyManualInstall()
		{
			return false;
		}

		// tells us if the user has a valid manual install.
		protected abstract SDKStatus HasRequiredManualSDKInternal();

		// some platforms will fail if there is a manual install that is the WRONG manual install.
		protected virtual bool AllowInvalidManualInstall()
		{
			return true;
		}

		// platforms can choose if they prefer a correct the the AutoSDK install over the manual install.
		protected virtual bool PreferAutoSDK()
		{
			return true;
		}

		// some platforms don't support parallel SDK installs.  AutoSDK on these platforms will
		// actively damage an existing manual install by overwriting files in it.  AutoSDK must NOT
		// run any setup if a manual install exists in this case.
		protected virtual bool IsAutoSDKDestructive()
		{
			return false;
		}

		/// <summary>
		/// Runs batch files if necessary to set up required AutoSDK.
		/// AutoSDKs are SDKs that have not been setup through a formal installer, but rather come from
		/// a source control directory, or other local copy.
		/// </summary>
		private void SetupAutoSDK(SDKOutputLevel OutputLevel)
		{
			if (IsAutoSDKSafe() && PlatformSupportsAutoSDKs() && HasAutoSDKSystemEnabled())
			{
				// run installation for autosdk if necessary.
				if (HasRequiredAutoSDKInstalled() == SDKStatus.Invalid)
				{
					//reset check status so any checking sdk status after the attempted setup will do a real check again.
					SDKCheckStatus = -1;

					string AutoSDKRoot = GetPathToPlatformAutoSDKs();
					string CurrentSDKString;
					GetCurrentlyInstalledSDKString(AutoSDKRoot, out CurrentSDKString);

					// switch over (note that version string can be empty)
					if (!RunAutoSDKHooks(OutputLevel, AutoSDKRoot, CurrentSDKString, SDKHookType.Uninstall))
					{
						LogAutoSDK(OutputLevel, "Failed to uninstall currently installed SDK {0}", CurrentSDKString);
						InvalidateCurrentlyInstalledAutoSDK();
						return;
					}
					// delete Manifest file to avoid multiple uninstalls
					InvalidateCurrentlyInstalledAutoSDK();

					if (!RunAutoSDKHooks(OutputLevel, AutoSDKRoot, GetRequiredSDKString(), SDKHookType.Install, false))
					{
						LogAutoSDK(OutputLevel, "Failed to install required SDK {0}.  Attemping to uninstall", GetRequiredSDKString());
						RunAutoSDKHooks(OutputLevel, AutoSDKRoot, GetRequiredSDKString(), SDKHookType.Uninstall, false);
						return;
					}

					string EnvVarFile = Path.Combine(AutoSDKRoot, SDKEnvironmentVarsFile);
					if (!File.Exists(EnvVarFile))
					{
						LogAutoSDK(OutputLevel, "Installation of required SDK {0}.  Did not generate Environment file {1}", GetRequiredSDKString(), EnvVarFile);
						RunAutoSDKHooks(OutputLevel, AutoSDKRoot, GetRequiredSDKString(), SDKHookType.Uninstall, false);
						return;
					}

					SetCurrentlyInstalledAutoSDKString(GetRequiredSDKString());
					SetLastRunAutoSDKScriptVersion(GetRequiredScriptVersionString());
				}

				// fixup process environment to match autosdk
				SetupEnvironmentFromAutoSDK(OutputLevel);
			}
		}

		#endregion

		#region public AutoSDKs Utility

		/// <summary>
		/// Enum describing types of hooks a platform SDK can have
		/// </summary>
		public enum SDKHookType
		{
			Install,
			Uninstall
		};

		/// <summary>
		/// Returns platform-specific name used in SDK repository
		/// </summary>
		/// <returns>path to SDK Repository</returns>
		public virtual string GetSDKTargetPlatformName()
		{
			return "";
		}

		/* Whether or not we should try to automatically switch SDKs when asked to validate the platform's SDK state. */
		public static bool bAllowAutoSDKSwitching = true;

		public SDKStatus SetupEnvironmentFromAutoSDK(SDKOutputLevel OutputLevel)
		{
			string PlatformSDKRoot = GetPathToPlatformAutoSDKs();

			// load environment variables from current SDK
			if (!SetupEnvironmentFromAutoSDK(PlatformSDKRoot, OutputLevel))
			{
				LogAutoSDK(OutputLevel, "Failed to load environment from required SDK {0}", GetRequiredSDKString());
				InvalidateCurrentlyInstalledAutoSDK();
				return SDKStatus.Invalid;
			}
			return SDKStatus.Valid;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform.
		/// Could be either a manual install or an AutoSDK.
		/// </summary>
		public SDKStatus HasRequiredSDKsInstalled()
		{
			// avoid redundant potentially expensive SDK checks.
			if (SDKCheckStatus == -1)
			{
				bool bHasManualSDK = HasRequiredManualSDK() == SDKStatus.Valid;
				bool bHasAutoSDK = HasRequiredAutoSDKInstalled() == SDKStatus.Valid;

				// Per-Platform implementations can choose how to handle non-Auto SDK detection / handling.
				SDKCheckStatus = (bHasManualSDK || bHasAutoSDK) ? 1 : 0;
			}
			return SDKCheckStatus == 1 ? SDKStatus.Valid : SDKStatus.Invalid;
		}

		// Arbitrates between manual SDKs and setting up AutoSDK based on program options and platform preferences.
		public void ManageAndValidateSDK(SDKOutputLevel OutputLevel)
		{
			// do not modify installed manifests if parent process has already set everything up.
			// this avoids problems with determining IsAutoSDKSafe and doing an incorrect invalidate.
			if (bAllowAutoSDKSwitching && !HasParentProcessSetupAutoSDK())
			{
				bool bSetSomeSDK = false;
				bool bHasRequiredManualSDK = HasRequiredManualSDK() == SDKStatus.Valid;
				if (IsAutoSDKSafe() && (PreferAutoSDK() || !bHasRequiredManualSDK))
				{
					SetupAutoSDK(OutputLevel);
					bSetSomeSDK = true;
				}

				//Setup manual SDK if autoSDK setup was skipped or failed for whatever reason.
				if (bHasRequiredManualSDK && (HasRequiredAutoSDKInstalled() != SDKStatus.Valid))
				{
					SetupManualSDK();
					bSetSomeSDK = true;
				}

				if (!bSetSomeSDK)
				{
					InvalidateCurrentlyInstalledAutoSDK();
				}
			}


			if (OutputLevel != SDKOutputLevel.Quiet)
			{
				PrintSDKInfo(OutputLevel);
			}
		}

		public void PrintSDKInfo(SDKOutputLevel OutputLevel)
		{
			if (HasRequiredSDKsInstalled() == SDKStatus.Valid)
			{
				bool bHasRequiredManualSDK = HasRequiredManualSDK() == SDKStatus.Valid;
				if (HasSetupAutoSDK())
				{
					string PlatformSDKRoot = GetPathToPlatformAutoSDKs();
					LogAutoSDK(OutputLevel, GetSDKTargetPlatformName() + " using SDK from: " + Path.Combine(PlatformSDKRoot, GetRequiredSDKString()));
				}
				else if (bHasRequiredManualSDK)
				{
					LogAutoSDK(OutputLevel, this.ToString() + " using manually installed SDK " + GetRequiredSDKString());
				}
				else
				{
					LogAutoSDK(OutputLevel, this.ToString() + " setup error.  Inform platform team.");
				}
			}
			else
			{
				LogAutoSDK(OutputLevel, this.ToString() + " has no valid SDK");
			}
		}

		protected static void LogAutoSDK(SDKOutputLevel OutputLevel, string Format, params object[] Args)
		{
			if (OutputLevel != SDKOutputLevel.Quiet)
			{
				Log.WriteLine(LogEventType.Log, Format, Args);
			}
		}

		protected static void LogAutoSDK(SDKOutputLevel OutputLevel, String Message)
		{
			if (OutputLevel != SDKOutputLevel.Quiet)
			{
				Log.WriteLine(LogEventType.Log, Message);
			}
		}

		#endregion
	}
}
