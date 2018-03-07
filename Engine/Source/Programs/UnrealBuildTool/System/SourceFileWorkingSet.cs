using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Defines an interface which allows querying the working set. Files which are in the working set are excluded from unity builds, to improve iterative compile times.
	/// </summary>
	interface ISourceFileWorkingSet : IDisposable
	{
		/// <summary>
		/// Checks if the given file is part of the working set
		/// </summary>
		/// <param name="File">File to check</param>
		/// <returns>True if the file is part of the working set, false otherwise</returns>
		bool Contains(FileReference File);
	}

	/// <summary>
	/// Implementation of ISourceFileWorkingSet which does not contain any files
	/// </summary>
	class EmptySourceFileWorkingSet : ISourceFileWorkingSet
	{
		/// <summary>
		/// Dispose of the current instance.
		/// </summary>
		public void Dispose()
		{
		}

		/// <summary>
		/// Checks if the given file is part of the working set
		/// </summary>
		/// <param name="File">File to check</param>
		/// <returns>True if the file is part of the working set, false otherwise</returns>
		public bool Contains(FileReference File)
		{
			return false;
		}
	}

	/// <summary>
	/// Queries the working set for files tracked by Perforce.
	/// </summary>
	class PerforceSourceFileWorkingSet : ISourceFileWorkingSet
	{
		/// <summary>
		/// Dispose of the current instance.
		/// </summary>
		public void Dispose()
		{
		}

		/// <summary>
		/// Checks if the given file is part of the working set
		/// </summary>
		/// <param name="File">File to check</param>
		/// <returns>True if the file is part of the working set, false otherwise</returns>
		public bool Contains(FileReference File)
		{
			// Generated .cpp files should never be treated as part of the working set
			if (File.HasExtension(".gen.cpp"))
			{
				return false;
			}

			// Check if the file is read-only
			try
			{
				return !FileReference.GetAttributes(File).HasFlag(FileAttributes.ReadOnly);
			}
			catch (FileNotFoundException)
			{
				return false;
			}
		}
	}

	/// <summary>
	/// Queries the working set for files tracked by Git.
	/// </summary>
	class GitSourceFileWorkingSet : ISourceFileWorkingSet
	{
		DirectoryReference RootDir;
		Process BackgroundProcess;
		HashSet<FileReference> Files;
		List<DirectoryReference> Directories;
		List<string> ErrorOutput;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="GitPath">Path to the Git executable</param>
		/// <param name="RootDir">Root directory to run queries from (typically the directory containing the .git folder, to ensure all subfolders can be searched)</param>
		public GitSourceFileWorkingSet(string GitPath, DirectoryReference RootDir)
		{
			this.RootDir = RootDir;
			this.Files = new HashSet<FileReference>();
			this.Directories = new List<DirectoryReference>();
			this.ErrorOutput = new List<string>();

			Log.WriteLine(LogEventType.Console, "Using 'git status' to determine working set for adaptive non-unity build.");

			BackgroundProcess = new Process();
			BackgroundProcess.StartInfo.FileName = GitPath;
			BackgroundProcess.StartInfo.Arguments = "status --porcelain";
			BackgroundProcess.StartInfo.WorkingDirectory = RootDir.FullName;
			BackgroundProcess.StartInfo.RedirectStandardOutput = true;
			BackgroundProcess.StartInfo.RedirectStandardError = true;
			BackgroundProcess.StartInfo.UseShellExecute = false;
			BackgroundProcess.ErrorDataReceived += ErrorDataReceived;
			BackgroundProcess.OutputDataReceived += OutputDataReceived;
			try
			{
				BackgroundProcess.Start();
				BackgroundProcess.BeginErrorReadLine();
				BackgroundProcess.BeginOutputReadLine();
			}
			catch
			{
				BackgroundProcess.Dispose();
				BackgroundProcess = null;
			}
		}

		/// <summary>
		/// Terminates the background process.
		/// </summary>
		private void TerminateBackgroundProcess()
		{
			if (BackgroundProcess != null)
			{
				if (!BackgroundProcess.HasExited)
				{
					try
					{
						BackgroundProcess.Kill();
					}
					catch
					{
					}
				}
				WaitForBackgroundProcess();
			}
		}

		/// <summary>
		/// Waits for the background to terminate.
		/// </summary>
		private void WaitForBackgroundProcess()
		{
			if (BackgroundProcess != null)
			{
				if(!BackgroundProcess.WaitForExit(500))
				{
					Log.WriteLine(LogEventType.Console, "Waiting for 'git status' command to complete");
				}
				if(!BackgroundProcess.WaitForExit(15000))
				{
					Log.WriteLine(LogEventType.Console, "Terminating git child process due to timeout");
					try
					{
						BackgroundProcess.Kill();
					}
					catch
					{
					}
				}
				BackgroundProcess.WaitForExit();
				BackgroundProcess.Dispose();
				BackgroundProcess = null;
			}
		}

		/// <summary>
		/// Dispose of this object
		/// </summary>
		public void Dispose()
		{
			TerminateBackgroundProcess();
		}

		/// <summary>
		/// Checks if the given file is part of the working set
		/// </summary>
		/// <param name="File">File to check</param>
		/// <returns>True if the file is part of the working set, false otherwise</returns>
		public bool Contains(FileReference File)
		{
			WaitForBackgroundProcess();
			return Files.Contains(File) || Directories.Any(x => File.IsUnderDirectory(x));
		}

		/// <summary>
		/// Parse output text from Git
		/// </summary>
		void OutputDataReceived(object Sender, DataReceivedEventArgs Args)
		{
			if (Args.Data != null && Args.Data.Length > 3 && Args.Data[2] == ' ')
			{
				int MinIdx = 3;
				int MaxIdx = Args.Data.Length;

				while (MinIdx < MaxIdx && Char.IsWhiteSpace(Args.Data[MinIdx]))
				{
					MinIdx++;
				}

				while (MinIdx < MaxIdx && Char.IsWhiteSpace(Args.Data[MaxIdx - 1]))
				{
					MaxIdx--;
				}

				int ArrowIdx = Args.Data.IndexOf(" -> ", MinIdx, MaxIdx - MinIdx);
				if (ArrowIdx == -1)
				{
					AddPath(Args.Data.Substring(MinIdx, MaxIdx - MinIdx));
				}
				else
				{
					AddPath(Args.Data.Substring(MinIdx, ArrowIdx - MinIdx));
					int ArrowEndIdx = ArrowIdx + 4;
					AddPath(Args.Data.Substring(ArrowEndIdx, MaxIdx - ArrowEndIdx));
				}
			}
		}

		/// <summary>
		/// Handle error output text from Git
		/// </summary>
		void ErrorDataReceived(object Sender, DataReceivedEventArgs Args)
		{
			if (Args.Data != null)
			{
				ErrorOutput.Add(Args.Data);
			}
		}

		/// <summary>
		/// Add a path to the working set
		/// </summary>
		/// <param name="Path">Path to be added</param>
		void AddPath(string Path)
		{
			if (Path.EndsWith("/"))
			{
				Directories.Add(DirectoryReference.Combine(RootDir, Path));
			}
			else
			{
				Files.Add(FileReference.Combine(RootDir, Path));
			}
		}
	}

	/// <summary>
	/// Utility class for ISourceFileWorkingSet
	/// </summary>
	static class SourceFileWorkingSet
	{
		enum ProviderType
		{
			None,
			Default,
			Perforce,
			Git
		};

		/// <summary>
		/// Sets the provider to use for determining the working set
		/// </summary>
		[XmlConfigFile]
		static ProviderType Provider = ProviderType.Default;

		/// <summary>
		/// Sets the path to use for the repository. Interpreted relative to the UE root directory (ie. folder above the Engine folder) if relative.
		/// </summary>
		[XmlConfigFile]
		public static string RepositoryPath = null;

		/// <summary>
		/// Sets the path to use for the Git executable. Defaults to "git" (assuming it's in the PATH).
		/// </summary>
		[XmlConfigFile]
		public static string GitPath = "git";

		/// <summary>
		/// Create an ISourceFileWorkingSet instance suitable for the given project or root directory
		/// </summary>
		/// <param name="RootDir">The root directory</param>
		/// <param name="ProjectDir">The current project</param>
		/// <returns>Working set instance for the given directory</returns>
		public static ISourceFileWorkingSet Create(DirectoryReference RootDir, DirectoryReference ProjectDir)
		{
			if (Provider == ProviderType.None || ProjectFileGenerator.bGenerateProjectFiles)
			{
				return new EmptySourceFileWorkingSet();
			}
			else if (Provider == ProviderType.Git)
			{
				DirectoryReference RepoDir;
				if (!String.IsNullOrEmpty(RepositoryPath))
				{
					RepoDir = DirectoryReference.Combine(RootDir, RepositoryPath);
				}
				else if (!TryGetGitRepositoryPath(RootDir, ProjectDir, out RepoDir))
				{
					RepoDir = RootDir;
				}
				return new GitSourceFileWorkingSet(GitPath, RepoDir);
			}
			else if (Provider == ProviderType.Perforce)
			{
				return new PerforceSourceFileWorkingSet();
			}
			else
			{
				DirectoryReference RepoDir;
				if(TryGetGitRepositoryPath(RootDir, ProjectDir, out RepoDir))
				{
					return new GitSourceFileWorkingSet(GitPath, RepoDir);
				}
				else
				{
					return new PerforceSourceFileWorkingSet();
				}
			}
		}

		/// <summary>
		/// Tries to find the directory containing a git repository
		/// </summary>
		/// <param name="RootDir">Root directory containing the engine folder</param>
		/// <param name="ProjectDir">Directory containing the project</param>
		/// <param name="RepoDir">On success, receives the path to the repository</param>
		/// <returns>True if a Git repository was found, false otherwise</returns>
		static bool TryGetGitRepositoryPath(DirectoryReference RootDir, DirectoryReference ProjectDir, out DirectoryReference RepoDir)
		{
			if (DirectoryReference.Exists(DirectoryReference.Combine(RootDir, ".git")))
			{
				RepoDir = RootDir;
				return true;
			}

			if (ProjectDir != null)
			{
				if (DirectoryReference.Exists(DirectoryReference.Combine(ProjectDir, ".git")))
				{
					RepoDir = ProjectDir;
					return true;
				}

				DirectoryReference ParentProjectDir = ProjectDir.ParentDirectory;
				if (DirectoryReference.Exists(DirectoryReference.Combine(ParentProjectDir, ".git")))
				{
					RepoDir = ParentProjectDir;
					return true;
				}
			}

			RepoDir = null;
			return false;
		}
	}
}
