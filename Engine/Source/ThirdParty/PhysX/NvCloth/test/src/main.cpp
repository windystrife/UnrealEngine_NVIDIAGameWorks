#include "utilities/CallbackImplementations.h"

struct PauseOnEnd
{
	~PauseOnEnd()
	{
		std::cout << "Press ENTER to continue...\n";
		std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
	}
};

int main(int argc, char **argv)
{
	PauseOnEnd poe;

	::testing::InitGoogleTest(&argc, argv);

	NvClothEnvironment::AllocateEnv();
	::testing::AddGlobalTestEnvironment(NvClothEnvironment::GetEnv()); //gtest takes ownership

	auto result = RUN_ALL_TESTS();

	NvClothEnvironment::ReportEnvFreed();
	return result;
}