// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace UnrealBuildTool
{
	class JunkDeleter
	{
		/// <summary>
		/// Loads JunkManifest.txt file and removes all junk files/folders defined in it.
		/// </summary>
		static public void DeleteJunk()
		{
			DateTime JunkStartTime = DateTime.UtcNow;

			if (UnrealBuildTool.IsEngineInstalled() == false)
			{
				List<string> JunkManifest = LoadJunkManifest();
				DeleteAllJunk(JunkManifest);
			}

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				double JunkTime = (DateTime.UtcNow - JunkStartTime).TotalSeconds;
				Log.TraceInformation("DeleteJunk took " + JunkTime + "s");
			}
		}

		/// <summary>
		/// Loads JunkManifest.txt file.
		/// </summary>
		/// <returns>Junk manifest file contents.</returns>
		static private List<string> LoadJunkManifest()
		{
			string ManifestPath = ".." + Path.DirectorySeparatorChar + "Build" + Path.DirectorySeparatorChar + "JunkManifest.txt";
			List<string> JunkManifest = new List<string>();
			if (File.Exists(ManifestPath))
			{
				string MachineName = Environment.MachineName;
				using (StreamReader reader = new StreamReader(ManifestPath))
				{
					string CurrentToRootDir = ".." + Path.DirectorySeparatorChar + "..";
					string LineRead;
					while ((LineRead = reader.ReadLine()) != null)
					{
						string JunkEntry = LineRead.Trim();
						if (String.IsNullOrEmpty(JunkEntry) == false)
						{
							string[] Tokens = JunkEntry.Split(":".ToCharArray());
							bool bIsValidJunkLine = true;
							foreach (string Token in Tokens)
							{
								if (Token.StartsWith("Machine=", StringComparison.InvariantCultureIgnoreCase) == true)
								{
									string[] InnerTokens = Token.Split("=".ToCharArray());
									// check if the machine name on the line matches the current machine name, if not, we don't apply this junk
									if (InnerTokens.Length == 2 && MachineName.StartsWith(InnerTokens[1]) == false)
									{
										// Not meant for this machine
										bIsValidJunkLine = false;
									}
								}
								else if (Token.StartsWith("Platform=", StringComparison.InvariantCultureIgnoreCase) == true)
								{
									string[] InnerTokens = Token.Split("=".ToCharArray());
									// check if the machine name on the line matches the current machine name, if not, we don't apply this junk
									if (InnerTokens.Length == 2)
									{
										UnrealTargetPlatform ParsedPlatform = UEBuildPlatform.ConvertStringToPlatform(InnerTokens[1]);
										// if the platform is valid, then we want to keep the files, which means that we don't want to apply the junk line
										if (ParsedPlatform != UnrealTargetPlatform.Unknown)
										{
											if (UEBuildPlatform.GetBuildPlatform(ParsedPlatform, bInAllowFailure: true) != null)
											{
												// this is a good platform, so don't delete any files!
												bIsValidJunkLine = false;
											}
										}
									}
								}
							}

							// All paths within the manifest are UE4 root directory relative.
							// UBT's working directory is Engine\Source so add "..\..\" to each of the entires.
							if (bIsValidJunkLine)
							{
								// the entry is always the last element in the token array (after the final :)
								string FixedPath = Path.Combine(CurrentToRootDir, Tokens[Tokens.Length - 1]);
								FixedPath = FixedPath.Replace('\\', Path.DirectorySeparatorChar);
								JunkManifest.Add(FixedPath);
							}
						}
					}
				}
			}
			return JunkManifest;
		}

		/// <summary>
		/// Goes through each entry from the junk manifest and deletes it.
		/// </summary>
		/// <param name="JunkManifest">JunkManifest.txt entries.</param>
		static private void DeleteAllJunk(List<string> JunkManifest)
		{
			foreach (string Junk in JunkManifest)
			{
				if (IsFile(Junk))
				{
					string FileName = Path.GetFileName(Junk);
					if (FileName.Contains('*'))
					{
						// Wildcard search and delete
						string DirectoryToLookIn = Path.GetDirectoryName(Junk);
						if (Directory.Exists(DirectoryToLookIn))
						{
							// Delete all files within the specified folder
							string[] FilesToDelete = Directory.GetFiles(DirectoryToLookIn, FileName, SearchOption.TopDirectoryOnly);
							foreach (string JunkFile in FilesToDelete)
							{
								DeleteFile(JunkFile);
							}

							// Delete all subdirectories with the specified folder
							string[] DirectoriesToDelete = Directory.GetDirectories(DirectoryToLookIn, FileName, SearchOption.TopDirectoryOnly);
							foreach (string JunkFolder in DirectoriesToDelete)
							{
								DeleteDirectory(JunkFolder);
							}
						}
					}
					else
					{
						// Delete single file
						DeleteFile(Junk);
					}
				}
				else if (Directory.Exists(Junk))
				{
					// Delete the selected folder and all its contents
					DeleteDirectory(Junk);
				}
			}
		}

		static private bool IsFile(string PathToCheck)
		{
			string FileName = Path.GetFileName(PathToCheck);
			if (String.IsNullOrEmpty(FileName) == false)
			{
				if (FileName.Contains('*'))
				{
					// Assume wildcards are file because the path will be searched for files and directories anyway.
					return true;
				}
				else
				{
					return File.Exists(PathToCheck);
				}
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Deletes a directory recursively gracefully handling all exceptions.
		/// </summary>
		/// <param name="DirectoryPath">Path.</param>
		static private void DeleteDirectory(string DirectoryPath)
		{
			try
			{
				Log.TraceInformation("Deleting junk directory: \"{0}\".", DirectoryPath);
				Directory.Delete(DirectoryPath, true);
			}
			catch (Exception Ex)
			{
				// Ignore all exceptions
				Log.TraceInformation("Unable to delete junk directory: \"{0}\". Error: {1}", DirectoryPath, Ex.Message);
			}
		}

		/// <summary>
		/// Deletes a file gracefully handling all exceptions.
		/// </summary>
		/// <param name="Filename">Filename.</param>
		static private void DeleteFile(string Filename)
		{
			try
			{
				Log.TraceInformation("Deleting junk file: \"{0}\".", Filename);
				File.Delete(Filename);
			}
			catch (Exception Ex)
			{
				// Ingore all exceptions
				Log.TraceInformation("Unable to delete junk file: \"{0}\". Error: {1}", Filename, Ex.Message);
			}
		}
	}
}
