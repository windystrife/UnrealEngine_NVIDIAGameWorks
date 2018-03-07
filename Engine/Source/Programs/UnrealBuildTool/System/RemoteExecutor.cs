using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	class RemoteExecutor : LocalExecutor
	{
		double AdjustedProcessorCountMultiplierValue;

		public RemoteExecutor()
		{
			Int32 RemoteCPUCount = RPCUtilHelper.GetCommandSlots();
			if (RemoteCPUCount == 0)
			{
				RemoteCPUCount = Environment.ProcessorCount;
			}

			AdjustedProcessorCountMultiplierValue = (Double)RemoteCPUCount / (Double)Environment.ProcessorCount;
			Log.TraceVerbose("Adjusting the remote Mac compile process multiplier to " + AdjustedProcessorCountMultiplierValue.ToString());
		}

		public override string Name
		{
			get { return "Remote"; }
		}

		public override double AdjustedProcessorCountMultiplier
		{
			get { return AdjustedProcessorCountMultiplierValue; }
		}
	}
}
