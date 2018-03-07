// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Decorators/BTDecorator_ForceSuccess.h"
#include "BTBuilder.h"
#include "AITestsCommon.h"
#include "BehaviorTree/TestBTDecorator_CantExecute.h"

#define LOCTEXT_NAMESPACE "AITestSuite_BTTest"

struct FAITest_BTBasicSelector : public FAITest_SimpleBT
{
	FAITest_BTBasicSelector()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Failed);

			FBTBuilder::AddTask(CompNode, 1, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecorator<UTestBTDecorator_CantExecute>(CompNode);
			}

			FBTBuilder::AddTask(CompNode, 2, EBTNodeResult::Succeeded, 2);

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Failed);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(2);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTBasicSelector, "System.Engine.AI.Behavior Trees.Composite node: selector")

struct FAITest_BTBasicSequence : public FAITest_SimpleBT
{
	FAITest_BTBasicSequence()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);

			FBTBuilder::AddTask(CompNode, 1, EBTNodeResult::Failed);
			{
				FBTBuilder::WithDecorator<UTestBTDecorator_CantExecute>(CompNode);
				FBTBuilder::WithDecorator<UBTDecorator_ForceSuccess>(CompNode);
			}

			FBTBuilder::AddTask(CompNode, 2, EBTNodeResult::Failed, 2);

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(2);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTBasicSequence, "System.Engine.AI.Behavior Trees.Composite node: sequence")

struct FAITest_BTBasicParallelWait : public FAITest_SimpleBT
{
	FAITest_BTBasicParallelWait()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			UBTCompositeNode& CompNode2 = FBTBuilder::AddParallel(CompNode, EBTParallelMode::WaitForBackground);
			{
				FBTBuilder::AddTaskLogFinish(CompNode2, 0, 10, EBTNodeResult::Succeeded, 6);

				UBTCompositeNode& CompNode3 = FBTBuilder::AddSequence(CompNode2);
				{
					FBTBuilder::AddTaskLogFinish(CompNode3, 1, 11, EBTNodeResult::Succeeded, 3);
					FBTBuilder::AddTaskLogFinish(CompNode3, 2, 12, EBTNodeResult::Succeeded, 3);
					FBTBuilder::AddTaskLogFinish(CompNode3, 3, 13, EBTNodeResult::Succeeded, 3);
				}
			}

			FBTBuilder::AddTask(CompNode, 4, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(0);
			ExpectedResult.Add(1);
			ExpectedResult.Add(11);
			ExpectedResult.Add(2);
		ExpectedResult.Add(10);
			ExpectedResult.Add(12);
			ExpectedResult.Add(3);
			ExpectedResult.Add(13);
		ExpectedResult.Add(4);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTBasicParallelWait, "System.Engine.AI.Behavior Trees.Composite node: parallel (wait)")

struct FAITest_BTBasicParallelAbort : public FAITest_SimpleBT
{
	FAITest_BTBasicParallelAbort()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			UBTCompositeNode& CompNode2 = FBTBuilder::AddParallel(CompNode, EBTParallelMode::AbortBackground);
			{
				FBTBuilder::AddTaskLogFinish(CompNode2, 0, 10, EBTNodeResult::Succeeded, 6);

				UBTCompositeNode& CompNode3 = FBTBuilder::AddSequence(CompNode2);
				{
					FBTBuilder::AddTaskLogFinish(CompNode3, 1, 11, EBTNodeResult::Succeeded, 4);
					FBTBuilder::AddTaskLogFinish(CompNode3, 2, 12, EBTNodeResult::Succeeded, 4);
					FBTBuilder::AddTaskLogFinish(CompNode3, 3, 13, EBTNodeResult::Succeeded, 4);
				}
			}

