/**********************************************************************
Copyright ?013 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

?  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
?  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************/


#include "matrix_mult.hpp"

/* Data and buffers */
cl_float a_mat[MATRIX_DIM][MATRIX_DIM], b_mat[MATRIX_DIM][MATRIX_DIM], 
		 c_mat[MATRIX_DIM][MATRIX_DIM], check_mat[MATRIX_DIM][MATRIX_DIM];
		 
int 
main(int argc, char * argv[])
{
    MatrixMultiplication clMatrixMultiplication("OpenCL Matrix Multiplication");
	cl_int i, j, k, err, check;
	
	/* Initialize A, B, and check matrices */
	srand((unsigned int)time(0));
	for(i=0; i<MATRIX_DIM; i++) {
	  for(j=0; j<MATRIX_DIM; j++) {
		 a_mat[i][j] = (cl_float)rand()/RAND_MAX;
	  }
	}
	srand((unsigned int)time(0));
	for(i=0; i<MATRIX_DIM; i++) {
	  for(j=0; j<MATRIX_DIM; j++) {
		 b_mat[i][j] = (cl_float)rand()/RAND_MAX;
		 check_mat[i][j] = 0.0f;
	  }
	}
	for(i=0; i<MATRIX_DIM; i++) {
	  for(j=0; j<MATRIX_DIM; j++) {
		 for(k=0; k<MATRIX_DIM; k++) {
			check_mat[i][j] += a_mat[i][k] * b_mat[k][j];
		 }
	  }
	}
	
    if(clMatrixMultiplication.initialize() != SDK_SUCCESS)
        return SDK_FAILURE;

    if(clMatrixMultiplication.parseCommandLine(argc, argv) != SDK_SUCCESS)
        return SDK_FAILURE;

    if(clMatrixMultiplication.isDumpBinaryEnabled() != SDK_SUCCESS)
    {
        return clMatrixMultiplication.genBinaryImage();
    }
    else
    {
        // Setup
        if(clMatrixMultiplication.setup() != SDK_SUCCESS)
            return SDK_FAILURE;
        // Run
        if(clMatrixMultiplication.run() != SDK_SUCCESS)
            return SDK_FAILURE;
        // Cleanup
        if(clMatrixMultiplication.cleanup() != SDK_SUCCESS)
            return SDK_FAILURE;
        
        clMatrixMultiplication.printStats();
    }

	/* Check result */
	check = 1;
	for(i=0; i<MATRIX_DIM; i++) {
	  for(j=0; j<MATRIX_DIM; j++) {
		 if(fabs(c_mat[i][j] - check_mat[i][j]) > 0.01f) {
			check = 0;
			break;
		 }
	  }
	}
	if(check)
	  printf("Multiplication check succeeded.\n");
	else
	  printf("Multiplication check failed.\n");

	  
    return SDK_SUCCESS;
}

int 
MatrixMultiplication::setupMatrixMultiplication()
{
    return SDK_SUCCESS;
}

/**
 * genBinary Image function is used to when we want to create the binary 
 * for the kernel and run it at a later time. This is useful where offline  
 * compilation is the preferred mechanism. 
 */
int 
MatrixMultiplication::genBinaryImage()
{
    streamsdk::bifData binaryData;
    binaryData.kernelName = std::string("matrix_mult_Kernel.cl");
    binaryData.flagsStr = std::string("");
    if(isComplierFlagsSpecified())
        binaryData.flagsFileName = std::string(flags.c_str());

    binaryData.binaryName = std::string(dumpBinary.c_str());
    int status = sampleCommon->generateBinaryImage(binaryData);
    return status;
}

