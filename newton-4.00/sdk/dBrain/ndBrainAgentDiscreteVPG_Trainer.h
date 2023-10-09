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

#ifndef _ND_DQN_BRAIN_AGENT_DESCRETE_POLICY_GRAD_TRAINER_H__
#define _ND_DQN_BRAIN_AGENT_DESCRETE_POLICY_GRAD_TRAINER_H__

#include "ndBrainStdafx.h"
#include "ndBrain.h"
#include "ndBrainAgent.h"
#include "ndBrainTrainer.h"
#include "ndBrainReplayBuffer.h"
#include "ndBrainLossLeastSquaredError.h"

// this is an implementation of the vanilla policy Gradient as described in:
// https://spinningup.openai.com/en/latest/algorithms/vpg.html

template<ndInt32 statesDim, ndInt32 actionDim>
class ndBrainAgentDiscreteVPG_Trainer : public ndBrainAgent, public ndBrainThreadPool
{
	public:
	class HyperParameters
	{
		public:
		HyperParameters()
		{
			m_bashBufferSize = 32;
			m_numberOfHiddenLayers = 3;
			m_maxTrajectorySteps = 1024 * 2;
			m_hiddenLayersNumberOfNeurons = 64;

			m_learnRate = ndBrainFloat(0.001f);
			m_regularizer = ndBrainFloat(1.0e-6f);
			m_discountFactor = ndBrainFloat(0.99f);
			m_threadsCount = ndMin(ndBrainThreadPool::GetMaxThreads(), m_bashBufferSize / 4);
		}

		ndBrainFloat m_learnRate;
		ndBrainFloat m_regularizer;
		ndBrainFloat m_discountFactor;

		ndInt32 m_threadsCount;
		ndInt32 m_bashBufferSize;
		ndInt32 m_maxTrajectorySteps;
		ndInt32 m_numberOfHiddenLayers;
		ndInt32 m_hiddenLayersNumberOfNeurons;
	};

	class ndTrajectoryStep
	{
		public:
		ndTrajectoryStep()
			//:m_probability()
			:m_observation()
			//,m_categoricalSample()
			,m_reward(ndBrainFloat(0.0f))
			,m_action(0)
		{
		}

		ndTrajectoryStep(const ndTrajectoryStep& src)
			//:m_probability(src.m_probability)
			:m_observation(src.m_observation)
			//,m_categoricalSample(src.m_categoricalSample)
			,m_reward(src.m_reward)
			,m_action(src.m_action)
		{
		}

		ndTrajectoryStep& operator=(const ndTrajectoryStep& src)
		{
			new (this) ndTrajectoryStep(src);
			return*this;
		}

		//ndBrainFixSizeVector<actionDim> m_probability;
		ndBrainFixSizeVector<statesDim> m_observation;
		//ndBrainFixSizeVector<actionDim> m_categoricalSample;
		ndBrainFloat m_reward;
		ndInt32 m_action;
	};

	ndBrainAgentDiscreteVPG_Trainer(const HyperParameters& hyperParameters);
	virtual ~ndBrainAgentDiscreteVPG_Trainer();

	ndInt32 GetFramesCount() const;
	ndInt32 GetEposideCount() const;
	ndInt32 GetEpisodeFrames() const;
	ndBrainFloat GetCurrentValue() const;

	protected:
	void Step();
	void OptimizeStep();
	bool IsTrainer() const;
	void Save(ndBrainSave* const loadSave) const;

	void InitWeights();
	void InitWeights(ndBrainFloat weighVariance, ndBrainFloat biasVariance);

	bool IsSampling() const;
	bool IsTerminal() const;
	ndBrainFloat GetReward() const;
	void AddExploration(ndBrainFloat* const actions);

	private:
	void Optimize();
	void BackPropagate();
	void SetBufferSize(ndInt32 size);

	void CalcucateRewards();
	ndInt32 SelectAction(const ndBrainVector& probabilities) const;

	protected:
	ndBrain m_actor;
	//ndBrainOptimizerAdam* m_optimizer;
	//ndArray<ndBrainTrainer*> m_trainers;

	ndArray<ndTrajectoryStep> m_trajectory;
	ndBrainVector m_rewads;

