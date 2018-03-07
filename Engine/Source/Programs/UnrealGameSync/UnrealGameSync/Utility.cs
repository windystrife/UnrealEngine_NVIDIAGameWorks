// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#define USE_NEW_PROCESS_JOBS

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace UnrealGameSync
{
	static class Utility
	{
		public static string GetPathWithCorrectCase(FileInfo Info)
		{
			DirectoryInfo ParentInfo = Info.Directory;
			if(Info.Exists)
			{
				return Path.Combine(GetPathWithCorrectCase(ParentInfo), ParentInfo.GetFiles(Info.Name)[0].Name); 
			}
			else
			{
				return Path.Combine(GetPathWithCorrectCase(ParentInfo), Info.Name);
			}
		}

		public static string GetPathWithCorrectCase(DirectoryInfo Info)
		{
			DirectoryInfo ParentInfo = Info.Parent;
			if(ParentInfo == null)
			{
				return Info.FullName.ToUpperInvariant();
			}
			else if(Info.Exists)
			{
				return Path.Combine(GetPathWithCorrectCase(ParentInfo), ParentInfo.GetDirectories(Info.Name)[0].Name);
			}
			else
			{
				return Path.Combine(GetPathWithCorrectCase(ParentInfo), Info.Name);
			}
		}

		public static void ForceDeleteFile(string FileName)
		{
			if(File.Exists(FileName))
			{
				File.SetAttributes(FileName, File.GetAttributes(FileName) & ~FileAttributes.ReadOnly);
				File.Delete(FileName);
			}
		}

		public static void WriteException(this TextWriter Writer, Exception Ex, string Format, params object[] Args)
		{
			Writer.WriteLine(Format, Args);
			foreach(string Line in Ex.ToString().Split('\n'))
			{
				Writer.WriteLine(">>>  " + Line);
			}
		}

		public static bool SpawnProcess(string FileName, string CommandLine)
		{
			using(Process ChildProcess = new Process())
			{
				ChildProcess.StartInfo.FileName = FileName;
				ChildProcess.StartInfo.Arguments = String.IsNullOrEmpty(CommandLine) ? "" : CommandLine;
				ChildProcess.StartInfo.UseShellExecute = false;
				return ChildProcess.Start();
			}
		}

		public static bool SpawnHiddenProcess(string FileName, string CommandLine)
		{
			using(Process ChildProcess = new Process())
			{
				ChildProcess.StartInfo.FileName = FileName;
				ChildProcess.StartInfo.Arguments = String.IsNullOrEmpty(CommandLine) ? "" : CommandLine;
				ChildProcess.StartInfo.UseShellExecute = false;
				ChildProcess.StartInfo.RedirectStandardOutput = true;
				ChildProcess.StartInfo.RedirectStandardError = true;
				ChildProcess.StartInfo.CreateNoWindow = true;
				return ChildProcess.Start();
			}
		}

		public static int ExecuteProcess(string FileName, string CommandLine, string Input, TextWriter Log)
		{
			return ExecuteProcess(FileName, CommandLine, Input, Line => Log.WriteLine(Line));
		}

		public static int ExecuteProcess(string FileName, string CommandLine, string Input, out List<string> OutputLines)
		{
			using(ChildProcess NewProcess = new ChildProcess(FileName, CommandLine, Input))
			{
				OutputLines = NewProcess.ReadAllLines();
				return NewProcess.ExitCode;
			}
		}

		public static int ExecuteProcess(string FileName, string CommandLine, string Input, Action<string> OutputLine)
		{
#if USE_NEW_PROCESS_JOBS
			using(ChildProcess NewProcess = new ChildProcess(FileName, CommandLine, Input))
			{
				string Line;
				while(NewProcess.TryReadLine(out Line))
				{
					OutputLine(Line);
				}
				return NewProcess.ExitCode;
			}
#else
			using(Process ChildProcess = new Process())
			{
				try
				{
					object LockObject = new object();

					DataReceivedEventHandler OutputHandler = (x, y) => { if(y.Data != null){ lock(LockObject){ OutputLine(y.Data.TrimEnd()); } } };

					ChildProcess.StartInfo.FileName = FileName;
					ChildProcess.StartInfo.Arguments = String.IsNullOrEmpty(CommandLine) ? "" : CommandLine;
					ChildProcess.StartInfo.UseShellExecute = false;
					ChildProcess.StartInfo.RedirectStandardOutput = true;
					ChildProcess.StartInfo.RedirectStandardError = true;
					ChildProcess.OutputDataReceived += OutputHandler;
					ChildProcess.ErrorDataReceived += OutputHandler;
					ChildProcess.StartInfo.RedirectStandardInput = Input != null;
					ChildProcess.StartInfo.CreateNoWindow = true;
					ChildProcess.StartInfo.StandardOutputEncoding = new System.Text.UTF8Encoding(false, false);
					ChildProcess.Start();
					ChildProcess.BeginOutputReadLine();
					ChildProcess.BeginErrorReadLine();

					if(!String.IsNullOrEmpty(Input))
					{
						ChildProcess.StandardInput.WriteLine(Input);
						ChildProcess.StandardInput.Close();
					}

					// Busy wait for the process to exit so we can get a ThreadAbortException if the thread is terminated. It won't wait until we enter managed code
					// again before it throws otherwise.
					for(;;)
					{
						if(ChildProcess.WaitForExit(20))
						{
							ChildProcess.WaitForExit();
							break;
						}
					}

					return ChildProcess.ExitCode;
				}
				finally
				{
					CloseHandle(JobObject);
				}
			}
#endif
		}

		public static bool SafeIsFileUnderDirectory(string FileName, string DirectoryName)
		{
			try
			{
				string FullDirectoryName = Path.GetFullPath(DirectoryName).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
				string FullFileName = Path.GetFullPath(FileName);
				return FullFileName.StartsWith(FullDirectoryName, StringComparison.InvariantCultureIgnoreCase);
			}
			catch(Exception)
			{
				return false;
			}
		}

		/// <summary>
		/// Expands variables in $(VarName) format in the given string. Variables are retrieved from the given dictionary, or through the environment of the current process.
		/// Any unknown variables are ignored.
		/// </summary>
		/// <param name="InputString">String to search for variable names</param>
		/// <param name="Variables">Lookup of variable names to values</param>
		/// <returns>String with all variables replaced</returns>
		public static string ExpandVariables(string InputString, Dictionary<string, string> AdditionalVariables = null)
		{
			string Result = InputString;
			for (int Idx = Result.IndexOf("$("); Idx != -1; Idx = Result.IndexOf("$(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = Result.IndexOf(')', Idx + 2);
				if (EndIdx == -1)
				{
					break;
				}

				// Extract the variable name from the string
				string Name = Result.Substring(Idx + 2, EndIdx - (Idx + 2));

				// Strip the format from the name
				string Format = null;
				int FormatIdx = Name.IndexOf(':');
				if(FormatIdx != -1)
				{ 
					Format = Name.Substring(FormatIdx + 1);
					Name = Name.Substring(0, FormatIdx);
				}

				// Find the value for it, either from the dictionary or the environment block
				string Value;
				if (AdditionalVariables == null || !AdditionalVariables.TryGetValue(Name, out Value))
				{
					Value = Environment.GetEnvironmentVariable(Name);
					if (Value == null)
					{
						Idx = EndIdx + 1;
						continue;
					}
				}

				// Encode the variable if necessary
				if(Format != null)
				{
					if(String.Equals(Format, "URI", StringComparison.InvariantCultureIgnoreCase))
					{
						Value = Uri.EscapeDataString(Value);
					}
				}

				// Replace the variable, or skip past it
				Result = Result.Substring(0, Idx) + Value + Result.Substring(EndIdx + 1);
			}
			return Result;
		}

		/// <summary>
		/// Determines if a project is an enterprise project
		/// </summary>
		/// <param name="FileName">Path to the project file</param>
		/// <returns>True if the given filename is an enterprise project</returns>
		public static bool IsEnterpriseProject(string FileName)
		{
			try
			{
				string Text = File.ReadAllText(FileName);

				JavaScriptSerializer Serializer = new JavaScriptSerializer();
				Dictionary<string, object> RawObject = Serializer.Deserialize<Dictionary<string, object>>(Text);

				object Enterprise;
				if(RawObject.TryGetValue("Enterprise", out Enterprise) && Enterprise is bool)
				{
					return (bool)Enterprise;
				}

				return false;
			}
			catch
			{
				return false;
			}
		}
	}
}
