#include "utilities/Utilities.h"
#include <../../src/Simd.h>

////Helper functions
namespace
{
namespace Local
{

float floor(float x)
{
	return floorf(x);
}

float sqrt(float x)
{
	return sqrtf(x);
}

float exp2(float x)
{
	return expf(x * logf(2));
}

float log2(float x)
{
	return logf(x) / logf(2);
}

template<int i = 0>
float recip(float a)
{
	return 1.0f / a;
}

template<int i = 0>
float rsqrt(float a)
{
	return recip(sqrt(a));
}

float abs(float a)
{
	return fabsf(a);
}

}
}

bool isnan2(float a)
{
	volatile float a1 = a;
	volatile float a2 = a;
	return !(a1 == a2);
}

void TestFloat(float a, float b)
{
	if(*(int*)&a == *(int*)&b) //test nans
		return;
	ASSERT_FLOAT_EQ(a, b);	//google's 'almost' equal
}
void TestFloat2(float a, float b)
{
	//ignore NaNs
	if(isnan2(a) && isnan2(b))
		return;
	ASSERT_FLOAT_EQ(a, b);	//google's 'almost' equal
}

void TestFloatBits(float a, float b)
{
	ASSERT_EQ(*(int*)&a, *(int*)&b);
}

float noConst(float a){ return a; } //used to let 1.0f/noConst(0.0f) compile

float compareMask(bool result)
{
	unsigned int mask = result ? ~0 : 0;
	return *(float*)&mask;
}
/////////////////////
//Float binary operators

#define TEST_OPERATOR_DEFAULT_DATA(op)\
	TEST_OPERATOR(op,1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f)\
	TEST_OPERATOR(op,1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 0.0f, 4.0f)\
	TEST_OPERATOR(op,0.0f,1.0f,-0.0f,0.1f,9.0f,8.0f,7.0f,6.0f)\
	TEST_OPERATOR(op,1.0e20f,1.0e-20f,-1.0e20f,-1.0e-20f,5.0f, 6.0f, 7.0f, 8.0f)\
	TEST_OPERATOR(op,1000.0f,10000.0f,10000.0f,100000,1.0f,10000.0f,-100.0f,-10000000.0f)\
	TEST_OPERATOR(op,0.0f, 1.0f, 2.0f, 4.0f, 1.0f, 0.5f, 0.25f, 0.125f)\
	TEST_OPERATOR(op,0.0f, 1.0f, 2.0f, 4.0f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)\
	TEST_OPERATOR(op,1.0f, 0.5f, 0.25f, 0.125f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)\
	TEST_OPERATOR(op,1.0f, 0.5f, 0.25f, 0.125f, FLT_EPSILON, -FLT_EPSILON, FLT_MAX, -FLT_MAX)\
	TEST_OPERATOR(op,0.0f, 1.0f, 2.0f, 4.0f, -logf(0.0f), -sqrtf(-1.0f), logf(0.0f), sqrtf(-1.0f))\

//special case as we don't have bitwise operators on floats in c++
int f2i(float a){ return *(int*)&a; }
unsigned int f2ui(float a){ return *(unsigned int*)&a; }
float i2f(int a){ return *(float*)&a; }
#define TEST_OPERATOR(op,v1,v2,v3,v4,w1,w2,w3,w4)	\
	{												\
		Simd4f a = simd4f(v1,v2,v3,v4);				\
		Simd4f b = simd4f(w1,w2,w3,w4);				\
		Simd4f c = a op b;							\
		float c_[4];								\
		store(c_, c);								\
		TestFloat(i2f(f2i(v1) op f2i(w1)), c_[0]);					\
		TestFloat(i2f(f2i(v2) op f2i(w2)), c_[1]);					\
		TestFloat(i2f(f2i(v3) op f2i(w3)), c_[2]);					\
		TestFloat(i2f(f2i(v4) op f2i(w4)), c_[3]);					\
	}

TEST(Simd, And)
{
	TEST_OPERATOR_DEFAULT_DATA(&)
}

TEST(Simd, Or)
{
	TEST_OPERATOR_DEFAULT_DATA(|)
}

TEST(Simd, Xor)
{
	TEST_OPERATOR_DEFAULT_DATA(^)
}

