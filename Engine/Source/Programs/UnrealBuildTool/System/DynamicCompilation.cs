// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.CodeDom.Compiler;
using Microsoft.CSharp;
using System.Reflection;
using System.Diagnostics;
using Tools.DotNETCommon;

#if NET_CORE
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Emit;
using System.Reflection.Metadata;
using Microsoft.CodeAnalysis.Text;
#endif

namespace UnrealBuildTool
{
	/// <summary>
	/// Methods for dynamically compiling C# source files
	/// </summary>
	public class DynamicCompilation
	{
		/// File information for UnrealBuildTool.exe, cached at program start
		private static FileInfo UBTExecutableFileInfo = new FileInfo(Assembly.GetEntryAssembly().GetOriginalLocation());

		/*
		 * Checks to see if the assembly needs compilation
		 */
		private static bool RequiresCompilation(List<FileReference> SourceFileNames, FileReference AssemblySourceListFilePath, FileReference OutputAssemblyPath)
		{
			// Check to see if we already have a compiled assembly file on disk
			FileInfo OutputAssemblyInfo = new FileInfo(OutputAssemblyPath.FullName);
			if (OutputAssemblyInfo.Exists)
			{
				// Check the time stamp of the UnrealBuildTool.exe file.  If Unreal Build Tool was compiled more
				// recently than the dynamically-compiled assembly, then we'll always recompile it.  This is
				// because Unreal Build Tool's code may have changed in such a way that invalidate these
				// previously-compiled assembly files.
				if (UBTExecutableFileInfo.LastWriteTimeUtc > OutputAssemblyInfo.LastWriteTimeUtc)
				{
					// UnrealBuildTool.exe has been recompiled more recently than our cached assemblies
					Log.TraceVerbose("UnrealBuildTool.exe has been recompiled more recently than " + OutputAssemblyInfo.Name);

					return true;
				}
				else
				{
					// Make sure we have a manifest of source files used to compile the output assembly.  If it doesn't exist
					// for some reason (not an expected case) then we'll need to recompile.
					FileInfo AssemblySourceListFile = new FileInfo(AssemblySourceListFilePath.FullName);
					if (!AssemblySourceListFile.Exists)
					{
						return true;
					}
					else
					{
						// Make sure the source files we're compiling are the same as the source files that were compiled
						// for the assembly that we want to load
						List<FileReference> ExistingAssemblySourceFileNames = new List<FileReference>();
						{
							using (FileStream Reader = AssemblySourceListFile.OpenRead())
							{
								using (StreamReader TextReader = new StreamReader(Reader))
								{
									for (string ExistingSourceFileName = TextReader.ReadLine(); ExistingSourceFileName != null; ExistingSourceFileName = TextReader.ReadLine())
									{
										FileReference FullExistingSourceFileName = new FileReference(ExistingSourceFileName);

										ExistingAssemblySourceFileNames.Add(FullExistingSourceFileName);

										// Was the existing assembly compiled with a source file that we aren't interested in?  If so, then it needs to be recompiled.
										if (!SourceFileNames.Contains(FullExistingSourceFileName))
										{
											return true;
										}
									}
								}
							}
						}

						// Test against source file time stamps
						foreach (FileReference SourceFileName in SourceFileNames)
						{
							// Was the existing assembly compiled without this source file?  If so, then we definitely need to recompile it!
							if (!ExistingAssemblySourceFileNames.Contains(SourceFileName))
							{
								return true;
							}

							FileInfo SourceFileInfo = new FileInfo(SourceFileName.FullName);

							// Check to see if the source file exists
							if (!SourceFileInfo.Exists)
							{
								throw new BuildException("Could not locate source file for dynamic compilation: {0}", SourceFileName);
							}

							// Ignore temp files
							if (!SourceFileInfo.Extension.Equals(".tmp", StringComparison.CurrentCultureIgnoreCase))
							{
								// Check to see if the source file is newer than the compiled assembly file.  We don't want to
								// bother recompiling it if it hasn't changed.
								if (SourceFileInfo.LastWriteTimeUtc > OutputAssemblyInfo.LastWriteTimeUtc)
								{
									// Source file has changed since we last compiled the assembly, so we'll need to recompile it now!
									Log.TraceVerbose(SourceFileInfo.Name + " has been modified more recently than " + OutputAssemblyInfo.Name);

									return true;
								}
							}
						}
					}
				}
			}
			else
			{
				// File doesn't exist, so we'll definitely have to compile it!
				Log.TraceVerbose(OutputAssemblyInfo.Name + " doesn't exist yet");
				return true;
			}

			return false;
		}

