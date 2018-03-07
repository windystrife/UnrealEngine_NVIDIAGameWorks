using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	class LinuxCommon
	{
		public static string Which(string name)
		{
			Process proc = new Process();
			proc.StartInfo.FileName = "/bin/sh";
			proc.StartInfo.Arguments = String.Format("-c 'which {0}'", name);
			proc.StartInfo.UseShellExecute = false;
			proc.StartInfo.CreateNoWindow = true;
			proc.StartInfo.RedirectStandardOutput = true;
			proc.StartInfo.RedirectStandardError = true;

			proc.Start();
			proc.WaitForExit();

			string path = proc.StandardOutput.ReadLine();
			Log.TraceVerbose(String.Format("which {0} result: ({1}) {2}", name, proc.ExitCode, path));

			if (proc.ExitCode == 0 && String.IsNullOrEmpty(proc.StandardError.ReadToEnd()))
			{
				return path;
			}
			return null;
		}

		public static string WhichClang()
		{
			string[] ClangNames = { "clang++", "clang++-3.9", "clang++-3.8", "clang++-3.7", "clang++-3.6", "clang++-3.5" };
			string ClangPath;
			foreach (var ClangName in ClangNames)
			{
				ClangPath = Which(ClangName);
				if (!String.IsNullOrEmpty(ClangPath))
				{
					return ClangPath;
				}
			}
			return null;
		}

		public static string WhichGcc()
		{
			return Which("g++");
		}
	}
}

