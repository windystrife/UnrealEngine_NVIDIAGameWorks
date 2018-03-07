// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Security;
using System.Security.Principal;
using System.Security.Permissions;
using System.Runtime.InteropServices;
using System.Runtime.ConstrainedExecution;
using Microsoft.Win32.SafeHandles;
using AutomationTool;
using System.Threading;
using UnrealBuildTool;

[Help("Tests P4 functionality. Runs 'p4 info'.")]
[RequireP4]
[DoesNotNeedP4CL]
class TestP4_Info : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("P4CLIENT={0}", GetEnvVar("P4CLIENT"));
		Log("P4PORT={0}", GetEnvVar("P4PORT"));

		var Result = P4.P4("info");
		if (Result.ExitCode != 0)
		{
			throw new AutomationException("p4 info failed: {0}", Result.Output);
		}
	}
}

[Help("GitPullRequest")]
[Help("Dir=", "Directory of the Git repo.")]
[Help("PR=", "PR # to shelve, can use a range PR=5-25")]
[RequireP4]
[DoesNotNeedP4CL]
class GitPullRequest : BuildCommand
{
	/// URL to our UnrealEngine repository on GitHub
	static readonly string GitRepositoryURL_Engine = "https://github.com/EpicGames/UnrealEngine.git";
	static readonly string GitRepositoryURL_UT = "https://github.com/EpicGames/UnrealTournament.git";
	string GitRepositoryURL = null;

	bool bDoingUT = false;

	string FindExeFromPath(string ExeName, string ExpectedPathSubstring = null)
	{
		if (File.Exists(ExeName))
		{
			return Path.GetFullPath(ExeName);
		}

		foreach (string BasePath in Environment.GetEnvironmentVariable("PATH").Split(Path.PathSeparator))
		{
			var FullPath = Path.Combine(BasePath, ExeName);
			if (ExpectedPathSubstring == null || FullPath.IndexOf(ExpectedPathSubstring, StringComparison.InvariantCultureIgnoreCase) != -1)
			{
				if (File.Exists(FullPath))
				{
					return FullPath;
				}
			}
		}

		return null;
	}
	string RunGit(string GitCommandLine)
	{
		string GitExePath = FindExeFromPath("Git.exe", "PortableGit");
		if (GitExePath == null)
		{
			throw new AutomationException("Unable to find Git.exe in the system path under a 'PortableGit' subdirectory.  Make sure the GitHub client is installed, and you are running this script from within a GitHub Command Shell.  We want to make sure we're using the correct version of Git, in case multiple versions are on the computer, which is why we check for a PortableGit folder that the GitHub Shell uses.");
		}

		Log("Running {0} {1}", GitExePath, GitCommandLine);

		IProcessResult Result = Run(App: GitExePath, CommandLine: GitCommandLine, Options: (ERunOptions.NoLoggingOfRunCommand | ERunOptions.AllowSpew | ERunOptions.AppMustExist));
		if (Result.ExitCode != 0)
		{
			throw new AutomationException(String.Format("Command failed (Result:{2}): {0} {1}", GitExePath, GitCommandLine, Result.ExitCode));
		}

		// Return the results (sans leading and trailing whitespace)
		return Result.Output.Trim();
	}

	bool ScanForBranchAndCL_BaseVersion(string GitCommand, out string Depot, out int CL)
	{
		Depot = null;
		CL = 0;
		try
		{
			var Base = RunGit(GitCommand);

			string BaseStart = "Engine source (";
			string BaseEnd = ")";

			if (Base.Contains(BaseStart) && Base.Contains(BaseEnd))
			{
				Base = Base.Substring(Base.IndexOf(BaseStart) + BaseStart.Length);
				if (Base.StartsWith("4."))
				{
					Depot = "//depot/UE4-Releases/4." + Base.Substring(2, 1);
				}
				else if (Base.StartsWith("Main"))
				{
					Depot = "//depot/UE4";
				}
				else if (Base.StartsWith("UT"))
				{
					Depot = "//depot/UE4-UT";
				}
				else
				{
					throw new AutomationException("Unrecognized branch.");
				}
				Log("Depot {0}", Depot);

				Base = Base.Substring(0, Base.IndexOf(BaseEnd));
				if (!Base.Contains(" "))
				{
					throw new AutomationException("Unrecognized commit3.");
				}
				Base = Base.Substring(Base.LastIndexOf(" "));
				Log("CL String {0}", Base);
				CL = int.Parse(Base);
			}
			Log("CL int {0}", CL);
			if (CL < 2000000 || String.IsNullOrWhiteSpace(Depot))
			{
				throw new AutomationException("Unrecognized commit3.");
			}

			return true;
		}
		catch (Exception)
		{
			CL = 0;
			return false;
		}
	}

	bool ScanForBranchAndCL_LiveVersion(string GitCommand, out string Depot, out int CL)
	{
		Depot = null;
		CL = 0;
		try
		{
			var Base = RunGit(GitCommand);

			string LiveStart = "[CL ";
			string LiveEnd = " branch]";

			if (Base.Contains(LiveStart) && Base.Contains(LiveEnd))
			{
				var CLStuff = Base.Substring(Base.IndexOf(LiveStart) + LiveStart.Length);
				if (CLStuff.IndexOf(" ") <= 0)
				{
					throw new AutomationException("Unrecognized commit5.");
				}
				CLStuff = CLStuff.Substring(0, CLStuff.IndexOf(" "));
				Log("CL String {0}", CLStuff);
				CL = int.Parse(CLStuff);

				var BranchStuff = Base.Substring(Base.IndexOf(LiveStart) + LiveStart.Length, Base.IndexOf(LiveEnd) - Base.IndexOf(LiveStart) - LiveStart.Length);
				if (BranchStuff.IndexOf(" ") <= 0)
				{
					throw new AutomationException("Unrecognized commit6.");
				}
				BranchStuff = BranchStuff.Substring(BranchStuff.LastIndexOf(" ") + 1);
				Log("Branch String {0}", BranchStuff);
				if (BranchStuff.StartsWith("4."))
				{
					Depot = "//depot/UE4-Releases/4." + BranchStuff.Substring(2, 1);
				}
				else if (BranchStuff.StartsWith("Main"))
				{
					Depot = "//depot/UE4";
				}
				else if (BranchStuff.StartsWith("UT"))
				{
					Depot = "//depot/UE4-UT";
				}
				else
				{
					throw new AutomationException("Unrecognized branch2.");
				}
			}
			Log("CL int {0}", CL);
			if (CL < 2000000 || String.IsNullOrWhiteSpace(Depot))
			{
				throw new AutomationException("Unrecognized commit3.");
			}

			return true;
		}
		catch (Exception)
		{
			CL = 0;
			return false;
		}
	}

