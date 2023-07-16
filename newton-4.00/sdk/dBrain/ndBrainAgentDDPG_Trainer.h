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

#ifndef _ND_BRAIN_AGENT_DDPG_TRAINER_H__
#define _ND_BRAIN_AGENT_DDPG_TRAINER_H__

#include "ndBrainStdafx.h"
#include "ndBrain.h"
#include "ndBrainAgent.h"
#include "ndBrainTrainer_old.h"
#include "ndBrainReplayBuffer.h"

// this is an implementation of the vanilla 
// Continuous control with deep re enforcement learning (ddpg agent)
// trainer as described in: https://arxiv.org/pdf/1509.02971.pdf

// default hyper parameters defaults
#define D_DDPG_LEARN_RATE				ndReal(2.0e-4f)
#define D_DDPG_TARGET_LEARN_RATE		(D_DDPG_LEARN_RATE * ndReal(0.5f))
#define D_DDPG_DISCOUNT_FACTOR			ndReal (0.99f)
#define D_DDPG_REPLAY_BUFFERSIZE		(1024 * 512)
//#define D_DDPG_REPLAY_BUFFERSIZE		(1024 * 4)
#define D_DDPG_REPLAY_BASH_SIZE			32
#define D_DDPG_TARGET_UPDATE_PERIOD		1000
#define D_DDPG_REGULARIZER				ndReal (2.0e-6f)
#define D_DDPG_SOFT_TARGET_FACTOR		ndReal (1.0e-3f)
#define D_DDPG_ACTION_NOISE_DEVIATION	ndReal (0.03125f)

template<ndInt32 statesDim, ndInt32 actionDim>
class ndBrainAgentDDPG_Trainer : public ndBrainAgent
{
	public:
	ndBrainAgentDDPG_Trainer(const ndSharedPtr<ndBrain>& actor, const ndSharedPtr<ndBrain>& critic);
	virtual ~ndBrainAgentDDPG_Trainer();

	ndReal GetCurrentValue() const;
	ndInt32 GetFramesCount() const;
	ndInt32 GetEposideCount() const;
	ndInt32 GetEpisodeFrames() const;

	protected:
	void Step();
	void OptimizeStep();
	void Save(ndBrainSave* const loadSave) const;

	bool IsTerminal() const;
	ndReal GetReward() const;
	ndReal IsSampling() const;

	private:
	void Optimize();
	void BackPropagate();
	void SelectActions();
	void PopulateReplayBuffer();
	void SetBufferSize(ndInt32 size);

	class ndCriticOptimizer: public ndBrainTrainer_old
	{
		public:
		ndCriticOptimizer(ndBrain* const brain)
			:ndBrainTrainer_old(brain)
			,m_truth()
			,m_inputBatch()
			,m_outputBatch()
			,m_actorState()
			,m_actorAction()
			,m_agent(nullptr)
		{
			m_truth.SetCount(1);
			m_outputBatch.SetCount(1);
			m_actorState.SetCount(statesDim);
			m_actorAction.SetCount(actionDim);
			m_inputBatch.SetCount(statesDim + actionDim);
		}

		//virtual void GetGroundTruth(ndInt32 index, ndBrainVector& groundTruth, const ndBrainVector& output) const
		void EvaluateBellmanEquation(ndInt32 index)
		{
			ndAssert(m_truth.GetCount() == m_output.GetCount());
			ndAssert(m_truth.GetCount() == m_outputBatch.GetCount());
			const ndBrainReplayTransitionMemory<ndReal, statesDim, actionDim>& transition = m_agent->m_replayBuffer[index];
			
			for (ndInt32 i = 0; i < statesDim; ++i)
			{
				m_inputBatch[i] = transition.m_state[i];
			}
			for (ndInt32 i = 0; i < actionDim; ++i)
			{
				m_inputBatch[i + statesDim] = transition.m_action[i];
			}
			MakePrediction(m_inputBatch);
			
			if (transition.m_terminalState)
			{
				m_truth[0] = transition.m_reward;
			}
			else
			{
				for (ndInt32 i = 0; i < statesDim; ++i)
				{
					m_inputBatch[i] = transition.m_nextState[i];
					m_actorState[i] = transition.m_nextState[i];
				}
				m_agent->m_targetActor.MakePrediction(m_actorState, m_actorAction);
				for (ndInt32 i = 0; i < actionDim; ++i)
				{
					m_inputBatch[i + statesDim] = m_actorAction[i];
				}
				m_agent->m_targetCritic.MakePrediction(m_inputBatch, m_outputBatch);
				m_truth[0] = transition.m_reward + m_agent->m_gamma * m_outputBatch[0];
			}
			BackPropagate(m_truth);
		}

