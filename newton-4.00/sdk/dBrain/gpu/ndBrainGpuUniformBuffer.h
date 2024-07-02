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
#ifndef __ND_BRAIN_GPU_UNIFORM_BUFFER_H__
#define __ND_BRAIN_GPU_UNIFORM_BUFFER_H__

#include "ndBrainGpuBuffer.h"


class ndBrainGpuUniformBuffer : public ndBrainGpuBuffer
{
	public:
	ndBrainGpuUniformBuffer(ndBrainGpuContext* const context, ndInt32 sizeInBytes);
	ndBrainGpuUniformBuffer(ndBrainGpuContext* const context, ndInt32 sizeInBytes, const void* const data);

	//VkDescriptorType GetType() const;
	void LoadData(ndInt32 sizeInBytes, const void* const data);
};

#endif