			FBTBuilder::AddTask(CompNode, 4, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(0);
			ExpectedResult.Add(1);
			ExpectedResult.Add(11);
			ExpectedResult.Add(2);
		ExpectedResult.Add(10);
		ExpectedResult.Add(4);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTBasicParallelAbort, "System.Engine.AI.Behavior Trees.Composite node: parallel (abort)")

struct FAITest_BTCompositeDecorator : public FAITest_SimpleBT
{
	FAITest_BTCompositeDecorator()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);

			FBTBuilder::AddTask(CompNode, 1, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::NotSet, EBTFlowAbortMode::None, TEXT("Bool1"));
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::NotSet, EBTFlowAbortMode::None, TEXT("Bool2"));
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::None, TEXT("Bool3"));
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::NotSet, EBTFlowAbortMode::None, TEXT("Bool4"));

				TArray<FBTDecoratorLogic>& CompositeOps = CompNode.Children.Last().DecoratorOps;
				CompositeOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Or, 3));
					CompositeOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test, 0));
					CompositeOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Not, 1));
						CompositeOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::And, 2));
							CompositeOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test, 1));
							CompositeOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test, 2));
					CompositeOps.Add(FBTDecoratorLogic(EBTDecoratorLogic::Test, 3));
			}

			FBTBuilder::AddTask(CompNode, 2, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(1);
		ExpectedResult.Add(2);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTCompositeDecorator, "System.Engine.AI.Behavior Trees.Composite decorator")

struct FAITest_BTAbortSelfFail : public FAITest_SimpleBT
{
	FAITest_BTAbortSelfFail()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self);

				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskFlagChange(CompNode2, true, EBTNodeResult::Succeeded);
				FBTBuilder::AddTask(CompNode2, 2, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(1);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortSelfFail, "System.Engine.AI.Behavior Trees.Abort: self failure")

struct FAITest_BTAbortSelfSuccess : public FAITest_SimpleBT
{
	FAITest_BTAbortSelfSuccess()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self);
				FBTBuilder::WithDecorator<UBTDecorator_ForceSuccess>(CompNode);

				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskFlagChange(CompNode2, true, EBTNodeResult::Succeeded);
				FBTBuilder::AddTask(CompNode2, 2, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(1);
		ExpectedResult.Add(3);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortSelfSuccess, "System.Engine.AI.Behavior Trees.Abort: self success")

struct FAITest_BTAbortLowerPri : public FAITest_SimpleBT
{
	FAITest_BTAbortLowerPri()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
			}

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskFlagChange(CompNode2, true, EBTNodeResult::Succeeded);
				FBTBuilder::AddTask(CompNode2, 2, EBTNodeResult::Failed);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(0);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortLowerPri, "System.Engine.AI.Behavior Trees.Abort: lower priority")

struct FAITest_BTAbortMerge1 : public FAITest_SimpleBT
{
	FAITest_BTAbortMerge1()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
				FBTBuilder::WithDecorator<UTestBTDecorator_CantExecute>(CompNode);
			}

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self);

				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskFlagChange(CompNode2, true, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 2, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(2);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortMerge1, "System.Engine.AI.Behavior Trees.Abort: merge ranges 1")

struct FAITest_BTAbortMerge2 : public FAITest_SimpleBT
{
	FAITest_BTAbortMerge2()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Failed);

			FBTBuilder::AddTask(CompNode, 1, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
				FBTBuilder::WithDecorator<UTestBTDecorator_CantExecute>(CompNode);
			}

			FBTBuilder::AddTask(CompNode, 2, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Failed);
			FBTBuilder::AddTaskFlagChange(CompNode, true, EBTNodeResult::Failed);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(3);
		ExpectedResult.Add(2);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortMerge2, "System.Engine.AI.Behavior Trees.Abort: merge ranges 2")

