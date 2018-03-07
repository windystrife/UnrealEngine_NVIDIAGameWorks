// Copyright 2006-2016 Donya Labs AB. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using AutomationTool;
using UnrealBuildTool;
using Ionic.Zip;
using Tools.DotNETCommon;

[Help("ZipUtils is used to zip/unzip (i.e:RunUAT.bat ZipUtils -archive=D:/Content.zip -add=D:/UE/Pojects/SampleGame/Content/) or (i.e:RunUAT.bat ZipUtils -archive=D:/Content.zip -extract=D:/UE/Pojects/SampleGame/Content/)")]
[Help("archive=<PathToArchive>", "Path to folder that should be add to the archive.")]
[Help("add=<Path>", "Path to folder that should be add to the archive.")]
[Help("extract=<Path>", "Path to folder where the archive should be extracted")]
[Help("compression=<0..9>", "Compression Level 0 - Copy  9-Best Compression")]
public class ZipUtils : BuildCommand
{
	public override ExitCode Execute()
	{

		Log("************************ STARTING ZIPUTILS ************************");

		if (Params.Length < 2)
		{
			LogError("Invalid number of arguments {0}", Params.Length);
			return ExitCode.Error_Arguments;
		}

		string ZipFilePath = ParseParamValue("archive", "");
		var ExtractArgs = ParseParamValues("extract");
		var AddArgs = ParseParamValues("add");
		int CompressionLevel = ParseParamInt("compression", 5);


		if (string.IsNullOrWhiteSpace(ZipFilePath) || System.IO.Path.GetExtension(ZipFilePath) != ".zip")
		{
			LogError("No zip file specified");
			return ExitCode.Error_Arguments;
		}

		bool bUnzip = ExtractArgs.Count() > 0;
		bool bZip = AddArgs.Count() > 0;

		if (!bUnzip && !bZip)
		{
			LogError("Invalid arguments. Please specify -archive or -extract option.");
			return ExitCode.Error_Arguments;
		}

		//setup filter to include all files
		FileFilter Filter = new FileFilter();
		Filter.Include("*.*");

		if (bUnzip)
		{
			string OutputFolderPath = ExtractArgs[0];

			Log("Running unzip : {0} OuputFolder:{1}", ZipFilePath.ToString(), OutputFolderPath);

			if (!System.IO.File.Exists(ZipFilePath))
			{
				LogError("Invalid zip file path.");
				return ExitCode.Error_Arguments;
			}

			var ExtractedFiles = InternalUnzipFiles(ZipFilePath, OutputFolderPath);

			if (ExtractedFiles.Count() == 0)
			{
				LogWarning("No files extracted from file zip.");
				return ExitCode.Error_Unknown;
			}
			else
			{
				Log("List of files extracted:");
				foreach (var file in ExtractedFiles)
				{
					Log("\t{0}", file);
				}
			}
		}
		else /*(bZip)*/
		{

			string ArchiveFolder = AddArgs[0];
			Log("Compress level : {0}", CompressionLevel);
			Log("Running zip : {0}", ArchiveFolder.ToString());

			FileAttributes attr = File.GetAttributes(ArchiveFolder);

			if (!System.IO.Directory.Exists(ArchiveFolder) || !attr.HasFlag(FileAttributes.Directory))
			{
				LogError("Invalid zip file path.");
				return ExitCode.Error_Arguments;
			}

			InternalZipFiles(new FileReference(ZipFilePath), new DirectoryReference(ArchiveFolder), Filter, CompressionLevel);
		}

		Log("************************ ZIPUTIL WORK COMPLETED ************************");

		return ExitCode.Success;
	}

	/// <summary>
	/// Creates a zip file containing the given input files
	/// </summary>
	/// <param name="ZipFileName">Filename for the zip</param>
	/// <param name="Filter">Filter which selects files to be included in the zip</param>
	/// <param name="BaseDirectory">Base directory to store relative paths in the zip file to</param>
	/// <param name="CopyModeOnly">No compression will be done. Only acts like a container. The default value is set to false.</param>
	internal static void InternalZipFiles(FileReference ZipFileName, DirectoryReference BaseDirectory, FileFilter Filter, int CompressionLevel = 0)
	{
		// Ionic.Zip.Zip64Option.Always option produces broken archives on Mono, so we use system zip tool instead
		if (Utils.IsRunningOnMono)
		{
			DirectoryReference.CreateDirectory(ZipFileName.Directory);
			CommandUtils.PushDir(BaseDirectory.FullName);
			string FilesList = "";
			foreach (FileReference FilteredFile in Filter.ApplyToDirectory(BaseDirectory, true))
			{
				FilesList += " \"" + FilteredFile.MakeRelativeTo(BaseDirectory) + "\"";
				if (FilesList.Length > 32000)
				{
					CommandUtils.RunAndLog(CommandUtils.CmdEnv, "zip", "-g -q \"" + ZipFileName + "\"" + FilesList);
					FilesList = "";
				}
			}
			if (FilesList.Length > 0)
			{
				CommandUtils.RunAndLog(CommandUtils.CmdEnv, "zip", "-g -q \"" + ZipFileName + "\"" + FilesList);
			}
			CommandUtils.PopDir();
		}
		else
		{
			using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile())
			{
				Zip.UseZip64WhenSaving = Ionic.Zip.Zip64Option.Always;

				Zip.CompressionLevel = (Ionic.Zlib.CompressionLevel)CompressionLevel;

				if (Zip.CompressionLevel == Ionic.Zlib.CompressionLevel.Level0)
					Zip.CompressionMethod = Ionic.Zip.CompressionMethod.None;

				foreach (FileReference FilteredFile in Filter.ApplyToDirectory(BaseDirectory, true))
				{
					Zip.AddFile(FilteredFile.FullName, Path.GetDirectoryName(FilteredFile.MakeRelativeTo(BaseDirectory)));
				}
				CommandUtils.CreateDirectory(Path.GetDirectoryName(ZipFileName.FullName));
				Zip.Save(ZipFileName.FullName);
			}
		}
	}

	/// <summary>
	/// Extracts the contents of a zip file
	/// </summary>
	/// <param name="ZipFileName">Name of the zip file</param>
	/// <param name="BaseDirectory">Output directory</param>
	/// <returns>List of files written</returns>
	public static IEnumerable<string> InternalUnzipFiles(string ZipFileName, string BaseDirectory)
	{
		// manually extract the files. There was a problem with the Ionic.Zip library that required this on non-PC at one point,
		// but that problem is now fixed. Leaving this code as is as we need to return the list of created files and fix up their permissions anyway.
		using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile(ZipFileName))
		{
			List<string> OutputFileNames = new List<string>();
			foreach (Ionic.Zip.ZipEntry Entry in Zip.Entries)
			{
				string OutputFileName = Path.Combine(BaseDirectory, Entry.FileName);
				Directory.CreateDirectory(Path.GetDirectoryName(OutputFileName));

				//Check to make sure that we do not output to file stream if the entry is a directory.
				if (!Entry.IsDirectory)
				{
					using (FileStream OutputStream = new FileStream(OutputFileName, FileMode.Create, FileAccess.Write))
					{
						Entry.Extract(OutputStream);
					}
				}

				if (UnrealBuildTool.Utils.IsRunningOnMono && CommandUtils.IsProbablyAMacOrIOSExe(OutputFileName))
				{
					FixUnixFilePermissions(OutputFileName);
				}
				OutputFileNames.Add(OutputFileName);
			}
			return OutputFileNames;
		}
	}
}



