#pragma once
#include "stdafx.h"
#include "GameTimer.h"

#include <vector>

using namespace std;
using namespace DirectX;

const int N = 12;
void GetNextPos(XMFLOAT4X3 &node_pos, XMFLOAT4X3 &node_vel, _GameTimer::GameTimer& gt);
void Gaussian_elimination(float arr[N][N], float result[N][N]);
XMFLOAT4X3 StvkImplicit3D(XMFLOAT4X3 &node_pos, XMFLOAT4X3 &node_vel, XMFLOAT4X3 &node_force, _GameTimer::GameTimer& gt);
//矩阵乘法，传入a，b矩阵及其相应的行列数
void mulMatrix(float a[][12], int r1, int c1, float b[], int r2, float* res);
//矩阵加法，传入a,b矩阵及其行列数
void addMatrix(float a[], float b[], int r, float* res);

//XMFLOAT3X3 Build_Edge_Matrix(int tet, vector<XMFLOAT3>& X, vector<int32_t>& Tet);
