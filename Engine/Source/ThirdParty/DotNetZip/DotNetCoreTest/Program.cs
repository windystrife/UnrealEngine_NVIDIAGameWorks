using System;
using System.IO;
using Ionic.Zip;

namespace DotNetCoreTest
{
    class Program
    {
        static void Main(string[] args)
        {
			if (args.Length != 3)
			{
				throw new ArgumentException("USAGE: dotnetcoretest <inputdir> <zipfilename> <extractdir>");
			}

            DirectoryInfo InputDirectory = new DirectoryInfo(args[0]);
			FileInfo OutputZipFile = new FileInfo(args[1]);
			DirectoryInfo OutputDirectory = new DirectoryInfo(args[2]);

            if (OutputZipFile.Exists)
            {
                OutputZipFile.Delete();
            }

			if (InputDirectory.Exists)
            {
				using (ZipFile File = new ZipFile())
				{
					File.AddDirectory(InputDirectory.FullName);
					File.Save(OutputZipFile.FullName);
				}
            }

			if (OutputDirectory.Exists)
			{
				OutputDirectory.Delete();
			}

			OutputDirectory.Create();

			if (OutputZipFile.Exists)
			{
				using (ZipFile File = new ZipFile(OutputZipFile.FullName))
				{
					foreach (ZipEntry Entry in File.Entries)
					{
						string OutputFileName = Path.Combine(OutputDirectory.FullName, Entry.FileName);
						(new DirectoryInfo(Path.GetDirectoryName(OutputFileName))).Create();
				
						if (!Entry.IsDirectory)
						{
							using (FileStream OutputStream = new FileStream(OutputFileName, FileMode.Create, FileAccess.Write))
							{
								Entry.Extract(OutputStream);
							}
						}
					}
				}
			}
/*
			using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile(@"d:\test.zip"))
			{
				foreach (Ionic.Zip.ZipEntry Entry in Zip.Entries)
				{
					// support-v4 and support-v13 has the jar file named with "internal_impl-XX.X.X.jar"
					// this causes error "Found 2 versions of internal_impl-XX.X.X.jar"
					// following codes adds "support-v4-..." to the output jar file name to avoid the collision
					string OutputFileName = Path.Combine(BaseDirectory, Entry.FileName);

					if (!Entry.IsDirectory)
					{
						using (FileStream OutputStream = new FileStream(OutputFileName, FileMode.Create, FileAccess.Write))
						{
							Entry.Extract(OutputStream);
						}
					}
				}
			}
            */
        }
    }
}
