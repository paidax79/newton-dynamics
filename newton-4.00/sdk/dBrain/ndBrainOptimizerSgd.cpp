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

#include "ndBrainStdafx.h"
#include "ndBrain.h"
#include "ndBrainMatrix.h"
#include "ndBrainTrainer.h"
#include "ndBrainOptimizerSgd.h"

ndBrainOptimizerSgd::ndBrainOptimizerSgd()
	:ndBrainOptimizer()
{
}

ndBrainOptimizerSgd::~ndBrainOptimizerSgd()
{
}

void ndBrainOptimizerSgd::Update(ndBrainThreadPool* const threadPool, ndArray<ndBrainTrainer*>& partialGradients, ndBrainFloat learnRate)
{
	ndAssert(0);
	//ndBrainFloat regularizer = -GetRegularizer();
	//ndBrainFloat denScale = -learnRate / ndBrainFloat(bashSize);
	//
	//ndBrain* const brian = m_trainer->GetBrain();
	//for (ndInt32 i = 0; i < brian->GetCount(); ++i)
	//{
	//	ndBrainLayer* const layer = (*brian)[i];
	//	if (layer->HasParameters())
	//	{
	//		ndBrainVector& bias = *m_trainer->GetBias(i);
	//		ndBrainMatrix& weight = *m_trainer->GetWeight(i);
	//		ndBrainVector& biasGradients = *m_trainer->GetBiasGradients(i);
	//		ndBrainMatrix& weightGradients = *m_trainer->GetWeightGradients(i);
	//
	//		//biasGradients.Scale(den);
	//		//biasGradients.Scale(learnRate);
	//		biasGradients.Scale(denScale);
	//		biasGradients.ScaleAdd(bias, regularizer);
	//		bias.Add(biasGradients);
	//		bias.FlushToZero();
	//
	//		//weightGradients.Scale(den);
	//		//weightGradients.Scale(learnRate);
	//		weightGradients.Scale(denScale);
	//		weightGradients.ScaleAdd(weight, regularizer);
	//		weight.Add(weightGradients);
	//		weight.FlushToZero();
	//	}
	//}
}