	void ExecuteInner(string Dir, int PR)
	{
		string PRNum = PR.ToString();

		// Discard any old changes we may have committed
		RunGit("reset --hard");

		// show-ref is just a double check that the PR exists
		//var Refs = RunGit("show-ref");
		/*if (!Refs.Contains("refs/remotes/origin/pr/" + PRNum))
		{
			throw new AutomationException("This is not among the refs: refs/remotes/origin/pr/{0}", PRNum);
		}*/
		RunGit(String.Format("fetch origin refs/pull/{0}/head:pr/{1}", PRNum, PRNum));
		RunGit(String.Format("checkout pr/{0} --", PRNum));

		int CLBase;
		string DepotBase;
		ScanForBranchAndCL_BaseVersion(String.Format("log --author=TimSweeney --author=UnrealBot -100 pr/{0} --", PRNum), out DepotBase, out CLBase);


		int CLLive;
		string DepotLive;
		ScanForBranchAndCL_LiveVersion(String.Format("log -100 pr/{0} --", PRNum), out DepotLive, out CLLive);

		if (CLLive == 0 && CLBase == 0)
		{
			throw new AutomationException("Could not find a base change and branch using either method.");
		}

		int CL = 0;
		string Depot = "";

		if (CLBase > CLLive)
		{
			CL = CLBase;
			Depot = DepotBase;
		}
		else
		{
			CL = CLLive;
			Depot = DepotLive;
		}
		if (CL < 2000000 || String.IsNullOrWhiteSpace(Depot))
		{
			throw new AutomationException("Could not find a base change and branch using either method.");
		}


		P4ClientInfo NewClient = P4.GetClientInfo(P4Env.Client);

		foreach (var p in NewClient.View)
		{
			Log("{0} = {1}", p.Key, p.Value);
		}

		string TestClient = P4Env.User + "_" + Environment.MachineName + "_PullRequests_" + Depot.Replace("/", "_");
		NewClient.Owner = P4Env.User;
		NewClient.Host = Environment.MachineName;
		NewClient.RootPath = Dir;
		NewClient.Name = TestClient;
		NewClient.View = new List<KeyValuePair<string, string>>();
		NewClient.View.Add(new KeyValuePair<string, string>(Depot + "/...", "/..."));
		NewClient.Stream = null;
		if (!P4.DoesClientExist(TestClient))
		{
			P4.CreateClient(NewClient);
		}

		var P4Sub = new P4Connection(P4Env.User, TestClient, P4Env.ServerAndPort);

		P4Sub.Sync(String.Format("-f -k -q {0}/...@{1}", Depot, CL));

		var Change = P4Sub.CreateChange(null, String.Format("GitHub pull request #{0}", PRNum));
		P4Sub.ReconcileNoDeletes(Change, CommandUtils.MakePathSafeToUseWithCommandLine(CombinePaths(Dir, bDoingUT ? "UnrealTournament" : "Engine", "...")));
		P4Sub.Shelve(Change);
		P4Sub.Revert(Change, "-k //...");
	}

	public override void ExecuteBuild()
	{
		if (ParseParam("UT"))
		{
			bDoingUT = true;
			GitRepositoryURL = GitRepositoryURL_UT;
		}
		else
		{
			bDoingUT = false;
			GitRepositoryURL = GitRepositoryURL_Engine;
		}
		var Dir = ParseParamValue("Dir");
		if (String.IsNullOrEmpty(Dir))
		{
			// No Git repo directory was specified, so we'll choose a directory automatically
			Dir = Path.GetFullPath(Path.Combine(CmdEnv.LocalRoot, "Engine", "Intermediate", bDoingUT ? "PullRequestGitRepo_UT" : "PullRequestGitRepo"));
		}

		var PRNum = ParseParamValue("PR");
		if (String.IsNullOrEmpty(PRNum))
		{
			throw new AutomationException("Must Provide PR arg, the number of the PR, or a range.");
		}
		int PRMin;
		int PRMax;

		if (PRNum.Contains("-"))
		{
			var Nums = PRNum.Split("-".ToCharArray());
			PRMin = int.Parse(Nums[0]);
			PRMax = int.Parse(Nums[1]);
		}
		else
		{
			PRMin = int.Parse(PRNum);
			PRMax = PRMin;
		}
		var Failures = new List<string>();


		// Setup Git repo
		{
			if (ParseParam("Clean"))
			{
				Console.WriteLine("Cleaning temporary Git repository folder... ");

				// Wipe the Git repo directory
				if (!InternalUtils.SafeDeleteDirectory(Dir))
				{
					throw new AutomationException("Unable to clean out temporary Git repo directory: " + Dir);
				}
			}

			// Change directory to the Git repository
			bool bRepoAlreadyExists = InternalUtils.SafeDirectoryExists(Dir);
			if (!bRepoAlreadyExists)
			{
				InternalUtils.SafeCreateDirectory(Dir);
			}
			PushDir(Dir);

			if (!bRepoAlreadyExists)
			{
				// Don't init Git if we didn't clean, because the old repo is probably still there.

				// Create the Git repository
				RunGit("init");
			}

			// Make sure that creating the repository worked OK
			RunGit("status");

			// Check to see if we already have a remote origin setup
			{
				string Result = RunGit("remote -v");
				if (Result == "origin")
				{
					// OK, we already have an origin but no remote is associated with it.  We'll do that now.
					RunGit("remote set-url origin " + GitRepositoryURL);
				}
				else if (Result.IndexOf(GitRepositoryURL, StringComparison.InvariantCultureIgnoreCase) != -1)
				{
					// Origin is already setup!  Nothing to do.
				}
				else
				{
					// No origin is set, so let's add it!
					RunGit("remote add origin " + GitRepositoryURL);
				}
			}

			// Fetch all of the remote branches/tags into our local index.  This is needed so that we can figure out
			// which branches exist already.
			RunGit("fetch");
		}

		for (int PR = PRMin; PR <= PRMax; PR++)
		{
			try
			{
				ExecuteInner(Dir, PR);
			}
			catch (Exception Ex)
			{
				Log(" Exception was {0}", LogUtils.FormatException(Ex));
				Failures.Add(String.Format("PR {0} Failed with {1}", PR, LogUtils.FormatException(Ex)));
			}
		}
		PopDir();

		foreach (var Failed in Failures)
		{
			Log("{0}", Failed);
		}
	}
}

[Help("Throws an automation exception.")]
class TestFail : BuildCommand
{
	public override void ExecuteBuild()
	{
		throw new AutomationException("TestFail throwing an exception, cause that is what I do.");
	}
}

[Help("Prints a message and returns success.")]
class TestSuccess : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("TestSuccess message.");
	}
}

[Help("Prints a message and returns success.")]
class TestMessage : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("TestMessage message.");
		Log("you must be in error");
		Log("Behold the Error of you ways");
		Log("ERROR, you are silly");
		Log("ERROR: Something must be broken");
	}
}

[Help("Calls UAT recursively with a given command line.")]
class TestRecursion : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("TestRecursion *********************");
		string Params = ParseParamValue("Cmd");
		Log("Recursive Command: {0}", Params);
		RunUAT(CmdEnv, Params);

	}
}
[Help("Calls UAT recursively with a given command line.")]
class TestRecursionAuto : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("TestRecursionAuto *********************");
		RunUAT(CmdEnv, "TestMessage");
		RunUAT(CmdEnv, "TestMessage");
		RunUAT(CmdEnv, "TestRecursion -Cmd=TestMessage");
		RunUAT(CmdEnv, "TestRecursion -Cmd=TestFail");

	}
}

[Help("Makes a zip file in Rocket/QFE")]
class TestMacZip : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("TestMacZip *********************");

		if (UnrealBuildTool.Utils.IsRunningOnMono)
		{
			PushDir(CombinePaths(CmdEnv.LocalRoot, "Rocket/QFE"));
			RunAndLog(CommandUtils.CmdEnv, "zip", "-r TestZip .");
			PopDir();
		}
		else
		{
			throw new AutomationException("This probably only works on the mac.");
		}
	}
}

[Help("Tests P4 functionality. Creates a new changelist under the workspace %P4CLIENT%")]
[RequireP4]
class TestP4_CreateChangelist : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("P4CLIENT={0}", GetEnvVar("P4CLIENT"));
		Log("P4PORT={0}", GetEnvVar("P4PORT"));

		var CLDescription = "AutomationTool TestP4";

		Log("Creating new changelist \"{0}\" using client \"{1}\"", CLDescription, GetEnvVar("P4CLIENT"));
		var ChangelistNumber = P4.CreateChange(Description: CLDescription);
		Log("Created changelist {0}", ChangelistNumber);
	}
}

