float4 PossibleColors[6];
bool bConstColor;
uint ConstColorIndex;

void TestMain(
	in int ColorIndex1 : COLOR_INDEX1,
	in uint ColorIndex2 : COLOR_INDEX2,
	out float4 OutColor : SV_Target0
	)
{
	uint BaseColor = 0;
	OutColor = 
		PossibleColors[BaseColor] +
		PossibleColors[ColorIndex1] +
		PossibleColors[ColorIndex2];

	if (bConstColor)
	{
		OutColor = PossibleColors[ConstColorIndex];
	}
}
