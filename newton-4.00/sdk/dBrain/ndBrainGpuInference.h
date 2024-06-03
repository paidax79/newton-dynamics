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
#ifndef __ND_BRAIN_GPU_INFERENCE_H__
#define __ND_BRAIN_GPU_INFERENCE_H__

#include "ndBrainStdafx.h"
#include "ndBrainVector.h"

class ndBrain;
class ndBrainGpuContext;
//class ndBrainGpuBufferBase;
class ndBrainGpuFloatBuffer;
class ndBrainGpuIntegerBuffer;

#if !defined (D_USE_VULKAN_SDK)

class ndBrainGpuInference : public ndClassAlloc
{
	public:
	ndBrainGpuInference(ndBrainGpuContext* const, ndBrain* const, const ndBrainMatrix&, ndInt32) {}
	virtual ~ndBrainGpuInference() {}
};

#else

class ndBrainGpuInference : public ndClassAlloc
{
	public:
	ndBrainGpuInference(ndBrainGpuContext* const context, ndBrain* const brain, const ndBrainMatrix& testDigits, ndInt32 inputBatchSize);
	virtual ~ndBrainGpuInference();
	
	protected:
	void SetWorkingBuffer();
	void SetParameterVector();
	void LoadInput(const ndBrainMatrix& input);

	ndBrain* m_brain;
	ndBrainGpuContext* m_context;

	ndBrainGpuFloatBuffer* m_input;
	ndBrainGpuFloatBuffer* m_gpuParameters;
	ndBrainGpuFloatBuffer* m_gpuWorkingBuffer;
	ndBrainGpuIntegerBuffer* m_gpuParametersOffsets;
	

	ndBrainVector m_parameters;
	ndArray<ndInt32> m_offsets;

	ndInt32 m_inputBatchSize;
};


#endif

#endif