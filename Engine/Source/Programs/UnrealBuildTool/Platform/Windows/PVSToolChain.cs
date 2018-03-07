using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class PVSToolChain : UEToolChain
	{
		WindowsCompiler Compiler;

		public PVSToolChain(CppPlatform InCppPlatform, WindowsCompiler InCompiler) : base(InCppPlatform)
		{
			this.Compiler = InCompiler;
		}

		static string GetFullIncludePath(string IncludePath)
		{
			return Path.GetFullPath(ActionThread.ExpandEnvironmentVariables(IncludePath));
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName, ActionGraph ActionGraph)
		{
			VCEnvironment EnvVars = VCEnvironment.SetEnvironment(CppPlatform, Compiler);

			// Get the MSVC arguments required to compile all files in this batch
			List<string> SharedArguments = new List<string>();
			SharedArguments.Add("/nologo");
			SharedArguments.Add("/P"); // Preprocess
			SharedArguments.Add("/C"); // Preserve comments when preprocessing
			SharedArguments.Add("/D PVS_STUDIO");
			SharedArguments.Add("/wd4005");
			foreach (string IncludePath in CompileEnvironment.IncludePaths.UserIncludePaths)
			{
				SharedArguments.Add(String.Format("/I \"{0}\"", GetFullIncludePath(IncludePath)));
			}
			foreach (string IncludePath in CompileEnvironment.IncludePaths.SystemIncludePaths)
			{
				SharedArguments.Add(String.Format("/I \"{0}\"", GetFullIncludePath(IncludePath)));
			}
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				SharedArguments.Add(String.Format("/D \"{0}\"", Definition));
			}

			// Get the path to PVS studio
			FileReference AnalyzerFile = new FileReference(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "PVS-Studio", "x64", "PVS-Studio.exe"));
			if(!FileReference.Exists(AnalyzerFile))
			{
				throw new BuildException("Unable to find PVS-Studio at {0}", AnalyzerFile);
			}

			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				// Get the file names for everything we need
				string BaseFileName = SourceFile.Reference.GetFileName();

				// Write the response file
				FileReference PreprocessedFileLocation = FileReference.Combine(CompileEnvironment.OutputDirectory, BaseFileName + ".i");

				List<string> Arguments = new List<string>(SharedArguments);
				Arguments.Add(String.Format("/Fi\"{0}\"", PreprocessedFileLocation)); // Preprocess to a file
				Arguments.Add(String.Format("\"{0}\"", SourceFile.AbsolutePath));

				FileReference ResponseFileLocation = FileReference.Combine(CompileEnvironment.OutputDirectory, BaseFileName + ".i.response");
				FileItem ResponseFileItem = FileItem.CreateIntermediateTextFile(ResponseFileLocation, String.Join("\n", Arguments));

				// Preprocess the source file
				FileItem PreprocessedFileItem = FileItem.GetItemByFileReference(PreprocessedFileLocation);

				Action PreprocessAction = ActionGraph.Add(ActionType.Compile);
				PreprocessAction.CommandPath = EnvVars.CompilerPath.FullName;
				PreprocessAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				PreprocessAction.CommandArguments = " @\"" + ResponseFileItem.AbsolutePath + "\"";
				PreprocessAction.PrerequisiteItems.Add(SourceFile);
				PreprocessAction.PrerequisiteItems.Add(ResponseFileItem);
				PreprocessAction.ProducedItems.Add(PreprocessedFileItem);
				PreprocessAction.bShouldOutputStatusDescription = false;

				// Write the PVS studio config file
				StringBuilder ConfigFileContents = new StringBuilder();
				ConfigFileContents.AppendFormat("exclude-path={0}\n", EnvVars.VCInstallDir.FullName);
				if (CppPlatform == CppPlatform.Win64)
				{
					ConfigFileContents.Append("platform=x64\n");
				}
				else if(CppPlatform == CppPlatform.Win32)
				{
					ConfigFileContents.Append("platform=Win32\n");
				}
				else
				{
					throw new BuildException("PVS studio does not support this platform");
				}
				ConfigFileContents.Append("preprocessor=visualcpp\n");
				ConfigFileContents.Append("language=C++\n");
				ConfigFileContents.Append("skip-cl-exe=yes\n");
				ConfigFileContents.AppendFormat("i-file={0}\n", PreprocessedFileItem.Reference.FullName);

				FileReference ConfigFileLocation = FileReference.Combine(CompileEnvironment.OutputDirectory, BaseFileName + ".cfg");
				FileItem ConfigFileItem = FileItem.CreateIntermediateTextFile(ConfigFileLocation, ConfigFileContents.ToString());

				// Run the analzyer on the preprocessed source file
				FileReference OutputFileLocation = FileReference.Combine(CompileEnvironment.OutputDirectory, BaseFileName + ".pvslog");
				FileItem OutputFileItem = FileItem.GetItemByFileReference(OutputFileLocation);

				Action AnalyzeAction = ActionGraph.Add(ActionType.Compile);
				AnalyzeAction.CommandDescription = "Analyzing";
				AnalyzeAction.StatusDescription = BaseFileName;
				AnalyzeAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				AnalyzeAction.CommandPath = AnalyzerFile.FullName;
				AnalyzeAction.CommandArguments = String.Format("--cl-params \"{0}\" --source-file \"{1}\" --output-file \"{2}\" --cfg \"{3}\" --analysis-mode 4", PreprocessAction.CommandArguments, SourceFile.AbsolutePath, OutputFileLocation, ConfigFileItem.AbsolutePath);
				AnalyzeAction.PrerequisiteItems.Add(ConfigFileItem);
				AnalyzeAction.PrerequisiteItems.Add(PreprocessedFileItem);
				AnalyzeAction.ProducedItems.Add(OutputFileItem);
				AnalyzeAction.bShouldDeleteProducedItems = true; // PVS Studio will append by default, so need to delete produced items

				Result.ObjectFiles.AddRange(AnalyzeAction.ProducedItems);
			}
			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			throw new BuildException("Unable to link with PVS toolchain.");
		}

		public override void FinalizeOutput(ReadOnlyTargetRules Target, List<FileItem> OutputItems, ActionGraph ActionGraph)
		{
			FileReference OutputFile;
			if (Target.ProjectFile == null)
			{
				OutputFile = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Saved", "PVS-Studio", String.Format("{0}.pvslog", Target.Name));
			}
			else
			{
				OutputFile = FileReference.Combine(Target.ProjectFile.Directory, "Saved", "PVS-Studio", String.Format("{0}.pvslog", Target.Name));
			}

			List<FileReference> InputFiles = OutputItems.Select(x => x.Reference).ToList();

			Action AnalyzeAction = ActionGraph.Add(ActionType.Compile);
			AnalyzeAction.CommandPath = "Dummy.exe";
			AnalyzeAction.CommandArguments = "";
			AnalyzeAction.CommandDescription = "Combining output from PVS-Studio";
			AnalyzeAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			AnalyzeAction.PrerequisiteItems.AddRange(OutputItems);
			AnalyzeAction.ProducedItems.Add(FileItem.GetItemByFileReference(OutputFile));
			AnalyzeAction.ActionHandler = (Action Action, out int ExitCode, out string Output) => WriteResults(OutputFile, InputFiles, out ExitCode, out Output);
			AnalyzeAction.bShouldDeleteProducedItems = true;

			OutputItems.AddRange(AnalyzeAction.ProducedItems);
		}

		void WriteResults(FileReference OutputFile, List<FileReference> InputFiles, out int ExitCode, out string Output)
		{
			StringBuilder OutputBuilder = new StringBuilder();
			OutputBuilder.Append("Processing PVS Studio output...\n");

			// Create the combined output file, and print the diagnostics to the log
			HashSet<string> UniqueItems = new HashSet<string>();
			using (StreamWriter RawWriter = new StreamWriter(OutputFile.FullName))
			{
				foreach (FileReference InputFile in InputFiles)
				{
					string[] Lines = File.ReadAllLines(InputFile.FullName);
					foreach (string Line in Lines)
					{
						if (!String.IsNullOrWhiteSpace(Line) && UniqueItems.Add(Line))
						{
							string[] Tokens = Line.Split(new string[] { "<#~>" }, StringSplitOptions.None);

							string Trial = Tokens[1];
							int LineNumber = int.Parse(Tokens[2]);
							string FileName = Tokens[3];
							string WarningCode = Tokens[5];
							string WarningMessage = Tokens[6];
							bool bFalseAlarm = bool.Parse(Tokens[7]);
							int Level = int.Parse(Tokens[8]);

							// Ignore anything in ThirdParty folders
							if(FileName.Replace('/', '\\').IndexOf("\\ThirdParty\\", StringComparison.InvariantCultureIgnoreCase) == -1)
							{
								// Output the line to the raw output file
								RawWriter.WriteLine(Line);

								// Output the line to the log
								if (!bFalseAlarm && Level == 1)
								{
									OutputBuilder.AppendFormat("{0}({1}): warning {2}: {3}\n", FileName, LineNumber, WarningCode, WarningMessage);
								}
							}
						}
					}
				}
			}
			OutputBuilder.AppendFormat("Written {0} {1} to {2}.", UniqueItems.Count, (UniqueItems.Count == 1)? "diagnostic" : "diagnostics", OutputFile.FullName);

			// Return the text to be output. We don't have a custom output handler, so we can just return an empty string.
			Output = OutputBuilder.ToString();
			ExitCode = 0;
		}
	}
}