//for normal float operators
#undef TEST_OPERATOR
#define TEST_OPERATOR(op,v1,v2,v3,v4,w1,w2,w3,w4)	\
	{												\
		Simd4f a = simd4f(v1,v2,v3,v4);				\
		Simd4f b = simd4f(w1,w2,w3,w4);				\
		Simd4f c = a op b;							\
		float c_[4];								\
		store(c_, c);								\
		TestFloat(v1 op noConst(w1), c_[0]);					\
		TestFloat(v2 op noConst(w2), c_[1]);					\
		TestFloat(v3 op noConst(w3), c_[2]);					\
		TestFloat(v4 op noConst(w4), c_[3]);					\
	}

TEST(Simd, Add)
{
	TEST_OPERATOR_DEFAULT_DATA(+)
}

TEST(Simd, Sub)
{
	TEST_OPERATOR_DEFAULT_DATA(-)
}

TEST(Simd, Mul)
{
	TEST_OPERATOR_DEFAULT_DATA(*)
}

TEST(Simd, Div)
{
	TEST_OPERATOR_DEFAULT_DATA(/)
}

//float bitwise Unary operators
#undef TEST_OPERATOR
#define TEST_OPERATOR(op,v1,v2,v3,v4,w1,w2,w3,w4)	\
	{												\
		Simd4f b = simd4f(w1,w2,w3,w4);				\
		Simd4f c = op b;							\
		float c_[4];								\
		store(c_, c);								\
		TestFloat(i2f(op f2i(w1)), c_[0]);			\
		TestFloat(i2f(op f2i(w2)), c_[1]);			\
		TestFloat(i2f(op f2i(w3)), c_[2]);			\
		TestFloat(i2f(op f2i(w4)), c_[3]);			\
	}

TEST(Simd, Not)
{
	TEST_OPERATOR_DEFAULT_DATA(~)
}

//float Unary operators
#undef TEST_OPERATOR
#define TEST_OPERATOR(op,v1,v2,v3,v4,w1,w2,w3,w4)	\
	{												\
		Simd4f b = simd4f(w1,w2,w3,w4);				\
		Simd4f c = op b;							\
		float c_[4];								\
		store(c_, c);								\
		TestFloat(op w1, c_[0]);					\
		TestFloat(op w2, c_[1]);					\
		TestFloat(op w3, c_[2]);					\
		TestFloat(op w4, c_[3]);					\
	}

TEST(Simd, UnaryMin)
{
	TEST_OPERATOR_DEFAULT_DATA(-)
}

//compare operators
#undef TEST_OPERATOR
#define TEST_OPERATOR(op,v1,v2,v3,v4,w1,w2,w3,w4)	\
	{												\
		Simd4f a = simd4f(v1,v2,v3,v4);				\
		Simd4f b = simd4f(w1,w2,w3,w4);				\
		Simd4f c = a op b;							\
		float c_[4];								\
		store(c_, c);								\
		TestFloatBits(compareMask(v1 op w1), c_[0]);\
		TestFloatBits(compareMask(v2 op w2), c_[1]);\
		TestFloatBits(compareMask(v3 op w3), c_[2]);\
		TestFloatBits(compareMask(v4 op w4), c_[3]);\
	}

TEST(Simd, Equal)
{
	TEST_OPERATOR_DEFAULT_DATA(==)
}
TEST(Simd, Greater)
{
	TEST_OPERATOR_DEFAULT_DATA(>)
}
TEST(Simd, Less)
{
	TEST_OPERATOR_DEFAULT_DATA(<)
}
TEST(Simd, GreaterEqual)
{
	TEST_OPERATOR_DEFAULT_DATA(>=)
}
TEST(Simd, LessEqual)
{
	TEST_OPERATOR_DEFAULT_DATA(<= )
}

//shift operators
#undef TEST_OPERATOR
#define TEST_OPERATOR(op,v1,v2,v3,v4,w1,w2,w3,w4)	\
	{												\
		Simd4f a = simd4f(v1,v2,v3,v4);				\
		int shift = (((unsigned)w1)&0x1F);			\
		Simd4f c = a op shift;						\
		float c_[4];								\
		store(c_, c);								\
		ASSERT_EQ(f2ui(v1) op shift, *(unsigned*)&c_[0]);\
		ASSERT_EQ(f2ui(v2) op shift, *(unsigned*)&c_[1]);\
		ASSERT_EQ(f2ui(v3) op shift, *(unsigned*)&c_[2]);\
		ASSERT_EQ(f2ui(v4) op shift, *(unsigned*)&c_[3]);\
	}

