// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	public abstract class LineBasedTextWriter : TextWriter
	{
		StringBuilder CurrentLine = new StringBuilder();

		public LineBasedTextWriter()
		{
		}

		public override void Write(char Character)
		{
			if(Character == '\n')
			{
				FlushLine(CurrentLine.ToString());
				CurrentLine.Clear();
			}
			else if(Character != '\r')
			{
				CurrentLine.Append(Character);
			}
		}

		public override void WriteLine(string Line)
		{
			foreach(string SubLine in Line.Split('\n'))
			{
				FlushLine(SubLine.TrimEnd());
			}
		}

		protected abstract void FlushLine(string Line);

		public override Encoding Encoding
		{
			get { return Encoding.UTF8; }
		}
	}

	class NullTextWriter : LineBasedTextWriter
	{
		protected override void FlushLine(string Line)
		{
		}
	}

	class PrefixedTextWriter : LineBasedTextWriter
	{
		string Prefix;
		TextWriter Inner;

		public PrefixedTextWriter(string InPrefix, TextWriter InInner)
		{
			Prefix = InPrefix;
			Inner = InInner;
		}

		protected override void Dispose(bool bDisposing)
		{
			if(bDisposing)
			{
				Inner.Dispose();
			}
		}

		protected override void FlushLine(string Line)
		{
			Inner.WriteLine(Prefix + Line);
		}
	}
	
	class BoundedLogWriter : LineBasedTextWriter
	{
		string FileName;
		string BackupFileName;
		int MaxSize;
		StreamWriter Inner;

		public BoundedLogWriter(string InFileName, int InMaxSize = 128 * 1024)
		{
			FileName = InFileName;
			BackupFileName = FileName + ".bak";
			MaxSize = InMaxSize;
			OpenInner();
		}

		protected override void Dispose(bool bDisposing)
		{
			if(bDisposing)
			{
				CloseInner();
			}
		}

		protected override void FlushLine(string Line)
		{
			if(Inner != null)
			{
				Inner.WriteLine(Line);
				if(Inner.BaseStream.Position > MaxSize)
				{
					CloseInner();
					OpenInner();
				}
			}
		}

		private void OpenInner()
		{
			CloseInner();
			try
			{
				if(File.Exists(BackupFileName))
				{
					File.Delete(BackupFileName);
				}
				if(File.Exists(FileName))
				{
					File.Move(FileName, BackupFileName);
				}

				Inner = new StreamWriter(FileName);
				Inner.AutoFlush = true;
			}
			catch(Exception)
			{
				Inner = null;
			}
		}

		private void CloseInner()
		{
			if(Inner != null)
			{
				Inner.Close();
				Inner = null;
			}
		}
	}

	class ComposedTextWriter : TextWriter
	{
		TextWriter[] Inners;

		public ComposedTextWriter(params TextWriter[] InInners)
		{
			Inners = InInners;
		}

		protected override void Dispose(bool bDisposing)
		{
			if(bDisposing)
			{
				foreach(TextWriter Inner in Inners)
				{
					Inner.Dispose();
				}
			}
		}

		public override void Write(char Character)
		{
			foreach(TextWriter Inner in Inners)
			{
				Inner.Write(Character);
			}
		}

		public override void WriteLine(string Line)
		{
			foreach(TextWriter Inner in Inners)
			{
				Inner.WriteLine(Line);
			}
		}

		public override Encoding Encoding
		{
			get { return Encoding.UTF8; }
		}
	}

	class ProgressValue
	{
		Tuple<string, float> State;
		Stack<Tuple<float, float>> Ranges = new Stack<Tuple<float,float>>();

		public ProgressValue()
		{
			Clear();
		}

		public void Clear()
		{
			State = new Tuple<string,float>("Starting...", 0.0f);

			Ranges.Clear();
			Ranges.Push(new Tuple<float, float>(0.0f, 1.0f));
		}

		public Tuple<string, float> Current
		{
			get { return State; }
		}

		public void Set(string Message)
		{
			if(Ranges.Count == 1)
			{
				State = new Tuple<string,float>(Message, State.Item2);
			}
		}

		public void Set(string Message, float Fraction)
		{
			if(Ranges.Count == 1)
			{
				State = new Tuple<string, float>(Message, RelativeToAbsoluteFraction(Fraction));
			}
			else
			{
				State = new Tuple<string, float>(State.Item1, RelativeToAbsoluteFraction(Fraction));
			}
		}

		public void Set(float Fraction)
		{
			State = new Tuple<string, float>(State.Item1, RelativeToAbsoluteFraction(Fraction));
		}

		public void Increment(float Fraction)
		{
			Set(State.Item2 + RelativeToAbsoluteFraction(Fraction));
		}

		public void Push(float MaxFraction)
		{
			Ranges.Push(new Tuple<float,float>(State.Item2, RelativeToAbsoluteFraction(MaxFraction)));
		}

		public void Pop()
		{
			if(Ranges.Count > 1)
			{
				State = new Tuple<string,float>(State.Item1, Ranges.Pop().Item2);
			}
		}

		float RelativeToAbsoluteFraction(float Fraction)
		{
			Tuple<float, float> Range = Ranges.Peek();
			return Range.Item1 + (Range.Item2 - Range.Item1) * Fraction;
		}
	}

	class ProgressTextWriter : LineBasedTextWriter
	{
		const string DirectivePrefix = "@progress ";

		ProgressValue Value;
		TextWriter Inner;

		public ProgressTextWriter(ProgressValue InValue, TextWriter InInner)
		{ 
			Value = InValue;
			Inner = InInner;
		}

		protected override void FlushLine(string Line)
		{
			if(Line.StartsWith(DirectivePrefix))
			{
				// Line that just contains a progress directive
				bool bSkipLine = false;
				ProcessInternal(Line.Substring(DirectivePrefix.Length), ref bSkipLine);
			}
			else
			{
				bool bSkipLine = false;
				string RemainingLine = Line;

				// Look for a progress directive at the end of a line, in square brackets
				string TrimLine = Line.TrimEnd();
				if(TrimLine.EndsWith("]"))
				{
					for(int LastIdx = TrimLine.Length - 2; LastIdx >= 0 && TrimLine[LastIdx] != ']'; LastIdx--)
					{
						if(TrimLine[LastIdx] == '[')
						{
							string DirectiveSubstring = TrimLine.Substring(LastIdx + 1, TrimLine.Length - LastIdx - 2);
							if(DirectiveSubstring.StartsWith(DirectivePrefix))
							{
								ProcessInternal(DirectiveSubstring.Substring(DirectivePrefix.Length), ref bSkipLine);
								RemainingLine = Line.Substring(0, LastIdx).TrimEnd();
							}
							break;
						}
					}
				}

				if(!bSkipLine)
				{
					Inner.WriteLine(RemainingLine);
				}
			}
		}

		void ProcessInternal(string Line, ref bool bSkipLine)
		{
			List<string> Tokens = ParseTokens(Line);
			for(int TokenIdx = 0; TokenIdx < Tokens.Count; )
			{
				float Fraction;
				if(ReadFraction(Tokens, ref TokenIdx, out Fraction))
				{
					Value.Set(Fraction);
				}
				else if(Tokens[TokenIdx] == "push")
				{
					TokenIdx++;
					if(ReadFraction(Tokens, ref TokenIdx, out Fraction))
					{
						Value.Push(Fraction);
					}
				}
				else if(Tokens[TokenIdx] == "pop")
				{
					TokenIdx++;
					Value.Pop();
				}
				else if(Tokens[TokenIdx] == "increment")
				{
					TokenIdx++;
					if(ReadFraction(Tokens, ref TokenIdx, out Fraction))
					{
						Value.Increment(Fraction);
					}
				}
				else if(Tokens[TokenIdx] == "skipline")
				{
					TokenIdx++;
					bSkipLine = true;
				}
				else if(Tokens[TokenIdx].Length >= 2 && (Tokens[TokenIdx][0] == '\'' || Tokens[TokenIdx][0] == '\"') && Tokens[TokenIdx].Last() == Tokens[TokenIdx].First())
				{
					string Message = Tokens[TokenIdx++];
					Value.Set(Message.Substring(1, Message.Length - 2));
				}
				else
				{
					TokenIdx++;
				}
			}
		}

		static List<string> ParseTokens(string Line)
		{
			List<string> Tokens = new List<string>();
			for(int Idx = 0;;)
			{
				// Skip whitespace
				while(Idx < Line.Length && Char.IsWhiteSpace(Line[Idx]))
				{
					Idx++;
				}
				if(Idx == Line.Length)
				{
					break;
				}

				// Read the next token
				if(Char.IsLetterOrDigit(Line[Idx]))
				{
					int StartIdx = Idx++;
					while(Idx < Line.Length && Char.IsLetterOrDigit(Line[Idx]))
					{
						Idx++;
					}
					Tokens.Add(Line.Substring(StartIdx, Idx - StartIdx));
				}
				else if(Line[Idx] == '\'' || Line[Idx] == '\"')
				{
					int StartIdx = Idx++;
					while(Idx < Line.Length && Line[Idx] != Line[StartIdx])
					{
						Idx++;
					}
					Tokens.Add(Line.Substring(StartIdx, ++Idx - StartIdx));
				}
				else
				{
					Tokens.Add(Line.Substring(Idx++, 1));
				}
			}
			return Tokens;
		}

		bool ReadFraction(List<string> Tokens, ref int TokenIdx, out float Fraction)
		{
			// Read a fraction in the form x%
			if(TokenIdx + 2 <= Tokens.Count && Tokens[TokenIdx + 1] == "%")
			{
				int Numerator;
				if(int.TryParse(Tokens[TokenIdx], out Numerator))
				{
					Fraction = (float)Numerator / 100.0f;
					TokenIdx += 2;
					return true;
				}
			}
			
			// Read a fraction in the form x/y
			if(TokenIdx + 3 <= Tokens.Count && Tokens[TokenIdx + 1] == "/")
			{
				int Numerator, Denominator;
				if(int.TryParse(Tokens[TokenIdx], out Numerator) && int.TryParse(Tokens[TokenIdx + 2], out Denominator))
				{
					Fraction = (float)Numerator / (float)Denominator;
					TokenIdx += 3;
					return true;
				}
			}

			Fraction = 0.0f;
			return false;
		}
	}
}

