/* Copyright (c) <2003-2022> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/
#include "ndBrainStdafx.h"
#include "ndBrain.h"
#include "ndBrainVector.h"
#include "ndBrainMatrix.h"
#include "ndBrainGpuBuffer.h"
#include "ndBrainGpuCommand.h"
#include "ndBrainGpuContext.h"
#include "ndBrainGpuInference.h"


#if defined (D_USE_VULKAN_SDK)

class ndBrainGpuInference::ndBrainLoadInputData : public ndBrainGpuCommand
{
	public:
	struct UniformBufferObject
	{
		ndInt32 m_batchIndex;
		ndInt32 m_inputSize;
		ndInt32 m_inputBlockSize;
		ndInt32 m_outputBlockSize;
	};

	ndBrainLoadInputData(ndBrainGpuInference* const me, const ndBrainMatrix& input)
		:ndBrainGpuCommand(me->m_context)
		,m_parammeters(me->m_context, sizeof (UniformBufferObject))
	{
		UniformBufferObject uniformParam;
		uniformParam.m_batchIndex = 0;
		uniformParam.m_inputSize = input.GetColumns();
		uniformParam.m_inputBlockSize = me->m_inputBuffer.m_offsets[0];
		uniformParam.m_outputBlockSize = me->m_workingBuffer.m_offsets[me->m_workingBuffer.m_offsets.GetCount() - 1];

		m_parammeters.LoadData(sizeof (uniformParam), &uniformParam);

		ndBrainGpuBuffer* params[3];
		params[0] = &m_parammeters;
		params[1] = me->m_inputBuffer.m_buffer;
		params[2] = me->m_workingBuffer.m_buffer;
		Assembly(me->m_context->m_ndBrainCopyInput, me->m_inputBatchSize, 3, params);
	}

	ndBrainGpuUniformBuffer m_parammeters;
};

class ndBrainGpuInference::ndBrainGetResultData : public ndBrainGpuCommand
{
	public:
	struct UniformBufferObject
	{
		ndInt32 m_batchIndex;
		ndInt32 m_outputSize;
		ndInt32 m_workBufferStart;
		ndInt32 m_workBufferSize;
	};

	ndBrainGetResultData(ndBrainGpuInference* const me)
		:ndBrainGpuCommand(me->m_context)
		,m_parammeters(me->m_context, sizeof(UniformBufferObject))
	{
		UniformBufferObject uniformParam;
		uniformParam.m_batchIndex = 0;
		uniformParam.m_outputSize = me->m_outputBuffer.m_offsets[0];
		uniformParam.m_workBufferStart = me->m_workingBuffer.m_offsets[me->m_workingBuffer.m_offsets.GetCount() - 2];
		uniformParam.m_workBufferSize = me->m_workingBuffer.m_offsets[me->m_workingBuffer.m_offsets.GetCount() - 1];

		m_parammeters.LoadData(sizeof(uniformParam), &uniformParam);
		
		ndBrainGpuBuffer* params[3];
		params[0] = &m_parammeters;
		params[1] = me->m_workingBuffer.m_buffer;
		params[2] = me->m_outputBuffer.m_buffer;
		Assembly(me->m_context->m_ndBrainGetResults, me->m_inputBatchSize, 3, params);
	}

	ndBrainGpuUniformBuffer m_parammeters;
};

ndBrainGpuInference::ndBrainGpuInference(ndBrainGpuContext* const context, ndBrain* const brain, const ndBrainMatrix& input, ndInt32 inputBatchSize)
	:ndClassAlloc()
	,m_brain(brain)
	,m_context(context)
	,m_inputBuffer()
	,m_outputBuffer()
	,m_workingBuffer()
	,m_displayList()
	,m_inputBatchSize(inputBatchSize)
{
	SetInputAndOutputBuffers(input);
	SetWorkingBuffer();
	SetParameterVector();
	
	BuildDisplayList(input);
}

ndBrainGpuInference::~ndBrainGpuInference()
{
}

const ndList<ndSharedPtr<ndBrainGpuCommand>>& ndBrainGpuInference::GetDisplayList() const
{
	return m_displayList;
}

void ndBrainGpuInference::GetResults(ndBrainVector& results) const
{
	m_outputBuffer.m_buffer->UnloadData(results);
}

void ndBrainGpuInference::GetWorkBuffer(ndBrainVector& results) const
{
	m_workingBuffer.m_buffer->UnloadData(results);
}

const ndArray<ndInt32>& ndBrainGpuInference::GetWorkBufferOffsets() const
{
	return m_workingBuffer.m_offsets;
}

void ndBrainGpuInference::SetParameterVector()
{
	m_paramBuffer.m_offsets.SetCount(0);
	ndBrainVector parameters;
	parameters.SetCount(0);
	
	const ndArray<ndBrainLayer*>& layers = *m_brain;
	for (ndInt32 i = 0; i < m_brain->GetCount(); ++i)
	{
		const ndBrainLayer* const layer = layers[i];
		layer->GetNumberOfGPUParameters(parameters, m_paramBuffer.m_offsets);
	}
	
	ndInt32 sum = 0;
	for (ndInt32 i = 0; i < m_paramBuffer.m_offsets.GetCount(); ++i)
	{
		ndInt32 count = m_paramBuffer.m_offsets[i];
		m_paramBuffer.m_offsets[i] = sum;
		sum += count;
	}
	m_paramBuffer.m_offsets.PushBack(sum);
	m_paramBuffer.m_buffer = new ndBrainGpuFloatBuffer(m_context, parameters);
}

void ndBrainGpuInference::SetWorkingBuffer()
{
	ndInt32 rounding = ND_GPU_BUFFER_ALIGNMENT / sizeof(ndBrainFloat);
	ndAssert(!(rounding & (rounding - 1)));
	const ndArray<ndBrainLayer*>& layers = *m_brain;
	
	ndInt32 size = (layers[0]->GetInputSize() + rounding - 1) & -rounding;
	m_workingBuffer.m_offsets.PushBack(size);
	for (ndInt32 i = 0; i < m_brain->GetCount(); ++i)
	{
		size = (layers[i]->GetOutputBufferSize() + rounding - 1) & -rounding;
		m_workingBuffer.m_offsets.PushBack(size);
	}

	ndInt32 sum = 0;
	for (ndInt32 i = 0; i < m_workingBuffer.m_offsets.GetCount(); ++i)
	{
		ndInt32 offset = m_workingBuffer.m_offsets[i];
		m_workingBuffer.m_offsets[i] = sum;
		sum += offset;
	}

	m_workingBuffer.m_offsets.PushBack(sum);
	m_workingBuffer.m_buffer = new ndBrainGpuFloatBuffer(m_context, sum * m_inputBatchSize);
}

void ndBrainGpuInference::SetInputAndOutputBuffers(const ndBrainMatrix& input)
{
	const ndInt32 rounding = ND_GPU_BUFFER_ALIGNMENT / sizeof(ndBrainFloat); 
	const ndInt32 width = (input.GetColumns() + rounding - 1) & -rounding;

	const ndInt32 size = width * input.GetRows();
	ndBrainVector temp;
	temp.SetCount(size);
	for (ndInt32 i = 0; i < input.GetRows(); ++i)
	{
		const ndBrainVector& src = input[i];
		ndBrainMemVector dst(&temp[i * width], src.GetCount());
		dst.Set(src);
	}

	m_inputBuffer.m_offsets.PushBack(width);
	m_inputBuffer.m_buffer = new ndBrainGpuFloatBuffer(m_context, temp);

	const ndBrainLayer* const outputLayer = (*m_brain)[m_brain->GetCount() - 1];
	m_outputBuffer.m_offsets.PushBack(outputLayer->GetOutputSize());
	m_outputBuffer.m_buffer = new ndBrainGpuFloatBuffer(m_context, outputLayer->GetOutputSize() * input.GetRows());
}

void ndBrainGpuInference::BuildDisplayList(const ndBrainMatrix& input)
{
	ndFixSizeArray<ndBrainLayer::ndBufferOffsetPair*, 8> buffers;
	buffers.PushBack(&m_paramBuffer);
	buffers.PushBack(&m_workingBuffer);


	ndInt32 workGroupBatch = 1000;
	ndList<ndSharedPtr<ndBrainGpuCommand>> reusableList;
	const ndArray<ndBrainLayer*>& layers = *m_brain;
	for (ndInt32 i = 0; i < m_brain->GetCount(); ++i)
	{
		ndBrainLayer* const layer = layers[i];
		//reusableList.Append(layer->AssemblyGPUCommand(m_context, i, m_inputBatchSize, buffers));
		reusableList.Append(layer->AssemblyGPUCommand(m_context, i, workGroupBatch, buffers));
	}

	for (ndInt32 i = 0; i < m_inputBatchSize; i += workGroupBatch)
	{
		m_displayList.Append(new ndBrainLoadInputData(this, input));
		for (ndList<ndSharedPtr<ndBrainGpuCommand>>::ndNode* node = reusableList.GetFirst(); node; node = node->GetNext())
		{
			m_displayList.Append(node->GetInfo());
		}
		m_displayList.Append(new ndBrainGetResultData(this));
	}
}
#endif