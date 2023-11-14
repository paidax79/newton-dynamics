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

#include "ndSandboxStdafx.h"
#include "ndTestDeepBrain.h"

static void LoadTrainingData(ndSharedPtr<ndBrainMatrix>& trainingImages, ndSharedPtr<ndBrainMatrix>& trainingLabels)
{
	ndInt32 batches = 5;
	trainingLabels = new ndBrainMatrix(ndInt32(batches * 10000), ndInt32(10));
	trainingImages = new ndBrainMatrix(ndInt32(batches * 10000), ndInt32(32 * 32 * 3));

	ndBrainMatrix& labelMatrix = *(*trainingLabels);
	ndBrainMatrix& imageMatrix = *(*trainingImages);

	char filename[1024];
	char outPathName[1024];
	ndUnsigned8 data[32 * 32 * 3];

	labelMatrix.Set(ndBrainFloat(0.0f));
	for (ndInt32 i = 0; i < batches; ++i)
	{
		sprintf(filename, "cifar-10-batches-bin/data_batch_%d.bin", i + 1);
		ndGetWorkingFileName(filename, outPathName);
		FILE* const fp = fopen(outPathName, "rb");
		if (fp)
		{
			for (ndInt32 j = 0; j < 10000; ++j)
			{
				size_t ret = 0;
				ndInt32 label = 0;
				ret = fread(&label, 1, 1, fp);

				labelMatrix[j][label] = ndBrainFloat(1.0f);
				ndBrainVector& image = imageMatrix[ndInt32(j)];
				ret = fread(data, 1, 32 * 32 * 3, fp);
				for (ndInt32 k = 0; k < 32 * 32 * 3; ++k)
				{
					image[k] = ndBrainFloat(data[k]) / ndBrainFloat(255.0f);
				}

				for (ndInt32 k = 0; k < 3; ++k)
				{
					ndBrainMemVector imageChannel (&image[k * 32 * 32], 32 * 32);
					imageChannel.GaussianNormalize();
				}
			}
			fclose(fp);
		}
	}
}

static void LoadTestData(ndSharedPtr<ndBrainMatrix>& images, ndSharedPtr<ndBrainMatrix>& labels)
{
	labels = new ndBrainMatrix(ndInt32(10000), ndInt32(10));
	images = new ndBrainMatrix(ndInt32(10000), ndInt32(32 * 32 * 3));

	ndBrainMatrix& labelMatrix = *(*labels);
	ndBrainMatrix& imageMatrix = *(*images);

	char outPathName[1024];
	ndUnsigned8 data[32 * 32 * 3];

	labelMatrix.Set(ndBrainFloat(0.0f));
	ndGetWorkingFileName("cifar-10-batches-bin/test_batch.bin", outPathName);
	FILE* const fp = fopen(outPathName, "rb");
	if (fp)
	{
		for (ndInt32 j = 0; j < 10000; ++j)
		{
			size_t ret = 0;
			ndInt32 label = 0;
			ret = fread(&label, 1, 1, fp);

			labelMatrix[j][label] = ndBrainFloat(1.0f);
			ndBrainVector& image = imageMatrix[ndInt32(j)];
			ret = fread(data, 1, 32 * 32 * 3, fp);
			for (ndInt32 k = 0; k < 32 * 32 * 3; ++k)
			{
				image[k] = ndBrainFloat(data[k]) / ndBrainFloat(255.0f);
			}

			for (ndInt32 k = 0; k < 3; ++k)
			{
				ndBrainMemVector imageChannel(&image[k * 32 * 32], 32 * 32);
				imageChannel.GaussianNormalize();
			}
		}
		fclose(fp);
	}
}

