// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class DeleteFilesTask : IModalTask
	{
		List<FileInfo> Files;
		List<DirectoryInfo> Directories;

		public DeleteFilesTask(List<FileInfo> InFiles, List<DirectoryInfo> InDirectories)
		{
			Files = InFiles;
			Directories = InDirectories;
		}

		public bool Run(out string ErrorMessage)
		{
			StringBuilder FailMessage = new StringBuilder();
			foreach(FileInfo File in Files)
			{
				try
				{
					File.IsReadOnly = false;
					File.Delete();
				}
				catch(Exception Ex)
				{
					FailMessage.AppendFormat("{0} ({1})\r\n", File.FullName, Ex.Message.Trim());
				}
			}
			foreach(DirectoryInfo Directory in Directories)
			{
				try
				{
					Directory.Delete(true);
				}
				catch(Exception Ex)
				{
					FailMessage.AppendFormat("{0} ({1})\r\n", Directory.FullName, Ex.Message.Trim());
				}
			}

			if(FailMessage.Length == 0)
			{
				ErrorMessage = null;
				return true;
			}
			else 
			{
				ErrorMessage = FailMessage.ToString();
				return false;
			}
		}
	}
}