		/*
		 * Compiles an assembly from source files
		 */

#if NET_CORE
		private static void LogDiagnostics(IEnumerable<Diagnostic> Diagnostics)
		{
			foreach (Diagnostic Diag in Diagnostics)
			{
				switch (Diag.Severity)
				{
					case DiagnosticSeverity.Error: 
					{
						Log.TraceError(Diag.ToString()); 
						break;
					}
					case DiagnosticSeverity.Hidden: 
					{
						break;
					}
					case DiagnosticSeverity.Warning: 
					{
						Log.TraceWarning(Diag.ToString()); 
						break;
					}
					case DiagnosticSeverity.Info: 
					{
						Log.TraceInformation(Diag.ToString()); 
						break;
					}
				}
			}
		}

		private static Assembly CompileAssembly(FileReference OutputAssemblyPath, List<FileReference> SourceFileNames, List<string> ReferencedAssembies, List<string> PreprocessorDefines = null, bool TreatWarningsAsErrors = false)
		{
			CSharpParseOptions ParseOptions = new CSharpParseOptions(
				languageVersion:LanguageVersion.Latest, 
				kind:SourceCodeKind.Regular,
				preprocessorSymbols:PreprocessorDefines
			);

			List<SyntaxTree> SyntaxTrees = new List<SyntaxTree>();

			foreach (FileReference SourceFileName in SourceFileNames)
			{
				SourceText Source = SourceText.From(File.ReadAllText(SourceFileName.FullName));
				SyntaxTree Tree = CSharpSyntaxTree.ParseText(Source, ParseOptions, SourceFileName.FullName);

				IEnumerable<Diagnostic> Diagnostics = Tree.GetDiagnostics();
				if (Diagnostics.Count() > 0)
				{
					Log.TraceWarning($"Errors generated while parsing '{SourceFileName.FullName}'");
					LogDiagnostics(Tree.GetDiagnostics());
					return null;
				}

				SyntaxTrees.Add(Tree);
			}

			// Create the output directory if it doesn't exist already
			DirectoryInfo DirInfo = new DirectoryInfo(OutputAssemblyPath.Directory.FullName);
			if (!DirInfo.Exists)
			{
				try
				{
					DirInfo.Create();
				}
				catch (Exception Ex)
				{
					throw new BuildException(Ex, "Unable to create directory '{0}' for intermediate assemblies (Exception: {1})", OutputAssemblyPath, Ex.Message);
				}
			}

			List<MetadataReference> MetadataReferences = new List<MetadataReference>();
			if (ReferencedAssembies != null)
			{
				foreach (string Reference in ReferencedAssembies)
				{
					MetadataReferences.Add(MetadataReference.CreateFromFile(Reference));
				}
			}

			MetadataReferences.Add(MetadataReference.CreateFromFile(typeof(object).Assembly.Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(Assembly.Load("System.Runtime").Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(Assembly.Load("System.Collections").Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(Assembly.Load("System.IO").Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(Assembly.Load("System.IO.FileSystem").Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(Assembly.Load("System.Console").Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(Assembly.Load("System.Runtime.Extensions").Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(Assembly.Load("Microsoft.Win32.Registry").Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(typeof(UnrealBuildTool).Assembly.Location));
			MetadataReferences.Add(MetadataReference.CreateFromFile(typeof(FileReference).Assembly.Location));

			CSharpCompilationOptions CompilationOptions = new CSharpCompilationOptions(
				outputKind:OutputKind.DynamicallyLinkedLibrary,
				optimizationLevel:OptimizationLevel.Release,
				warningLevel:4,
				assemblyIdentityComparer:DesktopAssemblyIdentityComparer.Default,
				reportSuppressedDiagnostics:true
				);

			CSharpCompilation Compilation = CSharpCompilation.Create(
				assemblyName:OutputAssemblyPath.GetFileNameWithoutAnyExtensions(),
				syntaxTrees:SyntaxTrees,
				references:MetadataReferences,
				options:CompilationOptions
				);

			using (FileStream AssemblyStream = FileReference.Open(OutputAssemblyPath, FileMode.Create))
			{
				EmitOptions EmitOptions = new EmitOptions(
					includePrivateMembers:true
				);

				EmitResult Result = Compilation.Emit(
					peStream:AssemblyStream,
					options:EmitOptions);

				if (!Result.Success)
				{
					LogDiagnostics(Result.Diagnostics);
					return null;
				}
			}

			return Assembly.LoadFile(OutputAssemblyPath.FullName);
		}

#else

		private static Assembly CompileAssembly(FileReference OutputAssemblyPath, List<FileReference> SourceFileNames, List<string> ReferencedAssembies, List<string> PreprocessorDefines = null, bool TreatWarningsAsErrors = false)
		{
			TempFileCollection TemporaryFiles = new TempFileCollection();

			// Setup compile parameters
			CompilerParameters CompileParams = new CompilerParameters();
			{
				// Always compile the assembly to a file on disk, so that we can load a cached version later if we have one
				CompileParams.GenerateInMemory = false;

				// This is the full path to the assembly file we're generating
				CompileParams.OutputAssembly = OutputAssemblyPath.FullName;

				// We always want to generate a class library, not an executable
				CompileParams.GenerateExecutable = false;

				// Never fail compiles for warnings
				CompileParams.TreatWarningsAsErrors = false;

				// Set the warning level so that we will actually receive warnings -
				// doesn't abort compilation as stated in documentation!
				CompileParams.WarningLevel = 4;

				// Always generate debug information as it takes minimal time
				CompileParams.IncludeDebugInformation = true;
#if !DEBUG
				// Optimise the managed code in Development
				CompileParams.CompilerOptions += " /optimize";
#endif
				Log.TraceVerbose("Compiling " + OutputAssemblyPath);

				// Keep track of temporary files emitted by the compiler so we can clean them up later
				CompileParams.TempFiles = TemporaryFiles;

				// Warnings as errors if desired
				CompileParams.TreatWarningsAsErrors = TreatWarningsAsErrors;

				// Add assembly references
				{
					if (ReferencedAssembies == null)
					{
						// Always depend on the CLR System assembly
						CompileParams.ReferencedAssemblies.Add("System.dll");
					}
					else
					{
						// Add in the set of passed in referenced assemblies
						CompileParams.ReferencedAssemblies.AddRange(ReferencedAssembies.ToArray());
					}

					// The assembly will depend on this application
					Assembly UnrealBuildToolAssembly = Assembly.GetExecutingAssembly();
					CompileParams.ReferencedAssemblies.Add(UnrealBuildToolAssembly.Location);

					// The assembly will depend on the utilities assembly. Find that assembly
					// by looking for the one that contains a common utility class
					Assembly UtilitiesAssembly = Assembly.GetAssembly(typeof(FileReference));
					CompileParams.ReferencedAssemblies.Add(UtilitiesAssembly.Location);
				}

				// Add preprocessor definitions
				if (PreprocessorDefines != null && PreprocessorDefines.Count > 0)
				{
					CompileParams.CompilerOptions += " /define:";
					for (int DefinitionIndex = 0; DefinitionIndex < PreprocessorDefines.Count; ++DefinitionIndex)
					{
						if (DefinitionIndex > 0)
						{
							CompileParams.CompilerOptions += ";";
						}
						CompileParams.CompilerOptions += PreprocessorDefines[DefinitionIndex];
					}
				}

				// @todo: Consider embedding resources in generated assembly file (version/copyright/signing)
			}

			// Create the output directory if it doesn't exist already
			DirectoryInfo DirInfo = new DirectoryInfo(OutputAssemblyPath.Directory.FullName);
			if (!DirInfo.Exists)
			{
				try
				{
					DirInfo.Create();
				}
				catch (Exception Ex)
				{
					throw new BuildException(Ex, "Unable to create directory '{0}' for intermediate assemblies (Exception: {1})", OutputAssemblyPath, Ex.Message);
				}
			}

			// Compile the code
			CompilerResults CompileResults;
			try
			{
				// Enable .NET 4.0 as we want modern language features like 'var'
				Dictionary<string, string> ProviderOptions = new Dictionary<string, string>() { { "CompilerVersion", "v4.0" } };
				CSharpCodeProvider Compiler = new CSharpCodeProvider(ProviderOptions);
				CompileResults = Compiler.CompileAssemblyFromFile(CompileParams, SourceFileNames.Select(x => x.FullName).ToArray());
			}
			catch (Exception Ex)
			{
				throw new BuildException(Ex, "Failed to launch compiler to compile assembly from source files '{0}' (Exception: {1})", SourceFileNames.ToString(), Ex.Message);
			}

			// Display compilation warnings and errors
			if (CompileResults.Errors.Count > 0)
			{
				Log.TraceInformation("While compiling {0}:", OutputAssemblyPath);
				foreach (CompilerError CurError in CompileResults.Errors)
				{
					if (CurError.IsWarning)
					{
						Log.TraceWarning(CurError.ToString());
					}
					else
					{
						Log.TraceError(CurError.ToString());
					}
				}
				if (CompileResults.Errors.HasErrors || TreatWarningsAsErrors)
				{
					throw new BuildException("Unable to compile source files.");
				}
			}

			// Grab the generated assembly
			Assembly CompiledAssembly = CompileResults.CompiledAssembly;
			if (CompiledAssembly == null)
			{
				throw new BuildException("UnrealBuildTool was unable to compile an assembly for '{0}'", SourceFileNames.ToString());
			}

			// Clean up temporary files that the compiler saved
			TemporaryFiles.Delete();

			return CompiledAssembly;
		}
#endif

		/// <summary>
		/// Dynamically compiles an assembly for the specified source file and loads that assembly into the application's
		/// current domain.  If an assembly has already been compiled and is not out of date, then it will be loaded and
		/// no compilation is necessary.
		/// </summary>
		/// <param name="OutputAssemblyPath">Full path to the assembly to be created</param>
		/// <param name="SourceFileNames">List of source file name</param>
		/// <param name="ReferencedAssembies"></param>
		/// <param name="PreprocessorDefines"></param>
		/// <param name="DoNotCompile"></param>
		/// <param name="TreatWarningsAsErrors"></param>
		/// <returns>The assembly that was loaded</returns>
		public static Assembly CompileAndLoadAssembly(FileReference OutputAssemblyPath, List<FileReference> SourceFileNames, List<string> ReferencedAssembies = null, List<string> PreprocessorDefines = null, bool DoNotCompile = false, bool TreatWarningsAsErrors = false)
		{
			// Check to see if the resulting assembly is compiled and up to date
			FileReference AssemblySourcesListFilePath = FileReference.Combine(OutputAssemblyPath.Directory, Path.GetFileNameWithoutExtension(OutputAssemblyPath.FullName) + "SourceFiles.txt");
			bool bNeedsCompilation = false;
			if (!DoNotCompile)
			{
				bNeedsCompilation = RequiresCompilation(SourceFileNames, AssemblySourcesListFilePath, OutputAssemblyPath);
			}

			// Load the assembly to ensure it is correct
			Assembly CompiledAssembly = null;
			if (!bNeedsCompilation)
			{
				try
				{
					// Load the previously-compiled assembly from disk
					CompiledAssembly = Assembly.LoadFile(OutputAssemblyPath.FullName);
				}
				catch (FileLoadException Ex)
				{
					Log.TraceInformation(String.Format("Unable to load the previously-compiled assembly file '{0}'.  Unreal Build Tool will try to recompile this assembly now.  (Exception: {1})", OutputAssemblyPath, Ex.Message));
					bNeedsCompilation = true;
				}
				catch (BadImageFormatException Ex)
				{
					Log.TraceInformation(String.Format("Compiled assembly file '{0}' appears to be for a newer CLR version or is otherwise invalid.  Unreal Build Tool will try to recompile this assembly now.  (Exception: {1})", OutputAssemblyPath, Ex.Message));
					bNeedsCompilation = true;
				}
				catch (Exception Ex)
				{
					throw new BuildException(Ex, "Error while loading previously-compiled assembly file '{0}'.  (Exception: {1})", OutputAssemblyPath, Ex.Message);
				}
			}

			// Compile the assembly if me
			if (bNeedsCompilation)
			{
				CompiledAssembly = CompileAssembly(OutputAssemblyPath, SourceFileNames, ReferencedAssembies, PreprocessorDefines, TreatWarningsAsErrors);

				// Save out a list of all the source files we compiled.  This is so that we can tell if whole files were added or removed
				// since the previous time we compiled the assembly.  In that case, we'll always want to recompile it!
				{
					FileInfo AssemblySourcesListFile = new FileInfo(AssemblySourcesListFilePath.FullName);
					using (StreamWriter Writer = AssemblySourcesListFile.CreateText())
					{
						SourceFileNames.ForEach(x => Writer.WriteLine(x));
					}
				}
			}

#if !NET_CORE
			// Load the assembly into our app domain
			try
			{
				AppDomain.CurrentDomain.Load(CompiledAssembly.GetName());
			}
			catch (Exception Ex)
			{
				throw new BuildException(Ex, "Unable to load the compiled build assembly '{0}' into our application's domain.  (Exception: {1})", OutputAssemblyPath, Ex.Message);
			}
#endif

			return CompiledAssembly;
		}
	}
}
