// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	class TelemetryTimingData
	{
		public string Action;
		public string Result;
		public string UserName;
		public string Project;
		public DateTime Timestamp;
		public float Duration;
	}

	enum TelemetryErrorType
	{
		Crash,
	}

	class TelemetryErrorData
	{
		public TelemetryErrorType Type;
		public string Text;
		public string UserName;
		public string Project;
		public DateTime Timestamp;
	}

	class TelemetryStopwatch : IDisposable
	{
		readonly string Action;
		readonly string Project;
		readonly DateTime StartTime;
		string Result;
		DateTime EndTime;

		public TelemetryStopwatch(string InAction, string InProject)
		{
			Action = InAction;
			Project = InProject;
			StartTime = DateTime.UtcNow;
		}

		public TimeSpan Stop(string InResult)
		{
			EndTime = DateTime.UtcNow;
			Result = InResult;
			return Elapsed;
		}

		public void Dispose()
		{
			if(Result == null)
			{
				Stop("Aborted");
			}
			TelemetryWriter.Enqueue(Action, Result, Project, StartTime, (float)Elapsed.TotalSeconds);
		}

		public TimeSpan Elapsed
		{
			get { return ((Result == null)? DateTime.UtcNow : EndTime) - StartTime; }
		}
	}

	class TelemetryWriter : IDisposable
	{
		static TelemetryWriter Instance;

		string SqlConnectionString;
		Thread WorkerThread;
		BoundedLogWriter LogWriter;
		bool bDisposing;
		ConcurrentQueue<TelemetryTimingData> QueuedTimingData = new ConcurrentQueue<TelemetryTimingData>(); 
		ConcurrentQueue<TelemetryErrorData> QueuedErrorData = new ConcurrentQueue<TelemetryErrorData>(); 
		AutoResetEvent RefreshEvent = new AutoResetEvent(false);

		public TelemetryWriter(string InSqlConnectionString, string InLogFileName)
		{
			Instance = this;

			SqlConnectionString = InSqlConnectionString;

			LogWriter = new BoundedLogWriter(InLogFileName);
			LogWriter.WriteLine("Using connection string: {0}", SqlConnectionString);

			WorkerThread = new Thread(() => WorkerThreadCallback());
			WorkerThread.Start();
		}

		public void Dispose()
		{
			bDisposing = true;

			if(WorkerThread != null)
			{
				RefreshEvent.Set();
				if(!WorkerThread.Join(100))
				{
					WorkerThread.Abort();
					WorkerThread.Join();
				}
				WorkerThread = null;
			}
			if(LogWriter != null)
			{
				LogWriter.Dispose();
				LogWriter = null;
			}

			Instance = null;
		}

		void WorkerThreadCallback()
		{
			string Version = Assembly.GetExecutingAssembly().GetName().Version.ToString();

			string IpAddress = "Unknown";
			try
			{
				IPHostEntry HostEntry = Dns.GetHostEntry(Dns.GetHostName());
				foreach (IPAddress Address in HostEntry.AddressList)
				{
					if (Address.AddressFamily == AddressFamily.InterNetwork)
					{
						IpAddress = Address.ToString();
						break;
					}
				}
			}
			catch
			{
			}

			while(!bDisposing)
			{
				// Wait for an update
				RefreshEvent.WaitOne();

				// Send all the timing data
				TelemetryTimingData TimingData;
				while(QueuedTimingData.TryDequeue(out TimingData))
				{
					while(!bDisposing && !SendTimingData(TimingData, Version, IpAddress) && SqlConnectionString != null)
					{
						RefreshEvent.WaitOne(10 * 1000);
					}
				}

				// Send all the error data
				TelemetryErrorData ErrorData;
				while(QueuedErrorData.TryDequeue(out ErrorData))
				{
					while(!SendErrorData(ErrorData, Version, IpAddress) && SqlConnectionString != null)
					{
						if(bDisposing) break;
						RefreshEvent.WaitOne(10 * 1000);
					}
				}
			}
		}

		bool SendTimingData(TelemetryTimingData Data, string Version, string IpAddress)
		{
			try
			{
				Stopwatch Timer = Stopwatch.StartNew();
				LogWriter.WriteLine("Posting timing data... ({0}, {1}, {2}, {3}, {4}, {5})", Data.Action, Data.Result, Data.UserName, Data.Project, Data.Timestamp, Data.Duration);
				using(SqlConnection Connection = new SqlConnection(SqlConnectionString))
				{
					Connection.Open();
					using (SqlCommand Command = new SqlCommand("INSERT INTO dbo.[Telemetry.v2] (Action, Result, UserName, Project, Timestamp, Duration, Version, IpAddress) VALUES (@Action, @Result, @UserName, @Project, @Timestamp, @Duration, @Version, @IpAddress)", Connection))
					{
						Command.Parameters.AddWithValue("@Action", Data.Action);
						Command.Parameters.AddWithValue("@Result", Data.Result);
						Command.Parameters.AddWithValue("@UserName", Data.UserName);
						Command.Parameters.AddWithValue("@Project", Data.Project);
						Command.Parameters.AddWithValue("@Timestamp", Data.Timestamp);
						Command.Parameters.AddWithValue("@Duration", Data.Duration);
						Command.Parameters.AddWithValue("@Version", Version);
						Command.Parameters.AddWithValue("@IPAddress", IpAddress);
						Command.ExecuteNonQuery();
					}
				}
				LogWriter.WriteLine("Done in {0}ms.", Timer.ElapsedMilliseconds);
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				return false;
			}
		}

		bool SendErrorData(TelemetryErrorData Data, string Version, string IpAddress)
		{
			try
			{
				Stopwatch Timer = Stopwatch.StartNew();
				LogWriter.WriteLine("Posting error data... ({0}, {1})", Data.Type, Data.Timestamp);
				using(SqlConnection Connection = new SqlConnection(SqlConnectionString))
				{
					Connection.Open();
					using (SqlCommand Command = new SqlCommand("INSERT INTO dbo.[Errors] (Type, Text, UserName, Project, Timestamp, Version, IpAddress) VALUES (@Type, @Text, @UserName, @Project, @Timestamp, @Version, @IpAddress)", Connection))
					{
						Command.Parameters.AddWithValue("@Type", Data.Type.ToString());
						Command.Parameters.AddWithValue("@Text", Data.Text);
						Command.Parameters.AddWithValue("@UserName", Data.UserName);
						if(Data.Project == null)
						{
							Command.Parameters.AddWithValue("@Project", DBNull.Value);
						}
						else
						{
							Command.Parameters.AddWithValue("@Project", Data.Project);
						}
						Command.Parameters.AddWithValue("@Timestamp", Data.Timestamp);
						Command.Parameters.AddWithValue("@Version", Version);
						Command.Parameters.AddWithValue("@IPAddress", IpAddress);
						Command.ExecuteNonQuery();
					}
				}
				LogWriter.WriteLine("Done in {0}ms.", Timer.ElapsedMilliseconds);
				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteException(Ex, "Failed with exception.");
				return false;
			}
		}

		public static void Enqueue(string Action, string Result, string Project, DateTime Timestamp, float Duration)
		{
			TelemetryWriter Writer = Instance;
			if(Writer != null)
			{
				TelemetryTimingData Telemetry = new TelemetryTimingData();
				Telemetry.Action = Action;
				Telemetry.Result = Result;
				Telemetry.UserName = Environment.UserName;
				Telemetry.Project = Project;
				Telemetry.Timestamp = Timestamp;
				Telemetry.Duration = Duration;

				Writer.QueuedTimingData.Enqueue(Telemetry);
				Writer.RefreshEvent.Set();
			}
		}

		public static void Enqueue(TelemetryErrorType Type, string Text, string Project, DateTime Timestamp)
		{
			TelemetryWriter Writer = Instance;
			if(Writer != null)
			{
				TelemetryErrorData Error = new TelemetryErrorData();
				Error.Type = Type;
				Error.Text = Text;
				Error.UserName = Environment.UserName;
				Error.Project = Project;
				Error.Timestamp = Timestamp;

				Writer.QueuedErrorData.Enqueue(Error);
				Writer.RefreshEvent.Set();
			}
		}
	}
}