static void ValidateData(const char* const title, ndBrain& brain, ndBrainMatrix* const testLabels, ndBrainMatrix* const testDigits)
{
	ndBrainVector output;
	output.SetCount((*testLabels)[0].GetCount());

	ndInt32 failCount = 0;
	ndBrainVector workingBuffer;
	for (ndInt32 i = 0; i < testDigits->GetCount(); i++)
	{
		const ndBrainVector& input = (*testDigits)[i];
		brain.MakePrediction(input, output, workingBuffer);

		const ndBrainVector& truth = (*testLabels)[i];

		ndInt32 index = -1;
		ndBrainFloat maxProbability = -1.0f;
		for (ndInt32 j = 0; j < output.GetCount(); j++)
		{
			if (output[j] > maxProbability)
			{
				index = j;
				maxProbability = output[j];
			}
		}

		ndAssert(index >= 0);
		//if (truth[index] < 0.5f)
		if (truth[index] == ndReal(0.0f))
		{
			failCount++;
		}
	}
	ndExpandTraceMessage("%s\n", title);
	ndExpandTraceMessage("num_right: %d  out of %d\n", testDigits->GetCount() - failCount, testDigits->GetCount());
	ndExpandTraceMessage("num_wrong: %d  out of %d\n", failCount, testDigits->GetCount());
	ndExpandTraceMessage("success rate %f%%\n", (ndFloat32)(testDigits->GetCount() - failCount) * 100.0f / (ndFloat32)testDigits->GetCount());
}