int
MatrixMultiplication::setupCL(void)
{
    cl_int status = 0;
    cl_device_type dType;
    
    if(deviceType.compare("cpu") == 0)
    {
        dType = CL_DEVICE_TYPE_CPU;
    }
    else //deviceType = "gpu" 
    {
        dType = CL_DEVICE_TYPE_GPU;
        if(isThereGPU() == false)
        {
            std::cout << "GPU not found. Falling back to CPU device" << std::endl;
            dType = CL_DEVICE_TYPE_CPU;
        }
    }

    /*
     * Have a look at the available platforms and pick either
     * the AMD one if available or a reasonable default.
     */
    cl_platform_id platform = NULL;
    int retValue = sampleCommon->getPlatform(platform, platformId, isPlatformEnabled());
    CHECK_ERROR(retValue, SDK_SUCCESS, "sampleCommon::getPlatform() failed");
    
    // Display available devices.
    retValue = sampleCommon->displayDevices(platform, dType);
    CHECK_ERROR(retValue, SDK_SUCCESS, "sampleCommon::displayDevices() failed");

    // creating context
    cl_context_properties cps[3] = 
    {
        CL_CONTEXT_PLATFORM, 
        (cl_context_properties)platform, 
        0
    };

    context = clCreateContextFromType(
                  cps,
                  dType,
                  NULL,
                  NULL,
                  &status);
    CHECK_OPENCL_ERROR(status, "clCreateContextFromType failed.");

    // getting device on which to run the sample
    status = sampleCommon->getDevices(context, &devices, deviceId, isDeviceIdEnabled());
    CHECK_ERROR(status, SDK_SUCCESS, "sampleCommon::getDevices() failed");
 
    //Set device info of given cl_device_id
    retValue = deviceInfo.setDeviceInfo(devices[deviceId]);
    CHECK_ERROR(retValue, SDK_SUCCESS, "SDKDeviceInfo::setDeviceInfo() failed");

    {
        // The block is to move the declaration of prop closer to its use 
        cl_command_queue_properties prop = 0;
        if(!eAppGFLOPS)
            prop |= CL_QUEUE_PROFILING_ENABLE;

        commandQueue = clCreateCommandQueue(
                           context, 
                           devices[deviceId], 
                           prop, 
                           &status);
        CHECK_OPENCL_ERROR(status, "clCreateCommandQueue failed.");
    }

    // Set Presistent memory only for AMD platform
    cl_mem_flags inMemFlags = CL_MEM_READ_ONLY;
    if(isAmdPlatform())
        inMemFlags |= CL_MEM_USE_PERSISTENT_MEM_AMD;

    // Create buffer for matrix A 
    inputBuffer0 = clCreateBuffer(
                      context, 
                      inMemFlags,
                      sizeof(cl_float) * width0 * height0,
                      0, 
                      &status);
    CHECK_OPENCL_ERROR(status, "clCreateBuffer failed. (inputBuffer0)");

    // Create buffer for matrix B 
    inputBuffer1 = clCreateBuffer(
                      context, 
                      inMemFlags,
                      sizeof(cl_float) * width1 * height1,
                      0, 
                      &status);
    CHECK_OPENCL_ERROR(status, "clCreateBuffer failed. (inputBuffer1)");

    outputBuffer = clCreateBuffer(
                      context, 
                      CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR,
                      sizeof(cl_float) * height0 * width1,
                      0, 
                      &status);
    CHECK_OPENCL_ERROR(status, "clCreateBuffer failed. (outputBuffer)");

    // create a CL program using the kernel source 
    streamsdk::buildProgramData buildData;
    buildData.kernelName = std::string("matrix_mult_Kernel.cl");
    buildData.devices = devices;
    buildData.deviceId = deviceId;
    buildData.flagsStr = std::string("");
    if(isLoadBinaryEnabled())
        buildData.binaryName = std::string(loadBinary.c_str());

    if(isComplierFlagsSpecified())
        buildData.flagsFileName = std::string(flags.c_str());

    retValue = sampleCommon->buildOpenCLProgram(program, context, buildData);
    CHECK_ERROR(retValue, SDK_SUCCESS, "sampleCommon::buildOpenCLProgram() failed");
    
    // If local memory is present then use the specific kernel 
    if(lds)
        kernel = clCreateKernel(program, "mmmKernel_local", &status);
    else
        kernel = clCreateKernel(program, "mmmKernel", &status);
    CHECK_OPENCL_ERROR(status, "clCreateKernel failed.");
    return SDK_SUCCESS;
}