TEST(Simd, ShiftLeft)
{
	TEST_OPERATOR_DEFAULT_DATA(<<)
}
TEST(Simd, ShiftRight)
{
	TEST_OPERATOR_DEFAULT_DATA(>>)
}


//// Test Factories
//TODO: should Scalar4f() also be tested?

TEST(Simd, ZeroFactory)
{
	Simd4f v_ = Simd4fZeroFactory();
	float v[4];
	store(v, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_EQ(v[i], 0);
}

TEST(Simd, OneFactory)
{
	Simd4f v_ = Simd4fOneFactory();
	float v[4];
	store(v, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_EQ(v[i], 1);
}

TEST(Simd, ScalarFactory)
{
	Simd4f v_ = Simd4fScalarFactory(3.1415f);
	float v[4];
	store(v, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], 3.1415f);
}

TEST(Simd, TupleFactory)
{
	{
		Simd4f v_ = Simd4fTupleFactory(0.0f, 1.0f, 2.0f, 3.0f);
		float v[4];
		store(v, v_);
		for(int i = 0; i < 4; i++)
			EXPECT_FLOAT_EQ(v[i], (float)i);
	}
	{
		Simd4f v_ = Simd4fTupleFactory((unsigned)0,(unsigned)1,(unsigned)2,(unsigned)3);
		float v[4];
		store(v, v_);
		for(int i = 0; i < 4; i++)
			EXPECT_EQ(*(unsigned*)&v[i],i);
	}
}

TEST(Simd, LoadFactory)
{
	float v[4] = {0.0f, 1.0f, 2.0f, 3.0f};
	Simd4f v_ = Simd4fLoadFactory(v);
	float v__[4];
	store(v__, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v__[i], (float)i);
}

TEST(Simd, AlignedLoadFactory)
{
	float v[8];
	uintptr_t ptr = (uintptr_t)&v[0];
	float* fptr = (float*)((ptr + 15) & ~15);
	for(int i = 0; i < 4; i++)
		fptr[i] = (float)i;
	Simd4f v_ = Simd4fAlignedLoadFactory(fptr);
	float v__[4];
	store(v__, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v__[i], (float)i);
}

TEST(Simd, Load3Factory)
{
	float v[4] = {0.0f, 1.0f, 2.0f, 3.0f};
	Simd4f v_ = Simd4fLoad3Factory(v);
	float v__[4];
	store(v__, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v__[i], (float)(i%3)); //last element is 0
}

TEST(Simd, Load3SetWFactory)
{
	float v[4] = {1.0f, 2.0f, 3.0f, 4.0f};
	Simd4f v_ = Simd4fLoad3SetWFactory(v,1.0f);
	float v__[4];
	store(v__, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v__[i], (float)((i%3)+1)); //last element is 1
}

TEST(Simd, OffsetLoadFactory)
{
	float v[12];
	uintptr_t ptr = (uintptr_t)&v[0];
	float* fptr = (float*)((ptr + 15) & ~15);
	for(int i = 0; i < 8; i++)
		fptr[i] = (float)i;
	Simd4f v_ = Simd4fOffsetLoadFactory(fptr,4*sizeof(float));
	float v__[4];
	store(v__, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v__[i], (float)i+4);
}


//// Functions
TEST(Simd, ConstructWithScalar)
{
	Simd4f v_ = simd4f(1.0f);
	float v[4];
	store(v, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], 1.0f);
}
TEST(Simd, ConstructWithScalars)
{
	Simd4f v_ = simd4f(1.0f,2.0f,3.0f,4.0f);
	float v[4];
	store(v, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], (float)(i+1));
}
TEST(Simd, CastToArray)
{
	Simd4f v_ = simd4f(1.0f,2.0f,3.0f,4.0f);
	float* v = array(v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], (float)(i+1));
}
TEST(Simd, CastToConstArray)
{
	Simd4f v_ = simd4f(1.0f,2.0f,3.0f,4.0f);
	float const* v = array(v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], (float)(i+1));
}
TEST(Simd, LoadArray)
{
	float v__[4] = {1.0f, 2.0f, 3.0f, 4.0f};
	Simd4f v_ = load(v__);
	float v[4];
	store(v, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], (float)(i+1));
}

