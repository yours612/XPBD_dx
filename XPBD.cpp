#include "XPBD.h"
#include <set>

XPBD_Softbody::XPBD_Softbody(bunnyMesh tetMesh, float Compliance1, float Compliance2){
    Mesh = tetMesh;

    numVerts = tetMesh.vertsPos.size() / 3;
	numTets = tetMesh.tetId.size() / 4;

    //Xpos.resize(numVerts);
	X = tetMesh.vertsPos;
	//prevX = tetMesh.vertsPos;
    vel.resize(numVerts, {0,0,0});

	Tet = tetMesh.tetId;
	edgeIds = tetMesh.tetEdgeIds;
	restVol.resize(numTets, 0.0f);
	edgeLengths.resize(edgeIds.size() / 2);
	invMass.resize(numVerts, 0.0f);

	edgeCompliance = Compliance1;
	volCompliance = Compliance2;

    for (size_t i = 0; i < X.size(); i += 3) {
        XMFLOAT3 vertex;
        vertex.x = X[i] * 0.5;
        vertex.y = X[i + 1] * 0.5;
        vertex.z = X[i + 2] * 0.5;
        Xpos.push_back(vertex);
    }
    prevX.resize(Xpos.size());

    //初始化质量矩阵，剩余体积矩阵, 边长矩阵
    for (int i = 0; i < numTets; i++) {
        float vol = getTetVolume(i);
        restVol[i] = vol;
        float pInvMass = vol > 0.0 ? 1.0 / (vol / 4.0) : 0.0;
        invMass[Tet[4 * i]] += pInvMass;
        invMass[Tet[4 * i + 1]] += pInvMass;
        invMass[Tet[4 * i + 2]] += pInvMass;
        invMass[Tet[4 * i + 3]] += pInvMass;
    }

    /*固定尝试
    std::set<int> values = { 32, 121, 119, 122, 120, 117, 118, 107, 106, 104, 109, 108, 111, 110, 112, 126, 127, 57, 103, 30, 123, 105, 116, 114, 124, 87, 125, 20, 115, 21, 113, 33, 99 };
    std::set<int> values1 = { 10, 68, 77, 37, 67, 36, 5, 52, 34, 35, 40, 18, 12, 53, 39, 6, 69, 19, 90, 92, 
    71, 91, 29, 95, 81, 82, 43, 38, 55, 34, 83};
    set<int>::iterator it;
    for (int i = 0; i < X.size() / 3; ++i) {
        if (values1.find(i+1) != values1.end()) { // 不在里面
            invMass[i] = 0;
        }
    }
    */
    
    for (int i = 0; i < edgeLengths.size(); i++) {
        int id0 = edgeIds[2 * i];
        int id1 = edgeIds[2 * i + 1];
        edgeLengths[i] = XMVectorGetX(XMVector3Length(XMLoadFloat3(&Xpos[id0]) - XMLoadFloat3(&Xpos[id1])));
    }
}

float XPBD_Softbody::getTetVolume(int tet) {
    XMFLOAT3X3 Dm;

    XMVECTOR X10 = XMLoadFloat3(&Xpos[Tet[tet * 4 + 1]]) - XMLoadFloat3(&Xpos[Tet[tet * 4 + 0]]);
    XMVECTOR X20 = XMLoadFloat3(&Xpos[Tet[tet * 4 + 2]]) - XMLoadFloat3(&Xpos[Tet[tet * 4 + 0]]);
    XMVECTOR X30 = XMLoadFloat3(&Xpos[Tet[tet * 4 + 3]]) - XMLoadFloat3(&Xpos[Tet[tet * 4 + 0]]);
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

    XMVECTOR det = XMMatrixDeterminant(XMLoadFloat3x3(&Dm));
    float volume = XMVectorGetX(det) * 0.1666667; //体积
    return volume;
}