[RequireP4]
class TestP4_StrandCheckout : BuildCommand
{
	public override void ExecuteBuild()
	{
		int WorkingCL = P4.CreateChange(P4Env.Client, String.Format("TestP4_StrandCheckout, head={0}", P4Env.Changelist));

		Log("Build from {0}    Working in {1}", P4Env.Changelist, WorkingCL);

		List<string> Sign = new List<string>();
		Sign.Add(CombinePaths(CmdEnv.LocalRoot, @"\Engine\Binaries\DotNET\AgentInterface.dll"));

		Log("Signing and adding {0} build products to changelist {1}...", Sign.Count, WorkingCL);

		CodeSign.SignMultipleIfEXEOrDLL(this, Sign);
		foreach (var File in Sign)
		{
			P4.Sync("-f -k " + File + "#head"); // sync the file without overwriting local one
			if (!FileExists(File))
			{
				throw new AutomationException("BUILD FAILED {0} was a build product but no longer exists", File);
			}

			P4.ReconcileNoDeletes(WorkingCL, File);
		}
	}
}

[RequireP4]
class TestP4_LabelDescription : BuildCommand
{
	public override void ExecuteBuild()
	{
		string Label = GetEnvVar("SBF_LabelFromUser");
		string Desc;
		Log("LabelDescription {0}", Label);
		var Result = P4.LabelDescription(Label, out Desc);
		if (!Result)
		{
			throw new AutomationException("Could not get label description");
		}
		Log("Result***\n{0}\n***\n", Desc);
	}
}

[RequireP4]
class TestP4_ClientOps : BuildCommand
{
	public override void ExecuteBuild()
	{
		string TemplateClient = "ue4_licensee_workspace";
		var Clients = P4.GetClientsForUser("UE4_Licensee");

		string TestClient = "UAT_Test_Client";

		P4ClientInfo NewClient = null;
		foreach (var Client in Clients)
		{
			if (Client.Name == TemplateClient)
			{
				NewClient = Client;
				break;
			}
		}
		if (NewClient == null)
		{
			throw new AutomationException("Could not find template");
		}
		NewClient.Owner = P4Env.User; // this is not right, we need the actual licensee user!
		NewClient.Host = Environment.MachineName.ToLower();
		NewClient.RootPath = @"C:\TestClient";
		NewClient.Name = TestClient;
		if (P4.DoesClientExist(TestClient))
		{
			P4.DeleteClient(TestClient);
		}
		P4.CreateClient(NewClient);

		//P4CLIENT         Name of client workspace        p4 help client
		//P4PASSWD         User password passed to server  p4 help passwd

		string[] VarsToSave = 
        {
            "P4CLIENT",
            //"P4PASSWD",

        };
		var KeyValues = new Dictionary<string, string>();
		foreach (var ToSave in VarsToSave)
		{
			KeyValues.Add(ToSave, GetEnvVar(ToSave));
		}

		SetEnvVar("P4CLIENT", NewClient.Name);
		//SetEnv("P4PASSWD", ParseParamValue("LicenseePassword");


		//Sync(CombinePaths(PathSeparator.Slash, P4Env.BuildRootP4, "UE4Games.uprojectdirs"));
		string Output;
		P4.P4Output(out Output, "files -m 10 " + CombinePaths(PathSeparator.Slash, P4Env.Branch, "..."));

		var Lines = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
		string SlashRoot = CombinePaths(PathSeparator.Slash, P4Env.Branch, "/");
		string LocalRoot = CombinePaths(CmdEnv.LocalRoot, @"\");
		foreach (string Line in Lines)
		{
			if (Line.Contains(" - ") && Line.Contains("#"))
			{
				string depot = Line.Substring(0, Line.IndexOf("#"));
				if (!depot.Contains(SlashRoot))
				{
					throw new AutomationException("{0} does not contain {1} and it is supposed to.", depot, P4Env.Branch);
				}
				string local = CombinePaths(depot.Replace(SlashRoot, LocalRoot));
				Log("{0}", depot);
				Log("    {0}", local);
			}
		}

		// should be used as a sanity check! make sure there are no files!
		P4.LogP4Output(out Output, "files -m 10 " + CombinePaths(PathSeparator.Slash, P4Env.Branch, "...", "NoRedist", "..."));

		// caution this doesn't actually use the client _view_ from the template at all!
		// if you want that sync -f -k /...
		// then use client syntax in the files command instead
		// don't think we care, we don't rely on licensee _views_

		foreach (var ToLoad in VarsToSave)
		{
			SetEnvVar(ToLoad, KeyValues[ToLoad]);
		}
		P4.DeleteClient(TestClient);
	}
}


class CleanDDC : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("*********************** Clean DDC");

		bool DoIt = ParseParam("DoIt");

		for (int i = 0; i < 10; i++)
		{
			for (int j = 0; j < 10; j++)
			{
				for (int k = 0; k < 10; k++)
				{
					string Dir = CombinePaths(String.Format(@"P:\UE4DDC\{0}\{1}\{2}", i, j, k));
					if (!DirectoryExists_NoExceptions(Dir))
					{
						throw new AutomationException("Missing DDC dir {0}", Dir);
					}
					var files = FindFiles_NoExceptions("*.*", false, Dir);
					foreach (var file in files)
					{
						if (FileExists_NoExceptions(file))
						{
							FileInfo Info = new FileInfo(file);
							Log("{0}", file);
							if ((DateTime.UtcNow - Info.LastWriteTimeUtc).TotalDays > 20)
							{
								Log("  is old.");
								if (DoIt)
								{
									DeleteFile_NoExceptions(file);
								}
							}
							else
							{
								Log("  is NOT old.");
							}
						}
					}
				}
			}
		}
	}

}

class TestTestFarm : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("*********************** TestTestFarm");

		string Exe = CombinePaths(CmdEnv.LocalRoot, "Engine", "Binaries", "Win64", "UE4Editor.exe");
		string ClientLogFile = CombinePaths(CmdEnv.LogFolder, "HoverGameRun");
		string CmdLine = " ../../../Samples/HoverShip/HoverShip.uproject -game -log -abslog=" + ClientLogFile;

		IProcessResult App = Run(Exe, CmdLine, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);

		LogFileReaderProcess(ClientLogFile, App, (string Output) =>
		{
			if (!String.IsNullOrEmpty(Output) &&
					Output.Contains("Game Engine Initialized"))
			{
				Log("It looks started, let me kill it....");
				App.StopProcess();
			}
			return true; //keep reading
		});
	}
}

[Help("Test command line argument parsing functions.")]
class TestArguments : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("List of provided arguments: ");
		for (int Index = 0; Index < Params.Length; ++Index)
		{
			Log("{0}={1}", Index, Params[Index].ToString());
		}

		object[] TestArgs = new object[] { "NoXGE", "SomeArg", "Map=AwesomeMap", "run=whatever", "Stuff" };

		if (!ParseParam(TestArgs, "noxge"))
		{
			throw new AutomationException("ParseParam test failed.");
		}

		if (ParseParam(TestArgs, "Dude"))
		{
			throw new AutomationException("ParseParam test failed.");
		}

		if (!ParseParam(TestArgs, "Stuff"))
		{
			throw new AutomationException("ParseParam test failed.");
		}

		string Val = ParseParamValue(TestArgs, "map");
		if (Val != "AwesomeMap")
		{
			throw new AutomationException("ParseParamValue test failed.");
		}

		Val = ParseParamValue(TestArgs, "run");
		if (Val != "whatever")
		{
			throw new AutomationException("ParseParamValue test failed.");
		}
	}
}