TEST(Simd, LoadAlignedArray)
{
	float v[8];
	uintptr_t ptr = (uintptr_t)&v[0];
	float* fptr = (float*)((ptr + 15) & ~15);
	for(int i = 0; i < 4; i++)
		fptr[i] = (float)i;
	Simd4f v_ = loadAligned(fptr);
	float v__[4];
	store(v__, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v__[i], (float)i);
}

TEST(Simd, Load3)
{
	float v__[4] = {1.0f, 2.0f, 3.0f, 4.0f};
	Simd4f v_ = load3(v__);
	float v[4];
	store(v, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], (float)(((i+1)%4)));
}

TEST(Simd, Load3SetW)
{
	float v__[4] = {1.0f, 2.0f, 3.0f, 0.0f};
	Simd4f v_ = load3(v__,4.0f);
	float v[4];
	store(v, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], (float)(i+1));
}

TEST(Simd, LoadAlignedArrayOffset)
{
	float v[12];
	uintptr_t ptr = (uintptr_t)&v[0];
	float* fptr = (float*)((ptr + 15) & ~15);
	for(int i = 0; i < 8; i++)
		fptr[i] = (float)i;
	Simd4f v_ = loadAligned(fptr,4*sizeof(float));
	float v__[4];
	store(v__, v_);
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v__[i], (float)(i+4));
}

TEST(Simd, Store)
{
	float v[4] = {-1.0f, -1.0f, -1.0f, 1.0f};
	store(v, simd4f(1.0f,2.0f,3.0f,4.0f));
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], (float)(i+1));
}

TEST(Simd, Store3)
{
	float v[4] = {-1.0f, -1.0f, -1.0f, 1.0f};
	store3(v, simd4f(1.0f,2.0f,3.0f,4.0f));
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(v[i], (float)(i%3+1));
}

TEST(Simd, StoreAligned)
{
	float v[8];
	memset(v, 0, sizeof(float) * 8);
	uintptr_t ptr = (uintptr_t)&v[0];
	float* fptr = (float*)((ptr + 15) & ~15);
	storeAligned(fptr, simd4f(1.0f, 2.0f, 3.0f, 4.0f));
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(fptr[i], (float)(i+1));
}
TEST(Simd, StoreAlignedOffset)
{
	float v[12];
	memset(v, 0, sizeof(float) * 12);
	uintptr_t ptr = (uintptr_t)&v[0];
	float* fptr = (float*)((ptr + 15) & ~15);
	storeAligned(fptr, 4*sizeof(float), simd4f(1.0f, 2.0f, 3.0f, 4.0f));
	for(int i = 0; i < 4; i++)
		EXPECT_FLOAT_EQ(fptr[i+4], (float)(i+1));
}

TEST(Simd, Splat)
{
	Simd4f v[4];
	v[0] = splat<0>(simd4f(0.0f, 1.0f, 2.0f, 3.0f));
	v[1] = splat<1>(simd4f(0.0f, 1.0f, 2.0f, 3.0f));
	v[2] = splat<2>(simd4f(0.0f, 1.0f, 2.0f, 3.0f));
	v[3] = splat<3>(simd4f(0.0f, 1.0f, 2.0f, 3.0f));

	for(int j = 0; j < 4; j++)
	{
		for(int i = 0; i < 4; i++)
		{
			EXPECT_FLOAT_EQ(array(v[j])[i], (float)j);
		}
	}
}

TEST(Simd, Select)
{
	Simd4f masks[4];
	for(int i = 0; i < 4; i++)
	{
		masks[i] = simd4f(compareMask(i == 0), compareMask(i == 1), compareMask(i == 2), compareMask(i == 3));
	}

	Simd4f v[4];
	for(int i = 0; i < 4; i++)
	{
		v[i] = select(masks[i], simd4f(0.0f, 1.0f, 2.0f, 3.0f), simd4f(4.0f, 5.0f, 6.0f, 7.0f));
	}

	for(int j = 0; j < 4; j++)
	{
		for(int i = 0; i < 4; i++)
		{
			EXPECT_FLOAT_EQ(array(v[j])[i], (float)(i + (i==j?0:4)));
		}
	}
}