int
MatrixMultiplication::setWorkGroupSize()
{
    /* 
     * Kernel runs over complete output matrix with blocks of blockSize x blockSize 
     * running concurrently
     */
    cl_int status = 0;
    globalThreads[0] = width1 / 4;
    globalThreads[1] = height0/ 4;
    localThreads[0] = blockSize;
    localThreads[1] = blockSize;

    // Setting the KernelWorkGroupInfo values
    status = kernelInfo.setKernelWorkGroupInfo(kernel, devices[deviceId]);
    CHECK_ERROR(status,0, "setKernelWrkGroupInfo failed");
    
    availableLocalMemory = deviceInfo.localMemSize - kernelInfo.localMemoryUsed;
    neededLocalMemory    = 2 * blockSize * blockSize * sizeof(cl_float); 
    if(neededLocalMemory > availableLocalMemory)
    {
        std::cout << "Unsupported: Insufficient local memory on device." << std::endl;
        return SDK_SUCCESS;
    }

    if((cl_uint)(localThreads[0] * localThreads[1]) > kernelInfo.kernelWorkGroupSize)
    {
       if(kernelInfo.kernelWorkGroupSize >= 64)
        {
            blockSize = 8; 
            localThreads[0] = blockSize;
            localThreads[1] = blockSize;
        }
        else if(kernelInfo.kernelWorkGroupSize >= 32)
        {
            blockSize = 4; 
            localThreads[0] = blockSize;
            localThreads[1] = blockSize;
        }
        else
        {
            std::cout << "Out of Resources!" << std::endl;
            std::cout << "Group Size specified : " << localThreads[0] * localThreads[1] << std::endl;
            std::cout << "Max Group Size supported on the kernel : " 
                      << kernelInfo.kernelWorkGroupSize<<std::endl;
            return SDK_FAILURE;
        }
    }

    if(localThreads[0] > deviceInfo.maxWorkItemSizes[0] ||
       localThreads[1] > deviceInfo.maxWorkItemSizes[1] ||
       localThreads[0] * localThreads[1] > deviceInfo.maxWorkGroupSize)
    {
        std::cout << "Unsupported: Device does not support requested number of work items." << std::endl;
        return SDK_FAILURE;
    }
    return SDK_SUCCESS;
}