struct FAITest_BTAbortMerge3 : public FAITest_SimpleBT
{
	FAITest_BTAbortMerge3()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			UBTCompositeNode& CompNode2 = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::AddTask(CompNode2, 0, EBTNodeResult::Succeeded);
				{
					FBTBuilder::WithDecorator<UTestBTDecorator_CantExecute>(CompNode2);
				}

				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Succeeded);
				{
					FBTBuilder::WithDecoratorBlackboard(CompNode2, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
					FBTBuilder::WithDecorator<UTestBTDecorator_CantExecute>(CompNode2);
				}
			}

			UBTCompositeNode& CompNode3 = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::AddTask(CompNode3, 2, EBTNodeResult::Succeeded);
				{
					FBTBuilder::WithDecoratorBlackboard(CompNode3, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
				}

				FBTBuilder::AddTaskFlagChange(CompNode3, true, EBTNodeResult::Failed);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Failed);
		}

		ExpectedResult.Add(2);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortMerge3, "System.Engine.AI.Behavior Trees.Abort: merge ranges 3")

struct FAITest_BTAbortParallelInternal : public FAITest_SimpleBT
{
	FAITest_BTAbortParallelInternal()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			UBTCompositeNode& CompNode2 = FBTBuilder::AddParallel(CompNode, EBTParallelMode::WaitForBackground);
			{
				FBTBuilder::AddTask(CompNode2, 0, EBTNodeResult::Succeeded, 5);

				UBTCompositeNode& CompNode3 = FBTBuilder::AddSequence(CompNode2);
				{
					FBTBuilder::AddTask(CompNode3, 1, EBTNodeResult::Succeeded, 1);

					UBTCompositeNode& CompNode4 = FBTBuilder::AddSelector(CompNode3);
					{
						FBTBuilder::AddTask(CompNode4, 2, EBTNodeResult::Succeeded, 3);
						{
							FBTBuilder::WithDecoratorBlackboard(CompNode4, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
						}

						FBTBuilder::AddTask(CompNode4, 3, EBTNodeResult::Succeeded, 1);
					}

					FBTBuilder::AddTaskFlagChange(CompNode3, true, EBTNodeResult::Succeeded);
				}
			}

			FBTBuilder::AddTask(CompNode, 4, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(1);
		ExpectedResult.Add(3);
		ExpectedResult.Add(2);
		ExpectedResult.Add(4);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortParallelInternal, "System.Engine.AI.Behavior Trees.Abort: parallel internal")

struct FAITest_BTAbortParallelOut : public FAITest_SimpleBT
{
	FAITest_BTAbortParallelOut()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
			}

			UBTCompositeNode& CompNode2 = FBTBuilder::AddParallel(CompNode, EBTParallelMode::WaitForBackground);
			{
				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Failed, 5);

				UBTCompositeNode& CompNode3 = FBTBuilder::AddSequence(CompNode2);
				{
					FBTBuilder::AddTask(CompNode3, 2, EBTNodeResult::Succeeded, 1);
					FBTBuilder::AddTaskFlagChange(CompNode3, true, EBTNodeResult::Succeeded);
					FBTBuilder::AddTask(CompNode3, 3, EBTNodeResult::Succeeded, 1);
				}
			}

			FBTBuilder::AddTask(CompNode, 4, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(2);
		ExpectedResult.Add(0);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortParallelOut, "System.Engine.AI.Behavior Trees.Abort: parallel out")

struct FAITest_BTAbortParallelOutAndBack : public FAITest_SimpleBT
{
	FAITest_BTAbortParallelOutAndBack()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
				FBTBuilder::WithDecorator<UTestBTDecorator_CantExecute>(CompNode);
			}

			UBTCompositeNode& CompNode2 = FBTBuilder::AddParallel(CompNode, EBTParallelMode::WaitForBackground);
			{
				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Failed, 5);