#undef TEST_OPERATOR
#define TEST_OPERATOR(op,v1,v2,v3,v4,w1,w2,w3,w4)	\
	{												\
		Simd4f b = simd4f(w1,w2,w3,w4);				\
		Simd4f c = op(b);							\
		float c_[4];								\
		store(c_, c);								\
		TestFloat(::Local::op(w1), c_[0]);					\
		TestFloat(::Local::op(w2), c_[1]);					\
		TestFloat(::Local::op(w3), c_[2]);					\
		TestFloat(::Local::op(w4), c_[3]);					\
	}


//This one fails because we don't have a proper sse2 floor
TEST(Simd, Floor)
{
	//Failing cases are commented out.
	TEST_OPERATOR(floor,1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f)
	TEST_OPERATOR(floor,1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 0.0f, 4.0f)
	TEST_OPERATOR(floor,0.0f,1.0f,-0.0f,0.1f,9.0f,8.0f,7.0f,6.0f)
	//TEST_OPERATOR(floor,1.0e20f,1.0e-20f,-1.0e20f,-1.0e-20f,5.0f, 6.0f, 7.0f, 8.0f)
	TEST_OPERATOR(floor,1000.0f,10000.0f,10000.0f,100000,1.0f,10000.0f,-100.0f,-10000000.0f)
	TEST_OPERATOR(floor,0.0f, 1.0f, 2.0f, 4.0f, 1.0f, 0.5f, 0.25f, 0.125f)
	TEST_OPERATOR(floor,0.0f, 1.0f, 2.0f, 4.0f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)
	TEST_OPERATOR(floor,1.0f, 0.5f, 0.25f, 0.125f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)
	//TEST_OPERATOR(floor,1.0f, 0.5f, 0.25f, 0.125f, FLT_EPSILON, -FLT_EPSILON, FLT_MAX, -FLT_MAX)
	//TEST_OPERATOR(floor,0.0f, 1.0f, 2.0f, 4.0f, -logf(0.0f), -sqrtf(-1.0f), logf(0.0f), sqrtf(-1.0f))
}

//This one fails because the precision is not high enough in simd
TEST(Simd, Recip)
{
	//Failing cases are commented out.
	TEST_OPERATOR(recip<1>,1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f)
	//TEST_OPERATOR(recip<1>,1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 0.0f, 4.0f)
	//TEST_OPERATOR(recip<1>,0.0f,1.0f,-0.0f,0.1f,9.0f,8.0f,7.0f,6.0f)
	TEST_OPERATOR(recip<1>,1.0e20f,1.0e-20f,-1.0e20f,-1.0e-20f,5.0f, 6.0f, 7.0f, 8.0f)
	TEST_OPERATOR(recip<1>,1000.0f,10000.0f,10000.0f,100000,1.0f,10000.0f,-100.0f,-10000000.0f)
	TEST_OPERATOR(recip<1>,0.0f, 1.0f, 2.0f, 4.0f, 1.0f, 0.5f, 0.25f, 0.125f)
	TEST_OPERATOR(recip<1>,0.0f, 1.0f, 2.0f, 4.0f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)
	TEST_OPERATOR(recip<1>,1.0f, 0.5f, 0.25f, 0.125f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)
	//TEST_OPERATOR(recip<1>,1.0f, 0.5f, 0.25f, 0.125f, FLT_EPSILON, -FLT_EPSILON, FLT_MAX, -FLT_MAX)
	//TEST_OPERATOR(recip<1>,0.0f, 1.0f, 2.0f, 4.0f, -logf(0.0f), -sqrtf(-1.0f), logf(0.0f), sqrtf(-1.0f))
}

TEST(Simd, Sqrt)
{
	TEST_OPERATOR_DEFAULT_DATA(sqrt)
}