[Help("Checks if combine paths works as excpected")]
public class TestCombinePaths : BuildCommand
{
	public override void ExecuteBuild()
	{
		var Results = new string[]
		{
			CombinePaths("This/", "Is", "A", "Test"),
			CombinePaths("This", "Is", "A", "Test"),
			CombinePaths("This/", @"Is\A", "Test"),
			CombinePaths(@"This\Is", @"A\Test"),
			CombinePaths(@"This\Is\A\Test"),
			CombinePaths("This/", @"Is\", "A", null, "Test", String.Empty),
			CombinePaths(null, "This/", "Is", @"A\", "Test")
		};

		for (int Index = 0; Index < Results.Length; ++Index)
		{
			var CombinedPath = Results[Index];
			Log(CombinedPath);
			if (CombinedPath != Results[0])
			{
				throw new AutomationException("Path {0} does not match path 0: {1} (expected: {2})", Index, CombinedPath, Results[0]);
			}
		}
	}
}

[Help("Tests file utility functions.")]
[Help("file=Filename", "Filename to perform the tests on.")]
class TestFileUtility : BuildCommand
{
	public override void ExecuteBuild()
	{
		string DummyFilename = ParseParamValue("file", "");
		DeleteFile(DummyFilename);
	}
}

class TestLog : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("************************* ShooterGameRun");
		string MapToRun = ParseParamValue("map", "/game/maps/sanctuary");

		PushDir(CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/"));
		//		string GameServerExe = CombinePaths(CmdEnv.LocalRoot, @"ShooterGame/Binaries/Win64/ShooterGameServer.exe");
		//		string GameExe = CombinePaths(CmdEnv.LocalRoot, @"ShooterGame/Binaries/Win64/ShooterGame.exe");
		string EditorExe = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/UE4Editor.exe");

		string ServerLogFile = CombinePaths(CmdEnv.LogFolder, "Server.log");

		string ServerApp = EditorExe;
		string OtherArgs = String.Format("{0} -server -abslog={1} -log", MapToRun, ServerLogFile);
		string StartArgs = "ShooterGame ";

		string ServerCmdLine = StartArgs + OtherArgs;

		IProcessResult ServerProcess = Run(ServerApp, ServerCmdLine, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);

		LogFileReaderProcess(ServerLogFile, ServerProcess, (string Output) =>
		{
			if (String.IsNullOrEmpty(Output) == false)
			{
				Console.Write(Output);
			}
			return true; // keep reading
		});
	}
}

[Help("Tests P4 change filetype functionality.")]
[Help("File", "Binary file to change the filetype to writeable")]
[RequireP4]
class TestChangeFileType : BuildCommand
{
	public override void ExecuteBuild()
	{
		var Filename = ParseParamValue("File", "");
		if (P4.FStat(Filename).Type == P4FileType.Binary)
		{
			P4.ChangeFileType(Filename, P4FileAttributes.Writeable);
		}
	}
}

class TestGamePerf : BuildCommand
{
	[DllImport("advapi32.dll", SetLastError = true)]
	private static extern bool LogonUser(string UserName, string Domain, string Password, int LogonType, int LogonProvider, out SafeTokenHandle SafeToken);

	public override void ExecuteBuild()
	{
		Log("*********************** TestGamePerf");

		string UserName = "test.automation";
		string Password = "JJGfh9CX";
		string Domain = "epicgames.net";
		string FileName = string.Format("UStats_{0:MM-d-yyyy_hh-mm-ss-tt}.csv", DateTime.Now);
		string Exe = CombinePaths(CmdEnv.LocalRoot, "Engine", "Binaries", "Win64", "UE4Editor.exe");

		string ClientLogFile = "";
		string CmdLine = "";
		string UStatsPath = "";

		string LevelParam = ParseParamValue("level", "");

		#region Arguments
		switch (LevelParam)
		{
			case "PerfShooterGame_Santuary":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "ShooterGame.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "ShooterGame", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "ShooterGame", "ShooterGame.uproject ") + CmdEnv.LocalRoot + @"/ShooterGame/Content/Maps/TestMaps/Sanctuary_PerfLoader.umap?game=ffa -game -novsync";
				break;

			case "PerfShooterGame_Highrise":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "ShooterGame.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "ShooterGame", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "ShooterGame", "ShooterGame.uproject ") + CmdEnv.LocalRoot + @"/ShooterGame/Content/Maps/TestMaps/Highrise_PerfLoader.umap?game=ffa -game -novsync";
				break;

			case "PerfHoverShip":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "HoverShip.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "SampleGames", "HoverShip", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "HoverShip", "HoverShip.uproject ") + CmdEnv.LocalRoot + @"/Samples/HoverShip/Content/Maps/HoverShip_01.umap -game -novsync";
				break;

			case "PerfInfiltrator":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "Infiltrator.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "Infiltrator", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "Infiltrator", "Infiltrator.uproject ") + CmdEnv.LocalRoot + @"/Samples/Showcases/Infiltrator/Content/Maps/TestMaps/Visuals/VIS_ArtDemo_PerfLoader.umap -game -novsync";
				break;

			case "PerfElemental":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "Elemental.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "Elemental", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "Elemental", "Elemental.uproject ") + CmdEnv.LocalRoot + @"/Samples/Showcases/Elemental/Content/Misc/Automated_Perf/Elemental_PerfLoader.umap -game -novsync";
				break;

			case "PerfStrategyGame":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "StrategyGame.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "StrategyGame", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "StrategyGame", "StrategyGame.uproject ") + CmdEnv.LocalRoot + @"/StrategyGame/Content/Maps/TestMaps/TowerDefenseMap_PerfLoader.umap -game -novsync";
				break;

			case "PerfVehicleGame":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "VehicleGame.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "VehicleGame", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "VehicleGame", "VehicleGame.uproject ") + CmdEnv.LocalRoot + @"/VehicleGame/Content/Maps/TestMaps/VehicleTrack_PerfLoader.umap -game -novsync";
				break;

			case "PerfPlatformerGame":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "PlatformerGame.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "PlatformerGame", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "PlatformerGame", "PlatformerGame.uproject ") + CmdEnv.LocalRoot + @"/PlatformerGame/Content/Maps/Platformer_StreetSection.umap -game -novsync";
				break;

			case "PerfLanderGame":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "LanderGame.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "LanderGame", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "LanderGame", "LanderGame.uproject ") + CmdEnv.LocalRoot + @"/LanderGame/Content/Maps/sphereworld.umap -game -novsync";
				break;

			case "PerfDemoLets_DynamicLighting":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "DemoLets_DynamicLighting.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "DemoLets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "DemoLets", "DemoLets.uproject ") + CmdEnv.LocalRoot + @"/BasicMaterials/Content/Maps/Performance/Demolet_DynamicLighting_PerfLoader.umap -game -novsync";
				break;

			case "PerfDemoLets_BasicMaterials":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "DemoLets_BasicMaterials.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "DemoLets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "DemoLets", "DemoLets.uproject ") + CmdEnv.LocalRoot + @"/EffectsGallery/Content/Maps/Performance/BasicMaterials_PerfLoader.umap -game -novsync";
				break;

			case "PerfDemoLets_DemoRoom":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "DemoLets_DemoRoom.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "DemoLets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "DemoLets", "DemoLets.uproject ") + CmdEnv.LocalRoot + @"/EffectsGallery/Content/Maps/Performance/DemoRoom_Effects_hallway_PerfLoader.umap -game -novsync";
				break;

