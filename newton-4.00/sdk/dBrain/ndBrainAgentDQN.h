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

#ifndef _ND_DQN_BRAIN_AGENT_DQN_H__
#define _ND_DQN_BRAIN_AGENT_DQN_H__

#include "ndBrainStdafx.h"
#include "ndBrain.h"
#include "ndBrainAgent.h"
#include "ndBrainTrainer_old.h"
#include "ndBrainReplayBuffer.h"

template<ndInt32 statesDim, ndInt32 actionDim>
class ndBrainAgentDQN: public ndBrainAgent
{
	public:
	ndBrainAgentDQN(const ndSharedPtr<ndBrain>& actor);

	void Step();

	protected:
	void OptimizeStep();
	void ResetModel() const;
	bool IsTerminal() const;
	ndReal GetReward() const;
	ndReal GetCurrentValue() const;
	ndInt32 GetEpisodeFrames() const;
	ndInt32 SelectBestAction() const;
	void Save(ndBrainSave* const loadSave) const;

	ndSharedPtr<ndBrain> m_actor;
	ndBrainVector m_state;
	ndBrainVector m_actions;
};

template<ndInt32 statesDim, ndInt32 actionDim>
ndBrainAgentDQN<statesDim, actionDim>::ndBrainAgentDQN(const ndSharedPtr<ndBrain>& actor)
	:ndBrainAgent()
	,m_actor(actor)
{
	m_state.SetCount(statesDim);
	m_actions.SetCount(actionDim);
	m_state.Set(ndReal(0.0f));
	m_actions.Set(ndReal(0.0f));
}

template<ndInt32 statesDim, ndInt32 actionDim>
bool ndBrainAgentDQN<statesDim, actionDim>::IsTerminal() const
{
	ndAssert(0);
	return false;
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndReal ndBrainAgentDQN<statesDim, actionDim>::GetReward() const
{
	ndAssert(0);
	return ndReal(0.0f);
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndReal ndBrainAgentDQN<statesDim, actionDim>::GetCurrentValue() const
{
	return ndReal(0.0f);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDQN<statesDim, actionDim>::ResetModel() const
{
	ndAssert(0);
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDQN<statesDim, actionDim>::GetEpisodeFrames() const
{
	ndAssert(0);
	return 0;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDQN<statesDim, actionDim>::Save(ndBrainSave* const) const
{
	ndAssert(0);
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDQN<statesDim, actionDim>::OptimizeStep()
{
}

template<ndInt32 statesDim, ndInt32 actionDim>
ndInt32 ndBrainAgentDQN<statesDim, actionDim>::SelectBestAction() const
{
	ndInt32 bestAction = 0;
	ndReal maxQValue = m_actions[0];
	for (ndInt32 i = 1; i < actionDim; ++i)
	{
		if (m_actions[i] > maxQValue)
		{
			bestAction = i;
			maxQValue = m_actions[i];
		}
	}
	return bestAction;
}

template<ndInt32 statesDim, ndInt32 actionDim>
void ndBrainAgentDQN<statesDim, actionDim>::Step()
{
	GetObservation(&m_state[0]);
	m_actor->MakePrediction(m_state, m_actions);

	ndReal bestAction = ndReal(SelectBestAction());
	ApplyActions(&bestAction);
}

#endif 