int 
MatrixMultiplication::runCLKernels(void)
{
    cl_int   status;
    status = setWorkGroupSize();
    CHECK_ERROR(status, SDK_SUCCESS, "getWorkGroupSize() failed");

    cl_event ndrEvt;
    cl_int eventStatus = CL_QUEUED;

    // Set input data to matrix A and matrix B
    cl_event inMapEvt1, inMapEvt2, inUnmapEvt1, inUnmapEvt2, outMapEvt, outUnmapEvt;
    void* mapPtr1 = clEnqueueMapBuffer(
                        commandQueue, 
                        inputBuffer0, 
                        CL_FALSE, 
                        CL_MAP_WRITE, 
                        0, 
                        width0 * height0 * sizeof(cl_float), 
                        0, 
                        NULL, 
                        &inMapEvt1, 
                        &status);
    CHECK_OPENCL_ERROR(status, "clEnqueueMapBuffer failed. (inputBuffer0)");

    void* mapPtr2 = clEnqueueMapBuffer(
                        commandQueue, 
                        inputBuffer1, 
                        CL_FALSE, 
                        CL_MAP_WRITE, 
                        0, 
                        width1 * height1 * sizeof(cl_float), 
                        0, 
                        NULL, 
                        &inMapEvt2, 
                        &status);
    CHECK_OPENCL_ERROR(status, "clEnqueueMapBuffer failed. (inputBuffer1)");    

    status = clFlush(commandQueue);
    CHECK_OPENCL_ERROR(status, "clFlush failed.");

    status = sampleCommon->waitForEventAndRelease(&inMapEvt1);
    CHECK_ERROR(status,SDK_SUCCESS, "WaitForEventAndRelease(inMapEvt1) Failed");
    memcpy(mapPtr1, input0, sizeof(cl_float) * width0  * height0);

    status = sampleCommon->waitForEventAndRelease(&inMapEvt2);
    CHECK_ERROR(status,SDK_SUCCESS, "WaitForEventAndRelease(inMapEvt2) Failed");
    memcpy(mapPtr2, input1, sizeof(cl_float) * width1  * height1);

    status = clEnqueueUnmapMemObject(
                commandQueue, 
                inputBuffer0, 
                mapPtr1, 
                0, 
                NULL, 
                &inUnmapEvt1);
    CHECK_OPENCL_ERROR(status, "clEnqueueUnmapMemObject failed. (inputBuffer0)");

    status = clEnqueueUnmapMemObject(
                commandQueue, 
                inputBuffer1, 
                mapPtr2, 
                0, 
                NULL, 
                &inUnmapEvt2);
    CHECK_OPENCL_ERROR(status, "clEnqueueUnmapMemObject failed. (inputBuffer1)");

    status = clFlush(commandQueue);
    CHECK_OPENCL_ERROR(status, "clFlush failed.");

    status = sampleCommon->waitForEventAndRelease(&inUnmapEvt1);
    CHECK_ERROR(status, SDK_SUCCESS, "waitForEventAndRelease(inUnmapEvt1) failed");

    status = sampleCommon->waitForEventAndRelease(&inUnmapEvt2);
    CHECK_ERROR(status,SDK_SUCCESS, "waitForEventAndRelease(inUnmapEvt2) failed");

    // Set appropriate arguments to the kernel

    // output array as the 1st argument : stores product of input0 and input1 
    status = clSetKernelArg(
                    kernel, 
                    0, 
                    sizeof(cl_mem), 
                    (void *)&inputBuffer0);
    CHECK_OPENCL_ERROR(status, "clSetKernelArg failed. (inputBuffer0)");

    // the input matrix  as 2nd argument - input0 
    status = clSetKernelArg(
                    kernel, 
                    1, 
                    sizeof(cl_mem), 
                    (void *)&inputBuffer1);
    CHECK_OPENCL_ERROR(status, "clSetKernelArg failed. (inputBuffer1)");

    // the input matrix as 3rd argument - input1
    status = clSetKernelArg(
                    kernel, 
                    2, 
                    sizeof(cl_mem), 
                    (void *)&outputBuffer);
    CHECK_OPENCL_ERROR(status, "clSetKernelArg failed. (outputBuffer)");

    // width0 of the input0 matrix as 4th argument - width0
    status = clSetKernelArg(
                    kernel, 
                    3, 
                    sizeof(cl_int),
                    (void*)&width0);
    CHECK_OPENCL_ERROR(status, "clSetKernelArg failed. (width0)");

    // Set local memory argument if Scratchpad is available
    if(lds)
    {
        status = clSetKernelArg(
                        kernel, 
                        4, 
                        (blockSize * 4) * (blockSize * 4) * sizeof(cl_float),
                        NULL);
        CHECK_OPENCL_ERROR(status, "clSetKernelArg failed. (local memory)");
    }
    else
    {
        status = clSetKernelArg(kernel, 4, sizeof(cl_int), &width1);
        CHECK_OPENCL_ERROR(status, "clSetKernelArg failed. (width1)");
    }

    // Enqueue a kernel run call
    status = clEnqueueNDRangeKernel(
                 commandQueue,
                 kernel,
                 2,
                 NULL,
                 globalThreads,
                 localThreads,
                 0,
                 NULL,
                 &ndrEvt);    
    CHECK_OPENCL_ERROR(status, "clEnqueueNDRangeKernel failed.");

    status = clFlush(commandQueue);
    CHECK_OPENCL_ERROR(status, "clFlush failed.");

    // wait for the kernel call to finish execution
    eventStatus = CL_QUEUED;
    while(eventStatus != CL_COMPLETE)
    {
        status = clGetEventInfo(
                        ndrEvt, 
                        CL_EVENT_COMMAND_EXECUTION_STATUS, 
                        sizeof(cl_int),
                        &eventStatus,
                        NULL);
        CHECK_OPENCL_ERROR(status, "clGetEventInfo failed.");
    }

    if(!eAppGFLOPS)
    {
        // Calculate performance
        cl_ulong startTime;
        cl_ulong endTime;
        
        // Get kernel profiling info
        status = clGetEventProfilingInfo(ndrEvt,
                                         CL_PROFILING_COMMAND_START,
                                         sizeof(cl_ulong),
                                         &startTime,
                                         0);
        CHECK_OPENCL_ERROR(status, "clGetEventProfilingInfo failed.(startTime)");

        status = clGetEventProfilingInfo(ndrEvt,
                                         CL_PROFILING_COMMAND_END,
                                         sizeof(cl_ulong),
                                         &endTime,
                                         0);
        CHECK_OPENCL_ERROR(status, "clGetEventProfilingInfo failed.(endTime)");

        // Print performance numbers
        double sec = 1e-9 * (endTime - startTime);
        kernelTime += sec;
    }

    status = clReleaseEvent(ndrEvt);
    CHECK_OPENCL_ERROR(status, "clReleaseEvent failed. (ndrEvt)");

    void* outMapPtr = clEnqueueMapBuffer(
                        commandQueue, 
                        outputBuffer, 
                        CL_FALSE, 
                        CL_MAP_READ, 
                        0, 
                        width1 * height0 * sizeof(cl_float), 
                        0, 
                        NULL, 
                        &outMapEvt, 
                        &status);
    CHECK_OPENCL_ERROR(status, "clEnqueueMapBuffer failed. (outputBuffer)");

    status = clFlush(commandQueue);
    CHECK_OPENCL_ERROR(status, "clFlush failed.");

    status = sampleCommon->waitForEventAndRelease(&outMapEvt);
    CHECK_ERROR(status,0, "waitForEventAndRelease(outMapEvt) failed");
    memcpy(output, outMapPtr, sizeof(cl_float) * width1  * height0);

    status = clEnqueueUnmapMemObject(
                commandQueue, 
                outputBuffer, 
                outMapPtr, 
                0, 
                NULL, 
                &outUnmapEvt);
    CHECK_OPENCL_ERROR(status, "clEnqueueUnmapMemObject failed. (outputBuffer)");

    status = clFlush(commandQueue);
    CHECK_OPENCL_ERROR(status, "clFlush failed.");

    status = sampleCommon->waitForEventAndRelease(&outUnmapEvt);
    CHECK_ERROR(status,0, "waitForEventAndRelease(outUnmapEvt) failed");

    return SDK_SUCCESS;
}