			case "PerfDemoLets_Reflections":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "DemoLets_Reflections.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "DemoLets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "DemoLets", "DemoLets.uproject ") + CmdEnv.LocalRoot + @"/EffectsGallery/Content/Maps/Performance/DemoLet_Reflections_PerfLoader.umap -game -novsync";
				break;

			case "PerfDemoLets_PostProcessing":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "DemoLets_PostProcessing.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "DemoLets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "DemoLets", "DemoLets.uproject ") + CmdEnv.LocalRoot + @"/EffectsGallery/Content/Maps/Performance/Demolet_PostProcessing_PerfLoader.umap -game -novsync";
				break;

			case "PerfDemoLets_Physics":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "DemoLets_Physics.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "DemoLets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "DemoLets", "DemoLets.uproject ") + CmdEnv.LocalRoot + @"/EffectsGallery/Content/Maps/Performance/Demolet_Physics_PerfLoader.umap -game -novsync";
				break;

			case "PerfDemoLets_ShadowMaps":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "DemoLets_ShadowMaps.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "DemoLets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "DemoLets", "DemoLets.uproject ") + CmdEnv.LocalRoot + @"/EffectsGallery/Content/Maps/Performance/Demolet_CascadingShadowMaps_PerfLoader.umap -game -novsync";
				break;

			case "PerfDemoLets_Weather":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "DemoLets_Weather.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "DemoLets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "DemoLets", "DemoLets.uproject ") + CmdEnv.LocalRoot + @"/EffectsGallery/Content/Maps/Performance/dynamic_weather_selector_PerfLoader.umap -game -novsync";
				break;

			case "PerfRealisticRendering_RoomNight":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "RealisticRendering_RoomNight.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "ShowCases", "RealisticRendering", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "RealisticRendering", "RealisticRendering.uproject ") + CmdEnv.LocalRoot + @"/RealisticRendering/Content/Maps/Performance/RoomNight_PerfLoader.umap -game -novsync";
				break;

			case "PerfRealisticRendering_NoLight":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "RealisticRendering_NoLight.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "ShowCases", "RealisticRendering", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "RealisticRendering", "RealisticRendering.uproject ") + CmdEnv.LocalRoot + @"/RealisticRendering/Content/Maps/Performance/RoomNightNoLights_PerfLoader.umap -game -novsync";
				break;

			case "PerfRealisticRendering_Room":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "RealisticRendering_Room.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "ShowCases", "RealisticRendering", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "RealisticRendering", "RealisticRendering.uproject ") + CmdEnv.LocalRoot + @"/RealisticRendering/Content/Maps/Performance/Room_PerfLoader.umap -game -novsync";
				break;

			case "PerfMorphTargets":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "MorphTargets.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "ShowCases", "MorphTargets", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "MorphTargets", "MorphTargets.uproject ") + CmdEnv.LocalRoot + @"/MorphTargets/Content/Maps/Performance/MorphTargets_PerfLoader.umap -game -novsync";
				break;

			case "PerfMatinee":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "Matinee.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "ShowCases", "PostProcessMatinee", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "PostProcessMatinee", "PostProcessMatinee.uproject ") + CmdEnv.LocalRoot + @"/PostProcessMatinee/Content/Maps/Performance/PostProcessMatinee_PerfLoader.umap -game -novsync";
				break;

			case "PerfLandScape":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "LandScape.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "ShowCases", "Landscape_WorldMachine", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "Landscape_WorldMachine", "Landscape_WorldMap.uproject ") + CmdEnv.LocalRoot + @"/Landscape_WorldMachine/Content/Maps/Performance/LandscapeMap_PerfLoader.umap -game -novsync";
				break;

			case "PerfLandScape_Moutain":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "LandScape_Moutain.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "ShowCases", "Landscape", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "Landscape", "Landscape.uproject ") + CmdEnv.LocalRoot + @"/Landscape/Content/Maps/Performance/MoutainRange_PerfLoader.umap -game -novsync";
				break;

			case "PerfMobile":
				ClientLogFile = CombinePaths(CmdEnv.LogFolder, "Mobile.txt");
				UStatsPath = CombinePaths(CmdEnv.LocalRoot, "Samples", "ShowCases", "Mobile", "Saved", "Profiling", "UnrealStats");
				CmdLine = CombinePaths(CmdEnv.LocalRoot, "Samples", "Showcases", "Mobile", "Mobile.uproject ") + CmdEnv.LocalRoot + @"/Mobile/Content/Maps/Performance/Testmap_PerfLoader.umap -game -novsync";
				break;

			case "PerfSampleGames":
				ClientLogFile = CmdEnv.LogFolder;
				break;

			default:
				break;
		}
		#endregion

		try
		{
			SafeTokenHandle SafeToken;

			int LogonInteractive = 2;
			int ProviderDefualt = 0;

			/*bool bLogonSuccess = */
			LogonUser(UserName, Domain, Password, LogonInteractive, ProviderDefualt, out SafeToken);

			using (SafeToken)
			{
				using (WindowsIdentity NewUser = new WindowsIdentity(SafeToken.DangerousGetHandle()))
				{
					using (WindowsImpersonationContext TestAccount = NewUser.Impersonate())
					{
						if (LevelParam == "PerfSampleGames")
						{

						}
						else
						{
							if (!File.Exists(ClientLogFile))
							{
								Log("Creating log file");

								File.Create(ClientLogFile).Close();

								Log(ClientLogFile);

								Log("Log file created");
							}

							RunAndLog(Exe, CmdLine, ClientLogFile);
						}

						DirectoryInfo[] UStatsDirectories = new DirectoryInfo(UStatsPath).GetDirectories();
						DirectoryInfo CurrentUStatsDirectory = UStatsDirectories[0];

						for (int i = 0; i < UStatsDirectories.Length; i++)
						{
							if (UStatsDirectories[i].LastWriteTime > CurrentUStatsDirectory.LastWriteTime)
							{
								CurrentUStatsDirectory = UStatsDirectories[i];
							}
						}

						FileInfo[] UStatsFilePaths = new DirectoryInfo(CurrentUStatsDirectory.FullName).GetFiles();
						FileInfo CurrentUStatsFilePath = UStatsFilePaths[0];

						for (int i = 0; i < UStatsFilePaths.Length; i++)
						{
							if (UStatsFilePaths[i].LastWriteTime > CurrentUStatsFilePath.LastWriteTime)
							{
								CurrentUStatsFilePath = UStatsFilePaths[i];
							}
						}
						try
						{
							string FrontEndExe = CombinePaths(CmdEnv.LocalRoot, "Engine", "Binaries", "Win64", "UnrealFrontend.exe");

							CmdLine = " -run=convert -infile=" + CurrentUStatsFilePath.FullName + @" -outfile=D:\" + FileName + " -statlist=GameThread+RenderThread+StatsThread+STAT_FrameTime+STAT_PhysicalAllocSize+STAT_VirtualAllocSize";

							RunAndLog(FrontEndExe, CmdLine, ClientLogFile);
						}

						catch (Exception Err)
						{
							Log(Err.Message);
						}
					}
				}
			}
		}

		catch (Exception Err)
		{
			Log(Err.Message);
		}
	}
}

public sealed class SafeTokenHandle : SafeHandleZeroOrMinusOneIsInvalid
{
	private SafeTokenHandle()
		: base(true)
	{

	}

	[DllImport("kernel32.dll")]
	[ReliabilityContract(Consistency.WillNotCorruptState, Cer.Success)]
	[SuppressUnmanagedCodeSecurity]
	[return: MarshalAs(UnmanagedType.Bool)]
	private static extern bool CloseHandle(IntPtr Handle);