		virtual void Optimize(ndValidation&, const ndBrainMatrix&, ndReal, ndInt32)
		{
			ndArray<ndInt32>& shuffleBuffer = m_agent->m_replayBuffer.m_shuffleBuffer;

			ClearGradientsAcc();
			for (ndInt32 i = 0; i < m_agent->m_bashBufferSize; ++i)
			{
				ndInt32 index = shuffleBuffer[i];
				//GetGroundTruth(index, truth, m_output);
				EvaluateBellmanEquation(index);
			}
			UpdateWeights(m_agent->m_learnRate, m_agent->m_bashBufferSize);
		}

		ndReal GetQValue() 
		{
			for (ndInt32 i = 0; i < statesDim; ++i)
			{
				m_inputBatch[i] = m_agent->m_state[i];
			}
			for (ndInt32 i = 0; i < actionDim; ++i)
			{
				m_inputBatch[i + statesDim] = m_agent->m_actions[i];
			}
			m_agent->m_critic->MakePrediction(m_inputBatch, m_outputBatch);
			return m_outputBatch[0];
		}

		ndBrainVector m_truth;
		ndBrainVector m_inputBatch;
		ndBrainVector m_outputBatch;
		ndBrainVector m_actorState;
		ndBrainVector m_actorAction;
		ndBrainAgentDDPG_Trainer<statesDim, actionDim>* m_agent;
	};

	class ndActorOptimizer : public ndBrainTrainer_old
	{
		public:
		ndActorOptimizer(ndBrain* const brain)
			:ndBrainTrainer_old(brain)
			,m_truth()
			,m_inputBatch()
			,m_outputBatch()
			,m_agent(nullptr)
		{
			m_truth.SetCount(actionDim);
			m_inputBatch.SetCount(statesDim);
			m_outputBatch.SetCount(actionDim);
		}

		//virtual void GetGroundTruth(ndInt32 index, ndBrainVector& groundTruth, const ndBrainVector& output) const
		void EvaluateBellmanEquation(ndInt32 index)
		{
			ndAssert(m_truth.GetCount() == m_output.GetCount());
			ndAssert(m_truth.GetCount() == m_outputBatch.GetCount());
			const ndBrainReplayTransitionMemory<ndReal, statesDim, actionDim>& transition = m_agent->m_replayBuffer[index];

			for (ndInt32 i = 0; i < statesDim; ++i)
			{
				m_inputBatch[i] = transition.m_state[i];
			}
			MakePrediction(m_inputBatch, m_truth);

			for (ndInt32 i = 0; i < statesDim; ++i)
			{
				m_agent->m_criticOptimizer.m_inputBatch[i] = transition.m_state[i];
			}
			for (ndInt32 i = 0; i < actionDim; ++i)
			{
				m_agent->m_criticOptimizer.m_inputBatch[i + statesDim] = m_truth[i];
			}
			m_agent->m_targetCritic.MakePrediction(m_agent->m_criticOptimizer.m_inputBatch, m_agent->m_criticOptimizer.m_outputBatch);
			m_agent->m_critic->CalculateInpuGradients(m_agent->m_criticOptimizer.m_inputBatch, m_agent->m_criticOptimizer.m_outputBatch, m_agent->m_criticOptimizer.m_inputBatch);
			for (ndInt32 i = 0; i < actionDim; ++i)
			{
				m_truth[i] -= m_agent->m_criticOptimizer.m_inputBatch[i + statesDim];
			}
			BackPropagate(m_truth);
		}

		virtual void Optimize(ndValidation&, const ndBrainMatrix&, ndReal, ndInt32)
		{
			ndArray<ndInt32>& shuffleBuffer = m_agent->m_replayBuffer.m_shuffleBuffer;
			ClearGradientsAcc();
			for (ndInt32 i = 0; i < m_agent->m_bashBufferSize; ++i)
			{
				ndInt32 index = shuffleBuffer[i];
				//GetGroundTruth(index, truth, m_output);
				EvaluateBellmanEquation(index);
			}
			UpdateWeights(m_agent->m_targetLearnRate, m_agent->m_bashBufferSize);
		}

