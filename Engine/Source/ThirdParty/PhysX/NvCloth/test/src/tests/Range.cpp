#include "utilities/Utilities.h"
#include <NvCloth/Range.h>
TEST_LEAK_FIXTURE(Range)

TEST_F(Range, Constructor)
{
	nv::cloth::Range<char> char_range;
	EXPECT_EQ(char_range.size(), 0);
	EXPECT_NULLPTR(char_range.begin());
	EXPECT_NULLPTR(char_range.end());

	nv::cloth::Range<char> a((char*)0x123, (char*)0x456);
	nv::cloth::Range<char> b(a);
	EXPECT_EQ(b.begin(), a.begin());
	EXPECT_EQ(b.end(), a.end());
}


TEST_F(Range, Size)
{
	std::vector<char> char_array;
	char_array.resize(100);
	nv::cloth::Range<char> char_range(&char_array[0], &char_array[0] + char_array.size());
	EXPECT_EQ(char_array.size(), char_range.size());

	std::vector<float> float_array;
	float_array.resize(100);
	nv::cloth::Range<float> float_range(&float_array[0], &float_array[0] + float_array.size());
	EXPECT_EQ(float_array.size(), float_range.size());
}

TEST_F(Range, Empty)
{
	{
		nv::cloth::Range<char> char_range;
		EXPECT_TRUE(char_range.empty());

		nv::cloth::Range<float> float_range;
		EXPECT_TRUE(float_range.empty());
	}

	{
		std::vector<char> char_array;
		char_array.resize(100);
		nv::cloth::Range<char> char_range = CreateRange(char_array);
		EXPECT_FALSE(char_range.empty());

		std::vector<float> float_array;
		float_array.resize(100);
		nv::cloth::Range<float> float_range = CreateRange(float_array);
		EXPECT_FALSE(float_range.empty());
	}
}

TEST_F(Range, Pop)
{
	std::vector<char> char_array;
	char_array.resize(100);
	nv::cloth::Range<char> char_range(&char_array[0], &char_array[0] + char_array.size());
	EXPECT_EQ(&char_array.front(), &char_range.front());
	EXPECT_EQ(&char_array.back(), &char_range.back());
	char_range.popBack();
	EXPECT_EQ(&char_array.back() - 1, &char_range.back());
	char_range.popFront();
	EXPECT_EQ(&char_array.front() + 1, &char_range.front());

	std::vector<float> float_array;
	float_array.resize(100);
	nv::cloth::Range<float> float_range(&float_array[0], &float_array[0] + float_array.size());
	EXPECT_EQ(&float_array.front(), &float_range.front());
	EXPECT_EQ(&float_array.back(), &float_range.back());
	float_range.popBack();
	EXPECT_EQ(&float_array.back() - 1, &float_range.back());
	float_range.popFront();
	EXPECT_EQ(&float_array.front() + 1, &float_range.front());
}

TEST_F(Range, BeginEnd)
{
	std::vector<char> char_array;
	char_array.resize(100);
	nv::cloth::Range<char> char_range(&char_array[0], &char_array[0]+char_array.size());
	EXPECT_EQ(&char_array[0], char_range.begin());
	EXPECT_EQ(&char_array[0] + char_array.size(), char_range.end());

	std::vector<float> float_array;
	float_array.resize(100);
	nv::cloth::Range<float> float_range(&float_array[0], &float_array[0] + float_array.size());
	EXPECT_EQ(&float_array[0], float_range.begin());
	EXPECT_EQ(&float_array[0] + float_array.size(), float_range.end());
}

TEST_F(Range, FrontBack)
{
	std::vector<char> char_array;
	char_array.resize(100);
	nv::cloth::Range<char> char_range(&char_array[0], &char_array[0] + char_array.size());
	EXPECT_EQ(&char_array.front(), &char_range.front());
	EXPECT_EQ(&char_array.back(), &char_range.back());

	std::vector<float> float_array;
	float_array.resize(100);
	nv::cloth::Range<float> float_range(&float_array[0], &float_array[0] + float_array.size());
	EXPECT_EQ(&float_array.front(), &float_range.front());
	EXPECT_EQ(&float_array.back(), &float_range.back());
}

TEST_F(Range, ArrayOperator)
{
	std::vector<char> char_array;
	char_array.resize(100);
	nv::cloth::Range<char> char_range(&char_array[0], &char_array[0] + char_array.size());
	EXPECT_EQ(&char_array[0], &char_range[0]);
	EXPECT_EQ(&char_array[99], &char_range[99]);

	std::vector<float> float_array;
	float_array.resize(100);
	nv::cloth::Range<float> float_range(&float_array[0], &float_array[0] + float_array.size());
	EXPECT_EQ(&float_array[0], &float_range[0]);
	EXPECT_EQ(&float_array[99], &float_range[99]);
}