int 
MatrixMultiplication::initialize()
{
    // Call base class Initialize to get default configuration
    if(this->SDKSample::initialize() != SDK_SUCCESS)
        return SDK_FAILURE;

    // add an option for getting blockSize from commandline
    streamsdk::Option* xParam = new streamsdk::Option;
    CHECK_ALLOCATION(xParam, "Memory Allocation error.\n");
    xParam->_sVersion = "x";
    xParam->_lVersion = "height0";
    xParam->_description = "height of matrix A";
    xParam->_type     = streamsdk::CA_ARG_INT;
    xParam->_value    = &n;
    sampleArgs->AddOption(xParam);
    delete xParam;

    streamsdk::Option* yParam = new streamsdk::Option;
    CHECK_ALLOCATION(yParam, "Memory Allocation error.\n");
    yParam->_sVersion = "y";
    yParam->_lVersion = "width0";
    yParam->_description = "width of matrix A and Height of matrix B";
    yParam->_type     = streamsdk::CA_ARG_INT;
    yParam->_value    = &m;
    sampleArgs->AddOption(yParam);
    delete yParam;

    streamsdk::Option* zParam = new streamsdk::Option;
    CHECK_ALLOCATION(zParam, "Memory Allocation error.\n");
    zParam->_sVersion = "z";
    zParam->_lVersion = "width1";
    zParam->_description = "width of matrix B";
    zParam->_type     = streamsdk::CA_ARG_INT;
    zParam->_value    = &k;
    sampleArgs->AddOption(zParam);
    delete zParam;

    streamsdk::Option* blockSizeParam = new streamsdk::Option;
    CHECK_ALLOCATION(blockSizeParam, "Memory Allocation error.\n");
    blockSizeParam->_sVersion = "b";
    blockSizeParam->_lVersion = "blockSize";
    blockSizeParam->_description = "Use local memory of dimensions blockSize x blockSize";
    blockSizeParam->_type     = streamsdk::CA_ARG_INT;
    blockSizeParam->_value    = &blockSize;
    sampleArgs->AddOption(blockSizeParam);
    delete blockSizeParam;

    streamsdk::Option* num_iterations = new streamsdk::Option;
    CHECK_ALLOCATION(num_iterations, "Memory Allocation error.\n");
    num_iterations->_sVersion = "i";
    num_iterations->_lVersion = "iterations";
    num_iterations->_description = "Number of iterations for kernel execution";
    num_iterations->_type = streamsdk::CA_ARG_INT;
    num_iterations->_value = &iterations;
    sampleArgs->AddOption(num_iterations);
    delete num_iterations;

    streamsdk::Option* appGflops_option = new streamsdk::Option;
    CHECK_ALLOCATION(appGflops_option, "Memory Allocation error.\n");
    appGflops_option->_sVersion = "";
    appGflops_option->_lVersion = "eAppGflops";
    appGflops_option->_description = "Prints GFLOPS calculated from transfer + kernel time";
    appGflops_option->_type = streamsdk::CA_NO_ARGUMENT;
    appGflops_option->_value = &eAppGFLOPS;
    sampleArgs->AddOption(appGflops_option);
    delete appGflops_option;

    return SDK_SUCCESS;
}

int 
MatrixMultiplication::setup()
{  
    width0  = m;
    height0 = n;
    width1  = k;
    height1 = m;

	input0 = a_mat[0];
	input1 = b_mat[0];
	output = c_mat[0];

    int timer = sampleCommon->createTimer();
    sampleCommon->resetTimer(timer);
    sampleCommon->startTimer(timer);

    if(setupCL()!=SDK_SUCCESS)
        return SDK_FAILURE;

    sampleCommon->stopTimer(timer);

    setupTime = (cl_double)sampleCommon->readTimer(timer);

    return SDK_SUCCESS;
}