	protected override bool ReleaseHandle()
	{
		return CloseHandle(handle);
	}
}

[Help("Tests if UE4Build properly copies all relevent UAT build products to the Binaries folder.")]
public class TestUATBuildProducts : BuildCommand
{
	public override void ExecuteBuild()
	{
		UE4Build Build = new UE4Build(this);
		Build.AddUATFilesToBuildProducts();

		Log("Build products:");
		foreach (var Product in Build.BuildProductFiles)
		{
			Log("  " + Product);
		}
	}
}



[Help("Tests WatchdogTimer functionality. The correct result is to exit the application with ExitCode=1 after a few seconds.")]
public class TestWatchdogTimer : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("Starting 1st timer (1s). This should not crash.");
		using (var SafeTimer = new WatchdogTimer(1))
		{
			// Wait 500ms
			Log("Started {0}", SafeTimer.GetProcessName());
			Thread.Sleep(500);
		}
		Log("First timer disposed successfully.");

		Log("Starting 2nd timer (2s). This should throw an exception after 1 second.");
		try
		{
			using (var CrashTimer = new WatchdogTimer(2))
			{
				// Wait 5s (this will trigger the watchdog timer)
				Log("Started {0}", CrashTimer.GetProcessName());
				Thread.Sleep(1000);
				throw new Exception("Test exceptions under WatchdogTimer");
			}
		}
		catch (Exception Ex)
		{
			Log("Triggered exception guarded by WatchdogTimer:");
			Log(Ex.Message);
		}

		Log("Starting 3rd timer (2s). This should crash after 2 seconds.");
		using (var CrashTimer = new WatchdogTimer(2))
		{
			// Wait 5s (this will trigger the watchdog timer)
			Log("Started {0}", CrashTimer.GetProcessName());
			Thread.Sleep(5000);
		}
	}
}

class TestOSSCommands : BuildCommand
{
	public override void ExecuteBuild()
	{
		string Exe = CombinePaths(CmdEnv.LocalRoot, "Engine", "Binaries", "Win64", "UE4Editor.exe");
		string CmdLine = "qagame automation-osscommands -game -log -logcmds=" + "\"" + "global none, logonline verbose, loghttp log, LogBlueprintUserMessages log" + "\"";
		string ClientLogFile = CombinePaths(CmdEnv.LogFolder, String.Format("QALog_{0:yyyy-MM-dd_hh-mm-ss-tt}.txt", DateTime.Now));
		string LogDirectory = CombinePaths(CmdEnv.LocalRoot, "Saved", "Logs");

		if (!File.Exists(ClientLogFile))
		{
			File.Create(ClientLogFile).Close();
		}

		try
		{
			RunAndLog(Exe, CmdLine, ClientLogFile);
		}
		catch (Exception Err)
		{
			Log(Err.Message);
		}

		FileInfo[] LogFiles = new DirectoryInfo(LogDirectory).GetFiles();
		FileInfo QALogFile = LogFiles[0];

		for (int i = 0; i < LogFiles.Length; i++)
		{
			if (LogFiles[i].LastWriteTime > QALogFile.LastWriteTime)
			{
				QALogFile = LogFiles[i];
			}
		}

		string[] Lines = File.ReadAllLines(QALogFile.FullName);

		StreamWriter WriteErrors = new StreamWriter(new FileStream(ClientLogFile, FileMode.Append, FileAccess.Write, FileShare.ReadWrite));

		# region Error List
		/* 
         * online sub=amazon test friends - Warning: Failed to get friends interface for amazon
         * online sub=amazon test identity - Error: RegisterUser() : OnlineSubsystemAmazon is improperly configured in DefaultEngine.ini
         * ErrorCode 00002EE7 - http test <iterations> <url> - Desc: The server name or address could not be resolved
         * Error/code 404 - online sub=mcp test validateaccount <email> <validation code> - Warning: MCP: Mcp validate Epic account request failed. Invalid response. Not found.
         * Error/code 404 - online sub=mcp test deleteaccount <email> <password> - Warning: MCP: Mcp delete Epic account request failed. Not found.
         * online sub=mcp test friends - Warning: Failed to get friends interface for mcp
         * bSucess: 0 - online sub=mcp test sessionhost - LogOnline:Verbose: OnCreateSessionComplete Game bSuccess: 0
         * Code 401 - online sub=mcp test time - Failed to query server time
         * Error code 500 - online sub=mcp test entitlements - profile template not found
        */
		#endregion

		using (WriteErrors)
		{
			for (int i = 0; i < Lines.Length; i++)
			{
				if (Lines[i].Contains("error=404") || Lines[i].Contains("code=404"))
				{
					WriteErrors.WriteLine(Lines[i]);
				}
				else if (Lines[i].Contains("code=401"))
				{
					WriteErrors.WriteLine(Lines[i]);
				}
				else if (Lines[i].Contains("error=500"))
				{
					WriteErrors.WriteLine(Lines[i]);
				}
				else if (Lines[i].Contains("bSucess: 0"))
				{
					WriteErrors.WriteLine(Lines[i]);
				}
				else if (Lines[i].Contains("Warning: Failed to get friends interface"))
				{
					WriteErrors.WriteLine(Lines[i]);
				}
				else if (Lines[i].Contains("Error: RegisterUser() : OnlineSubsystemAmazon is improperly configured"))
				{
					WriteErrors.WriteLine(Lines[i]);
				}
				else if (Lines[i].Contains("ErrorCode 00002EE7"))
				{
					WriteErrors.WriteLine(Lines[i]);
				}
			}
		}

	}
}

[Help("Builds a project using UBT. Syntax is similar to UBT with the exception of '-', i.e. UBT -QAGame -Development -Win32")]
[Help("NoXGE", "Disable XGE")]
[Help("Clean", "Clean build products before building")]
public class UBT : BuildCommand
{
	public override void ExecuteBuild()
	{
		var Build = new UE4Build(this);
		var Agenda = new UE4Build.BuildAgenda();
		var Platform = UnrealBuildTool.UnrealTargetPlatform.Win64;
		var Configuration = UnrealBuildTool.UnrealTargetConfiguration.Development;
		var Targets = new List<string>();

		foreach (var ObjParam in Params)
		{
			var Param = (string)ObjParam;
			UnrealBuildTool.UnrealTargetPlatform ParsePlatform;
			if (Enum.TryParse<UnrealBuildTool.UnrealTargetPlatform>(Param, true, out ParsePlatform))
			{
				Platform = ParsePlatform;
				continue;
			}
			UnrealBuildTool.UnrealTargetConfiguration ParseConfiguration;
			if (Enum.TryParse<UnrealBuildTool.UnrealTargetConfiguration>(Param, true, out ParseConfiguration))
			{
				Configuration = ParseConfiguration;
				continue;
			}
			if (String.Compare("NoXGE", Param, true) != 0 && String.Compare("Clean", Param, true) != 0)
			{
				Targets.Add(Param);
			}
		}

		var Clean = ParseParam("Clean");

		Agenda.AddTargets(Targets.ToArray(), Platform, Configuration);

		Log("UBT Buid");
		Log("Targets={0}", String.Join(",", Targets));
		Log("Platform={0}", Platform);
		Log("Configuration={0}", Configuration);
		Log("Clean={0}", Clean);

		Build.Build(Agenda, InUpdateVersionFiles: false);

		Log("UBT Completed");
	}
}

