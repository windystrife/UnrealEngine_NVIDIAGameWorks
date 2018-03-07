// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool
{
	[RequireP4]
	class BuildForUGS : BuildCommand
	{
		public override void ExecuteBuild()
		{
			// Parse the target list
			string[] Targets = ParseParamValues("Target");
			if(Targets.Length == 0)
			{
				throw new AutomationException("No targets specified (eg. -Target=\"UE4Editor Win64 Development\")");
			}

			// Parse the archive path
			string ArchivePath = ParseParamValue("Archive");
			if(ArchivePath != null && (!ArchivePath.StartsWith("//") || ArchivePath.Sum(x => (x == '/')? 1 : 0) < 4))
			{
				throw new AutomationException("Archive path is not a valid depot filename");
			}

			// Prepare the build agenda
			UE4Build.BuildAgenda Agenda = new UE4Build.BuildAgenda();
			foreach(string Target in Targets)
			{
				string[] Tokens = Target.Split(new char[]{ ' ' }, StringSplitOptions.RemoveEmptyEntries);

				UnrealTargetPlatform Platform;
				UnrealTargetConfiguration Configuration;
				if(Tokens.Length < 3 || !Enum.TryParse(Tokens[1], true, out Platform) || !Enum.TryParse(Tokens[2], true, out Configuration))
				{
					throw new AutomationException("Invalid target '{0}' - expected <TargetName> <Platform> <Configuration>");
				}

				Agenda.AddTarget(Tokens[0], Platform, Configuration, InAddArgs: String.Join(" ", Tokens.Skip(3)));
			}

			// Build everything
			UE4Build Builder = new UE4Build(this);
			Builder.Build(Agenda, InUpdateVersionFiles: ArchivePath != null);

			// Include the build products for UAT and UBT if required
			if(ParseParam("WithUAT"))
			{
				Builder.AddUATFilesToBuildProducts();
			}
			if(ParseParam("WithUBT"))
			{
				Builder.AddUBTFilesToBuildProducts();
			}

			// Archive the build products
			if(ArchivePath != null)
			{
				// Create an output folder
				string OutputFolder = Path.Combine(CommandUtils.CmdEnv.LocalRoot, "ArchiveForUGS");
				Directory.CreateDirectory(OutputFolder);

				// Create a temp folder for storing stripped PDB files
				string SymbolsFolder = Path.Combine(OutputFolder, "Symbols");
				Directory.CreateDirectory(SymbolsFolder);

				// Get the Windows toolchain
				Platform WindowsTargetPlatform = Platform.GetPlatform(UnrealTargetPlatform.Win64);

				// Figure out all the files for the archive
				string ZipFileName = Path.Combine(OutputFolder, "Archive.zip");
				using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile())
				{
					Zip.UseZip64WhenSaving = Ionic.Zip.Zip64Option.Always;
					foreach(string BuildProduct in Builder.BuildProductFiles)
					{
						if(!File.Exists(BuildProduct))
						{
							throw new AutomationException("Missing build product: {0}", BuildProduct);
						}
						if(BuildProduct.EndsWith(".pdb", StringComparison.InvariantCultureIgnoreCase))
						{
							string StrippedFileName = CommandUtils.MakeRerootedFilePath(BuildProduct, CommandUtils.CmdEnv.LocalRoot, SymbolsFolder);
							Directory.CreateDirectory(Path.GetDirectoryName(StrippedFileName));
							WindowsTargetPlatform.StripSymbols(new FileReference(BuildProduct), new FileReference(StrippedFileName));
							Zip.AddFile(StrippedFileName, Path.GetDirectoryName(CommandUtils.StripBaseDirectory(StrippedFileName, SymbolsFolder)));
						}
						else
						{
							Zip.AddFile(BuildProduct, Path.GetDirectoryName(CommandUtils.StripBaseDirectory(BuildProduct, CommandUtils.CmdEnv.LocalRoot)));
						}
					}
					// Create the zip file
					Console.WriteLine("Writing {0}...", ZipFileName);
					Zip.Save(ZipFileName);
				}

				// Submit it to Perforce if required
				if(CommandUtils.AllowSubmit)
				{
					// Delete any existing clientspec for submitting
					string ClientName = Environment.MachineName + "_BuildForUGS";

					// Create a brand new one
					P4ClientInfo Client = new P4ClientInfo();
					Client.Owner = CommandUtils.P4Env.User;
					Client.Host = Environment.MachineName;
					Client.Stream = ArchivePath.Substring(0, ArchivePath.IndexOf('/', ArchivePath.IndexOf('/', 2) + 1));
					Client.RootPath = Path.Combine(OutputFolder, "Perforce");
					Client.Name = ClientName;
					Client.Options = P4ClientOption.NoAllWrite | P4ClientOption.NoClobber | P4ClientOption.NoCompress | P4ClientOption.Unlocked | P4ClientOption.NoModTime | P4ClientOption.RmDir;
					Client.LineEnd = P4LineEnd.Local;
					P4.CreateClient(Client, AllowSpew: false);

					// Create a new P4 connection for this workspace
					P4Connection SubmitP4 = new P4Connection(Client.Owner, Client.Name, P4Env.ServerAndPort);
					SubmitP4.Revert("-k //...");

					// Figure out where the zip file has to go in Perforce
					P4WhereRecord WhereZipFile = SubmitP4.Where(ArchivePath, false).FirstOrDefault(x => !x.bUnmap && x.Path != null);
					if(WhereZipFile == null)
					{
						throw new AutomationException("Couldn't locate {0} in this workspace");
					}

					// Get the latest version of it
					int NewCL = SubmitP4.CreateChange(Description: String.Format("[CL {0}] Updated binaries", P4Env.Changelist));
					SubmitP4.Sync(String.Format("-k \"{0}\"", ArchivePath), AllowSpew:false);
					CommandUtils.CopyFile(ZipFileName, WhereZipFile.Path);
					SubmitP4.Add(NewCL, String.Format("\"{0}\"", ArchivePath));
					SubmitP4.Edit(NewCL, String.Format("\"{0}\"", ArchivePath));

					// Submit it
					int SubmittedCL;
					SubmitP4.Submit(NewCL, out SubmittedCL);
					if(SubmittedCL <= 0)
					{
						throw new AutomationException("Submit failed.");
					}
					Console.WriteLine("Submitted in changelist {0}", SubmittedCL);
				}
			}
		}
	}
}