				UBTCompositeNode& CompNode3 = FBTBuilder::AddSequence(CompNode2);
				{
					FBTBuilder::AddTask(CompNode3, 2, EBTNodeResult::Succeeded, 2);
					FBTBuilder::AddTaskFlagChange(CompNode3, true, EBTNodeResult::Succeeded);
					FBTBuilder::AddTask(CompNode3, 3, EBTNodeResult::Succeeded, 3);
				}
			}

			FBTBuilder::AddTask(CompNode, 4, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(2);
		ExpectedResult.Add(3);
		ExpectedResult.Add(4);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortParallelOutAndBack, "System.Engine.AI.Behavior Trees.Abort: parallel out & back")

struct FAITest_BTAbortMultipleDelayed : public FAITest_SimpleBT
{
	FAITest_BTAbortMultipleDelayed()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Failed);

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::WithDecoratorDelayedAbort(CompNode, 2, false);

				FBTBuilder::AddTaskLogFinish(CompNode2, 1, 11, EBTNodeResult::Succeeded, 4);
				FBTBuilder::AddTaskLogFinish(CompNode2, 2, 12, EBTNodeResult::Succeeded, 4);
			}
			
			FBTBuilder::AddTask(CompNode, 4, EBTNodeResult::Failed);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(1);
		ExpectedResult.Add(4);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortMultipleDelayed, "System.Engine.AI.Behavior Trees.Abort: multiple delayed requests")

struct FAITest_BTAbortToInactiveParallel : public FAITest_SimpleBT
{
	FAITest_BTAbortToInactiveParallel()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			UBTCompositeNode& CompNode2 = FBTBuilder::AddParallel(CompNode, EBTParallelMode::WaitForBackground);
			{
				FBTBuilder::WithDecoratorDelayedAbort(CompNode, 5);

				FBTBuilder::AddTaskLogFinish(CompNode2, 1, 11, EBTNodeResult::Succeeded, 10);

				UBTCompositeNode& CompNode3 = FBTBuilder::AddSelector(CompNode2);
				{
					FBTBuilder::AddTask(CompNode3, 2, EBTNodeResult::Succeeded);
					{
						FBTBuilder::WithDecoratorBlackboard(CompNode3, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);
					}

					FBTBuilder::AddTaskLogFinish(CompNode3, 3, 13, EBTNodeResult::Succeeded, 8);
				}
			}

			UBTCompositeNode& CompNode4 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::AddTask(CompNode4, 4, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskFlagChange(CompNode4, true, EBTNodeResult::Succeeded);
				FBTBuilder::AddTask(CompNode4, 5, EBTNodeResult::Succeeded);
			}
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(3);
		ExpectedResult.Add(4);
		ExpectedResult.Add(5);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortToInactiveParallel, "System.Engine.AI.Behavior Trees.Abort: observer in inactive parallel")

struct FAITest_BTAbortDuringLatentAbort : public FAITest_SimpleBT
{
	FAITest_BTAbortDuringLatentAbort()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			UBTCompositeNode& Comp1Node = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::AddTask(Comp1Node, 0, EBTNodeResult::Succeeded);
				{
					FBTBuilder::WithDecoratorBlackboard(Comp1Node, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority, TEXT("Bool2"));
				}

				FBTBuilder::AddTaskLatentFlags(Comp1Node, EBTNodeResult::Succeeded,
					1, TEXT("Bool1"), 1, 2,
					1, TEXT("Bool2"), 3, 4);
				{
					FBTBuilder::WithDecoratorBlackboard(Comp1Node, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self, TEXT("Bool1"));
				}
			}

			FBTBuilder::AddTask(CompNode, 6, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(3);
		ExpectedResult.Add(4);
		ExpectedResult.Add(0);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortDuringLatentAbort, "System.Engine.AI.Behavior Trees.Abort: during latent task abort (lower pri)")

struct FAITest_BTAbortDuringLatentAbort2 : public FAITest_SimpleBT
{
	FAITest_BTAbortDuringLatentAbort2()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			UBTCompositeNode& Comp1Node = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::AddTaskLatentFlags(Comp1Node, EBTNodeResult::Succeeded,
					1, TEXT("Bool1"), 1, 2,
					1, TEXT("Bool2"), 3, 4);
				{
					FBTBuilder::WithDecoratorBlackboard(Comp1Node, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self, TEXT("Bool1"));
					FBTBuilder::WithDecoratorBlackboard(Comp1Node, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self, TEXT("Bool2"));
				}

