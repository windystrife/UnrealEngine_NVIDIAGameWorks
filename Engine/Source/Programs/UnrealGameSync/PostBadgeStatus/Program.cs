// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WriteBadgeStatus
{
	class Program
	{
		static int Main(string[] Args)
		{
			// Parse all the parameters
			List<string> Arguments = new List<string>(Args);
			string Name = ParseParam(Arguments, "Name");
			string Change = ParseParam(Arguments, "Change");
			string Project = ParseParam(Arguments, "Project");
			string Database = ParseParam(Arguments, "Database");
			string Status = ParseParam(Arguments, "Status");
			string Url = ParseParam(Arguments, "Url");

			// Check we've got all the arguments we need (and no more)
			if (Arguments.Count > 0 || Name == null || Change == null || Project == null || Database == null || Status == null || Url == null)
			{
				Console.WriteLine("Syntax:");
				Console.WriteLine("  PostBadgeStatus -Name=<Name> -Change=<CL> -Project=<DepotPath> -Database=<Connection String> -Status=<Status> -Url=<Url>");
				return 1;
			}

			// Open the connection and add the entry
			using(SqlConnection Connection = new SqlConnection(Database))
			{
				Connection.Open();
				using (SqlCommand Command = new SqlCommand("INSERT INTO dbo.CIS (ChangeNumber, BuildType, Result, URL, Project, ArchivePath) VALUES (@ChangeNumber, @BuildType, @Result, @URL, @Project, @ArchivePath)", Connection))
				{
					Command.Parameters.AddWithValue("@ChangeNumber", Change);
					Command.Parameters.AddWithValue("@BuildType", Name);
					Command.Parameters.AddWithValue("@Result", Status);
					Command.Parameters.AddWithValue("@URL", Url);
					Command.Parameters.AddWithValue("@Project", Project);
					Command.Parameters.AddWithValue("@ArchivePath", "");
					Command.ExecuteNonQuery();
				}
			}
			return 0;
		}

		static string ParseParam(List<string> Arguments, string ParamName)
		{
			string ParamPrefix = String.Format("-{0}=", ParamName);
			for (int Idx = 0; Idx < Arguments.Count; Idx++)
			{
				if (Arguments[Idx].StartsWith(ParamPrefix, StringComparison.InvariantCultureIgnoreCase))
				{
					string ParamValue = Arguments[Idx].Substring(ParamPrefix.Length);
					Arguments.RemoveAt(Idx);
					return ParamValue;
				}
			}
			return null;
		}
	}
}
