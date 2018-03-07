//initializing the NvCloth dll
#include <NvCloth/Callbacks.h>

//example allocator callbacks
#include "utilities/CallbackImplementations.h"

//low level cloth
#include <NvCloth/Factory.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Solver.h>

//mesh generation
#include "utilities/ClothMeshGenerator.h"

//fabric cooking
#include <NvClothExt/ClothFabricCooker.h>

//Google test stuff
#include <gtest/gtest.h>
#include "utilities/Utilities.h"

TEST_LEAK_FIXTURE(ExampleCode)
TEST_F(ExampleCode,ExampleCodeTest)
{
	////Initialization only once per application
	//Initialize the NvCloth dll callbacks
	//nv::cloth::InitializeNvCloth(mAllocator, mErrorCallback, mAssertHandler, nullptr); //Called in NvClothEnvironment

	//Initialization once per platform
	//Create a factory (if you want DX or CUDA simulation, use the respective createFactory* functions)
	nv::cloth::Factory* factory = NvClothCreateFactoryCPU();
	ASSERT_PTR(factory);

	//Initialization once per simulation 'scene'
	//Create a solver (this solver uses CPU/DX/CUDA depending on the factory)
	nv::cloth::Solver* solver = factory->createSolver();
	ASSERT_PTR(solver);

	//Helper functions to generate the mesh. Replace this with your actual cloth data. See implementations for more info.
	ClothMeshData clothMesh;
	clothMesh.GeneratePlaneCloth(10.0f, 10.0f, 2, 2); //Generate a 9 vertex plane cloth with quad and triangle data
	nv::cloth::ClothMeshDesc meshDesc = clothMesh.GetClothMeshDesc(); //Convert the mesh data to a format NvCloth can process
	ASSERT_TRUE(meshDesc.isValid());

	//Cook mesh data to fabric using the extension. This could be saved to disk to save runtime
	// (note: the mesh description contains particle/vertex positions for determining the rest lengths)
	nv::cloth::Vector<int32_t>::Type phaseTypeInfo; //solver phase type info, used later for config setup.
	nv::cloth::Fabric* fabric = NvClothCookFabricFromMesh(factory, meshDesc, physx::PxVec3(0.0f, -9.8f, 0.0f), &phaseTypeInfo, false);
	ASSERT_PTR(fabric);

	//Initialize start positions and masses for the actual cloth instance
	// (note: the particle/vertex positions do not have to match the mesh description here. Set the positions to the initial shape of this cloth instance)
	std::vector<physx::PxVec4> particlesCopy;
	particlesCopy.resize(clothMesh.mVertices.size());
	for(int i = 0; i < (int)clothMesh.mVertices.size(); i++)
		particlesCopy[i] = physx::PxVec4(clothMesh.mVertices[i], 1.0f); //w component is 1/mass, or 0.0f for anchored/fixed particles

	//Create the cloth from the initial positions/masses and the fabric
	nv::cloth::Cloth* cloth = factory->createCloth(nv::cloth::Range<physx::PxVec4>(&particlesCopy.front(), &particlesCopy.back()+1), *fabric);
	particlesCopy.clear(); particlesCopy.shrink_to_fit(); //We don't need this any more

	//Set all your cloth properties
	cloth->setGravity(physx::PxVec3(0.0f, -9.8f, 0.0f));

	//Setup phase configs
	std::vector<nv::cloth::PhaseConfig> phases(fabric->getNumPhases());
	for(int i = 0; i < (int)phases.size(); i++)
	{
		phases[i].mPhaseIndex = i; // Set index to the corresponding set

		//Give phases different configs depending on type
		switch(phaseTypeInfo[i])
		{
			case nv::cloth::ClothFabricPhaseType::eINVALID:
				ASSERT_TRUE(false) << "Cloth has invalid phase";
				break;
			case nv::cloth::ClothFabricPhaseType::eVERTICAL:
				break;
			case nv::cloth::ClothFabricPhaseType::eHORIZONTAL:
				break;
			case nv::cloth::ClothFabricPhaseType::eBENDING:
				break;
			case nv::cloth::ClothFabricPhaseType::eSHEARING:
				break;
		}

		//For this example we give very phase the same config
		phases[i].mStiffness = 1.0f;
		phases[i].mStiffnessMultiplier = 1.0f;
		phases[i].mCompressionLimit = 1.0f;
		phases[i].mStretchLimit = 1.0f;
	}
	cloth->setPhaseConfig(CreateRange(phases));

	//Add the cloth to the solver for simulation
	solver->addCloth(cloth);
	
	//Simulation loop
	for(int i = 0; i < 100; i++)
	{
		solver->beginSimulation(1.0f / 60.0f);
		for(int i = 0; i < solver->getSimulationChunkCount(); i++)
			solver->simulateChunk(i);
		solver->endSimulation();
	}

	//Remove the cloth from the simulation
	solver->removeCloth(cloth);

	//Delete all the created objects
	NV_CLOTH_DELETE(cloth);
	fabric->decRefCount();
	NV_CLOTH_DELETE(solver);
	NvClothDestroyFactory(factory);

	//NvCloth doesn't need any deinitialization
}