				FBTBuilder::AddTask(Comp1Node, 5, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 6, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(3);
		ExpectedResult.Add(4);
		ExpectedResult.Add(5);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortDuringLatentAbort2, "System.Engine.AI.Behavior Trees.Abort: during latent task abort (self)")

struct FAITest_BTAbortDuringInstantAbort : public FAITest_SimpleBT
{
	FAITest_BTAbortDuringInstantAbort()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			UBTCompositeNode& Comp1Node = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority, TEXT("Bool1"));

				FBTBuilder::AddTask(Comp1Node, 1, EBTNodeResult::Succeeded);
				{
					FBTBuilder::WithDecoratorBlackboard(Comp1Node, EBasicKeyOperation::Set, EBTFlowAbortMode::None, TEXT("Bool3"));
				}
			}

			UBTCompositeNode& Comp2Node = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self, TEXT("Bool1"));

				FBTBuilder::AddTaskLatentFlags(Comp2Node, EBTNodeResult::Succeeded,
					1, TEXT("Bool2"), 2, 3,
					0, TEXT("Bool1"), 4, 5);
				{
					FBTBuilder::WithDecoratorBlackboard(Comp2Node, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self, TEXT("Bool2"));
				}

				FBTBuilder::AddTask(Comp2Node, 6, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 7, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(2);
		ExpectedResult.Add(4);
		ExpectedResult.Add(5);
		ExpectedResult.Add(7);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortDuringInstantAbort, "System.Engine.AI.Behavior Trees.Abort: during instant task abort (lower pri)")

struct FAITest_BTAbortDuringInstantAbort2 : public FAITest_SimpleBT
{
	FAITest_BTAbortDuringInstantAbort2()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			UBTCompositeNode& Comp1Node = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::AddTaskLatentFlags(Comp1Node, EBTNodeResult::Succeeded,
					1, TEXT("Bool1"), 0, 1, 
					0, TEXT("Bool2"), 2, 3);
				{
					FBTBuilder::WithDecoratorBlackboard(Comp1Node, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Self, TEXT("Bool1"));
					FBTBuilder::WithDecoratorBlackboard(Comp1Node, EBasicKeyOperation::NotSet, EBTFlowAbortMode::Both, TEXT("Bool2"));
				}

				FBTBuilder::AddTask(Comp1Node, 4, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 5, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(2);
		ExpectedResult.Add(3);
		ExpectedResult.Add(4);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortDuringInstantAbort2, "System.Engine.AI.Behavior Trees.Abort: during instant task abort (self)")

struct FAITest_BTAbortOnValueChangePass : public FAITest_SimpleBT
{
	FAITest_BTAbortOnValueChangePass()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Failed);

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EArithmeticKeyOperation::NotEqual, 10, EBTFlowAbortMode::Self, EBTBlackboardRestart::ValueChange, TEXT("Int"));

				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskValueChange(CompNode2, 1, EBTNodeResult::Succeeded, TEXT("Int"));
				FBTBuilder::AddTask(CompNode2, 2, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Failed);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(1);
		ExpectedResult.Add(1);
		ExpectedResult.Add(2);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortOnValueChangePass, "System.Engine.AI.Behavior Trees.Abort: value change (pass)")

struct FAITest_BTAbortOnValueChangeFail : public FAITest_SimpleBT
{
	FAITest_BTAbortOnValueChangeFail()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Failed);

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EArithmeticKeyOperation::NotEqual, 10, EBTFlowAbortMode::Self, EBTBlackboardRestart::ValueChange, TEXT("Int"));

				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskValueChange(CompNode2, 10, EBTNodeResult::Succeeded, TEXT("Int"));
				FBTBuilder::AddTask(CompNode2, 2, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Failed);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(1);
		ExpectedResult.Add(3);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortOnValueChangeFail, "System.Engine.AI.Behavior Trees.Abort: value change (fail)")