void XPBD_Softbody::preSolve(float dt, XMFLOAT3 gravity) {
    for (int i = 0; i < numVerts; ++i) {
        if (invMass[i] == 0.0) {        
            continue; 
        }
        vel[i] = { vel[i].x * 0.99f,vel[i].y * 0.99f, vel[i].z * 0.99f };
        vel[i].x += gravity.x * dt; vel[i].y += gravity.y * dt; vel[i].z += gravity.z * dt;
        prevX[i] = Xpos[i];
        Xpos[i].x += vel[i].x * dt; 
        Xpos[i].y += vel[i].y * dt; 
        Xpos[i].z += vel[i].z * dt;
        float x = Xpos[i].x;
        float y = Xpos[i].y;
        float z = Xpos[i].z;

        if (abs(x) > 5) {
            int _x = x / abs(x);
            Xpos[i] = prevX[i];
            Xpos[i].x = 5 * _x;
        }
        if (abs(z) > 5) {
            int _z = z / abs(z);
            Xpos[i] = prevX[i];
            Xpos[i].z = 5 * _z;
        }
        if (y < -2) {
            Xpos[i] = prevX[i];
            Xpos[i].y = -2;
        }
        
    }
}
void XPBD_Softbody::solve(float dt) {
    solveEdges(edgeCompliance, dt);
    solveVolumes(volCompliance, dt);
    //solveCollision();
}
void XPBD_Softbody::postSolve(float dt) {
    for (int i = 0; i < numVerts; i++) {
        if (invMass[i] == 0.0)
            continue;

        vel[i].x = -(prevX[i].x - Xpos[i].x) / dt;
        vel[i].y = -(prevX[i].y - Xpos[i].y) / dt;
        vel[i].z = -(prevX[i].z - Xpos[i].z) / dt;
    }
}

void XPBD_Softbody::solveEdges(float compliance, float dt) {
    float alpha = compliance / dt / dt;

    for (int i = 0; i < edgeLengths.size(); i++) {
        int id0 = edgeIds[2 * i];
        int id1 = edgeIds[2 * i + 1];
        float w0 = invMass[id0];
        float w1 = invMass[id1];
        float w = w0 + w1;
        if (w == 0.0)
            continue;

        XMFLOAT3 director = { Xpos[id0].x - Xpos[id1].x , Xpos[id0].y - Xpos[id1].y , Xpos[id0].z - Xpos[id1].z };
        float len = XMVectorGetX(XMVector3Length(XMLoadFloat3(&Xpos[id0]) - XMLoadFloat3(&Xpos[id1])));
        if (len == 0.0)
            continue;
        //方向向量
        director.x = director.x * 1.0 / len; director.y = director.y * 1.0 / len; director.z = director.z * 1.0 / len;

        float restLen = edgeLengths[i];
        float C = len - restLen;
        float s = -C / (w + alpha);
        Xpos[id0].x += director.x * s * w0; Xpos[id0].y += director.y * s * w0; Xpos[id0].z += director.z * s * w0;
        Xpos[id1].x += -director.x * s * w1; Xpos[id1].y += -director.y * s * w1; Xpos[id1].z += -director.z * s * w1;
    }
}
void XPBD_Softbody::solveVolumes(float compliance, float dt) {
    float alpha = compliance / dt / dt;

    for (int i = 0; i < numTets; i++) {
        float w = 0.0;

        vector<XMVECTOR> grads;
        grads.resize(4);
        for (int j = 0; j < 4; j++) {
            int id0 = Tet[4 * i + volIdOrder[j][0]];
            int id1 = Tet[4 * i + volIdOrder[j][1]];
            int id2 = Tet[4 * i + volIdOrder[j][2]];

            XMFLOAT3 temp1 = { Xpos[id1].x - Xpos[id0].x , Xpos[id1].y - Xpos[id0].y , Xpos[id1].z - Xpos[id0].z };
            XMFLOAT3 temp2 = { Xpos[id2].x - Xpos[id0].x , Xpos[id2].y - Xpos[id0].y , Xpos[id2].z - Xpos[id0].z };
            grads[j] = XMVector3Cross(XMLoadFloat3(&temp1), XMLoadFloat3(&temp2));
            grads[j] = XMVectorScale(grads[j], 1.0f / 6.0f);
            float y = XMVectorGetX(XMVector3Length(grads[j]));

            w += invMass[Tet[4 * i + j]] * y * y;

        }
        if (w == 0.0)
            continue;

        float vol = getTetVolume(i);
        float restvol = restVol[i];
        float C = vol - restvol;
        float s = -C / (w + alpha);

        for (int j = 0; j < 4; j++) {
            int id = Tet[4 * i + j];
            XMFLOAT3 grad;
            XMStoreFloat3(&grad, grads[j]);
            Xpos[id].x += grad.x * s *invMass[id];  Xpos[id].y += grad.y * s * invMass[id];  Xpos[id].z += grad.z * s * invMass[id];
        }
    }
}
void XPBD_Softbody::solveCollision() {
    for (int i = 0; i < numVerts; i++) {
        XMFLOAT3 p = Xpos[i];
    }
}

void XPBD_Softbody::simulate(float dt) {
    for (int i = 0; i < 10; ++i) {
        preSolve(dt, { 0, -10.0, 0 });
        solve(dt);
        postSolve(dt);
    }
}