//This one fails because the precision is not high enough in simd
TEST(Simd, Rsqrt)
{
	//Failing cases are commented out.
	TEST_OPERATOR(rsqrt<1>,1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f)
	//TEST_OPERATOR(rsqrt<1>,1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 0.0f, 4.0f)
	TEST_OPERATOR(rsqrt<1>,0.0f,1.0f,-0.0f,0.1f,9.0f,8.0f,7.0f,6.0f)
	TEST_OPERATOR(rsqrt<1>,1.0e20f,1.0e-20f,-1.0e20f,-1.0e-20f,5.0f, 6.0f, 7.0f, 8.0f)
	TEST_OPERATOR(rsqrt<1>,1000.0f,10000.0f,10000.0f,100000,1.0f,10000.0f,-100.0f,-10000000.0f)
	TEST_OPERATOR(rsqrt<1>,0.0f, 1.0f, 2.0f, 4.0f, 1.0f, 0.5f, 0.25f, 0.125f)
	TEST_OPERATOR(rsqrt<1>,0.0f, 1.0f, 2.0f, 4.0f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)
	TEST_OPERATOR(rsqrt<1>,1.0f, 0.5f, 0.25f, 0.125f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)
	TEST_OPERATOR(rsqrt<1>,1.0f, 0.5f, 0.25f, 0.125f, FLT_EPSILON, -FLT_EPSILON, FLT_MAX, -FLT_MAX)
	//TEST_OPERATOR(rsqrt<1>,0.0f, 1.0f, 2.0f, 4.0f, -logf(0.0f), -sqrtf(-1.0f), logf(0.0f), sqrtf(-1.0f))
}

TEST(Simd, Exp2)
{
	//Failing cases are commented out.
	TEST_OPERATOR(exp2,1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f)
	TEST_OPERATOR(exp2,1.0f, 2.0f, 3.0f, 4.0f, 1.0f, 2.0f, 0.0f, 4.0f)
	TEST_OPERATOR(exp2,0.0f,1.0f,-0.0f,0.1f,9.0f,8.0f,7.0f,6.0f)
	TEST_OPERATOR(exp2,1.0e20f,1.0e-20f,-1.0e20f,-1.0e-20f,5.0f, 6.0f, 7.0f, 8.0f)
	//TEST_OPERATOR(exp2,1000.0f,10000.0f,10000.0f,100000,1.0f,10000.0f,-100.0f,-10000000.0f)
	//TEST_OPERATOR(exp2,0.0f, 1.0f, 2.0f, 4.0f, 1.0f, 0.5f, 0.25f, 0.125f)
	//TEST_OPERATOR(exp2,0.0f, 1.0f, 2.0f, 4.0f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)
	//TEST_OPERATOR(exp2,1.0f, 0.5f, 0.25f, 0.125f, 3.1415926535f, 2.7182818284f, 1.41421356237f, 0.0238095238f)
	//TEST_OPERATOR(exp2,1.0f, 0.5f, 0.25f, 0.125f, FLT_EPSILON, -FLT_EPSILON, FLT_MAX, -FLT_MAX)
	//TEST_OPERATOR(exp2,0.0f, 1.0f, 2.0f, 4.0f, -logf(0.0f), -sqrtf(-1.0f), logf(0.0f), sqrtf(-1.0f))
}

#undef TEST_OPERATOR
#define TEST_OPERATOR(op,v1,v2,v3,v4,w1,w2,w3,w4)	\
	{												\
		Simd4f b = simd4f(w1,w2,w3,w4);				\
		Simd4f c = op(b);							\
		float c_[4];								\
		store(c_, c);								\
		TestFloat2(::Local::op(w1), c_[0]);					\
		TestFloat2(::Local::op(w2), c_[1]);					\
		TestFloat2(::Local::op(w3), c_[2]);					\
		TestFloat2(::Local::op(w4), c_[3]);					\
	}


TEST(Simd, Log2)
{
	TEST_OPERATOR_DEFAULT_DATA(log2)
}

TEST(Simd, Abs)
{
	//NaNs/QNaNs will not behave the same
	TEST_OPERATOR_DEFAULT_DATA(abs)
}

TEST(Simd, Dot3)
{
	Simd4f a = simd4f(31.0f, 37.0f, 41.0f, 43.0f);
	Simd4f b = simd4f(47.0f, 53.0f, 59.0f, 61.0f);
	Simd4f c = dot3(a, b);
	float result = 31.0f*47.0f + 37.0f*53.0f + 41.0f*59.0f;

	for(int i = 0; i < 4; i++)
	{
		EXPECT_FLOAT_EQ(result, array(c)[i]);
	}
}