struct FAITest_BTAbortOnValueChangeFailOther : public FAITest_SimpleBT
{
	FAITest_BTAbortOnValueChangeFailOther()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Failed);

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EArithmeticKeyOperation::NotEqual, 10, EBTFlowAbortMode::Self, EBTBlackboardRestart::ValueChange, TEXT("Int"));
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::NotSet, EBTFlowAbortMode::None, TEXT("Bool1"));

				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskFlagChange(CompNode2, true, EBTNodeResult::Succeeded, TEXT("Bool1"));
				FBTBuilder::AddTaskValueChange(CompNode2, 1, EBTNodeResult::Succeeded, TEXT("Int"));
				FBTBuilder::AddTask(CompNode2, 2, EBTNodeResult::Succeeded);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Failed);
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(1);
		ExpectedResult.Add(3);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTAbortOnValueChangeFailOther, "System.Engine.AI.Behavior Trees.Abort: value change (other failed)")

struct FAITest_BTLowPriObserverInLoop : public FAITest_SimpleBT
{
	FAITest_BTLowPriObserverInLoop()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 1, EBTNodeResult::Failed);

			UBTCompositeNode& CompNode2 = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::WithDecoratorLoop(CompNode);

				FBTBuilder::AddTaskLatentFlags(CompNode2, EBTNodeResult::Succeeded, 1, TEXT("Bool2"), 2, 3);
				{
					FBTBuilder::WithDecoratorBlackboard(CompNode2, EBasicKeyOperation::Set, EBTFlowAbortMode::None, TEXT("Bool1"));
				}

				UBTCompositeNode& CompNode4 = FBTBuilder::AddSequence(CompNode2);
				{
					FBTBuilder::AddTask(CompNode4, 4, EBTNodeResult::Succeeded);
					FBTBuilder::AddTaskFlagChange(CompNode4, true, EBTNodeResult::Failed, TEXT("Bool1"));
				}

				FBTBuilder::AddTask(CompNode2, 5, EBTNodeResult::Succeeded);
				{
					FBTBuilder::WithDecoratorBlackboard(CompNode2, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority, TEXT("Bool2"));
				}
			}

			FBTBuilder::AddTask(CompNode, 6, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(4);
		ExpectedResult.Add(2);
		ExpectedResult.Add(3);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTLowPriObserverInLoop, "System.Engine.AI.Behavior Trees.Other: low priority observer in looped branch")

struct FAITest_BTSubtreeSimple : public FAITest_SimpleBT
{
	FAITest_BTSubtreeSimple()
	{
		UBehaviorTree* ChildAsset1 = &FBTBuilder::CreateBehaviorTree(*BTAsset);
		{
			UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*ChildAsset1);
			{
				FBTBuilder::AddTask(CompNode, 10, EBTNodeResult::Succeeded);
				FBTBuilder::AddTask(CompNode, 11, EBTNodeResult::Succeeded);
			}
		}

		UBehaviorTree* ChildAsset2 = &FBTBuilder::CreateBehaviorTree(*BTAsset);
		{
			UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*ChildAsset2);
			{
				FBTBuilder::AddTask(CompNode, 20, EBTNodeResult::Failed);
				FBTBuilder::AddTask(CompNode, 21, EBTNodeResult::Succeeded);
			}
		}

		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			UBTCompositeNode& CompNode2 = FBTBuilder::AddSelector(CompNode);
			{
				FBTBuilder::AddTask(CompNode2, 0, EBTNodeResult::Failed);
				FBTBuilder::AddTaskSubtree(CompNode2, ChildAsset2);
				FBTBuilder::AddTask(CompNode2, 1, EBTNodeResult::Failed);
			}

			UBTCompositeNode& CompNode3 = FBTBuilder::AddSequence(CompNode);
			{
				FBTBuilder::AddTask(CompNode3, 2, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskSubtree(CompNode3, ChildAsset1);
				FBTBuilder::AddTask(CompNode3, 3, EBTNodeResult::Succeeded);
			}
		}

		ExpectedResult.Add(0);
		ExpectedResult.Add(20);
		ExpectedResult.Add(1);
		ExpectedResult.Add(2);
		ExpectedResult.Add(10);
		ExpectedResult.Add(11);
		ExpectedResult.Add(3);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTSubtreeSimple, "System.Engine.AI.Behavior Trees.Subtree: simple")

