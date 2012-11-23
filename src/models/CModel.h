/*
 * CModel.h
 *
 *  Created on: Nov 22, 2012
 *      Author: bkloppen
 *
 *  Base class for analytic-only visibility models. Inheriting classes simply
 *  need to define the GetVis function.
 */

#ifndef CMODEL_H_
#define CMODEL_H_

#include <complex>
#include <valarray>

#ifndef PI
#include <cmath>
#define PI M_PI
#endif

#if defined(__APPLE__) || defined(__MACOSX)
	#include <OpenCL/cl.hpp>
#else
	#include <CL/cl.hpp>
#endif

using namespace std;

class CModel
{
protected:
	static double RPMAS;
	double mAlpha;
	double mDelta;
	double mScale;

public:
	CModel(double alpha, double delta, double image_scale);
	virtual ~CModel();

	virtual complex<double> GetVis(pair<double,double> & uv) = 0;
	cl_float2 GetVis_CL(cl_float2 & uv);

	float GetV2(pair<double,double> & uv);
	cl_float GetV2_CL(cl_float2 & uv);

	complex<double> GetT3(pair<double,double> & uv_ab, pair<double,double> & uv_bc, pair<double,double> & uv_ca);
	cl_float2 GetT3_CL(cl_float2 & uv_ab, cl_float2 & uv_bc, cl_float2 & uv_ca);

	valarray<pair<double,double>> GenerateUVSpiral(unsigned int n_uv);
	valarray<cl_float2> GenerateUVSpiral_CL(unsigned int n_uv);

	virtual valarray<double> GetImage(unsigned int image_width, unsigned int image_height, float image_scale) = 0;
	virtual valarray<cl_float> GetImage_CL(unsigned int image_width, unsigned int image_height, float image_scale);

	unsigned int MasToPixel(double value);
	double MasToRad(double value);
};

#endif /* CMODEL_H_ */