	ndBrainFloat m_gamma;
	ndBrainFloat m_learnRate;
	ndBrainFloat m_currentQValue;
	ndInt32 m_frameCount;
	ndInt32 m_framesAlive;
	ndInt32 m_eposideCount;
	ndInt32 m_bashBufferSize;
	ndInt32 m_targetUpdatePeriod;
};

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::ndBrainAgentDiscreteVPG_Trainer(const HyperParameters& hyperParameters)
	:ndBrainAgent()
	,ndBrainThreadPool()
	,m_actor()
	,m_trajectory()
	,m_rewads()
	,m_gamma(hyperParameters.m_discountFactor)
	,m_learnRate(hyperParameters.m_learnRate)
	,m_currentQValue(ndBrainFloat(0.0f))
	,m_frameCount(0)
	,m_framesAlive(0)
	,m_eposideCount(0)
	,m_bashBufferSize(hyperParameters.m_bashBufferSize)
{
	// build neural net
	ndFixSizeArray<ndBrainLayer*, 32> layers;
	layers.PushBack(new ndBrainLayerLinear(m_stateSize, hyperParameters.m_hiddenLayersNumberOfNeurons));
	layers.PushBack(new ndBrainLayerTanhActivation(layers[layers.GetCount() - 1]->GetOutputSize()));
	for (ndInt32 i = 1; i < hyperParameters.m_numberOfHiddenLayers; ++i)
	{
		ndAssert(layers[layers.GetCount() - 1]->GetOutputSize() == hyperParameters.m_hiddenLayersNumberOfNeurons);
		layers.PushBack(new ndBrainLayerLinear(hyperParameters.m_hiddenLayersNumberOfNeurons, hyperParameters.m_hiddenLayersNumberOfNeurons));
		layers.PushBack(new ndBrainLayerTanhActivation(hyperParameters.m_hiddenLayersNumberOfNeurons));
	}
	layers.PushBack(new ndBrainLayerLinear(hyperParameters.m_hiddenLayersNumberOfNeurons, m_actionsSize));
	layers.PushBack(new ndBrainLayerSoftmaxActivation(m_actionsSize));
	for (ndInt32 i = 0; i < layers.GetCount(); ++i)
	{
		m_actor.AddLayer(layers[i]);
		//m_target.AddLayer(layers[i]->Clone());
	}

	//m_trainers.SetCount(0);
	//SetThreadCount(hyperParameters.m_threadsCount);
	//for (ndInt32 i = 0; i < m_bashBufferSize; ++i)
	//{
	//	ndBrainTrainer* const trainer = new ndBrainTrainer(&m_actor);
	//	m_trainers.PushBack(trainer);
	//}
	//SetBufferSize(hyperParameters.m_replayBufferSize);
	//m_explorationProbabilityAnnelining = (m_explorationProbability - m_minExplorationProbability) / ndBrainFloat(m_replayBuffer.GetCapacity());

	InitWeights();
	//m_optimizer = new ndBrainOptimizerAdam();
	//m_optimizer->SetRegularizer(hyperParameters.m_regularizer);

	//for (ndInt32 i = 0; i < 256; i++)
	//{
	//	m_observationMemory.PushBack(m_observation);
	//}

	m_rewads.SetCount(hyperParameters.m_maxTrajectorySteps);
	m_trajectory.SetCount(hyperParameters.m_maxTrajectorySteps);
	m_trajectory.SetCount(0);
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::~ndBrainAgentDiscreteVPG_Trainer()
{
	ndAssert(0);
	//for (ndInt32 i = 0; i < m_trainers.GetCount(); ++i)
	//{
	//	delete m_trainers[i];
	//}
	//delete m_optimizer;
}

template<ndInt32 statesDim, ndInt32 actionDim>
bool ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::IsTrainer() const
{
	return true;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::InitWeights()
{
	m_actor.InitWeightsXavierMethod();
	//m_target.CopyFrom(m_actor);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::InitWeights(ndBrainFloat weighVariance, ndBrainFloat biasVariance)
{
	ndAssert(0);
	m_actor.InitWeights(weighVariance, biasVariance);
	//m_target.CopyFrom(m_actor);
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::GetFramesCount() const
{
	return m_frameCount;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainFloat ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::GetCurrentValue() const
{
	return m_currentQValue;
}

template<ndInt32 statesDim, ndInt32 actionDim>
bool ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::IsSampling() const
{
	return false;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::GetEposideCount() const
{
	return m_eposideCount;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::GetEpisodeFrames() const
{
	return m_framesAlive;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::SetBufferSize(ndInt32 size)
{
	m_replayBuffer____.SetCount(size);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::BackPropagate()
{
	//ndUnsigned32 shuffleBuffer[1024];
	//for (ndInt32 i = 0; i < m_bashBufferSize; ++i)
	//{
	//	shuffleBuffer[i] = ndRandInt() % m_replayBuffer.GetCount();
	//}
	//
	//auto PropagateBash = ndMakeObject::ndFunction([this, &shuffleBuffer](ndInt32 threadIndex, ndInt32 threadCount)
	//{
	//	class Loss : public ndBrainLossLeastSquaredError
	//	{
	//		public:
	//		Loss(ndBrainTrainer& trainer, ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>* const agent)
	//			:ndBrainLossLeastSquaredError(trainer.GetBrain()->GetOutputSize())
	//			,m_trainer(trainer)
	//			,m_agent(agent)
	//			,m_index(0)
	//		{
	//		}
	//
	//		//void GetLoss(const ndBrainVector& output, ndBrainVector& loss)
	//		void GetLoss(const ndBrainVector& output, ndBrainVector&)
	//		{
	//			ndAssert(0);
	//			ndAssert(output.GetCount() == actionDim);
	//			ndAssert(m_truth.GetCount() == m_trainer.GetBrain()->GetOutputSize());
	//
	//			//ndBrainFloat stateBuffer[statesDim * 2];
	//			//ndBrainFloat actionBuffer[actionDim * 2];
	//			//ndBrainMemVector state(stateBuffer, statesDim);
	//			//ndBrainMemVector action(actionBuffer, actionDim);
	//			//
	//			//const ndBrainReplayTransitionMemory<statesDim, 1>& transition = m_agent->m_replayBuffer[m_index];
	//			//for (ndInt32 i = 0; i < actionDim; ++i)
	//			//{
	//			//	action[i] = output[i];
	//			//}
	//			//ndInt32 actionIndex = transition.m_action[0];
	//			//if (transition.m_terminalState)
	//			//{
	//			//	action[actionIndex] = transition.m_reward;
	//			//}
	//			//else
	//			//{
	//			//	ndBrainFloat actionBuffer1[actionDim * 2];
	//			//	ndBrainMemVector action1(actionBuffer1, actionDim);
	//			//
	//			//	for (ndInt32 i = 0; i < statesDim; ++i)
	//			//	{
	//			//		state[i] = transition.m_nextObservation[i];
	//			//	}
	//			//	m_agent->m_target.MakePrediction(state, action1);
	//			//	action[actionIndex] = transition.m_reward + m_agent->m_gamma * action1[actionIndex];
	//			//}
	//			//
	//			//SetTruth(action);
	//			//ndBrainLossLeastSquaredError::GetLoss(output, loss);
	//		}
	//
	//		ndBrainTrainer& m_trainer;
	//		ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>* m_agent;
	//		ndInt32 m_index;
	//	};
	//
	//	ndAssert(0);
	//	//ndBrainFloat stateBuffer[statesDim * 2];
	//	//ndBrainMemVector state(stateBuffer, statesDim);
	//	//ndAssert(m_bashBufferSize == m_trainers.GetCount());
	//	const ndStartEnd startEnd(m_bashBufferSize, threadIndex, threadCount);
	//	for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
	//	{
	//	//	ndBrainTrainer& trainer = *m_trainers[i];
	//	//	Loss loss(trainer, this);
	//	//	ndInt32 index = ndInt32(shuffleBuffer[i]);
	//	//	const ndBrainReplayTransitionMemory<statesDim, 1>& transition = m_replayBuffer[index];
	//	//	for (ndInt32 j = 0; j < statesDim; ++j)
	//	//	{
	//	//		state[j] = transition.m_state[j];
	//	//	}
	//	//	loss.m_index = index;
	//	//	trainer.BackPropagate(state, loss);
	//	}
	//});
	//
	//ndBrainThreadPool::ParallelExecute(PropagateBash);
	//m_optimizer->Update(this, m_trainers, m_learnRate);
	//
	//if ((m_frameCount % m_targetUpdatePeriod) == (m_targetUpdatePeriod - 1))
	//{
	//	// update on line network
	//	m_target.CopyFrom(m_actor);
	//}
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::Save(ndBrainSave* const loadSave) const
{
	loadSave->Save(&m_actor);
}

template<ndInt32 statesDim, ndInt32 actionDim>
bool ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::IsTerminal() const
{
	ndAssert(0);
	return false;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainFloat ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::GetReward() const
{
	ndAssert(0);
	return ndBrainFloat(0.0f);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::CalcucateRewards()
{
	const ndInt32 steps = m_trajectory.GetCount();
	#if 0
	for (ndInt32 i = 0; i < steps; ++i)
	{
		ndBrainFloat sum = ndBrainFloat(0.0f);
		ndBrainFloat discount = ndBrainFloat(1.0f);
		for (ndInt32 j = i; j < steps; ++j)
		{
			sum += discount * m_trajectory[j].m_reward;
			discount *= m_gamma;
		}
		m_rewads[i] = sum;
	}
	#endif

	// using the Bellman equation.
	m_rewads.SetCount(steps);
	m_rewads[steps - 1] = m_trajectory[steps - 1].m_reward;
	for (ndInt32 i = steps - 2; i >= 0; --i)
	{
		m_rewads[i] = m_trajectory[i].m_reward + m_gamma * m_rewads[i + 1];
	}
	m_rewads.GaussianNormalize();
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::Optimize()
{
	CalcucateRewards();
	BackPropagate();
	//if (IsSampling())
	//{
	//	ndExpandTraceMessage("%d start training: episode %d\n", m_frameCount, m_eposideCount);
	//}
}

template<ndInt32 statesDim, ndInt32 actionDim>
//void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::AddExploration(ndBrainFloat* const actions)
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::AddExploration(ndBrainFloat* const)
{
	ndAssert(0);
	//ndInt32 action = 0;
	//ndFloat32 explore = ndRand();
	//
	//const ndBrainMemVector qActionValues(actions, actionDim);
	//if (explore <= m_explorationProbability)
	//{
	//	// explore environment
	//	ndUnsigned32 randomIndex = ndRandInt();
	//	action = ndInt32(randomIndex % actionDim);
	//}
	//else
	//{
	//	action = qActionValues.ArgMax();
	//}
	//
	//m_currentQValue = qActionValues[action];
	//m_currentTransition.m_action[0] = action;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::SelectAction(const ndBrainVector& probabilities) const
{
	ndBrainFloat pdf[actionDim + 1];

	ndBrainFloat sum = ndBrainFloat(0.0f);
	for (ndInt32 i = 0; i < actionDim; ++i)
	{
		pdf[i] = sum;
		sum += probabilities[i];
	}
	pdf[actionDim] = sum;

	ndInt32 index = 0;
	ndFloat32 r = ndRand();
	for (ndInt32 i = 0; i < actionDim; ++i)
	{
		index = i;
		if (pdf[i + 1] >= r)
		{
			break;
		}
	}
	return index;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::Step()
{
	ndTrajectoryStep trajectoryStep;
	ndBrainFixSizeVector<actionDim> probability;

	GetObservation(&trajectoryStep.m_observation[0]);
	m_actor.MakePrediction(trajectoryStep.m_observation, probability);
	//trajectoryStep.m_categoricalSample.CategoricalSample(trajectoryStep.m_probability);
	//trajectoryStep.m_action = SelectAction(trajectoryStep.m_categoricalSample);
	trajectoryStep.m_action = SelectAction(probability);

	ndBrainFloat bestAction = ndBrainFloat(trajectoryStep.m_action);
	ApplyActions(&bestAction);
	trajectoryStep.m_reward = GetReward();

	ndAssert(m_trajectory.GetCount() < m_trajectory.GetCapacity());
	m_trajectory.PushBack(trajectoryStep);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDiscreteVPG_Trainer<statesDim, actionDim>::OptimizeStep()
{
	if (!m_frameCount)
	{
		ResetModel();
	}

	bool isTeminal = IsTerminal() || (m_trajectory.GetCount() == m_trajectory.GetCapacity());
	if (isTeminal)
	{
		Optimize();

		ResetModel();
		m_trajectory.SetCount(0);
		//if (IsSampling() && (m_eposideCount % 500 == 0))
		//{
		//	ndExpandTraceMessage("collecting samples: frame %d out of %d, episode %d \n", m_frameCount, m_replayBuffer.GetCapacity(), m_eposideCount);
		//}
		m_eposideCount++;
		m_framesAlive = 0;
	}
	
	m_frameCount++;
	m_framesAlive++;
}

#endif 