// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using Ionic.Zip;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	[DebuggerDisplay("{FileName}")]
	class ArchiveManifestFile
	{
		public string FileName;
		public long Length;
		public DateTime LastWriteTimeUtc;

		public ArchiveManifestFile(BinaryReader Reader)
		{
			FileName = Reader.ReadString();
			Length = Reader.ReadInt64();
			LastWriteTimeUtc = new DateTime(Reader.ReadInt64());
		}

		public ArchiveManifestFile(string InFileName, long InLength, DateTime InLastWriteTimeUtc)
		{
			FileName = InFileName;
			Length = InLength;
			LastWriteTimeUtc = InLastWriteTimeUtc;
		}

		public void Write(BinaryWriter Writer)
		{
			Writer.Write(FileName);
			Writer.Write(Length);
			Writer.Write(LastWriteTimeUtc.Ticks);
		}
	}

	class ArchiveManifest
	{
		const int Signature = ((int)'U' << 24) | ((int)'A' << 16) | ((int)'M' << 8) | 1;

		public List<ArchiveManifestFile> Files = new List<ArchiveManifestFile>();

		public ArchiveManifest()
		{
		}

		public ArchiveManifest(FileStream InputStream)
		{
			BinaryReader Reader = new BinaryReader(InputStream);
			if(Reader.ReadInt32() != Signature)
			{
				throw new Exception("Archive manifest signature does not match");
			}

			int NumFiles = Reader.ReadInt32();
			for(int Idx = 0; Idx < NumFiles; Idx++)
			{
				Files.Add(new ArchiveManifestFile(Reader));
			}
		}

		public void Write(FileStream OutputStream)
		{
			BinaryWriter Writer = new BinaryWriter(OutputStream);
			Writer.Write(Signature);
			Writer.Write(Files.Count);
			foreach(ArchiveManifestFile File in Files)
			{
				File.Write(Writer);
			}
		}
	}

	static class ArchiveUtils
	{
		public static void ExtractFiles(string ArchiveFileName, string BaseDirectoryName, string ManifestFileName, ProgressValue Progress, TextWriter LogWriter)
		{
			DateTime TimeStamp = DateTime.UtcNow;
			using(ZipFile Zip = new ZipFile(ArchiveFileName))
			{
				File.Delete(ManifestFileName);

				// Create the manifest
				ArchiveManifest Manifest = new ArchiveManifest();
				foreach(ZipEntry Entry in Zip.Entries)
				{
					if(!Entry.FileName.EndsWith("/") && !Entry.FileName.EndsWith("\\"))
					{ 
						Manifest.Files.Add(new ArchiveManifestFile(Entry.FileName, Entry.UncompressedSize, TimeStamp));
					}
				}

				// Write it out to a temporary file, then move it into place
				string TempManifestFileName = ManifestFileName + ".tmp";
				using(FileStream OutputStream = File.Open(TempManifestFileName, FileMode.Create, FileAccess.Write))
				{
					Manifest.Write(OutputStream);
				}
				File.Move(TempManifestFileName, ManifestFileName);

				// Extract all the files
				int EntryIdx = 0;
				foreach(ZipEntry Entry in Zip.Entries)
				{
					if(!Entry.FileName.EndsWith("/") && !Entry.FileName.EndsWith("\\"))
					{
						string FileName = Path.Combine(BaseDirectoryName, Entry.FileName);
						Directory.CreateDirectory(Path.GetDirectoryName(FileName));

						lock(LogWriter)
						{
							LogWriter.WriteLine("Writing {0}", FileName);
						}

						using(FileStream OutputStream = new FileStream(FileName, FileMode.Create, FileAccess.Write))
						{
							Entry.Extract(OutputStream);
						}
						File.SetLastWriteTimeUtc(FileName, TimeStamp);
					}
					Progress.Set((float)++EntryIdx / (float)Zip.Entries.Count);
				}
			}
		}

		public static void RemoveExtractedFiles(string BaseDirectoryName, string ManifestFileName, ProgressValue Progress, TextWriter LogWriter)
		{
			// Read the manifest in
			ArchiveManifest Manifest;
			using(FileStream InputStream = File.Open(ManifestFileName, FileMode.Open, FileAccess.Read))
			{
				Manifest = new ArchiveManifest(InputStream);
			}

			// Remove all the files that haven't been modified match
			for(int Idx = 0; Idx < Manifest.Files.Count; Idx++)
			{
				FileInfo File = new FileInfo(Path.Combine(BaseDirectoryName, Manifest.Files[Idx].FileName));
				if(File.Exists)
				{
					if(File.Length != Manifest.Files[Idx].Length)
					{
						LogWriter.WriteLine("Skipping {0} due to modified length", File.FullName);
					}
					else if(Math.Abs((File.LastWriteTimeUtc - Manifest.Files[Idx].LastWriteTimeUtc).TotalSeconds) > 2.0)
					{
						LogWriter.WriteLine("Skipping {0} due to modified timestamp", File.FullName);
					}
					else
					{
						LogWriter.WriteLine("Removing {0}", File.FullName);
						File.Delete();
					}
				}
				Progress.Set((float)(Idx + 1) / (float)Manifest.Files.Count);
			}
		}
	}
}