		ndBrainVector m_truth;
		ndBrainVector m_inputBatch;
		ndBrainVector m_outputBatch;
		ndBrainAgentDDPG_Trainer<statesDim, actionDim>* m_agent;
	};

	ndSharedPtr<ndBrain> m_actor;
	ndSharedPtr<ndBrain> m_critic;
	ndBrain m_targetActor;
	ndBrain m_targetCritic;
	ndActorOptimizer m_actorOptimizer;
	ndCriticOptimizer m_criticOptimizer;

	ndBrainVector m_state;
	ndBrainVector m_actions;
	ndArray<ndInt32> m_bashSamples;
	ndBrainReplayBuffer<ndReal, statesDim, actionDim> m_replayBuffer;
	ndBrainReplayTransitionMemory<ndReal, statesDim, actionDim> m_currentTransition;

	ndReal m_gamma;
	ndReal m_learnRate;
	ndReal m_currentQValue;
	ndReal m_targetLearnRate;
	ndReal m_softTargetFactor;
	ndReal m_actionNoiseDeviation;
	ndInt32 m_frameCount;
	ndInt32 m_framesAlive;
	ndInt32 m_eposideCount;
	ndInt32 m_bashBufferSize;
	ndInt32 m_targetUpdatePeriod;
	bool m_collectingSamples;
};

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentDDPG_Trainer<statesDim, actionDim>::ndBrainAgentDDPG_Trainer(const ndSharedPtr<ndBrain>& actor, const ndSharedPtr<ndBrain>& critic)
	:ndBrainAgent()
	,m_actor(actor)
	,m_critic(critic)
	,m_targetActor(*(*m_actor))
	,m_targetCritic(*(*m_critic))
	,m_actorOptimizer(*m_actor)
	,m_criticOptimizer(*m_critic)
	,m_replayBuffer()
	,m_gamma(D_DDPG_DISCOUNT_FACTOR)
	,m_learnRate(D_DDPG_LEARN_RATE)
	,m_currentQValue(ndReal(0.0f))
	,m_targetLearnRate(D_DDPG_TARGET_LEARN_RATE)
	,m_softTargetFactor(D_DDPG_SOFT_TARGET_FACTOR)
	,m_actionNoiseDeviation(D_DDPG_ACTION_NOISE_DEVIATION)
	,m_frameCount(0)
	,m_framesAlive(0)
	,m_eposideCount(0)
	,m_bashBufferSize(D_DDPG_REPLAY_BASH_SIZE)
	,m_targetUpdatePeriod(D_DDPG_TARGET_UPDATE_PERIOD)
	,m_collectingSamples(true)
{
	ndAssert(m_critic->GetOutputSize() == 1);
	ndAssert(((*(*m_actor))[m_actor->GetCount() - 1])->GetActivationType() == m_tanh);
	ndAssert(m_critic->GetInputSize() == (m_actor->GetInputSize() + m_actor->GetOutputSize()));

	m_state.SetCount(statesDim);
	m_actions.SetCount(actionDim);
	m_state.Set(ndReal(0.0f));
	m_actions.Set(ndReal(0.0f));

	m_actorOptimizer.m_agent = this;
	m_criticOptimizer.m_agent = this;
	m_actorOptimizer.SetRegularizer(D_DDPG_REGULARIZER);
	m_criticOptimizer.SetRegularizer(D_DDPG_REGULARIZER);

	SetBufferSize(D_DDPG_REPLAY_BUFFERSIZE);
	m_targetActor.CopyFrom(*(*m_actor));
	m_targetCritic.CopyFrom(*(*m_critic));
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentDDPG_Trainer<statesDim, actionDim>::~ndBrainAgentDDPG_Trainer()
{
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndReal ndBrainAgentDDPG_Trainer<statesDim, actionDim>::GetCurrentValue() const
{
	return m_currentQValue;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndReal ndBrainAgentDDPG_Trainer<statesDim, actionDim>::IsSampling() const
{
	return m_collectingSamples;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDDPG_Trainer<statesDim, actionDim>::GetFramesCount() const
{
	return m_frameCount;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDDPG_Trainer<statesDim, actionDim>::GetEposideCount() const
{
	return m_eposideCount;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDDPG_Trainer<statesDim, actionDim>::GetEpisodeFrames() const
{
	return m_framesAlive;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDDPG_Trainer<statesDim, actionDim>::SetBufferSize(ndInt32 size)
{
	m_replayBuffer.SetSize(size);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDDPG_Trainer<statesDim, actionDim>::BackPropagate()
{
	class ndTestValidator : public ndBrainTrainer_old::ndValidation
	{
		public:
		ndTestValidator(ndBrainTrainer_old& trainer)
			:ndBrainTrainer_old::ndValidation(trainer)
		{
		}

		ndReal Validate(const ndBrainMatrix&)
		{
			return ndReal(1.0f);
		}
	};

	m_replayBuffer.m_shuffleBuffer.RandomShuffle(m_replayBuffer.m_shuffleBuffer.GetCount());

	ndBrainMatrix inputBatch;
	ndTestValidator criticValidator(m_criticOptimizer);
	m_criticOptimizer.Optimize(criticValidator, inputBatch, m_learnRate, 1);

	ndTestValidator actorValidator(m_actorOptimizer);
	m_actorOptimizer.Optimize(actorValidator, inputBatch, m_learnRate, 1);

	m_targetActor.SoftCopy(*(*m_actor), m_softTargetFactor);
	m_targetCritic.SoftCopy(*(*m_critic), m_softTargetFactor);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDDPG_Trainer<statesDim, actionDim>::Save(ndBrainSave* const loadSave) const
{
	loadSave->Save(*m_actor);
}

template<ndInt32 statesDim, ndInt32 actionDim>
bool ndBrainAgentDDPG_Trainer<statesDim, actionDim>::IsTerminal() const
{
	ndAssert(0);
	return false;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndReal ndBrainAgentDDPG_Trainer<statesDim, actionDim>::GetReward() const
{
	ndAssert(0);
	return ndReal(0.0f);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDDPG_Trainer<statesDim, actionDim>::SelectActions()
{
	for (ndInt32 i = 0; i < actionDim; ++i)
	{
		ndReal noisyAction = ndGaussianRandom(ndFloat32(m_actions[i]), ndFloat32(m_actionNoiseDeviation));
		m_actions[i] = ndClamp(noisyAction, ndReal(-1.0f), ndReal(1.0f));
	}
	m_currentQValue = m_criticOptimizer.GetQValue();
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDDPG_Trainer<statesDim, actionDim>::Step()
{
	GetObservation(&m_state[0]);
	m_actor->MakePrediction(m_state, m_actions);

	SelectActions();
	ApplyActions(&m_actions[0]);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDDPG_Trainer<statesDim, actionDim>::OptimizeStep()
{
	if (!m_frameCount)
	{
		ndBrainAgentDDPG_Trainer<statesDim, actionDim>::m_state.Set(ndReal(0.0f));
		ndBrainAgentDDPG_Trainer<statesDim, actionDim>::m_actions.Set(ndReal(0.0f));
		ResetModel();
		m_currentTransition.Clear();
	}


	PopulateReplayBuffer();
	if (m_replayBuffer.GetCount() == m_replayBuffer.GetCapacity())
	{
		Optimize();
	}

	if (m_currentTransition.m_terminalState)
	{
		ndBrainAgentDDPG_Trainer<statesDim, actionDim>::m_state.Set(ndReal(0.0f));
		ndBrainAgentDDPG_Trainer<statesDim, actionDim>::m_actions.Set(ndReal(0.0f));
		ResetModel();
		m_currentTransition.Clear();
		if (IsSampling() && (m_eposideCount % 100 == 0))
		{
			ndExpandTraceMessage("collecting samples: frame %d out of %d, episode %d \n", m_frameCount, m_replayBuffer.GetCapacity(), m_eposideCount);
		}
		m_eposideCount++;
		m_framesAlive = 0;
	}

	m_frameCount++;
	m_framesAlive++;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDDPG_Trainer<statesDim, actionDim>::PopulateReplayBuffer()
{
	GetObservation(&m_currentTransition.m_nextState[0]);
	m_currentTransition.m_reward = GetReward();
	m_currentTransition.m_terminalState = IsTerminal();
	for (ndInt32 i = 0; i < statesDim; ++i)
	{
		m_currentTransition.m_state[i] = ndBrainAgentDDPG_Trainer<statesDim, actionDim>::m_state[i];
	}

	m_replayBuffer.AddTransition(m_currentTransition);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDDPG_Trainer<statesDim, actionDim>::Optimize()
{
	BackPropagate();
	if (IsSampling())
	{
		ndExpandTraceMessage("%d star training: episode %d\n", m_frameCount, m_eposideCount);
	}
	m_collectingSamples = false;
}

#endif 