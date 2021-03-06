/*
 * CRoutine.cpp
 *
 *  Created on: Nov 14, 2011
 *      Author: bkloppenborg
 *
 *  Description:
 *      Base class for OpenCL routines used in LIBOI.  Manages
 *      Creation/deletion of kernels and programs on the OpenCL device.
 */

 /* 
 * Copyright (c) 2012 Brian Kloppenborg
 *
 * If you use this software as part of a scientific publication, please cite as:
 *
 * Kloppenborg, B.; Baron, F. (2012), "LibOI: The OpenCL Interferometry Library"
 * (Version X). Available from  <https://github.com/bkloppenborg/liboi>.
 *
 * This file is part of the OpenCL Interferometry Library (LIBOI).
 * 
 * LIBOI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License 
 * as published by the Free Software Foundation, either version 3 
 * of the License, or (at your option) any later version.
 * 
 * LIBOI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public 
 * License along with LIBOI.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdio>
#include <iostream>
#include "CRoutine.h"

using namespace std;

namespace liboi
{

CRoutine::CRoutine(cl_device_id device, cl_context context, cl_command_queue queue)
{
	mDeviceID = device;
	mContext = context;
	mQueue = queue;
	mKernelPath = "";
}

CRoutine::~CRoutine()
{
	// Release any kernels or programs
	for(auto kernel: mKernels)
		clReleaseKernel(kernel);

	for(auto program: mPrograms)
		clReleaseProgram(program);
}

/// A more verbose wrapper for BuildKernel, emits the name of the kernel before compilation.
int CRoutine::BuildKernel(string source, string kernel_name, string kernel_filename)
{
#ifdef DEBUG
    string message = "Loading and Compiling program " +  kernel_filename + "\n";
	printf("%s", message.c_str());
#endif //DEBUG

	return BuildKernel(source, kernel_name);
}

/// Builds the kernel from the specified string.
/// Appends the compiled kernel to mKernels and mPrograms, returns the index at which this kernel is located.
int CRoutine::BuildKernel(string source, string kernel_name)
{
    const char * tmp = source.c_str();
    cl_program program;
    cl_kernel kernel;
    int err;
    //    string tmp_err;
    //    tmp_err.reserve(2048);

	// Create the program
	program = clCreateProgramWithSource(mContext, 1, &tmp, NULL, &err);
	if (!program || err != CL_SUCCESS)
	{
		size_t length;
		char build_log[2048];
		//printf("%s\n", block_source);
		cout<< "Error: Failed to create compute program from source: " << kernel_name << endl;
		clGetProgramBuildInfo(program, mDeviceID, CL_PROGRAM_BUILD_LOG, sizeof(build_log), build_log, &length);
		printf("%s\n", build_log);

		throw "Could not build compute program " + kernel_name;
	}

	// Build the program executable
	err = clBuildProgram(program, 1, &mDeviceID, NULL, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		size_t length;
		char build_log[2048];
		//printf("%s\n", block_source);
		cout<< "Error: Failed to build compute program: " << kernel_name << endl;
		clGetProgramBuildInfo(program, mDeviceID, CL_PROGRAM_BUILD_LOG, sizeof(build_log), build_log, &length);
		printf("%s\n", build_log);

		throw "Could not compile compute program " + kernel_name;
	}

    // Program built correctly, push it onto the vector
    mPrograms.push_back(program);

    // Create the compute kernel from within the program
    kernel = clCreateKernel(program, kernel_name.c_str(), &err);
	COpenCL::CheckOCLError("Failed to create kernel named " + kernel_name, err);

	// All is well, push the kernel onto the vector
	mKernels.push_back(kernel);

	// Return the index of the latest kernel
	return mPrograms.size();
}

/// Dumps the entire contents of the specified buffer to standard out.
void CRoutine::DumpFloatBuffer(cl_mem buffer, unsigned int size)
{
	int err = CL_SUCCESS;
	cl_float * tmp = new cl_float[size];
	err |= clEnqueueReadBuffer(mQueue, buffer, CL_TRUE, 0, size * sizeof(cl_float), tmp, 0, NULL, NULL);
	for(unsigned int i = 0; i < size; i++)
	{
		printf(" %i %f ", i, tmp[i]);
		if(i > 0 && i % 10 == 0)
			printf("\n");
	}
}

bool CRoutine::isPow2(unsigned int x)
{
    return ((x&(x-1))==0);
}

unsigned int CRoutine::nextPow2( unsigned int x )
{
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}

string CRoutine::ReadSource(string filename)
{
	return ReadFile(mKernelPath + '/' +  filename, "Could not read OpenCL kernel source " + mKernelPath + '/' +  filename);
}

void CRoutine::SetSourcePath(string path_to_kernels)
{
	mKernelPath = path_to_kernels;
}

/// Data verification for OpenCL type cl_float
bool CRoutine::Verify(valarray<cl_float> & cpu_buffer, cl_mem device_buffer, int num_elements, size_t offset)
{
	int err = CL_SUCCESS;
	valarray<cl_float> tmp(num_elements);

	err  = clEnqueueReadBuffer(mQueue, device_buffer, CL_TRUE, offset, num_elements * sizeof(cl_float), &tmp[0], 0, NULL, NULL);
	COpenCL::CheckOCLError("Could not copy back cl_float values for verification!", err);

	double error = 0;
	double cpu_sum = 0;
	double cl_sum = 0;
	for(int i = 0; i < num_elements; i++)
	{
		cpu_sum += fabs(float(cpu_buffer[i]));
		cl_sum += fabs(float(tmp[i]));
		error += fabs(float(cpu_buffer[i]) - float(tmp[i]));
	}

	printf("  sum(CPU):    %f\n", cpu_sum);
	printf("  sum(OpenCL): %f\n", cl_sum);
	printf("  Total Error [sum( |CPU - OpenCL| ) ]: %0.4e\n", error);
	printf("  Relative Error [error / sum(|CPU|) ]:   %0.4e\n", error / cpu_sum);

	if(error != error || (error/cpu_sum > MAX_REL_ERROR))
	{
		cout << "  Error too great/NAN, dumping buffers." << endl;
		cout << "  Format: cpu_buffer, cl_buffer, difference." << endl;
		for(int i = 0; i < num_elements; i++)
			printf("    %i %0.4f %0.4f %0.6f\n", i, cpu_buffer[i], tmp[i], cpu_buffer[i] - tmp[i]);

		return false;
	}

	return true;
}

/// Data verification for OpenCL type cl_float
bool CRoutine::Verify(valarray<complex<float>> & cpu_buffer, cl_mem device_buffer, int num_elements, size_t offset)
{
	int err = CL_SUCCESS;
	valarray<cl_float2> tmp(num_elements);

	err  = clEnqueueReadBuffer(mQueue, device_buffer, CL_TRUE, offset, num_elements * sizeof(cl_float2), &tmp[0], 0, NULL, NULL);
	COpenCL::CheckOCLError("Could not copy back cl_float2 values for verification!", err);

	double error = 0;
	double sum = 0;
	for(int i = 0; i < num_elements; i++)
	{
		sum += abs(cpu_buffer[i]);
		error += fabs(cpu_buffer[i].real() - tmp[i].s0) + fabs(cpu_buffer[i].imag() - tmp[i].s1);
	}

	printf("  Total Error [sum( |CPU - OpenCL| ) ]: %0.4e\n", error);
	printf("  Relative Error [error / sum(|CPU|) ]:   %0.4e\n", error / sum);

	if(error != error || (error/sum > MAX_REL_ERROR))
	{
		complex<float> diff;
		double tmp_real = 0;
		double tmp_imag = 0;
		cout << "  Error too great/NAN, dumping buffers." << endl;
		cout << "  Format: cpu_buffer, cl_buffer, difference." << endl;
		for(int i = 0; i < num_elements; i++)
		{
			tmp_real = cpu_buffer[i].real() - tmp[i].s0;
			tmp_imag = cpu_buffer[i].imag() - tmp[i].s1;
			diff = complex<float>(tmp_real, tmp_imag);
			printf("    %i (%0.4e %0.4e) (%0.4e %0.4e) (%0.4e %0.4e)\n", i,
					cpu_buffer[i].real(), cpu_buffer[i].imag(),
					tmp[i].s0, tmp[i].s1,
					diff.real(), diff.imag());
		}

		return false;
	}

	return true;
}

} /* namespace liboi */
