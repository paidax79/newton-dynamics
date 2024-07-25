/* Copyright (c) <2003-2022> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _ND_AGENT_CONTINUE_POLICY_GRADIENT_TRAINER_H__
#define _ND_AGENT_CONTINUE_POLICY_GRADIENT_TRAINER_H__

#include "ndBrainStdafx.h"
#include "ndBrain.h"
#include "ndBrainAgent.h"
#include "ndBrainTrainer.h"
#include "ndBrainThreadPool.h"
#include "ndBrainLayerLinear.h"
#include "ndBrainOptimizerAdam.h"
#include "ndBrainLayerActivationRelu.h"
#include "ndBrainLayerActivationTanh.h"
#include "ndBrainLossLeastSquaredError.h"
#include "ndBrainLayerActivationSigmoidLinear.h"

// this is an implementation of the vanilla policy Gradient as described in:
// https://spinningup.openai.com/en/latest/algorithms/vpg.html

#define ND_CONTINUE_POLICY_GRADIENT_USE_PAST_REWARDS
#define ND_CONTINUE_POLICY_GRADIENT_MIN_VARIANCE	ndBrainFloat(0.1f)

#define ND_CONTINUE_POLICY_GRADIENT_BUFFER_SIZE		(1024 * 256)

class ndBrainContinuePolicyGradientLastActivationLayer : public ndBrainLayerActivationTanh 
{
	public:
	ndBrainContinuePolicyGradientLastActivationLayer(ndInt32 neurons)
		:ndBrainLayerActivationTanh(neurons * 2)
		,m_sigma(ND_CONTINUE_POLICY_GRADIENT_MIN_VARIANCE)
	{
	}

	ndBrainContinuePolicyGradientLastActivationLayer(const ndBrainContinuePolicyGradientLastActivationLayer& src)
		:ndBrainLayerActivationTanh(src)
		,m_sigma(src.m_sigma)
	{
	}

	ndBrainLayer* Clone() const
	{
		return new ndBrainContinuePolicyGradientLastActivationLayer(*this);
	}

	void MakePrediction(const ndBrainVector& input, ndBrainVector& output) const
	{
		ndBrainLayerActivationTanh::MakePrediction(input, output);
		for (ndInt32 i = m_neurons / 2 - 1; i >= 0; --i)
		{
			output[i + m_neurons/2] = ndMax(input[i + m_neurons/2], m_sigma);
		}
	}

	void InputDerivative(const ndBrainVector& input, const ndBrainVector& output, const ndBrainVector& outputDerivative, ndBrainVector& inputDerivative) const
	{
		ndBrainLayerActivationTanh::InputDerivative(input, output, outputDerivative, inputDerivative);
		for (ndInt32 i = m_neurons / 2 - 1; i >= 0 ; --i)
		{
			inputDerivative[i + m_neurons / 2] = (input[i + m_neurons / 2] > ndBrainFloat(0.0f)) ? ndBrainFloat(1.0f) : ndBrainFloat(0.0f);
			inputDerivative[i + m_neurons / 2] *= outputDerivative[i + m_neurons / 2];
		}
	}

	ndBrainFloat m_sigma;
};

template<ndInt32 statesDim, ndInt32 actionDim> class ndBrainAgentContinuePolicyGradient_TrainerMaster;

class ndTrajectoryStep: protected ndBrainVector
{
	public:
	ndTrajectoryStep(ndInt32 actionsSize, ndInt32 obsevationsSize)
		:ndBrainVector()
		,m_actionsSize(actionsSize)
		,m_obsevationsSize(obsevationsSize)
	{
	}

	ndInt32 GetStepNumber() const 
	{
		ndInt32 stride = 2 + m_actionsSize * 2 + m_obsevationsSize;
		return ndInt32(GetCount()) / stride;
	}

	void SetStepNumber(ndInt32 count)
	{
		ndInt32 stride = 2 + m_actionsSize * 2 + m_obsevationsSize;
		SetCount(stride * count);
	}

	ndBrainFloat GetReward(ndInt32 entry) const
	{
		const ndTrajectoryStep& me = *this;
		ndInt32 stride = 2 + m_actionsSize * 2 + m_obsevationsSize;
		return me[stride * entry];
	}

	void SetReward(ndInt32 entry, ndBrainFloat reward)
	{
		ndTrajectoryStep& me = *this;
		ndInt32 stride = 2 + m_actionsSize * 2 + m_obsevationsSize;
		me[stride * entry] = reward;
	}

	ndBrainFloat GetAdvantage(ndInt32 entry) const
	{
		const ndTrajectoryStep& me = *this;
		ndInt32 stride = 2 + m_actionsSize * 2 + m_obsevationsSize;
		return me[stride * entry + 1];
	}

	void SetAdvantage(ndInt32 entry, ndBrainFloat advantage)
	{
		ndTrajectoryStep& me = *this;
		ndInt32 stride = 2 + m_actionsSize * 2 + m_obsevationsSize;
		me[stride * entry + 1] = advantage;
	}

	ndBrainFloat* GetActions(ndInt32 entry)
	{
		ndTrajectoryStep& me = *this;
		ndInt32 stride = 2 + m_actionsSize * 2 + m_obsevationsSize;
		return &me[stride * entry + 2 + m_obsevationsSize];
	}

	ndBrainFloat* GetObservations(ndInt32 entry)
	{
		ndTrajectoryStep& me = *this;
		ndInt32 stride = 2 + m_actionsSize * 2 + m_obsevationsSize;
		return &me[stride * entry + 2];
	}

	ndInt32 m_actionsSize;
	ndInt32 m_obsevationsSize;
};

template<ndInt32 statesDim, ndInt32 actionDim>
class ndBrainAgentContinuePolicyGradient_Trainer : public ndBrainAgent
{
	public:
	ndBrainAgentContinuePolicyGradient_Trainer(ndSharedPtr<ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>>& master);
	~ndBrainAgentContinuePolicyGradient_Trainer();

	ndBrain* GetActor();
	void SelectAction(ndBrainVector& actions) const;

	void InitWeights() { ndAssert(0); }
	virtual void OptimizeStep() { ndAssert(0); }
	void Save(ndBrainSave* const) { ndAssert(0); }
	bool IsTrainer() const { ndAssert(0); return true; }
	ndInt32 GetEpisodeFrames() const { ndAssert(0); return 0; }
	void InitWeights(ndBrainFloat, ndBrainFloat) { ndAssert(0); }

	virtual void Step();
	virtual void SaveTrajectory();
	virtual bool IsTerminal() const;

	ndBrainVector m_workingBuffer;
	ndTrajectoryStep m_trajectory;
	ndSharedPtr<ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>> m_master;

	mutable std::random_device m_rd;
	mutable std::mt19937 m_gen;
	mutable std::normal_distribution<ndFloat32> m_d;

	friend class ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>;
};

template<ndInt32 statesDim, ndInt32 actionDim>
class ndBrainAgentContinuePolicyGradient_TrainerMaster : public ndBrainThreadPool
{
	public:
	class HyperParameters
	{
		public:
		HyperParameters()
		{
			m_randomSeed = 47;
			m_numberOfLayers = 4;
			m_bashBufferSize = 64;
			m_neuronPerLayers = 64;
			m_numberOfActions = 0;
			m_numberOfObservations = 0;

			m_bashTrajectoryCount = 100;
			m_maxTrajectorySteps = 4096;
			m_extraTrajectorySteps = 1024;
			//m_baseLineOptimizationPases = 4;
			m_baseLineOptimizationPases = 1;

			//m_criticLearnRate = ndBrainFloat(0.0005f);
			//m_policyLearnRate = ndBrainFloat(0.0002f);
			m_criticLearnRate = ndBrainFloat(0.0002f);
			m_policyLearnRate = ndBrainFloat(0.0001f);

			m_regularizer = ndBrainFloat(1.0e-6f);
			m_discountFactor = ndBrainFloat(0.99f);
			m_threadsCount = ndMin(ndBrainThreadPool::GetMaxThreads(), m_bashBufferSize);
			//m_threadsCount = 1;
		}

		ndBrainFloat m_policyLearnRate;
		ndBrainFloat m_criticLearnRate;
		ndBrainFloat m_regularizer;
		ndBrainFloat m_discountFactor;

		ndInt32 m_threadsCount;
		ndInt32 m_numberOfActions;
		ndInt32 m_numberOfObservations;

		ndInt32 m_numberOfLayers;
		ndInt32 m_bashBufferSize;
		ndInt32 m_neuronPerLayers;
		ndInt32 m_maxTrajectorySteps;
		ndInt32 m_bashTrajectoryCount;
		ndInt32 m_extraTrajectorySteps;
		ndInt32 m_baseLineOptimizationPases;
		ndUnsigned32 m_randomSeed;
	};

	class MemoryStateValues: protected ndBrainVector
	{
		public:
		MemoryStateValues(ndInt32 obsevationsSize)
			:ndBrainVector()
			,m_obsevationsSize(obsevationsSize)
		{
			#ifdef ND_CONTINUE_POLICY_GRADIENT_USE_PAST_REWARDS
			SetCount(ND_CONTINUE_POLICY_GRADIENT_BUFFER_SIZE * (m_obsevationsSize + 1));
			#endif
		}

		ndBrainFloat GetReward(ndInt32 index) const
		{
			const ndBrainVector& me = *this;
			return me[index * (m_obsevationsSize + 1)];
		}

		const ndBrainFloat* GetObservations(ndInt32 index) const
		{
			const ndBrainVector& me = *this;
			return &me[index * (m_obsevationsSize + 1) + 1];
		}

		void SaveTransition(ndInt32 index, ndBrainFloat reward, const ndBrainFloat* const observations)
		{
			ndBrainVector& me = *this;
			ndInt32 stride = m_obsevationsSize + 1;
			me[index * stride] = reward;
			ndMemCpy(&me[index * stride + 1], observations, m_obsevationsSize);
		}

		ndInt32 m_obsevationsSize;
	};

	ndBrainAgentContinuePolicyGradient_TrainerMaster(const HyperParameters& hyperParameters);
	virtual ~ndBrainAgentContinuePolicyGradient_TrainerMaster();

	ndBrain* GetActor();
	ndBrain* GetCritic();

	const ndString& GetName() const;
	void SetName(const ndString& name);

	ndInt32 GetFramesCount() const;
	ndInt32 GetEposideCount() const;

	bool IsSampling() const;
	ndFloat32 GetAverageScore() const;
	ndFloat32 GetAverageFrames() const;

	void OptimizeStep();

	private:
	void Optimize();
	void OptimizePolicy();
	void OptimizeCritic();
	void UpdateBaseLineValue();

	ndBrain m_actor;
	ndBrain m_baseLineValue;
	ndBrainOptimizerAdam* m_optimizer;
	ndArray<ndBrainTrainer*> m_trainers;
	ndArray<ndBrainTrainer*> m_weightedTrainer;
	ndArray<ndBrainTrainer*> m_auxiliaryTrainers;
	ndBrainOptimizerAdam* m_baseLineValueOptimizer;
	ndArray<ndBrainTrainer*> m_baseLineValueTrainers;

	MemoryStateValues m_stateValues;
	ndArray<ndInt32> m_randomPermutation;
	ndTrajectoryStep m_trajectoryAccumulator;
	
	ndBrainFloat m_gamma;
	ndBrainFloat m_policyLearnRate;
	ndBrainFloat m_criticLearnRate;
	ndInt32 m_numberOfActions;
	ndInt32 m_numberOfObservations;
	ndInt32 m_frameCount;
	ndInt32 m_framesAlive;
	ndInt32 m_eposideCount;
	ndInt32 m_bashBufferSize;
	ndInt32 m_maxTrajectorySteps;
	ndInt32 m_extraTrajectorySteps;
	ndInt32 m_bashTrajectoryIndex;
	ndInt32 m_bashTrajectoryCount;
	ndInt32 m_bashTrajectorySteps;
	ndInt32 m_baseLineOptimizationPases;
	ndInt32 m_baseValueWorkingBufferSize;
	ndInt32 m_memoryStateIndex;
	ndInt32 m_memoryStateIndexFull;
	ndUnsigned32 m_randomSeed;
	ndBrainVector m_workingBuffer;
	ndMovingAverage<8> m_averageScore;
	ndMovingAverage<8> m_averageFramesPerEpisodes;
	ndString m_name;
	ndList<ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>*> m_agents;
	friend class ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>;
};

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>::ndBrainAgentContinuePolicyGradient_Trainer(ndSharedPtr<ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>>& master)
	:ndBrainAgent()
	,m_workingBuffer()
	,m_trajectory(master->m_numberOfActions, master->m_numberOfObservations)
	,m_master(master)
	,m_rd()
	,m_gen(m_rd())
	,m_d(ndFloat32(0.0f), ndFloat32(1.0f))
{
	m_gen.seed(m_master->m_randomSeed);
	m_master->m_randomSeed += 1;

	//std::mt19937 m_gen0(m_rd());
	//std::mt19937 m_gen1(m_rd());
	//m_gen0.seed(m_master->m_randomSeed);
	//m_gen1.seed(m_master->m_randomSeed);
	//std::uniform_real_distribution<ndFloat32> uniform0(ndFloat32(0.0f), ndFloat32(1.0f));
	//std::uniform_real_distribution<ndFloat32> uniform1(ndFloat32(0.0f), ndFloat32(1.0f));
	//ndFloat32 xxx0 = uniform0(m_gen0);
	//ndFloat32 xxx1 = uniform1(m_gen1);

	m_master->m_agents.Append(this);
	m_trajectory.SetStepNumber(0);
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>::~ndBrainAgentContinuePolicyGradient_Trainer()
{
	for (typename ndList<ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>*>::ndNode* node = m_master->m_agents.GetFirst(); node; node = node->GetNext())
	{
		if (node->GetInfo() == this)
		{
			m_master->m_agents.Remove(node);
			break;
		}
	}
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrain* ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>::GetActor()
{ 
	return m_master->GetActor(); 
}

template<ndInt32 statesDim, ndInt32 actionDim>
bool ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>::IsTerminal() const
{
	return false;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>::SelectAction(ndBrainVector& actions) const
{
	//for (ndInt32 i = 0; i < actionDim; ++i)
	for (ndInt32 i = actionDim - 1; i >= 0; --i)
	{
		ndBrainFloat sample = ndBrainFloat(actions[i] + m_d(m_gen) * actions[i + actionDim]);
		ndBrainFloat squashedAction = ndClamp(sample, ndBrainFloat(-1.0f), ndBrainFloat(1.0f));
		actions[i] = squashedAction;
	}
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>::Step()
{
	ndInt32 entryIndex = m_trajectory.GetStepNumber();
	m_trajectory.SetStepNumber(entryIndex + 1);

	ndBrainMemVector actions(m_trajectory.GetActions(entryIndex), actionDim * 2);
	ndBrainMemVector observation(m_trajectory.GetObservations(entryIndex), statesDim);
	
	GetObservation(&observation[0]);
	m_master->m_actor.MakePrediction(observation, actions, m_workingBuffer);
	 
	SelectAction(actions);
	ApplyActions(&actions[0]);
	m_trajectory.SetReward(entryIndex, CalculateReward());
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>::SaveTrajectory()
{
	if (m_trajectory.GetStepNumber())
	{
		m_master->m_bashTrajectoryIndex++;

		// remove last step because if it was a dead state, it will provide misleading feedback.
		m_trajectory.SetStepNumber(m_trajectory.GetStepNumber() - 1);
		
		// using the Bellman equation to calculate trajectory rewards. (Monte Carlo method)
		ndBrainFloat gamma = m_master->m_gamma;
		for (ndInt32 i = m_trajectory.GetStepNumber() - 2; i >= 0; --i)
		{
			ndBrainFloat r0 = m_trajectory.GetReward(i);
			ndBrainFloat r1 = m_trajectory.GetReward(i + 1);
			m_trajectory.SetReward(i, r0 + gamma * r1);
		}
		
		// get the max trajectory steps
		const ndInt32 maxSteps = ndMin(m_trajectory.GetStepNumber(), m_master->m_maxTrajectorySteps);
		ndAssert(maxSteps > 0);

		ndTrajectoryStep& trajectoryAccumulator = m_master->m_trajectoryAccumulator;
		for (ndInt32 i = 0; i < maxSteps; ++i)
		{
			ndInt32 index = trajectoryAccumulator.GetStepNumber();
			trajectoryAccumulator.SetStepNumber(index + 1);
			trajectoryAccumulator.SetReward(index, m_trajectory.GetReward(i));
			ndMemCpy(trajectoryAccumulator.GetActions(index), m_trajectory.GetActions(i), actionDim * 2);
			ndMemCpy(trajectoryAccumulator.GetObservations(index), m_trajectory.GetObservations(i), statesDim);
		}
	}
	m_trajectory.SetStepNumber(0);
}

// ***************************************************************************************
//
// ***************************************************************************************
template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::ndBrainAgentContinuePolicyGradient_TrainerMaster(const HyperParameters& hyperParameters)
	:ndBrainThreadPool()
	,m_actor()
	,m_baseLineValue()
	,m_optimizer(nullptr)
	,m_trainers()
	,m_weightedTrainer()
	,m_auxiliaryTrainers()
	,m_baseLineValueOptimizer(nullptr)
	,m_baseLineValueTrainers()
	,m_stateValues(hyperParameters.m_numberOfObservations)
	,m_randomPermutation()
	,m_trajectoryAccumulator(hyperParameters.m_numberOfActions, hyperParameters.m_numberOfObservations)
	,m_gamma(hyperParameters.m_discountFactor)
	,m_policyLearnRate(hyperParameters.m_policyLearnRate)
	,m_criticLearnRate(hyperParameters.m_criticLearnRate)
	,m_numberOfActions(hyperParameters.m_numberOfActions)
	,m_numberOfObservations(hyperParameters.m_numberOfObservations)
	,m_frameCount(0)
	,m_framesAlive(0)
	,m_eposideCount(0)
	,m_bashBufferSize(hyperParameters.m_bashBufferSize)
	,m_maxTrajectorySteps(hyperParameters.m_maxTrajectorySteps)
	,m_extraTrajectorySteps(hyperParameters.m_extraTrajectorySteps)
	,m_bashTrajectoryIndex(0)
	,m_bashTrajectoryCount(hyperParameters.m_bashTrajectoryCount)
	,m_bashTrajectorySteps(hyperParameters.m_bashTrajectoryCount * m_maxTrajectorySteps)
	,m_baseLineOptimizationPases(hyperParameters.m_baseLineOptimizationPases)
	,m_baseValueWorkingBufferSize(0)
	,m_memoryStateIndex(0)
	,m_memoryStateIndexFull(0)
	,m_randomSeed(hyperParameters.m_randomSeed)
	,m_workingBuffer()
	,m_averageScore()
	,m_averageFramesPerEpisodes()
	,m_agents()
{
	ndAssert(m_numberOfActions);
	ndAssert(m_numberOfObservations);
	ndAssert(m_numberOfActions == actionDim);
	ndAssert(m_numberOfObservations == statesDim);
	ndSetRandSeed(m_randomSeed);

	// build policy neural net
	SetThreadCount(hyperParameters.m_threadsCount);
	ndFixSizeArray<ndBrainLayer*, 32> layers;

	#define ACTIVATION_VPG_TYPE ndBrainLayerActivationTanh
	//#define ACTIVATION_VPG_TYPE ndBrainLayerActivationElu
	//#define ACTIVATION_VPG_TYPE ndBrainLayerActivationSigmoidLinear
	
	layers.SetCount(0);
	layers.PushBack(new ndBrainLayerLinear(statesDim, hyperParameters.m_neuronPerLayers));
	layers.PushBack(new ACTIVATION_VPG_TYPE(layers[layers.GetCount() - 1]->GetOutputSize()));
	for (ndInt32 i = 1; i < hyperParameters.m_numberOfLayers; ++i)
	{
		ndAssert(layers[layers.GetCount() - 1]->GetOutputSize() == hyperParameters.m_neuronPerLayers);
		layers.PushBack(new ndBrainLayerLinear(hyperParameters.m_neuronPerLayers, hyperParameters.m_neuronPerLayers));
		layers.PushBack(new ACTIVATION_VPG_TYPE(hyperParameters.m_neuronPerLayers));
	}
	layers.PushBack(new ndBrainLayerLinear(hyperParameters.m_neuronPerLayers, actionDim * 2));
	layers.PushBack(new ndBrainContinuePolicyGradientLastActivationLayer(actionDim));

	for (ndInt32 i = 0; i < layers.GetCount(); ++i)
	{
		m_actor.AddLayer(layers[i]);
	}

	m_actor.InitWeights();
	ndAssert(!strcmp((m_actor[m_actor.GetCount() - 1])->GetLabelId(), "ndBrainLayerActivationTanh"));

	m_trainers.SetCount(0);
	m_auxiliaryTrainers.SetCount(0);
	for (ndInt32 i = 0; i < m_bashBufferSize; ++i)
	{
		ndBrainTrainer* const trainer = new ndBrainTrainer(&m_actor);
		m_trainers.PushBack(trainer);
	
		ndBrainTrainer* const auxiliaryTrainer = new ndBrainTrainer(&m_actor);
		m_auxiliaryTrainers.PushBack(auxiliaryTrainer);
	}
	
	m_weightedTrainer.PushBack(m_trainers[0]);
	m_optimizer = new ndBrainOptimizerAdam();
	m_optimizer->SetRegularizer(hyperParameters.m_regularizer);

	// build state value critic neural net
	layers.SetCount(0);
	layers.PushBack(new ndBrainLayerLinear(statesDim, hyperParameters.m_neuronPerLayers * 2));
	layers.PushBack(new ACTIVATION_VPG_TYPE(layers[layers.GetCount() - 1]->GetOutputSize()));
	for (ndInt32 i = 1; i < hyperParameters.m_numberOfLayers; ++i)
	{
		ndAssert(layers[layers.GetCount() - 1]->GetOutputSize() == hyperParameters.m_neuronPerLayers * 2);
		layers.PushBack(new ndBrainLayerLinear(layers[layers.GetCount() - 1]->GetOutputSize(), hyperParameters.m_neuronPerLayers * 2));
		layers.PushBack(new ACTIVATION_VPG_TYPE(layers[layers.GetCount() - 1]->GetOutputSize()));
	}
	layers.PushBack(new ndBrainLayerLinear(layers[layers.GetCount() - 1]->GetOutputSize(), 1));
	for (ndInt32 i = 0; i < layers.GetCount(); ++i)
	{
		m_baseLineValue.AddLayer(layers[i]);
	}
	m_baseLineValue.InitWeights();
	
	ndAssert(m_baseLineValue.GetOutputSize() == 1);
	ndAssert(m_baseLineValue.GetInputSize() == m_actor.GetInputSize());
	ndAssert(!strcmp((m_baseLineValue[m_baseLineValue.GetCount() - 1])->GetLabelId(), "ndBrainLayerLinear"));
	
	m_baseLineValueTrainers.SetCount(0);
	for (ndInt32 i = 0; i < m_bashBufferSize; ++i)
	{
		ndBrainTrainer* const trainer = new ndBrainTrainer(&m_baseLineValue);
		m_baseLineValueTrainers.PushBack(trainer);
	}
	
	m_baseLineValueOptimizer = new ndBrainOptimizerAdam();
	//m_baseLineValueOptimizer->SetRegularizer(hyperParameters.m_regularizer);
	m_baseLineValueOptimizer->SetRegularizer(ndBrainFloat(1.0e-4f));
	
	m_baseValueWorkingBufferSize = m_baseLineValue.CalculateWorkingBufferSize();
	m_workingBuffer.SetCount(m_baseValueWorkingBufferSize * hyperParameters.m_threadsCount);

	//m_actor.SaveToFile("xxxx1.xxx");
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::~ndBrainAgentContinuePolicyGradient_TrainerMaster()
{
	for (ndInt32 i = 0; i < m_trainers.GetCount(); ++i)
	{
		delete m_trainers[i];
		delete m_auxiliaryTrainers[i];
	}
	delete m_optimizer;
	
	for (ndInt32 i = 0; i < m_baseLineValueTrainers.GetCount(); ++i)
	{
		delete m_baseLineValueTrainers[i];
	}
	delete m_baseLineValueOptimizer;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrain* ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::GetActor()
{ 
	return &m_actor; 
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrain* ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::GetCritic()
{
	return &m_baseLineValue;
}

template<ndInt32 statesDim, ndInt32 actionDim>
const ndString& ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::GetName() const
{
	return m_name;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::SetName(const ndString& name)
{
	m_name = name;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::GetFramesCount() const
{
	return m_frameCount;
}

template<ndInt32 statesDim, ndInt32 actionDim>
bool ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::IsSampling() const
{
	return false;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::GetEposideCount() const
{
	return m_eposideCount;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndFloat32 ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::GetAverageFrames() const
{
	return m_averageFramesPerEpisodes.GetAverage();
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndFloat32 ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::GetAverageScore() const
{
	return m_averageScore.GetAverage();
}


template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::OptimizePolicy()
{
	ndAtomic<ndInt32> iterator(0);
	auto ClearGradients = ndMakeObject::ndFunction([this, &iterator](ndInt32, ndInt32)
	{
		for (ndInt32 i = iterator++; i < m_bashBufferSize; i = iterator++)
		{
			ndBrainTrainer* const trainer = m_trainers[i];
			trainer->ClearGradients();
		}
	});
	ndBrainThreadPool::ParallelExecute(ClearGradients);

	const ndInt32 steps = ndInt32(m_trajectoryAccumulator.GetStepNumber());
	for (ndInt32 base = 0; base < steps; base += m_bashBufferSize)
	{
		auto CalculateGradients = ndMakeObject::ndFunction([this, &iterator, base](ndInt32, ndInt32)
		{
			class MaxLikelihoodLoss : public ndBrainLoss
			{
				public:
				MaxLikelihoodLoss(ndBrainTrainer& trainer, ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>* const agent, ndInt32 index)
					:ndBrainLoss()
					,m_trainer(trainer)
					,m_agent(agent)
					,m_index(index)
				{
				}

				void GetLoss(const ndBrainVector& output, ndBrainVector& loss)
				{
					// basically this fits a multivariate Gaussian process with zero cross covariance to the actions.
					//ndTrajectory_StepOld<statesDim, actionDim>& trajectoryStep = m_agent->m_trajectoryAccumulator[m_index];
					//const ndBrainFloat advantage = trajectoryStep.m_advantage;
					const ndBrainFloat advantage = m_agent->m_trajectoryAccumulator.GetAdvantage(m_index);
					const ndBrainFloat* const actions = m_agent->m_trajectoryAccumulator.GetActions(m_index);
					for (ndInt32 i = actionDim - 1; i >= 0; --i)
					{
						const ndBrainFloat mean = output[i];
						const ndBrainFloat sigma1 = output[i + actionDim];
						const ndBrainFloat sigma2 = sigma1 * sigma1;
						const ndBrainFloat sigma3 = sigma2 * sigma1;
						const ndBrainFloat num = (actions[i] - mean);
						ndAssert(sigma1 >= ND_CONTINUE_POLICY_GRADIENT_MIN_VARIANCE);
					
						loss[i] = advantage * num / sigma2;
						loss[i + actionDim ] = advantage * (num * num / sigma3 - ndBrainFloat(1.0f) / sigma1);
					}
				}

				ndBrainTrainer& m_trainer;
				ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>* m_agent;
				ndInt32 m_index;
			};

			for (ndInt32 i = iterator++; i < m_bashBufferSize; i = iterator++)
			{
				ndBrainTrainer& trainer = *m_auxiliaryTrainers[i];
				MaxLikelihoodLoss loss(trainer, this, base + i);
				if ((base + i) < m_trajectoryAccumulator.GetStepNumber())
				{
					const ndBrainMemVector observation(m_trajectoryAccumulator.GetObservations(base + i), statesDim);
					trainer.BackPropagate(observation, loss);
				}
				else
				{
					trainer.ClearGradients();
				}
			}
		});

		auto AddGradients = ndMakeObject::ndFunction([this, &iterator](ndInt32, ndInt32)
		{
			for (ndInt32 i = iterator++; i < m_bashBufferSize; i = iterator++)
			{
				ndBrainTrainer* const trainer = m_trainers[i];
				const ndBrainTrainer* const auxiliaryTrainer = m_auxiliaryTrainers[i];
				trainer->AddGradients(auxiliaryTrainer);
			}
		});

		iterator = 0;
		ndBrainThreadPool::ParallelExecute(CalculateGradients);
		iterator = 0;
		ndBrainThreadPool::ParallelExecute(AddGradients);
	}

	m_optimizer->AccumulateGradients(this, m_trainers);
	m_weightedTrainer[0]->ScaleWeights(ndBrainFloat(1.0f) / ndBrainFloat(m_trajectoryAccumulator.GetStepNumber()));
	m_optimizer->Update(this, m_weightedTrainer, -m_policyLearnRate);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::Optimize()
{
	OptimizeCritic();
	OptimizePolicy();
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::OptimizeStep()
{
	for (typename ndList<ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>*>::ndNode* node = m_agents.GetFirst(); node; node = node->GetNext())
	{
		ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>* const agent = node->GetInfo();

		bool isTeminal = agent->IsTerminal() || (agent->m_trajectory.GetStepNumber() >= (m_extraTrajectorySteps + m_maxTrajectorySteps));
		if (isTeminal)
		{
			agent->SaveTrajectory();
			agent->ResetModel();
		}
		m_frameCount++;
		m_framesAlive++;
	}

	if ((m_bashTrajectoryIndex >= (m_bashTrajectoryCount * 10)) || (m_trajectoryAccumulator.GetStepNumber() >= m_bashTrajectorySteps))
	{
		Optimize();
		m_eposideCount++;
		m_framesAlive = 0;
		m_bashTrajectoryIndex = 0;
		m_trajectoryAccumulator.SetStepNumber(0);
		for (typename ndList<ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>*>::ndNode* node = m_agents.GetFirst(); node; node = node->GetNext())
		{
			ndBrainAgentContinuePolicyGradient_Trainer<statesDim, actionDim>* const agent = node->GetInfo();
			agent->m_trajectory.SetStepNumber(0);
		}
	}
}

//#pragma optimize( "", off )
template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::OptimizeCritic()
{
	UpdateBaseLineValue();

	ndBrainFloat averageSum = ndBrainFloat(0.0f);
	const ndInt32 stepNumber = m_trajectoryAccumulator.GetStepNumber();
	for (ndInt32 i = stepNumber - 1; i >= 0; --i)
	{
		averageSum += m_trajectoryAccumulator.GetReward(i);
	}
	m_averageScore.Update(averageSum / ndBrainFloat(stepNumber));
	m_averageFramesPerEpisodes.Update(ndBrainFloat(stepNumber) / ndBrainFloat(m_bashTrajectoryIndex));
	
	ndAtomic<ndInt32> iterator(0);
	m_workingBuffer.SetCount(m_baseValueWorkingBufferSize * GetThreadCount());
	auto CalculateAdvantage = ndMakeObject::ndFunction([this, &iterator](ndInt32 threadIndex, ndInt32)
	{
		ndBrainFixSizeVector<1> actions;
		ndBrainMemVector workingBuffer(&m_workingBuffer[threadIndex * m_baseValueWorkingBufferSize], m_baseValueWorkingBufferSize);
	
		ndInt32 const count = m_trajectoryAccumulator.GetStepNumber();
		for (ndInt32 i = iterator++; i < count; i = iterator++)
		{
			const ndBrainMemVector observation(m_trajectoryAccumulator.GetObservations(i), statesDim);
			m_baseLineValue.MakePrediction(observation, actions, workingBuffer);
			ndBrainFloat baseLine = actions[0];
			ndBrainFloat reward = m_trajectoryAccumulator.GetReward(i);
			ndBrainFloat advantage = reward - baseLine;
			m_trajectoryAccumulator.SetAdvantage(i, advantage);
		}
	});
	ndBrainThreadPool::ParallelExecute(CalculateAdvantage);
	
	ndFloat64 advantageVariance2 = ndBrainFloat(0.0f);
	for (ndInt32 i = stepNumber - 1; i >= 0; --i)
	{
		ndBrainFloat advantage = m_trajectoryAccumulator.GetAdvantage(i);
		advantageVariance2 += advantage * advantage;
	}
	
	advantageVariance2 /= ndBrainFloat(stepNumber);
	ndBrainFloat invVariance = ndBrainFloat(1.0f) / ndBrainFloat(ndSqrt(advantageVariance2 + ndBrainFloat(1.0e-4f)));
	for (ndInt32 i = stepNumber - 1; i >= 0; --i)
	{
		const ndBrainFloat normalizedAdvantage = m_trajectoryAccumulator.GetAdvantage(i) * invVariance;
		m_trajectoryAccumulator.SetAdvantage(i, normalizedAdvantage);
	}
}

#pragma optimize( "", off )
template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentContinuePolicyGradient_TrainerMaster<statesDim, actionDim>::UpdateBaseLineValue()
{
#ifdef	ND_CONTINUE_POLICY_GRADIENT_USE_PAST_REWARDS

	m_randomPermutation.SetCount(m_trajectoryAccumulator.GetStepNumber());
	for (ndInt32 i = ndInt32(m_trajectoryAccumulator.GetStepNumber()) - 1; i >= 0; --i)
	{
		m_randomPermutation[i] = i;
	}
	m_randomPermutation.RandomShuffle(m_randomPermutation.GetCount());
	
	const ndInt32 start = m_memoryStateIndex;
	const ndInt32 samplesCount = ndMin (m_trajectoryAccumulator.GetStepNumber() / 5, ND_CONTINUE_POLICY_GRADIENT_BUFFER_SIZE / 4);
	for (ndInt32 i = samplesCount; i >= 0; --i)
	{
		ndInt32 srcIndex = m_randomPermutation[i];
		m_stateValues.SaveTransition(m_memoryStateIndex, m_trajectoryAccumulator.GetReward(srcIndex), m_trajectoryAccumulator.GetObservations(srcIndex));
		m_memoryStateIndex = (m_memoryStateIndex + 1) % ND_CONTINUE_POLICY_GRADIENT_BUFFER_SIZE;
	}

	if (!m_memoryStateIndexFull && (start < m_memoryStateIndex))
	{
		ndAtomic<ndInt32> iterator(0);
		const ndInt32 maxSteps = (m_trajectoryAccumulator.GetStepNumber() & -m_bashBufferSize) - m_bashBufferSize;
		for (ndInt32 base = 0; base < maxSteps; base += m_bashBufferSize)
		{
			auto BackPropagateBash = ndMakeObject::ndFunction([this, &iterator, base](ndInt32, ndInt32)
			{
				ndBrainLossLeastSquaredError loss(1);
				ndBrainFixSizeVector<1> stateValue;
				for (ndInt32 i = iterator++; i < m_bashBufferSize; i = iterator++)
				{
					const ndInt32 index = m_randomPermutation[base + i];
					ndBrainTrainer& trainer = *m_baseLineValueTrainers[i];
					const ndBrainMemVector observation(m_trajectoryAccumulator.GetObservations(index), statesDim);
					stateValue[0] = m_trajectoryAccumulator.GetReward(index);
					loss.SetTruth(stateValue);
					trainer.BackPropagate(observation, loss);
				}
			});
		
			iterator = 0;
			ndBrainThreadPool::ParallelExecute(BackPropagateBash);
			m_baseLineValueOptimizer->Update(this, m_baseLineValueTrainers, m_criticLearnRate);
		}
	}
	else
	{
		m_memoryStateIndexFull = 1;
		m_randomPermutation.SetCount(ND_CONTINUE_POLICY_GRADIENT_BUFFER_SIZE);
		for (ndInt32 i = ND_CONTINUE_POLICY_GRADIENT_BUFFER_SIZE - 1; i >= 0; --i)
		{
			m_randomPermutation[i] = i;
		}
		m_randomPermutation.RandomShuffle(m_randomPermutation.GetCount());

		ndAtomic<ndInt32> iterator(0);
		const ndInt32 maxSteps = (ND_CONTINUE_POLICY_GRADIENT_BUFFER_SIZE & -m_bashBufferSize) - m_bashBufferSize;
		for (ndInt32 base = 0; base < maxSteps; base += m_bashBufferSize)
		{
			auto BackPropagateBash = ndMakeObject::ndFunction([this, &iterator, base](ndInt32, ndInt32)
			{
				ndBrainLossLeastSquaredError loss(1);
				ndBrainFixSizeVector<1> stateValue;
				for (ndInt32 i = iterator++; i < m_bashBufferSize; i = iterator++)
				{
					const ndInt32 index = m_randomPermutation[base + i];
					ndBrainTrainer& trainer = *m_baseLineValueTrainers[i];

					stateValue[0] = m_stateValues.GetReward(index);
					const ndBrainMemVector observation(m_stateValues.GetObservations(index), statesDim);
					loss.SetTruth(stateValue);
					trainer.BackPropagate(observation, loss);
				}
			});

			iterator = 0;
			ndBrainThreadPool::ParallelExecute(BackPropagateBash);
			m_baseLineValueOptimizer->Update(this, m_baseLineValueTrainers, m_criticLearnRate);
		}
	}

#else
	m_randomPermutation.SetCount(m_trajectoryAccumulator.GetCount());
	for (ndInt32 i = ndInt32(m_trajectoryAccumulator.GetCount()) - 1; i >= 0; --i)
	{
		m_randomPermutation[i] = i;
	}

	ndAtomic<ndInt32> iterator(0);
	const ndInt32 maxSteps = (m_trajectoryAccumulator.GetCount() & -m_bashBufferSize) - m_bashBufferSize;
	for (ndInt32 passes = 0; passes < m_baseLineOptimizationPases; ++passes)
	{
		m_randomPermutation.RandomShuffle(m_randomPermutation.GetCount());
		for (ndInt32 base = 0; base < maxSteps; base += m_bashBufferSize)
		{
			auto BackPropagateBash = ndMakeObject::ndFunction([this, &iterator, base](ndInt32, ndInt32)
			{
				class ndPolicyLoss : public	ndBrainLossLeastSquaredError
				{
					public:
					ndPolicyLoss()
						:ndBrainLossLeastSquaredError(1)
					{
					}

					void GetLoss(const ndBrainVector& output, ndBrainVector& loss)
					{
						ndBrainLossLeastSquaredError::GetLoss(output, loss);
					}
				};

				ndPolicyLoss loss;
				ndBrainFixSizeVector<1> stateValue;
				for (ndInt32 i = iterator++; i < m_bashBufferSize; i = iterator++)
				{
					const ndInt32 index = m_randomPermutation[base + i];
					ndBrainTrainer& trainer = *m_baseLineValueTrainers[i];
					const ndBrainMemVector observation(&m_trajectoryAccumulator[index].m_observation[0], statesDim);
					stateValue[0] = m_trajectoryAccumulator[index].m_reward;
					loss.SetTruth(stateValue);
					trainer.BackPropagate(observation, loss);
				}
			});

			iterator = 0;
			ndBrainThreadPool::ParallelExecute(BackPropagateBash);
			m_baseLineValueOptimizer->Update(this, m_baseLineValueTrainers, m_criticLearnRate);
		}
	}
#endif
}

#endif 