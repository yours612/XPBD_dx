#pragma once
#include <vector>
#include <string>
#include "FrameResource.h"

using namespace std;

vector<string> SplitString(const string& str, const char delimiter);
void ReadEleFile(const string& filename, vector<int32_t> &Tet);
void ReadNodeFile(const string& filename, vector<Vertex>& Vertices, vector<int32_t>& Tet, 
	vector<XMFLOAT3> &X, vector<XMFLOAT3X3> &Dminv, vector<int32_t>& indices, vector<vector<int>> &X2Ver);
void ReadFaceFile(const string& filename, vector<int32_t>& indices);
void CreateIndeices(vector<int> Tet, vector<Vertex> Vertices, vector<int32_t>& indices);
XMFLOAT3X3 Build_Edge_Matrix(int tet, vector<XMFLOAT3>& X, vector<int32_t>& Tet);

