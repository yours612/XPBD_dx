#include "LoadModel.h"
#include <fstream>
#include <sstream>

vector<string> SplitString(const string& str, const char delimiter1)
{
    vector<string> tokens;
    stringstream ss(str);
    string token;
    
    const char delimiter2 = '\n';
    while (getline(ss, token, delimiter1))
    {       
        stringstream lineStream(token);
        string subToken;
        while (getline(lineStream, subToken, delimiter2)) {
            if (!subToken.empty()) {
                tokens.push_back(subToken);
            }
        }
    }
    return tokens;
}
/*XMFLOAT3X3 Build_Edge_Matrix(int tet, vector<XMFLOAT3>& X, vector<int32_t>& Tet) {
    XMFLOAT3X3 Dm;

    XMVECTOR X10 = XMLoadFloat3(&X[Tet[tet * 4 + 1]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
    XMVECTOR X20 = XMLoadFloat3(&X[Tet[tet * 4 + 2]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
    XMVECTOR X30 = XMLoadFloat3(&X[Tet[tet * 4 + 3]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
    XMFLOAT3 X10Float, X20Float, X30Float;
    XMStoreFloat3(&X10Float, X10);
    XMStoreFloat3(&X20Float, X20);
    XMStoreFloat3(&X30Float, X30);

    Dm._11 = X10Float.x;
    Dm._12 = X20Float.x;
    Dm._13 = X30Float.x;

    Dm._21 = X10Float.y;
    Dm._22 = X20Float.y;
    Dm._23 = X30Float.y;

    Dm._31 = X10Float.z;
    Dm._32 = X20Float.z;
    Dm._33 = X30Float.z;

    return Dm;
}
*/
void ReadEleFile(const string& filename, vector<int32_t> &Tet) {
    ifstream file(filename);
    if (!file)
    {
        return;
    }

    string fileContent((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    file.close();

    vector<string> Strings = SplitString(fileContent, ' ');
    int tet_number = stoi(Strings[0]);
    Tet.resize(tet_number * 4);

    for (int tet = 0; tet < tet_number; ++tet)
    {
        Tet[tet * 4 + 0] = std::stoi(Strings[(double)tet * 5 + 4]) ;
        Tet[tet * 4 + 1] = std::stoi(Strings[(double)tet * 5 + 5]) ;
        Tet[tet * 4 + 2] = std::stoi(Strings[(double)tet * 5 + 6]) ;
        Tet[tet * 4 + 3] = std::stoi(Strings[(double)tet * 5 + 7]) ;
    }
}
void ReadNodeFile(const string& filename, vector<Vertex>& Vertices, vector<int32_t>& Tet, 
    vector<XMFLOAT3>& X, vector<XMFLOAT3X3> &Dminv, vector<int32_t>& indices, vector<vector<int>> &X2Ver) {
    ifstream file(filename);
    if (!file)
    {
        return;
    }

    string fileContent((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    file.close();

    vector<string> Strings = SplitString(fileContent, ' ');
    int xNumber = stoi(Strings[0]);
    X.resize(xNumber);
    X2Ver.resize(xNumber);

    for (int i = 0; i < xNumber; ++i) {
        X[i].x = stod(Strings[i * 4 + 5]);
        X[i].y = stod(Strings[i * 4 + 6]);
        X[i].z = stod(Strings[i * 4 + 7]);
    }

    int tet_number = Tet.size() / 4;
    Vertices.resize(Tet.size()*3);
    int vertex_number = 0;
    for (int tet = 0; tet < tet_number; tet++)
    {
        //X2Ver[Tet[tet * 4 + 0]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 0]];
        //X2Ver[Tet[tet * 4 + 2]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 2]];
        //X2Ver[Tet[tet * 4 + 1]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 1]];

        //X2Ver[Tet[tet * 4 + 0]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 0]];
        //X2Ver[Tet[tet * 4 + 3]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 3]];
        //X2Ver[Tet[tet * 4 + 2]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 2]];

        //X2Ver[Tet[tet * 4 + 0]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 0]];
        //X2Ver[Tet[tet * 4 + 1]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 1]];
        //X2Ver[Tet[tet * 4 + 3]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 3]];

        //X2Ver[Tet[tet * 4 + 1]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 1]];
        //X2Ver[Tet[tet * 4 + 2]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 2]];
        //X2Ver[Tet[tet * 4 + 3]].push_back(vertex_number);
        Vertices[vertex_number++].Pos = X[Tet[tet * 4 + 3]];
    }
    //ÇóDminv
    Dminv.resize(tet_number);
    for (int tet = 0; tet < tet_number; ++tet) {
        XMFLOAT3X3 Dm = Build_Edge_Matrix(tet, X, Tet);
        /*
        XMVECTOR X10 = XMLoadFloat3(&X[Tet[tet * 4 + 1]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
        XMVECTOR X20 = XMLoadFloat3(&X[Tet[tet * 4 + 2]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
        XMVECTOR X30 = XMLoadFloat3(&X[Tet[tet * 4 + 3]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
        XMFLOAT3 X10Float, X20Float, X30Float;
        XMStoreFloat3(&X10Float, X10);
        XMStoreFloat3(&X20Float, X20);
        XMStoreFloat3(&X30Float, X30);
        Dm._11 = X10Float.x;
        Dm._12 = X20Float.x;
        Dm._13 = X30Float.x;

        Dm._21 = X10Float.y;
        Dm._22 = X20Float.y;
        Dm._23 = X30Float.y;

        Dm._31 = X10Float.z;
        Dm._32 = X20Float.z;
        Dm._33 = X30Float.z;
        */
        //XMStoreFloat3x3(&Dminv[tet], XMMatrixInverse(&XMMatrixDeterminant(XMLoadFloat3x3(&Dm)), XMLoadFloat3x3(&Dm)));
        XMStoreFloat3x3(&Dminv[tet], XMMatrixInverse(nullptr, XMLoadFloat3x3(&Dm)));
        
        XMFLOAT3X3 Test1;
        XMStoreFloat3x3(&Test1, XMMatrixMultiply(XMLoadFloat3x3(&Dm), XMLoadFloat3x3(&Dminv[tet])));
        XMFLOAT3X3 Test2;
        Test2 = Test1;
    }

    ///*
    indices.resize(tet_number * 12);
    for (int t = 0; t < tet_number * 4; t++)
    {
        indices[t * 3 + 0] = t * 3 + 0;
        indices[t * 3 + 1] = t * 3 + 1;
        indices[t * 3 + 2] = t * 3 + 2;

        XMFLOAT3 v1 = Vertices[indices[t * 3 + 0]].Pos;
        XMFLOAT3 v2 = Vertices[indices[t * 3 + 1]].Pos;
        XMFLOAT3 v3 = Vertices[indices[t * 3 + 2]].Pos;

        XMVECTOR edge1 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v2));
        XMVECTOR edge2 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v3));
        XMVECTOR normal = XMVector3Normalize(DirectX::XMVector3Cross(edge1, edge2));
        XMFLOAT3 Normal;
        XMStoreFloat3(&Normal, normal);

        Vertices[indices[t * 3 + 0]].Normal.x += Normal.x;
        Vertices[indices[t * 3 + 0]].Normal.y += Normal.y;
        Vertices[indices[t * 3 + 0]].Normal.z += Normal.z;

        Vertices[indices[t * 3 + 1]].Normal.x += Normal.x;
        Vertices[indices[t * 3 + 1]].Normal.y += Normal.y;
        Vertices[indices[t * 3 + 1]].Normal.z += Normal.z;

        Vertices[indices[t * 3 + 2]].Normal.x += Normal.x;
        Vertices[indices[t * 3 + 2]].Normal.y += Normal.y;
        Vertices[indices[t * 3 + 2]].Normal.z += Normal.z;
    }
    //*/
}
void ReadFaceFile(const string& filename, vector<int32_t>& indices) {
    ifstream file(filename);
    if (!file)
    {
        return;
    }

    string fileContent((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    file.close();

    vector<string> Strings = SplitString(fileContent, ' ');
    int triNumber = stoi(Strings[0]);
    indices.resize(triNumber*3);

    int j = 0;
    for (int i = 0, num = 0; i < triNumber; ++i) {
        if (stoi(Strings[6]) == -1) j = 5;
        else j = 4;

        indices[num] = stoi(Strings[i * j + 3]);
        indices[num + 1] = stoi(Strings[i * j + 4]);
        indices[num + 2] = stoi(Strings[i * j + 5]);
        num = num + 3;
    }
    
}

void CreateIndeices(vector<int> Tet, vector<Vertex> Vertices, vector<int32_t>& indices) {
    indices.resize(Tet.size());

    for (int i = 0; i < indices.size() / 4; ++i) {
        indices[i * 3 + 0] = i * 3 + 0;
        indices[i * 3 + 1] = i * 3 + 1;
        indices[i * 3 + 2] = i * 3 + 2;
    }
}

XMFLOAT3X3 Build_Edge_Matrix(int tet, vector<XMFLOAT3>& X, vector<int32_t>& Tet) {
    XMFLOAT3X3 Dm;

    XMVECTOR X10 = XMLoadFloat3(&X[Tet[tet * 4 + 1]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
    XMVECTOR X20 = XMLoadFloat3(&X[Tet[tet * 4 + 2]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
    XMVECTOR X30 = XMLoadFloat3(&X[Tet[tet * 4 + 3]]) - XMLoadFloat3(&X[Tet[tet * 4 + 0]]);
    XMFLOAT3 X10Float, X20Float, X30Float;
    XMStoreFloat3(&X10Float, X10);
    XMStoreFloat3(&X20Float, X20);
    XMStoreFloat3(&X30Float, X30);

    Dm._11 = X10Float.x;
    Dm._12 = X20Float.x;
    Dm._13 = X30Float.x;

    Dm._21 = X10Float.y;
    Dm._22 = X20Float.y;
    Dm._23 = X30Float.y;

    Dm._31 = X10Float.z;
    Dm._32 = X20Float.z;
    Dm._33 = X30Float.z;

    return Dm;
}