using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Contains information about a target required to deploy it. Written at build time, and read back in when UBT needs to run the deploy step.
	/// </summary>
	class UEBuildDeployTarget
	{
		/// <summary>
		/// Path to the project file
		/// </summary>
		public readonly FileReference ProjectFile;

		/// <summary>
		/// Path to the .target.cs
		/// </summary>
		public readonly FileReference TargetFile;

		/// <summary>
		/// The shared app name for this target (eg. UE4Editor)
		/// </summary>
		public readonly string AppName;

		/// <summary>
		/// The name of this target
		/// </summary>
		public readonly string TargetName;

		/// <summary>
		/// Type of the target to build
		/// </summary>
		public readonly TargetType TargetType;

		/// <summary>
		/// The platform being built
		/// </summary>
		public readonly UnrealTargetPlatform Platform;

		/// <summary>
		/// The configuration being built
		/// </summary>
		public readonly UnrealTargetConfiguration Configuration;

		/// <summary>
		/// The output path
		/// </summary>
		public FileReference OutputPath
		{
			get
			{
				if (OutputPaths.Count != 1)
				{
					throw new BuildException("Attempted to use UEBuildDeployTarget.OutputPath property, but there are multiple (or no) OutputPaths. You need to handle multiple in the code that called this (size = {0})", OutputPaths.Count);
				}
				return OutputPaths[0];
			}
		}

		/// <summary>
		/// The full list of output paths, for platforms that support building multiple binaries simultaneously
		/// </summary>
		public readonly List<FileReference> OutputPaths;

		/// <summary>
		/// Path to the directory for engine intermediates. May be under the project directory if not being built for the shared build environment.
		/// </summary>
		public readonly DirectoryReference EngineIntermediateDirectory;

		/// <summary>
		/// The project directory, or engine directory for targets without a project file.
		/// </summary>
		public readonly DirectoryReference ProjectDirectory;

		/// <summary>
		/// Path to the generated build receipt.
		/// </summary>
		public readonly FileReference BuildReceiptFileName;
		
		/// <summary>
		/// Whether to output build products to the Engine/Binaries folder.
		/// </summary>
		public readonly bool bOutputToEngineBinaries;

		/// <summary>
		/// If true, then a stub IPA will be generated when compiling is done (minimal files needed for a valid IPA)
		/// </summary>
		public readonly bool bCreateStubIPA;

		/// <summary>
		/// Which architectures to deploy for on Android
		/// </summary>
		public readonly string[] AndroidArchitectures;

		/// <summary>
		/// Which GPU architectures to deploy for on Android
		/// </summary>
		public readonly string[] AndroidGPUArchitectures;

		/// <summary>
		/// Construct the deployment info from a target
		/// </summary>
		/// <param name="Target">The target being built</param>
		public UEBuildDeployTarget(UEBuildTarget Target)
		{
			this.ProjectFile = Target.ProjectFile;
			this.TargetFile = Target.TargetRulesFile;
			this.AppName = Target.AppName;
			this.TargetName = Target.TargetName;
			this.TargetType = Target.TargetType;
			this.Platform = Target.Platform;
			this.Configuration = Target.Configuration;
			this.OutputPaths = new List<FileReference>(Target.OutputPaths);
			this.EngineIntermediateDirectory = Target.EngineIntermediateDirectory;
			this.ProjectDirectory = Target.ProjectDirectory;
			this.BuildReceiptFileName = Target.ReceiptFileName;
			this.bOutputToEngineBinaries = Target.Rules.bOutputToEngineBinaries;
			this.bCreateStubIPA = Target.Rules.bCreateStubIPA;
			this.AndroidArchitectures = Target.Rules.AndroidPlatform.Architectures.ToArray();
			this.AndroidGPUArchitectures = Target.Rules.AndroidPlatform.GPUArchitectures.ToArray();
		}

		/// <summary>
		/// Read the deployment info from a file on disk
		/// </summary>
		/// <param name="Location">Path to the file to read</param>
		public UEBuildDeployTarget(FileReference Location)
		{
			using (BinaryReader Reader = new BinaryReader(File.Open(Location.FullName, FileMode.Open, FileAccess.Read, FileShare.Read)))
			{
				ProjectFile = Reader.ReadFileReference();
				TargetFile = Reader.ReadFileReference();
				AppName = Reader.ReadString();
				TargetName = Reader.ReadString();
				TargetType = (TargetType)Reader.ReadInt32();
				Platform = (UnrealTargetPlatform)Reader.ReadInt32();
				Configuration = (UnrealTargetConfiguration)Reader.ReadInt32();
				OutputPaths = Reader.ReadList(x => x.ReadFileReference());
				EngineIntermediateDirectory = Reader.ReadDirectoryReference();
				ProjectDirectory = Reader.ReadDirectoryReference();
				BuildReceiptFileName = Reader.ReadFileReference();
				bOutputToEngineBinaries = Reader.ReadBoolean();
				bCreateStubIPA = Reader.ReadBoolean();
				AndroidArchitectures = Reader.ReadArray(x => x.ReadString());
				AndroidGPUArchitectures = Reader.ReadArray(x => x.ReadString());
			}
		}

		/// <summary>
		/// Write the deployment info to a file on disk
		/// </summary>
		/// <param name="Location">File to write to</param>
		public void Write(FileReference Location)
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using (BinaryWriter Writer = new BinaryWriter(File.Open(Location.FullName, FileMode.Create, FileAccess.Write, FileShare.Read)))
			{
				Writer.Write(ProjectFile);
				Writer.Write(TargetFile);
				Writer.Write(AppName);
				Writer.Write(TargetName);
				Writer.Write((Int32)TargetType);
				Writer.Write((Int32)Platform);
				Writer.Write((Int32)Configuration);
				Writer.Write(OutputPaths, (x, i) => x.Write(i));
				Writer.Write(EngineIntermediateDirectory);
				Writer.Write(ProjectDirectory);
				Writer.Write(BuildReceiptFileName);
				Writer.Write(bOutputToEngineBinaries);
				Writer.Write(bCreateStubIPA);
				Writer.Write(AndroidArchitectures, (w, e) => w.Write(e));
				Writer.Write(AndroidGPUArchitectures, (w, e) => w.Write(e));
			}
		}
	}
}