[Help("Helper command to sync only source code + engine files from P4.")]
[Help("Branch", "Optional branch depot path, default is: -Branch=//depot/UE4")]
[Help("CL", "Optional changelist to sync (default is latest), -CL=1234567")]
[Help("Sync", "Optional list of branch subforlders to always sync separated with '+', default is: -Sync=Engine/Binaries/ThirdParty+Engine/Content")]
[Help("Exclude", "Optional list of folder names to exclude from syncing, separated with '+', default is: -Exclude=Binaries+Content+Documentation+DataTables")]
[RequireP4]
public class SyncSource : BuildCommand
{
	private void SafeSync(string SyncCmdLine)
	{
		try
		{
			P4.Sync(SyncCmdLine);
		}
		catch (Exception Ex)
		{
			LogError("Unable to sync {0}", SyncCmdLine);
			LogError(Ex.Message);
		}
	}

	public override void ExecuteBuild()
	{
		var Changelist = ParseParamValue("cl", "");
		if (!String.IsNullOrEmpty(Changelist))
		{
			Changelist = "@" + Changelist;
		}

		var AlwaysSync = new List<string>(new string[]
			{
				"Engine/Binaries/ThirdParty",
				"Engine/Content",
			}
		);
		var AdditionalAlwaysSyncPaths = ParseParamValue("sync");
		if (!String.IsNullOrEmpty(AdditionalAlwaysSyncPaths))
		{
			var AdditionalPaths = AdditionalAlwaysSyncPaths.Split(new string[] { "+" }, StringSplitOptions.RemoveEmptyEntries);
			AlwaysSync.AddRange(AdditionalPaths);
		}

		var ExclusionList = new HashSet<string>(new string[]
			{
				"Binaries",
				"Content",
				"Documentation",
				"DataTables",
			},
			StringComparer.InvariantCultureIgnoreCase
		);
		var AdditionalExlusionPaths = ParseParamValue("exclude");
		if (!String.IsNullOrEmpty(AdditionalExlusionPaths))
		{
			var AdditionalPaths = AdditionalExlusionPaths.Split(new string[] { "+" }, StringSplitOptions.RemoveEmptyEntries);
			foreach (var Dir in AdditionalPaths)
			{
				ExclusionList.Add(Dir);
			}
		}

		var DepotPath = ParseParamValue("branch", "//depot/UE4/");
		foreach (var AlwaysSyncSubFolder in AlwaysSync)
		{
			var SyncCmdLine = CombinePaths(PathSeparator.Depot, DepotPath, AlwaysSyncSubFolder, "...") + Changelist;
			Log("Syncing {0}", SyncCmdLine, Changelist);
			SafeSync(SyncCmdLine);
		}

		DepotPath = CombinePaths(PathSeparator.Depot, DepotPath, "*");
		var ProjectDirectories = P4.Dirs(String.Format("-D {0}", DepotPath));
		foreach (var ProjectDir in ProjectDirectories)
		{
			var ProjectDirPath = CombinePaths(PathSeparator.Depot, ProjectDir, "*");
			var SubDirectories = P4.Dirs(ProjectDirPath);
			foreach (var SubDir in SubDirectories)
			{
				var SubDirName = Path.GetFileNameWithoutExtension(GetDirectoryName(SubDir));
				if (!ExclusionList.Contains(SubDirName))
				{
					var SyncCmdLine = CombinePaths(PathSeparator.Depot, SubDir, "...") + Changelist;
					Log("Syncing {0}", SyncCmdLine);
					SafeSync(SyncCmdLine);
				}
			}
		}
	}
}

[Help("Generates automation project based on a template.")]
[Help("project=ShortName", "Short name of the project, i.e. QAGame")]
[Help("path=FullPath", "Absolute path to the project root directory, i.e. C:\\UE4\\QAGame")]
public class GenerateAutomationProject : BuildCommand
{
	public override void ExecuteBuild()
	{
		var ProjectName = ParseParamValue("project");
		var ProjectPath = ParseParamValue("path");

		{
			var CSProjFileTemplate = ReadAllText(CombinePaths(CmdEnv.LocalRoot, "Engine", "Extras", "UnsupportedTools", "AutomationTemplate", "AutomationTemplate.Automation.xml"));
			StringBuilder CSProjBuilder = new StringBuilder(CSProjFileTemplate);
			CSProjBuilder.Replace("AutomationTemplate", ProjectName);

			{
				const string ProjectGuidTag = "<ProjectGuid>";
				int ProjectGuidStart = CSProjFileTemplate.IndexOf(ProjectGuidTag) + ProjectGuidTag.Length;
				int ProjectGuidEnd = CSProjFileTemplate.IndexOf("</ProjectGuid>", ProjectGuidStart);
				string OldProjectGuid = CSProjFileTemplate.Substring(ProjectGuidStart, ProjectGuidEnd - ProjectGuidStart);
				string NewProjectGuid = Guid.NewGuid().ToString("B");
				CSProjBuilder.Replace(OldProjectGuid, NewProjectGuid);
			}

			{
				const string OutputPathTag = "<OutputPath>";
				var OutputPath = CombinePaths(CmdEnv.LocalRoot, "Engine", "Binaries", "DotNET", "AutomationScripts");
				int PathStart = CSProjFileTemplate.IndexOf(OutputPathTag) + OutputPathTag.Length;
				int PathEnd = CSProjFileTemplate.IndexOf("</OutputPath>", PathStart);
				string OldOutputPath = CSProjFileTemplate.Substring(PathStart, PathEnd - PathStart);
				string NewOutputPath = ConvertSeparators(PathSeparator.Slash, UnrealBuildTool.Utils.MakePathRelativeTo(OutputPath, ProjectPath)) + "/";
				CSProjBuilder.Replace(OldOutputPath, NewOutputPath);
			}

			if (!DirectoryExists(ProjectPath))
			{
				CreateDirectory(ProjectPath);
			}
			WriteAllText(CombinePaths(ProjectPath, ProjectName + ".Automation.csproj"), CSProjBuilder.ToString());
		}

		{
			var PropertiesFileTemplate = ReadAllText(CombinePaths(CmdEnv.LocalRoot, "Engine", "Extras", "UnsupportedTools", "AutomationTemplate", "Properties", "AssemblyInfo.cs"));
			StringBuilder PropertiesBuilder = new StringBuilder(PropertiesFileTemplate);
			PropertiesBuilder.Replace("AutomationTemplate", ProjectName);

			const string AssemblyGuidTag = "[assembly: Guid(\"";
			int AssemblyGuidStart = PropertiesFileTemplate.IndexOf(AssemblyGuidTag) + AssemblyGuidTag.Length;
			int AssemblyGuidEnd = PropertiesFileTemplate.IndexOf("\")]", AssemblyGuidStart);
			string OldAssemblyGuid = PropertiesFileTemplate.Substring(AssemblyGuidStart, AssemblyGuidEnd - AssemblyGuidStart);
			string NewAssemblyGuid = Guid.NewGuid().ToString("D");
			PropertiesBuilder.Replace(OldAssemblyGuid, NewAssemblyGuid);

			string PropertiesPath = CombinePaths(ProjectPath, "Properties");
			if (!DirectoryExists(PropertiesPath))
			{
				CreateDirectory(PropertiesPath);
			}
			WriteAllText(CombinePaths(PropertiesPath, "AssemblyInfo.cs"), PropertiesBuilder.ToString());
		}

	}
}

class DumpBranch : BuildCommand
{

	public override void ExecuteBuild()
	{
		Log("************************* DumpBranch");

		var HostPlatforms = new List<UnrealTargetPlatform>();
		HostPlatforms.Add(UnrealTargetPlatform.Win64);
		HostPlatforms.Add(UnrealTargetPlatform.Mac);
		new BranchInfo(HostPlatforms);
	}
}