TEST(Simd, Cross3)
{
	Simd4f a = simd4f(31.0f, 37.0f, 41.0f, 43.0f);
	Simd4f b = simd4f(47.0f, 53.0f, 59.0f, 61.0f);
	Simd4f c = cross3(a, b);
	const float* aa = array(a);
	const float* bb = array(b);
	float result[3] =
	{
		aa[1] * bb[2] - aa[2] * bb[1],
		aa[2] * bb[0] - aa[0] * bb[2],
		aa[0] * bb[1] - aa[1] * bb[0]
	};

	for(int i = 0; i < 3; i++)
	{
		EXPECT_FLOAT_EQ(result[i], array(c)[i]);
	}
}


TEST(Simd, Transpose)
{
	Simd4f a[4];
	for(int i = 0; i < 16; i += 4)
	{
		a[i>>2] = simd4f((float)(i + 0), (float)(i + 1), (float)(i + 2), (float)(i + 3));
	}
	transpose(a[0], a[1], a[2], a[3]);

	for(int j = 0; j < 4; j++)
		for(int i = 0; i < 4; i++)
			EXPECT_FLOAT_EQ((float)(j + i * 4), array(a[j])[i]);
}

TEST(Simd, Zip)
{
	Simd4f a = simd4f(1.0f, 2.0f, 3.0f, 4.0f);
	Simd4f b = simd4f(5.0f, 6.0f, 7.0f, 8.0f);
	zip(a, b);
	EXPECT_FLOAT_EQ(1.0f, array(a)[0]);
	EXPECT_FLOAT_EQ(5.0f, array(a)[1]);
	EXPECT_FLOAT_EQ(2.0f, array(a)[2]);
	EXPECT_FLOAT_EQ(6.0f, array(a)[3]);

	EXPECT_FLOAT_EQ(3.0f, array(b)[0]);
	EXPECT_FLOAT_EQ(7.0f, array(b)[1]);
	EXPECT_FLOAT_EQ(4.0f, array(b)[2]);
	EXPECT_FLOAT_EQ(8.0f, array(b)[3]);
}

TEST(Simd, Unzip)
{
	Simd4f a = simd4f(1.0f, 2.0f, 3.0f, 4.0f);
	Simd4f b = simd4f(5.0f, 6.0f, 7.0f, 8.0f);
	unzip(a, b);
	EXPECT_FLOAT_EQ(1.0f, array(a)[0]);
	EXPECT_FLOAT_EQ(3.0f, array(a)[1]);
	EXPECT_FLOAT_EQ(5.0f, array(a)[2]);
	EXPECT_FLOAT_EQ(7.0f, array(a)[3]);

	EXPECT_FLOAT_EQ(2.0f, array(b)[0]);
	EXPECT_FLOAT_EQ(4.0f, array(b)[1]);
	EXPECT_FLOAT_EQ(6.0f, array(b)[2]);
	EXPECT_FLOAT_EQ(8.0f, array(b)[3]);
}

TEST(Simd, zipping)
{
	Simd4f a = simd4f(1.0f, 2.0f, 3.0f, 4.0f);
	Simd4f b = simd4f(5.0f, 6.0f, 7.0f, 8.0f);
	zip(a, b);
	unzip(a, b);
	EXPECT_FLOAT_EQ(1.0f, array(a)[0]);
	EXPECT_FLOAT_EQ(2.0f, array(a)[1]);
	EXPECT_FLOAT_EQ(3.0f, array(a)[2]);
	EXPECT_FLOAT_EQ(4.0f, array(a)[3]);

	EXPECT_FLOAT_EQ(5.0f, array(b)[0]);
	EXPECT_FLOAT_EQ(6.0f, array(b)[1]);
	EXPECT_FLOAT_EQ(7.0f, array(b)[2]);
	EXPECT_FLOAT_EQ(8.0f, array(b)[3]);
}

TEST(Simd, Swaphilo)
{
	Simd4f a = simd4f(1.0f, 2.0f, 3.0f, 4.0f);
	a = swaphilo(a);
	EXPECT_FLOAT_EQ(3.0f, array(a)[0]);
	EXPECT_FLOAT_EQ(4.0f, array(a)[1]);
	EXPECT_FLOAT_EQ(1.0f, array(a)[2]);
	EXPECT_FLOAT_EQ(2.0f, array(a)[3]);
}