static void Cifar10TrainingSet()
{
	#define BASH_BUFFER_SIZE	64

	ndSharedPtr<ndBrainMatrix> testLabels;
	ndSharedPtr<ndBrainMatrix> testImages;
	ndSharedPtr<ndBrainMatrix> trainingLabels;
	ndSharedPtr<ndBrainMatrix> trainingImages;

	LoadTestData(testImages, testLabels);
	LoadTrainingData(trainingImages, trainingLabels);

	class SupervisedTrainer : public ndBrainThreadPool
	{
		public:
		SupervisedTrainer(ndBrain* const brain)
			:ndBrainThreadPool()
			,m_brain(*brain)
			,m_prioritySamples()
			,m_learnRate(ndReal(1.0e-4f))
			,m_bashBufferSize(BASH_BUFFER_SIZE)
		{
			ndInt32 threadCount = ndMin(ndBrainThreadPool::GetMaxThreads(), ndMin(m_bashBufferSize, 16));
	//threadCount = 1;
			SetThreadCount(threadCount);
			for (ndInt32 i = 0; i < m_bashBufferSize; ++i)
			{
				ndBrainTrainer* const trainer = new ndBrainTrainer(&m_brain);
				m_trainers.PushBack(trainer);
			}
			m_prioritySamples.SetCount(m_bashBufferSize);
		}

		~SupervisedTrainer()
		{
			for (ndInt32 i = 0; i < m_trainers.GetCount(); ++i)
			{
				delete m_trainers[i];
			}
		}

		void Optimize(ndBrainMatrix* const trainingLabels, ndBrainMatrix* const trainingImages,
					  ndBrainMatrix* const testLabels, ndBrainMatrix* const testDigits)
		{
			ndUnsigned32 miniBashArray[64];
			ndUnsigned32 failCount[D_MAX_THREADS_COUNT];

			auto BackPropagateBash = ndMakeObject::ndFunction([this, trainingImages, trainingLabels, &miniBashArray, &failCount](ndInt32 threadIndex, ndInt32 threadCount)
			{
				class CategoricalLoss : public ndBrainLossCategoricalCrossEntropy
				{
					public:
					CategoricalLoss(ndInt32 size, ndUnsigned32* const failCount, ndFixSizeArray<ndUnsigned32, 16>& priority, ndUnsigned32 entry)
						:ndBrainLossCategoricalCrossEntropy(size)
						,m_entry(entry)
						,m_failCount(failCount)
						,m_priority(priority)
					{
					}

					void GetLoss(const ndBrainVector& output, ndBrainVector& loss)
					{
						ndInt32 index = -1;
						ndBrainFloat maxProbability = ndBrainFloat(-1.0f);
						for (ndInt32 j = 0; j < output.GetCount(); j++)
						{
							if (output[j] > maxProbability)
							{
								index = j;
								maxProbability = output[j];
							}
						}

						ndAssert(index >= 0);
						if (m_truth[index] == ndReal(0.0f))
						{
							(*m_failCount)++;
							if (m_priority.GetCount() < m_priority.GetCapacity())
							{
								m_priority.PushBack(m_entry);
							}
						}

						ndBrainLossCategoricalCrossEntropy::GetLoss(output, loss);
					}

					ndUnsigned32 m_entry;
					ndUnsigned32* m_failCount;
					ndFixSizeArray<ndUnsigned32, 16>& m_priority;
				};

				const ndStartEnd startEnd(m_bashBufferSize, threadIndex, threadCount);
				for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
				{
					ndBrainTrainer& trainer = *m_trainers[i];
					ndUnsigned32 index = miniBashArray[i];
					CategoricalLoss loss(m_brain.GetOutputSize(), &failCount[threadIndex], m_prioritySamples[i], index);

					loss.SetTruth((*trainingLabels)[ndInt32(index)]);
					trainer.BackPropagate((*trainingImages)[ndInt32(index)], loss);
				}
			});

			ndBrain bestBrain(m_brain);
			ndBrainOptimizerAdam optimizer;

			ndInt32 minTestFail = testLabels->GetCount();
			ndInt32 minTrainingFail = trainingLabels->GetCount();
			ndInt32 batches = trainingLabels->GetCount() / m_bashBufferSize;

			// so far best training result on the cifar-10 data set
			//optimizer.SetRegularizer(ndBrainFloat(0.0e-5f));	// test data score 
			optimizer.SetRegularizer(ndBrainFloat(1.0e-5f));	// test data score 
			//optimizer.SetRegularizer(ndBrainFloat(2.0e-5f));	// test data score 
			//optimizer.SetRegularizer(ndBrainFloat(3.0e-5f));	// test data score 

			//batches = 1;
			ndArray<ndUnsigned32> shuffleBuffer;
			ndFixSizeArray<ndUnsigned32, 1024 * 2> priorityList;

			for (ndInt32 i = 0; i < trainingLabels->GetCount(); ++i)
			{
				shuffleBuffer.PushBack(ndUnsigned32(i));
			}
			for (ndInt32 i = 0; i < priorityList.GetCapacity(); ++i)
			{
				priorityList.PushBack(ndRandInt() % trainingLabels->GetCount());
			}

			for (ndInt32 epoch = 0; epoch < 1000; ++epoch)
			{
				ndInt32 start = 0;
				ndMemSet(failCount, ndUnsigned32(0), D_MAX_THREADS_COUNT);

				for (ndInt32 bash = 0; bash < batches; ++bash)
				{
					ndMemCpy(miniBashArray, &shuffleBuffer[start], m_bashBufferSize);

					ndBrainThreadPool::ParallelExecute(BackPropagateBash);
					optimizer.Update(this, m_trainers, m_learnRate);

					start += m_bashBufferSize;
				}

				ndInt32 fails = 0;
				for (ndInt32 i = 0; i < GetThreadCount(); ++i)
				{
					fails += failCount[i];
				}

				if (fails <= minTrainingFail)
				{
					ndInt32 actualTraining = fails;
					bool traningTest = fails < minTrainingFail;
					minTrainingFail = ndMax(fails, ndInt32(200));

					auto CrossValidateTest = ndMakeObject::ndFunction([this, testDigits, testLabels, &failCount](ndInt32 threadIndex, ndInt32 threadCount)
					{
						ndBrainFloat outputBuffer[32];
						ndBrainMemVector output(outputBuffer, m_brain.GetOutputSize());

						failCount[threadIndex] = 0;
						const ndStartEnd startEnd(testLabels->GetCount(), threadIndex, threadCount);
						for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
						{
							const ndBrainVector& truth = (*testLabels)[i];
							const ndBrainVector& input = (*testDigits)[i];
							m_brain.MakePrediction(input, output, m_trainers[threadIndex]->GetWorkingBuffer());

							ndInt32 index = -1;
							ndBrainFloat maxProbability = ndBrainFloat(-1.0f);
							for (ndInt32 j = 0; j < output.GetCount(); j++)
							{
								if (output[j] > maxProbability)
								{
									index = j;
									maxProbability = output[j];
								}
							}
							if (truth[index] == ndReal(0.0f))
							{
								failCount[threadIndex]++;
							}
						}
					});

					ndBrainThreadPool::ParallelExecute(CrossValidateTest);

					fails = 0;
					for (ndInt32 j = 0; j < GetThreadCount(); ++j)
					{
						fails += failCount[j];
					}

					if (traningTest && (minTrainingFail > 200))
					{
						minTestFail = fails;
						bestBrain.CopyFrom(m_brain);
						ndInt32 size = batches * m_bashBufferSize;
						ndExpandTraceMessage("success rate: %f%%   ", (ndFloat32)(size - actualTraining) * 100.0f / (ndFloat32)size);
						ndExpandTraceMessage("failed count: %d   ", actualTraining);
						ndExpandTraceMessage("epoch: %d", epoch);
						ndExpandTraceMessage("\n");
					}
					else if (fails <= minTestFail)
					{
						minTestFail = fails;
						bestBrain.CopyFrom(m_brain);
						ndInt32 size = batches * m_bashBufferSize;
						ndExpandTraceMessage("success rate: %f%%   ", (ndFloat32)(size - actualTraining) * 100.0f / (ndFloat32)size);
						ndExpandTraceMessage("failed count: %d   ", actualTraining);
						ndExpandTraceMessage("epoch: %d", epoch);
						ndExpandTraceMessage(" %d\n", minTestFail);
					}
				}

				for (ndInt32 i = 0; i < m_prioritySamples.GetCount(); ++i)
				{
					ndFixSizeArray<ndUnsigned32, 16>& priority = m_prioritySamples[i];
					for (ndInt32 j = 0; j < priority.GetCount(); ++j)
					{
						priorityList.PushBack(priority[j]);
					}
					priority.SetCount(0);
				}

				shuffleBuffer.RandomShuffle(shuffleBuffer.GetCount());
			}
			m_brain.CopyFrom(bestBrain);
		}

		ndBrain& m_brain;
		ndArray<ndBrainTrainer*> m_trainers;
		ndFixSizeArray<ndFixSizeArray<ndUnsigned32, 16>, BASH_BUFFER_SIZE> m_prioritySamples;
		ndReal m_learnRate;
		ndInt32 m_bashBufferSize;
	};
	
	if (trainingLabels && trainingImages)
	{
		ndBrain brain;
		ndFixSizeArray<ndBrainLayer*, 32> layers;
		
		ndInt32 height = 32;
		ndInt32 width = trainingImages->GetColumns() / (height * 3);
		ndAssert((3 * height * width) == trainingImages->GetColumns());
	
		const ndBrainLayerConvolutional_2d* conv;
		const ndBrainLayerConvolutionalMaxPooling_2d* pooling;
	
		#if 0
			#define ACTIVATION_TYPE	ndBrainLayerReluActivation
		#else
			#define ACTIVATION_TYPE	ndBrainLayerTanhActivation
		#endif
	
		layers.PushBack(new ndBrainLayerConvolutional_2d(width, height, 3, 5, 32));
		conv = (ndBrainLayerConvolutional_2d*)(layers[layers.GetCount() - 1]);
		layers.PushBack(new ACTIVATION_TYPE(conv->GetOutputSize()));
		layers.PushBack(new ndBrainLayerConvolutionalMaxPooling_2d(conv->GetOutputWidth(), conv->GetOutputHeight(), conv->GetOutputChannels()));
		pooling = (ndBrainLayerConvolutionalMaxPooling_2d*)(layers[layers.GetCount() - 1]);
	
		layers.PushBack(new ndBrainLayerConvolutional_2d(pooling->GetOutputWidth(), pooling->GetOutputHeight(), pooling->GetOutputChannels(), 3, 64));
		conv = (ndBrainLayerConvolutional_2d*)(layers[layers.GetCount() - 1]);
		layers.PushBack(new ACTIVATION_TYPE(conv->GetOutputSize()));
		layers.PushBack(new ndBrainLayerConvolutionalMaxPooling_2d(conv->GetOutputWidth(), conv->GetOutputHeight(), conv->GetOutputChannels()));
		pooling = (ndBrainLayerConvolutionalMaxPooling_2d*)(layers[layers.GetCount() - 1]);
	
		layers.PushBack(new ndBrainLayerConvolutional_2d(pooling->GetOutputWidth(), pooling->GetOutputHeight(), pooling->GetOutputChannels(), 3, 64));
		conv = (ndBrainLayerConvolutional_2d*)(layers[layers.GetCount() - 1]);
		layers.PushBack(new ACTIVATION_TYPE(conv->GetOutputSize()));

		//layers.PushBack(new ndBrainLayerConvolutional_2d(conv->GetOutputWidth(), conv->GetOutputHeight(), conv->GetOutputChannels(), 3, 64));
		//conv = (ndBrainLayerConvolutional_2d*)(layers[layers.GetCount() - 1]);
		//layers.PushBack(new ACTIVATION_TYPE(conv->GetOutputSize()));

		ndInt32 neuronsPerLayers = 128;
		layers.PushBack(new ndBrainLayerLinear(layers[layers.GetCount() - 1]->GetOutputSize(), neuronsPerLayers));
		layers.PushBack(new ACTIVATION_TYPE(layers[layers.GetCount() - 1]->GetOutputSize()));

		layers.PushBack(new ndBrainLayerLinear(layers[layers.GetCount() - 1]->GetOutputSize(), neuronsPerLayers));
		layers.PushBack(new ACTIVATION_TYPE(layers[layers.GetCount() - 1]->GetOutputSize()));

		layers.PushBack(new ndBrainLayerLinear(layers[layers.GetCount() - 1]->GetOutputSize(), trainingLabels->GetColumns()));
		layers.PushBack(new ndBrainLayerCategoricalSoftmaxActivation(layers[layers.GetCount() - 1]->GetOutputSize()));
	
		for (ndInt32 i = 0; i < layers.GetCount(); ++i)
		{
			brain.AddLayer(layers[i]);
		}
		brain.InitWeightsXavierMethod();
	
		SupervisedTrainer optimizer(&brain);
		ndUnsigned64 time = ndGetTimeInMicroseconds();
		optimizer.Optimize(*trainingLabels, *trainingImages, *testLabels, *testImages);
		time = ndGetTimeInMicroseconds() - time;
	
		char path[256];
		ndGetWorkingFileName("cifar-10-batches-bin/cifar-cnn-dnn", path);
		
		ndBrainSave::Save(&brain, path);
		ValidateData("training data", brain, *trainingLabels, *trainingImages);
		ndExpandTraceMessage("time %f (sec)\n\n", ndFloat64(time) / 1000000.0f);
	}
}

static void Cifar10TestSet()
{
	ndAssert(0);
	//ndSharedPtr<ndBrainMatrix> testLabels (LoadMnistLabelData("cifar-10-batches-bin/t10k-labels.idx1-ubyte"));
	//ndSharedPtr<ndBrainMatrix> testDigits (LoadMnistSampleData("cifar-10-batches-bin/t10k-images.idx3-ubyte"));
	//
	//if (testLabels && testDigits)
	//{
	//	char path[256];
	//	ndGetWorkingFileName("cifar-10-batches-bin/cifar-cnn-dnn", path);
	//
	//	ndSharedPtr<ndBrain> brain (ndBrainLoad::Load(path));
	//	ndUnsigned64 time = ndGetTimeInMicroseconds();
	//	ValidateData("test data", *(*brain), *testLabels, *testDigits);
	//	time = ndGetTimeInMicroseconds() - time;
	//	ndExpandTraceMessage("time %f (sec)\n\n", ndFloat64(time) / 1000000.0f);
	//}
}

void ndCifar10ImageClassification()
{
	ndSetRandSeed(12345);

	Cifar10TrainingSet();
	//Cifar10TestSet();
}