[Help("Sleeps for 20 seconds and then exits")]
public class DebugSleep : BuildCommand
{
	public override void ExecuteBuild()
	{
		Thread.Sleep(20000);
	}
}

[Help("Tests if Mcp configs loaded properly.")]
class TestMcpConfigs : BuildCommand
{
	public override void ExecuteBuild()
	{
		EpicGames.MCP.Config.McpConfigHelper.Find("localhost");
	}
}



[Help("Test Blame P4 command.")]
[Help("File", "(Optional) Filename of the file to produce a blame output for")]
[Help("Out", "(Optional) File to save the blame result to.")]
[Help("Timelapse", "If specified, will use Timelapse command instead of Blame")]
[RequireP4]
class TestBlame : BuildCommand
{
	public override void ExecuteBuild()
	{
		var Filename = ParseParamValue("File", "//depot/UE4/Engine/Source/Runtime/PakFile/Public/IPlatformFilePak.h");
		var OutFilename = ParseParamValue("Out");

		Log("Creating blame file for {0}", Filename);
		P4Connection.BlameLineInfo[] Result = null;
		if (ParseParam("Timelapse"))
		{
			Result = P4.Timelapse(Filename);
		}
		else
		{
			Result = P4.Blame(Filename);
		}
		var BlameResult = new StringBuilder();
		foreach (var BlameLine in Result)
		{
			var ResultLine = String.Format("#{0} in {1} by {2}: {3}", BlameLine.Revision.Revision, BlameLine.Revision.Changelist, BlameLine.Revision.User, BlameLine.Line);
			Log(ResultLine);
			BlameResult.AppendLine(ResultLine);
		}
		if (String.IsNullOrEmpty(OutFilename) == false)
		{
			WriteAllText(OutFilename, BlameResult.ToString());
		}
	}
}

[Help("Test P4 changes.")]
[RequireP4]
[DoesNotNeedP4CL]
class TestChanges : BuildCommand
{
	public override void ExecuteBuild()
	{
		var CommandParam = ParseParamValue("CommandParam", "//depot/UE4-LauncherReleases/*/Source/...@2091742,2091950 //depot/UE4-LauncherReleases/*/Build/...@2091742,2091950");

		{
			List<P4Connection.ChangeRecord> ChangeRecords;
			if (!P4.Changes(out ChangeRecords, CommandParam, true, true, LongComment: true))
			{
				throw new AutomationException("failed");
			}
			foreach (var Record in ChangeRecords)
			{
				Log("{0} {1} {2}", Record.CL, Record.UserEmail, Record.Summary);
			}
		}
		{
			List<P4Connection.ChangeRecord> ChangeRecords;
			if (!P4.Changes(out ChangeRecords, "-L " + CommandParam, true, true, LongComment: false))
			{
				throw new AutomationException("failed");
			}
			foreach (var Record in ChangeRecords)
			{
				Log("{0} {1} {2}", Record.CL, Record.UserEmail, Record.Summary);
			}
		}
	}
}

[Help("Spawns a process to test if UAT kills it automatically.")]
class TestKillAll : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("*********************** TestKillAll");

		string Exe = CombinePaths(CmdEnv.LocalRoot, "Engine", "Binaries", "Win64", "UE4Editor.exe");
		string ClientLogFile = CombinePaths(CmdEnv.LogFolder, "HoverGameRun");
		string CmdLine = " ../../../Samples/Sandbox/HoverShip/HoverShip.uproject -game -log -abslog=" + ClientLogFile;

		Run(Exe, CmdLine, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);
		Thread.Sleep(10000);
	}
}

[Help("Tests CleanFormalBuilds.")]
class TestCleanFormalBuilds : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("*********************** TestCleanFormalBuilds");
		var Dir = ParseParamValue("Dir", @"P:\Builds\UE4\PackagedBuilds\++UE4+Release-4.11");
		CleanFormalBuilds(Dir, "CL-*");
	}
}


[Help("Spawns a process to test if it can be killed.")]
class TestStopProcess : BuildCommand
{
	public override void ExecuteBuild()
	{
		Log("*********************** TestStopProcess");

		string Exe = CombinePaths(CmdEnv.LocalRoot, "Engine", "Binaries", "Win64", "UE4Editor.exe");
		string ClientLogFile = CombinePaths(CmdEnv.LogFolder, "HoverGameRun");
		string CmdLine = " ../../../Samples/Sandbox/HoverShip/HoverShip.uproject -game -log -ddc=noshared -abslog=" + ClientLogFile;

		for (int Attempt = 0; Attempt < 5; ++Attempt)
		{
			Log("Attempt: {0}", Attempt);
			var Proc = Run(Exe, CmdLine, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);
			Thread.Sleep(10000);
			Proc.StopProcess();
		}

		Log("One final attempt to test KillAll");
		Run(Exe, CmdLine, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);
		Thread.Sleep(10000);
	}
}

[Help("Looks through an XGE xml for overlapping obj files")]
[Help("Source", "full path of xml to look at")]
class LookForOverlappingBuildProducts : BuildCommand
{
	public override void ExecuteBuild()
	{
		var SourcePath = ParseParamValue("Source=");
		if (String.IsNullOrEmpty(SourcePath))
		{
			SourcePath = "D:\\UAT_XGE.xml";
		}
		if (!FileExists_NoExceptions(SourcePath))
		{
			throw new AutomationException("Source path not found, please use -source=Path");
		}
		var Objs = new HashSet<string>( StringComparer.InvariantCultureIgnoreCase );
//    /Fo&quot;D:\BuildFarm\buildmachine_++depot+UE4\Engine\Intermediate\Build\Win64\UE4Editor\Development\Projects\Module.Projects.cpp.obj&quot;
		var FileText = ReadAllText(SourcePath);
		string Start = "/Fo&quot;";
		string End = "&quot;";
		while (true)
		{
			var Index = FileText.IndexOf(Start);
			if (Index >= 0)
			{
				FileText = FileText.Substring(Index + Start.Length);
				Index = FileText.IndexOf(End);
				if (Index >= 0)
				{
					var ObjFile = FileText.Substring(0, Index);
					if (Objs.Contains(ObjFile))
					{
						LogError("Duplicate obj: {0}", ObjFile);
					}
					else
					{
						Objs.Add(ObjFile);
					}
					FileText = FileText.Substring(Index + End.Length);
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}
}

[Help("Copies all files from source directory to destination directory using ThreadedCopyFiles")]
[Help("Source", "Source path")]
[Help("Dest", "Destination path")]
[Help("Threads", "Number of threads used to copy files (64 by default)")]
class TestThreadedCopyFiles : BuildCommand
{
	public override void ExecuteBuild()
	{
		var SourcePath = ParseParamValue("Source=");
		var DestPath = ParseParamValue("Dest=");
		var Threads = ParseParamInt("Threads=", 64);

		if (String.IsNullOrEmpty(SourcePath))
		{
			throw new AutomationException("Source path not specified, please use -source=Path");
		}
		if (String.IsNullOrEmpty(DestPath))
		{
			throw new AutomationException("Destination path not specified, please use -dest=Path");
		}
		if (!DirectoryExists(SourcePath))
		{
			throw new AutomationException("Source path {0} does not exist", SourcePath);
		}

		DeleteDirectory(DestPath);

		using (var ScopedCopyTimer = new ScopedTimer("ThreadedCopyFiles"))
		{
			ThreadedCopyFiles(SourcePath, DestPath, Threads);
		}
	}
}