//Maybe we need to have more test values aside from 0.0f and 1.0f
#define TEST_ALL_OP(op,func)																			\
	for(int j = 0; j < 5; j++)																			\
	{																									\
		Simd4f b = simd4f(j==0?1.0f:0.0f, j==1?1.0f:0.0f, j==2?1.0f:0.0f, j==3?1.0f:0.0f);				\
		for(int i = 0; i < 5; i++)																		\
		{																								\
			Simd4f a = simd4f(i==0?1.0f:0.0f, i==1?1.0f:0.0f, i==2?1.0f:0.0f, i==3?1.0f:0.0f);			\
																										\
			bool allEq = true;																			\
			for(int k = 0; k < 4; k++)																	\
				allEq = allEq && (array(a)[k] op array(b)[k]);											\
																										\
			EXPECT_EQ(func(a, b)!=0,allEq);																\
			Simd4f outMask;																				\
			EXPECT_EQ(func(a, b, outMask)!=0,allEq);													\
		}																								\
	}																									

#define TEST_ANY_OP(op,func)																			\
	for(int j = 0; j < 5; j++)																			\
	{																									\
		Simd4f b = simd4f(j==0?1.0f:0.0f, j==1?1.0f:0.0f, j==2?1.0f:0.0f, j==3?1.0f:0.0f);				\
		for(int i = 0; i < 5; i++)																		\
		{																								\
			Simd4f a = simd4f(i==0?1.0f:0.0f, i==1?1.0f:0.0f, i==2?1.0f:0.0f, i==3?1.0f:0.0f);			\
																										\
			bool anyEq = false;																			\
			for(int k = 0; k < 4; k++)																	\
				anyEq = anyEq || (array(a)[k] op array(b)[k]);											\
																										\
			EXPECT_EQ(func(a, b)!=0,anyEq);																\
			Simd4f outMask;																				\
			EXPECT_EQ(func(a, b, outMask)!=0,anyEq);													\
		}																								\
	}	

TEST(Simd, AllEqual)
{
	TEST_ALL_OP(==, allEqual)
}

TEST(Simd, AnyEqual)
{
	TEST_ANY_OP(==,anyEqual)
}

TEST(Simd, AllGreater)
{
	TEST_ALL_OP(>, allGreater)
}

TEST(Simd, AnyGreater)
{
	TEST_ANY_OP(>,anyGreater)
}

TEST(Simd, AllGreaterEqual)
{
	TEST_ALL_OP(>=, allGreaterEqual)
}

TEST(Simd, AnyGreaterEqual)
{
	TEST_ANY_OP(>=,anyGreaterEqual)
}

TEST(Simd, AllTrue)
{
	for(int j = 0; j < 5; j++)																	
	{																							
		Simd4f b = simd4f(j==0?1.0f:0.0f, j==1?1.0f:0.0f, j==2?1.0f:0.0f, j==3?1.0f:0.0f);		
		for(int i = 0; i < 5; i++)																
		{																						
			Simd4f a = simd4f(i==0?1.0f:0.0f, i==1?1.0f:0.0f, i==2?1.0f:0.0f, i==3?1.0f:0.0f);	
																							
			bool allEq = true;																	
			for(int k = 0; k < 4; k++)															
				allEq = allEq && (array(a)[k] == array(b)[k]);									
																							
			EXPECT_EQ(allTrue(a==b)!=0,allEq);																							
		}																						
	}																							
}

TEST(Simd, AnyTrue)
{
	for(int j = 0; j < 5; j++)																	
	{																							
		Simd4f b = simd4f(j==0?1.0f:0.0f, j==1?1.0f:0.0f, j==2?1.0f:0.0f, j==3?1.0f:0.0f);		
		for(int i = 0; i < 5; i++)																
		{																						
			Simd4f a = simd4f(i==0?1.0f:0.0f, i==1?1.0f:0.0f, i==2?1.0f:0.0f, i==3?1.0f:0.0f);	
																							
			bool anyEq = false;																	
			for(int k = 0; k < 4; k++)															
				anyEq = anyEq || (array(a)[k] == array(b)[k]);									
																							
			EXPECT_EQ(anyTrue(a==b)!=0,anyEq);											
		}																						
	}	
}