int 
MatrixMultiplication::run()
{
    // Warm up
    for(int i = 0; i < 2 && iterations != 1; i++)
    {
        // Arguments are set and execution call is enqueued on command buffer
        if(runCLKernels() != SDK_SUCCESS)
            return SDK_FAILURE;
    }

    int timer = sampleCommon->createTimer();
    sampleCommon->resetTimer(timer);
    sampleCommon->startTimer(timer);

    std::cout << "Executing kernel for " << iterations << " iterations" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;

    kernelTime = 0;
    for(int i = 0; i < iterations; i++)
    {
        // Arguments are set and execution call is enqueued on command buffer
        int kernelRun = runCLKernels();
        if(kernelRun != SDK_SUCCESS)
        {
            return kernelRun;
        }
    }

    sampleCommon->stopTimer(timer);
    appTime = (double)(sampleCommon->readTimer(timer)) / iterations;
    kernelTime = kernelTime / iterations;
/*
    if(!quiet) {
        sampleCommon->printArray<cl_float>("Output", output, width1, 1);
    }
*/
    return SDK_SUCCESS;
}

int 
MatrixMultiplication::verifyResults()
{
     return SDK_SUCCESS;
}

void 
MatrixMultiplication::printStats()
{
    if(eAppGFLOPS)
    {
        std::string strArray[4] = {"MatrixA", "MatrixB", "Time(sec)", "[Transfer+kernel]Time(sec)"};
        std::string stats[4];

        double flops = 2 * width0 * width1;
        double perf = (flops / appTime) * height0 * 1e-9;
        if(timing)
            std::cout << "GFlops achieved : " << perf << std::endl << std::endl;

        totalTime = setupTime + appTime;

        stats[0]  = sampleCommon->toString(height0, std::dec)
                    +"x"+sampleCommon->toString(width0, std::dec);
        stats[1]  = sampleCommon->toString(height1, std::dec)
                    +"x"+sampleCommon->toString(width1, std::dec);
        stats[2]  = sampleCommon->toString(totalTime, std::dec);
        stats[3]  = sampleCommon->toString(appTime, std::dec);
        
        this->SDKSample::printStats(strArray, stats, 4);
    }
    else
    {
        std::string strArray[4] = {"MatrixA", "MatrixB", "Time(sec)", "kernelTime(sec)"};
        std::string stats[4];

        double flops = 2 * width0 * width1;
        double perf = (flops / kernelTime) * height0 * 1e-9;
        if(timing)
            std::cout << "GFlops achieved : " << perf << std::endl << std::endl;

        totalTime = setupTime + kernelTime;

        stats[0]  = sampleCommon->toString(height0, std::dec)
                    +"x"+sampleCommon->toString(width0, std::dec);
        stats[1]  = sampleCommon->toString(height1, std::dec)
                    +"x"+sampleCommon->toString(width1, std::dec);
        stats[2]  = sampleCommon->toString(totalTime, std::dec);
        stats[3]  = sampleCommon->toString(kernelTime, std::dec);

        this->SDKSample::printStats(strArray, stats, 4);
    }
}

int 
MatrixMultiplication::cleanup()
{
    // Releases OpenCL resources (Context, Memory etc.)
    cl_int status;

    status = clReleaseKernel(kernel);
    CHECK_OPENCL_ERROR(status, "clReleaseKernel failed.(kernel)");

    status = clReleaseProgram(program);
    CHECK_OPENCL_ERROR(status, "clReleaseProgram failed.(program)");

    status = clReleaseMemObject(inputBuffer0);
    CHECK_OPENCL_ERROR(status, "clReleaseMemObject failed.(inputBuffer0)");

    status = clReleaseMemObject(inputBuffer1);
    CHECK_OPENCL_ERROR(status, "clReleaseMemObject failed.(inputBuffer1)");

    status = clReleaseMemObject(outputBuffer);
    CHECK_OPENCL_ERROR(status, "clReleaseMemObject failed.(outputBuffer)");

    status = clReleaseCommandQueue(commandQueue);
    CHECK_OPENCL_ERROR(status, "clReleaseCommandQueue failed.(commandQueue)");

    status = clReleaseContext(context);
    CHECK_OPENCL_ERROR(status, "clReleaseContext failed.(context)");

    // release program resources (input memory etc.)
    FREE(devices);

    return SDK_SUCCESS;
}

