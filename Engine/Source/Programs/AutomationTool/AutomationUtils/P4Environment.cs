// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Contains detected Perforce settings.
	/// </summary>
	public class P4Environment
	{
		/// <summary>
		/// The Perforce host and port number (eg. perforce:1666)
		/// </summary>
		public string ServerAndPort
		{
			get;
			private set;
		}

		/// <summary>
		/// The Perforce username
		/// </summary>
		public string User
		{
			get;
			private set;
		}

		/// <summary>
		/// Name of the Perforce client containing UAT.
		/// </summary>
		public string Client
		{
			get;
			private set;
		}

		/// <summary>
		/// The current branch containing UAT. This may be a depot path (eg. //depot/UE4) or the name of a stream (eg. //UE4/Main).
		/// </summary>
		public string Branch
		{
			get;
			private set;
		}

		/// <summary>
		/// Path to the root of the branch as a client path (//MyClient/UE4).
		/// </summary>
		public string ClientRoot
		{
			get;
			private set;
		}

		/// <summary>
		/// The Perforce host and port number (eg. perforce:1666)
		/// </summary>
		[Obsolete("The P4Port property has been deprecated in 4.18. Use the ServerAndPort property instead.")]
		public string P4Port
		{
			get { return ServerAndPort; }
		}

		/// <summary>
		/// Root directory of the current branch
		/// </summary>
		[Obsolete("The BuildRootP4 property has been deprecated in 4.18. Use the Branch property instead.")]
		public string BuildRootP4
		{
			get { return Branch; }
		}

		/// <summary>
		/// Escaped branch name
		/// </summary>
		[Obsolete("The BuildRootEscaped property has been deprecated in 4.18. Use the CommandUtils.EscapePath(P4Env.Branch) instead.")]
		public string BuildRootEscaped
		{
			get { return CommandUtils.EscapePath(BuildRootP4); }
		}

		/// <summary>
		/// The currently synced changelist.
		/// </summary>
		public int Changelist
		{
			get
			{
				if (ChangelistInternal <= 0)
				{
					throw new AutomationException("P4Environment.Changelist has not been initialized but is requested. Set uebp_CL env var or run UAT with -P4 to automatically detect changelist.");
				}
				return ChangelistInternal;
			}
			protected set
			{
				ChangelistInternal = value;
			}
		}

		/// <summary>
		/// Deprecated accessor for the current changelist as a string.
		/// </summary>
		[Obsolete("The ChangelistString property has been deprecated in 4.18. Use the integer Changelist property instead.")]
		public string ChangelistString
		{
			get { return Changelist.ToString(); }
		}

		/// <summary>
		/// Backing value for the current changelist property
		/// </summary>
		private int ChangelistInternal = -1;

		/// <summary>
		/// The currently synced code changelist.
		/// </summary>
		public int CodeChangelist
		{
			get
			{
				if (CodeChangelistInternal <= 0)
				{
					throw new AutomationException("P4Environment.CodeChangelist has not been initialized but is requested. Set uebp_CodeCL env var or run UAT with -P4 to automatically detect changelist.");
				}
				return CodeChangelistInternal;
			}
			protected set
			{
				CodeChangelistInternal = value;
			}
		}

		/// <summary>
		/// Backing value for the current code changelist
		/// </summary>
		private int CodeChangelistInternal = -1;

		/// <summary>
		/// Constructor. Derives the Perforce environment settings.
		/// </summary>
		internal P4Environment(CommandEnvironment CmdEnv)
		{
			// Get the Perforce port setting
			ServerAndPort = CommandUtils.GetEnvVar(EnvVarNames.P4Port);
			if(String.IsNullOrEmpty(ServerAndPort))
			{
				ServerAndPort = DetectP4Port();
				CommandUtils.SetEnvVar(EnvVarNames.P4Port, ServerAndPort);
			}

			// Get the Perforce user setting
			User = CommandUtils.GetEnvVar(EnvVarNames.User);
			if(String.IsNullOrEmpty(User))
			{
				P4Connection DefaultConnection = new P4Connection(User: null, Client: null, ServerAndPort: ServerAndPort);
				User = DetectUserName(DefaultConnection);
				CommandUtils.SetEnvVar(EnvVarNames.User, User);
			}

			// Get the Perforce client setting
			Client = CommandUtils.GetEnvVar(EnvVarNames.Client);
			if(String.IsNullOrEmpty(Client))
			{
				P4Connection DefaultConnection = new P4Connection(User: User, Client: null, ServerAndPort: ServerAndPort);
				P4ClientInfo ThisClient = DetectClient(DefaultConnection, User, Environment.MachineName.ToLower(), CmdEnv.UATExe);
				Log.TraceInformation("Using user {0} clientspec {1} {2}", User, ThisClient.Name, ThisClient.RootPath);

				string BranchPath;
				string ClientRootPath;
				P4Connection ClientConnection = new P4Connection(User: User, Client: ThisClient.Name, ServerAndPort: ServerAndPort);
				DetectRootPaths(ClientConnection, CmdEnv.LocalRoot, ThisClient, out BranchPath, out ClientRootPath);

				Client = ThisClient.Name;
				CommandUtils.SetEnvVar(EnvVarNames.Client, Client);

				Branch = BranchPath;
				CommandUtils.SetEnvVar(EnvVarNames.BuildRootP4, Branch);

				ClientRoot = ClientRootPath;
				CommandUtils.SetEnvVar(EnvVarNames.ClientRoot, ClientRootPath);
			}
			else
			{
				Branch = CommandUtils.GetEnvVar(EnvVarNames.BuildRootP4);
				ClientRoot = CommandUtils.GetEnvVar(EnvVarNames.ClientRoot);
				if(String.IsNullOrEmpty(Branch) || String.IsNullOrEmpty(ClientRoot))
				{
					throw new AutomationException("{0} and {1} must also be set with {2}", EnvVarNames.ClientRoot, EnvVarNames.BuildRootP4, EnvVarNames.Client);
				}
			}

			// We expect the build root to not end with a path separator
			if (Branch.EndsWith("/"))
			{
				Branch = Branch.TrimEnd('/');
				CommandUtils.SetEnvVar(EnvVarNames.BuildRootP4, Branch);
			}

			// Set the current changelist
			string ChangelistString = CommandUtils.GetEnvVar(EnvVarNames.Changelist, null);
			if(String.IsNullOrEmpty(ChangelistString) && CommandUtils.P4CLRequired)
			{
				P4Connection Connection = new P4Connection(User, Client, ServerAndPort);
				ChangelistString = DetectCurrentCL(Connection, ClientRoot);
				CommandUtils.SetEnvVar(EnvVarNames.Changelist, ChangelistString);
			}
			if(!String.IsNullOrEmpty(ChangelistString))
			{
				Changelist = int.Parse(ChangelistString);
			}

			// Set the current code changelist
			string CodeChangelistString = CommandUtils.GetEnvVar(EnvVarNames.CodeChangelist);
			if(String.IsNullOrEmpty(CodeChangelistString) && CommandUtils.P4CLRequired)
			{
				P4Connection Connection = new P4Connection(User, Client, ServerAndPort);
				CodeChangelistString = DetectCurrentCodeCL(Connection, ClientRoot);
				CommandUtils.SetEnvVar(EnvVarNames.CodeChangelist, CodeChangelistString);
			}
			if(!String.IsNullOrEmpty(CodeChangelistString))
			{
				CodeChangelist = int.Parse(CodeChangelistString);
			}

			// Set the standard environment variables based on the values we've found
			CommandUtils.SetEnvVar("P4PORT", ServerAndPort);
			CommandUtils.SetEnvVar("P4USER", User);
			CommandUtils.SetEnvVar("P4CLIENT", Client);

			// Write a summary of the settings to the output window
			if (!CommandUtils.CmdEnv.IsChildInstance)
			{
				Log.TraceInformation("Detected Perforce Settings:");
				Log.TraceInformation("  Server: {0}", ServerAndPort);
				Log.TraceInformation("  User: {0}", User);
				Log.TraceInformation("  Client: {0}", Client);
				Log.TraceInformation("  Branch: {0}", Branch);
				if (ChangelistInternal != -1)
				{
					Log.TraceInformation("  Last Change: {0}", Changelist);
				}
				if (CodeChangelistInternal != -1)
				{
					Log.TraceInformation("  Last Code Change: {0}", CodeChangelist);
				}
			}

			// Write all the environment variables to the log
			Log.TraceLog("Perforce Environment Variables:");
			Log.TraceLog("  {0}={1}", EnvVarNames.P4Port, InternalUtils.GetEnvironmentVariable(EnvVarNames.P4Port, "", true));
			Log.TraceLog("  {0}={1}", EnvVarNames.User, InternalUtils.GetEnvironmentVariable(EnvVarNames.User, "", true));
			Log.TraceLog("  {0}={1}", EnvVarNames.Client, InternalUtils.GetEnvironmentVariable(EnvVarNames.Client, "", true));
			Log.TraceLog("  {0}={1}", EnvVarNames.BuildRootP4, InternalUtils.GetEnvironmentVariable(EnvVarNames.BuildRootP4, "", true));
			Log.TraceLog("  {0}={1}", EnvVarNames.BuildRootEscaped, InternalUtils.GetEnvironmentVariable(EnvVarNames.BuildRootEscaped, "", true));
			Log.TraceLog("  {0}={1}", EnvVarNames.ClientRoot, InternalUtils.GetEnvironmentVariable(EnvVarNames.ClientRoot, "", true));
			Log.TraceLog("  {0}={1}", EnvVarNames.Changelist, InternalUtils.GetEnvironmentVariable(EnvVarNames.Changelist, "", true));
			Log.TraceLog("  {0}={1}", EnvVarNames.CodeChangelist, InternalUtils.GetEnvironmentVariable(EnvVarNames.CodeChangelist, "", true));
			Log.TraceLog("  {0}={1}", "P4PORT", InternalUtils.GetEnvironmentVariable("P4PORT", "", true));
			Log.TraceLog("  {0}={1}", "P4USER", InternalUtils.GetEnvironmentVariable("P4USER", "", true));
			Log.TraceLog("  {0}={1}", "P4CLIENT", InternalUtils.GetEnvironmentVariable("P4CLIENT", "", true));
		}

		/// <summary>
		/// Attempts to detect source control server address from environment variables.
		/// </summary>
		/// <returns>Source control server address.</returns>
		private static string DetectP4Port()
		{
			string P4Port = null;

			// If it's not set, spawn Perforce to get the current server port setting
			IProcessResult Result = CommandUtils.Run(HostPlatform.Current.P4Exe, "set P4PORT", null, CommandUtils.ERunOptions.NoLoggingOfRunCommand);
			if (Result.ExitCode == 0)
			{
				const string KeyName = "P4PORT=";
				if (Result.Output.StartsWith(KeyName))
				{
					int LastIdx = Result.Output.IndexOfAny(new char[] { ' ', '\n', '\r' });
					if (LastIdx == -1)
					{
						LastIdx = Result.Output.Length;
					}
					P4Port = Result.Output.Substring(KeyName.Length, LastIdx - KeyName.Length);
				}
			}

			// Otherwise fallback to the uebp variables, or the default
			if(String.IsNullOrEmpty(P4Port))
			{
				Log.TraceWarning("P4PORT is not set. Using perforce:1666");
				P4Port = "perforce:1666";
			}

			return P4Port;
		}

		/// <summary>
		/// Detects current user name.
		/// </summary>
		/// <returns></returns>
		private static string DetectUserName(P4Connection Connection)
		{
			var UserName = String.Empty;
			var P4Result = Connection.P4("info", AllowSpew: false);
			if (P4Result.ExitCode != 0)
			{
				throw new AutomationException("Perforce command failed: {0}. Please make sure your P4PORT or {1} is set properly.", P4Result.Output, EnvVarNames.P4Port);
			}

			// Retrieve the P4 user name			
			var Tags = Connection.ParseTaggedP4Output(P4Result.Output);
			if(!Tags.TryGetValue("User name", out UserName) || String.IsNullOrEmpty(UserName))
			{
				if (!String.IsNullOrEmpty(UserName))
				{
					Log.TraceWarning("Unable to retrieve perforce user name. Trying to fall back to {0} which is set to {1}.", EnvVarNames.User, UserName);
				}
				else
				{
					throw new AutomationException("Failed to retrieve user name.");
				}
			}

			return UserName;
		}

		/// <summary>
		/// Detects a workspace given the current user name, host name and depot path.
		/// </summary>
		/// <param name="UserName">User name</param>
		/// <param name="HostName">Host</param>
		/// <param name="UATLocation">Path to UAT exe, this will be checked agains the root path.</param>
		/// <returns>Client to use.</returns>
		private static P4ClientInfo DetectClient(P4Connection Connection, string UserName, string HostName, string UATLocation)
		{
			CommandUtils.LogVerbose("uebp_CLIENT not set, detecting current client...");

			var MatchingClients = new List<P4ClientInfo>();
			P4ClientInfo[] P4Clients = Connection.GetClientsForUser(UserName, UATLocation);
			foreach (var Client in P4Clients)
			{
				if (!String.IsNullOrEmpty(Client.Host) && String.Compare(Client.Host, HostName, true) != 0)
				{
					Log.TraceInformation("Rejecting client because of different Host {0} \"{1}\" != \"{2}\"", Client.Name, Client.Host, HostName);
					continue;
				}
				
				MatchingClients.Add(Client);
			}

			P4ClientInfo ClientToUse = null;
			if (MatchingClients.Count == 0)
			{
				throw new AutomationException("No matching clientspecs found!");
			}
			else if (MatchingClients.Count == 1)
			{
				ClientToUse = MatchingClients[0];
			}
			else
			{
				// We may have empty host clients here, so pick the first non-empty one if possible
				foreach (var Client in MatchingClients)
				{
					if (!String.IsNullOrEmpty(Client.Host) && String.Compare(Client.Host, HostName, true) == 0)
					{
						ClientToUse = Client;
						break;
					}
				}
				if (ClientToUse == null)
				{
					Log.TraceWarning("{0} clients found that match the current host and root path. The most recently accessed client will be used.", MatchingClients.Count);
					ClientToUse = GetMostRecentClient(MatchingClients);
				}
			}

			return ClientToUse;
		}

		/// <summary>
		/// Given a list of clients with the same owner and root path, tries to find the most recently accessed one.
		/// </summary>
		/// <param name="Clients">List of clients with the same owner and path.</param>
		/// <returns>The most recent client from the list.</returns>
		private static P4ClientInfo GetMostRecentClient(List<P4ClientInfo> Clients)
		{
			Log.TraceVerbose("Detecting the most recent client.");
			P4ClientInfo MostRecentClient = null;
			var MostRecentAccessTime = DateTime.MinValue;
			foreach (var ClientInfo in Clients)
			{
				if (ClientInfo.Access.Ticks > MostRecentAccessTime.Ticks)
				{
					MostRecentAccessTime = ClientInfo.Access;
					MostRecentClient = ClientInfo;
				}
			}
			if (MostRecentClient == null)
			{
				throw new AutomationException("Failed to determine the most recent client in {0}", Clients[0].RootPath);
			}
			return MostRecentClient;
		}

		/// <summary>
		/// Detects the current changelist the workspace is synced to.
		/// </summary>
		/// <param name="ClientRootPath">Workspace path.</param>
		/// <returns>Changelist number as a string.</returns>
		private static string DetectCurrentCL(P4Connection Connection, string ClientRootPath)
		{
			CommandUtils.LogVerbose("uebp_CL not set, detecting 'have' CL...");

			// Retrieve the current changelist 
			var P4Result = Connection.P4("changes -m 1 " + CommandUtils.CombinePaths(PathSeparator.Depot, ClientRootPath, "/...#have"), AllowSpew: false);
			var CLTokens = P4Result.Output.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
			var CLString = CLTokens[1];
			var CL = Int32.Parse(CLString);
			if (CLString != CL.ToString())
			{
				throw new AutomationException("Failed to retrieve current changelist.");
			}
			return CLString;
		}

		/// <summary>
		/// Detects the current code changelist the workspace is synced to.
		/// </summary>
		/// <param name="ClientRootPath">Workspace path.</param>
		/// <returns>Changelist number as a string.</returns>
		private static string DetectCurrentCodeCL(P4Connection Connection, string ClientRootPath)
		{
			CommandUtils.LogVerbose("uebp_CodeCL not set, detecting last code CL...");

			// Retrieve the current changelist 
			string P4Cmd = String.Format("changes -m 1 \"{0}/....cpp#have\" \"{0}/....h#have\" \"{0}/....inl#have\" \"{0}/....cs#have\" \"{0}/....usf#have\" \"{0}/....ush#have\"", CommandUtils.CombinePaths(PathSeparator.Depot, ClientRootPath));
			IProcessResult P4Result = Connection.P4(P4Cmd, AllowSpew: false);

			// Loop through all the lines of the output. Even though we requested one result, we'll get one for each search pattern.
			int CL = 0;
			foreach(string Line in P4Result.Output.Split('\n'))
			{
				string[] Tokens = Line.Trim().Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
				if(Tokens.Length >= 2)
				{
					int LineCL = Int32.Parse(Tokens[1]);
					CL = Math.Max(CL, LineCL);
				}
			}
			return CL.ToString();
		}

		/// <summary>
		/// Detects root paths for the specified client.
		/// </summary>
		/// <param name="UATLocation">AutomationTool.exe location</param>
		/// <param name="ThisClient">Client to detect the root paths for</param>
		/// <param name="BuildRootPath">Build root</param>
		/// <param name="LocalRootPath">Local root</param>
		/// <param name="ClientRootPath">Client root</param>
		private static void DetectRootPaths(P4Connection Connection, string LocalRootPath, P4ClientInfo ThisClient, out string BuildRootPath, out string ClientRootPath)
		{
			if(!String.IsNullOrEmpty(ThisClient.Stream))
			{
				BuildRootPath = ThisClient.Stream;
				ClientRootPath = String.Format("//{0}", ThisClient.Name);
			}
			else
			{
				// Figure out the build root
				string KnownFilePathFromRoot = CommandEnvironment.KnownFileRelativeToRoot;
				string KnownLocalPath = CommandUtils.MakePathSafeToUseWithCommandLine(CommandUtils.CombinePaths(PathSeparator.Slash, LocalRootPath, KnownFilePathFromRoot));
				IProcessResult P4Result = Connection.P4(String.Format("files -m 1 {0}", KnownLocalPath), AllowSpew: false);

				string KnownFileDepotMapping = P4Result.Output;

				// Get the build root
				Log.TraceVerbose("Looking for {0} in {1}", KnownFilePathFromRoot, KnownFileDepotMapping);
				int EndIdx = KnownFileDepotMapping.IndexOf(KnownFilePathFromRoot, StringComparison.CurrentCultureIgnoreCase);
				if (EndIdx < 0)
				{
					EndIdx = KnownFileDepotMapping.IndexOf(CommandUtils.ConvertSeparators(PathSeparator.Slash, KnownFilePathFromRoot), StringComparison.CurrentCultureIgnoreCase);
				}
				// Get the root path without the trailing path separator
				BuildRootPath = KnownFileDepotMapping.Substring(0, EndIdx - 1);

				// Get the client root
				if (LocalRootPath.StartsWith(CommandUtils.CombinePaths(PathSeparator.Slash, ThisClient.RootPath, "/"), StringComparison.InvariantCultureIgnoreCase) || LocalRootPath == CommandUtils.CombinePaths(PathSeparator.Slash, ThisClient.RootPath))
				{
					ClientRootPath = CommandUtils.CombinePaths(PathSeparator.Depot, String.Format("//{0}", ThisClient.Name), LocalRootPath.Substring(ThisClient.RootPath.Length));
				}
				else
				{
					throw new AutomationException("LocalRootPath ({0}) does not start with the client root path ({1})", LocalRootPath, ThisClient.RootPath);
				}
			}
		}
	}
}
