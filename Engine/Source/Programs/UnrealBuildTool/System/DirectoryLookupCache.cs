// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class DirectoryCache
	{
		public readonly DirectoryReference Location;
		public readonly bool Exists;
		HashSet<FileReference> Files;
		HashSet<DirectoryReference> Directories;

		public DirectoryCache(DirectoryReference InDirectory)
		{
			Location = InDirectory;
			Exists = DirectoryReference.Exists(Location);

			if (!Exists)
			{
				Files = new HashSet<FileReference>();
				Directories = new HashSet<DirectoryReference>();
			}
		}

		private void CacheFiles()
		{
			if (Files == null)
			{
				Files = new HashSet<FileReference>(DirectoryReference.EnumerateFiles(Location));
			}
		}

		public bool HasFile(FileReference CheckFile)
		{
			CacheFiles();
			return Files.Contains(CheckFile);
		}

		public IEnumerable<FileReference> EnumerateFiles()
		{
			CacheFiles();
			return Files;
		}

		private void CacheDirectories()
		{
			if (Directories == null)
			{
				Directories = new HashSet<DirectoryReference>(DirectoryReference.EnumerateDirectories(Location));
			}
		}

		public IEnumerable<DirectoryReference> EnumerateDirectories()
		{
			CacheDirectories();
			return Directories;
		}
	}

	static class DirectoryLookupCache
	{
		static DirectoryCache FindOrCreateDirectoryCache(DirectoryReference Directory)
		{
			DirectoryCache FoundDirectoryCache;
			if (!Directories.TryGetValue(Directory, out FoundDirectoryCache))
			{
				FoundDirectoryCache = new DirectoryCache(Directory);
				Directories.Add(Directory, FoundDirectoryCache);
			}
			return FoundDirectoryCache;
		}


		static public bool FileExists(FileReference File)
		{
			DirectoryCache FoundDirectoryCache = FindOrCreateDirectoryCache(File.Directory);
			return FoundDirectoryCache.HasFile(File);
		}

		static public bool DirectoryExists(DirectoryReference Directory)
		{
			DirectoryCache FoundDirectoryCache = FindOrCreateDirectoryCache(Directory);
			return FoundDirectoryCache.Exists;
		}

		static public IEnumerable<FileReference> EnumerateFiles(DirectoryReference Directory)
		{
			DirectoryCache FoundDirectoryCache = FindOrCreateDirectoryCache(Directory);
			return FoundDirectoryCache.EnumerateFiles();
		}

		static public IEnumerable<DirectoryReference> EnumerateDirectories(DirectoryReference Directory)
		{
			DirectoryCache FoundDirectoryCache = FindOrCreateDirectoryCache(Directory);
			return FoundDirectoryCache.EnumerateDirectories();
		}

		static public IEnumerable<DirectoryReference> EnumerateDirectoriesRecursively(DirectoryReference Directory)
		{
			DirectoryCache FoundDirectoryCache = FindOrCreateDirectoryCache(Directory);
			foreach (DirectoryReference SubDirectory in FoundDirectoryCache.EnumerateDirectories())
			{
				yield return SubDirectory;

				foreach (DirectoryReference ChildSubDirectory in EnumerateDirectoriesRecursively(SubDirectory))
				{
					yield return ChildSubDirectory;
				}
			}
		}

        static public void InvalidateCachedDirectory(DirectoryReference Directory)
        {
            Directories.Remove(Directory);
        }

		static Dictionary<DirectoryReference, DirectoryCache> Directories = new Dictionary<DirectoryReference, DirectoryCache>();
	}
}