struct FAITest_BTSubtreeAbortOut : public FAITest_SimpleBT
{
	FAITest_BTSubtreeAbortOut()
	{
		UBehaviorTree* ChildAsset = &FBTBuilder::CreateBehaviorTree(*BTAsset);
		{
			UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*ChildAsset);
			{
				FBTBuilder::AddTask(CompNode, 10, EBTNodeResult::Succeeded);
				FBTBuilder::AddTaskFlagChange(CompNode, true, EBTNodeResult::Succeeded);
				FBTBuilder::AddTask(CompNode, 11, EBTNodeResult::Succeeded);
			}
		}

		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded); 
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority);			
			}

			FBTBuilder::AddTaskSubtree(CompNode, ChildAsset);
		}

		ExpectedResult.Add(10);
		ExpectedResult.Add(0);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTSubtreeAbortOut, "System.Engine.AI.Behavior Trees.Subtree: abort out")

struct FAITest_BTServiceInstantTask : public FAITest_SimpleBT
{
	FAITest_BTServiceInstantTask()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithTaskServiceLog(CompNode, 1, 2);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(0);
		ExpectedResult.Add(2);
		ExpectedResult.Add(3);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTServiceInstantTask, "System.Engine.AI.Behavior Trees.Service: instant task")

struct FAITest_BTServiceLatentTask : public FAITest_SimpleBT
{
	FAITest_BTServiceLatentTask()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSequence(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded, 2);
			{
				FBTBuilder::WithTaskServiceLog(CompNode, 1, 2);
			}

			FBTBuilder::AddTask(CompNode, 3, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(1);
		ExpectedResult.Add(0);
		ExpectedResult.Add(2);
		ExpectedResult.Add(3);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTServiceLatentTask, "System.Engine.AI.Behavior Trees.Service: latent task")

struct FAITest_BTServiceAbortingTask : public FAITest_SimpleBT
{
	FAITest_BTServiceAbortingTask()
	{
		UBTCompositeNode& CompNode = FBTBuilder::AddSelector(*BTAsset);
		{
			FBTBuilder::AddTask(CompNode, 0, EBTNodeResult::Succeeded);
			{
				FBTBuilder::WithDecoratorBlackboard(CompNode, EBasicKeyOperation::Set, EBTFlowAbortMode::LowerPriority, TEXT("Bool1"));
			}

			FBTBuilder::AddTaskLatentFlags(CompNode, EBTNodeResult::Succeeded, 1, TEXT("Bool1"), 1, 2, 0, NAME_None, 3, 4);
			{
				FBTBuilder::WithTaskServiceLog(CompNode, 5, 6);
			}

			FBTBuilder::AddTask(CompNode, 7, EBTNodeResult::Succeeded);
		}

		ExpectedResult.Add(5);
		ExpectedResult.Add(1);
		ExpectedResult.Add(6);
		ExpectedResult.Add(3);
		ExpectedResult.Add(4);
		ExpectedResult.Add(0);
	}
};
IMPLEMENT_AI_LATENT_TEST(FAITest_BTServiceAbortingTask, "System.Engine.AI.Behavior Trees.Service: abort task")

#undef LOCTEXT_NAMESPACE
