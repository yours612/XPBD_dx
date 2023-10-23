#include "FEM.h"
using namespace std;
using namespace DirectX;


void GetNextPos(XMFLOAT4X3 &node_pos, XMFLOAT4X3 &node_vel, _GameTimer::GameTimer& gt) {	
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 3; ++j) {
			node_pos(i, j) += node_vel(i, j) * gt.DeltaTime();
		}
	}
	//return node_pos;
}


//使用高斯消元法对矩阵进行求逆
void Gaussian_elimination(float arr[N][N], float result[N][N])
{
	int i, j, k;
	float W[N][2 * N];
	float tem_1, tem_2, tem_3;

	// 对矩阵右半部分进行扩增
	for (i = 0; i < N; i++) {
		for (j = 0; j < 2 * N; j++) {
			if (j < N) {
				W[i][j] = (float)arr[i][j];
			}
			else {
				W[i][j] = (float)(j - N == i ? 1 : 0);
			}
		}
	}
	for (i = 0; i < N; i++)
	{
		// 判断矩阵第一行第一列的元素是否为0，若为0，继续判断第二行第一列元素，直到不为0，将其加到第一行
		if (((int)W[i][i]) == 0)
		{
			for (j = i + 1; j < N; j++)
			{
				if (((int)W[j][i]) != 0) break;
			}
			if (j == N)
			{
				printf("这个矩阵不能求逆");
				break;
			}
			//将前面为0的行加上后面某一行
			for (k = 0; k < 2 * N; k++)
			{
				W[i][k] += W[j][k];
			}
		}

		//将前面行首位元素置1
		tem_1 = W[i][i];
		for (j = 0; j < 2 * N; j++)
		{
			W[i][j] = W[i][j] / tem_1;
		}

		//将后面所有行首位元素置为0
		for (j = i + 1; j < N; j++)
		{
			tem_2 = W[j][i];
			for (k = i; k < 2 * N; k++)
			{
				W[j][k] = W[j][k] - tem_2 * W[i][k];
			}
		}
	}

	// 将矩阵前半部分标准化
	for (i = N - 1; i >= 0; i--)
	{
		for (j = i - 1; j >= 0; j--)
		{
			tem_3 = W[j][i];
			for (k = i; k < 2 * N; k++)
			{
				W[j][k] = W[j][k] - tem_3 * W[i][k];
			}
		}
	}

	//得出逆矩阵
	for (i = 0; i < N; i++)
	{
		for (j = N; j < 2 * N; j++)
		{
			result[i][j - N] = W[i][j];
		}
	}
}

XMFLOAT4X3 StvkImplicit3D(XMFLOAT4X3 &node_pos, XMFLOAT4X3 &node_vel, XMFLOAT4X3 &node_force, _GameTimer::GameTimer& gt) {
	//XMFLOAT4X3 node_vel, node_force;
	//memset(&node_vel, 0, sizeof(XMFLOAT4X3));
	//memset(&node_force, 0, sizeof(XMFLOAT4X3));
	
	XMFLOAT3X3 Dm ;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			Dm(i, j) = node_pos(i+1, j) - node_pos(0, j);
		}
	}
	XMFLOAT3X3 Dminv; //Dm的逆
	XMVECTOR det = XMMatrixDeterminant(XMLoadFloat3x3(&Dm));
	XMStoreFloat3x3(&Dminv, XMMatrixInverse(&det, XMLoadFloat3x3(&Dm)));
	float volume = XMVectorGetX(det) * 0.1666667; //体积

	float mass = 1.0f;
	float dt = 1.0f;
	float invMass = 1 / mass;

	//求四面体顶点的下一步位置
	GetNextPos(node_pos, node_vel, gt);
	/*
	//将 next_pos 的值赋给 node_pos
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 3; j++) {
			node_pos(i, j) = node_Nextpos(i, j);
		}
	}
	*/
	

	int time = 0;
	int timeFinal = 100;
	float* volumet = (float*)malloc(timeFinal);//体积数组

	XMFLOAT3X3 Ds;
	//while (time < timeFinal) {
		memset(&Ds, 0, sizeof(XMFLOAT3X3));
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				Ds(i, j) = node_pos(i + 1, j) - node_pos(0, j);
			}
		}

		//volumet[time] = XMVectorGetX(XMMatrixDeterminant(XMLoadFloat3x3(&Ds))) * 0.1666667;

		//deformation gradient 变形梯度
		XMFLOAT3X3 F;
		XMStoreFloat3x3(&F, XMMatrixMultiply(XMLoadFloat3x3(&Ds), XMLoadFloat3x3(&Dminv)));
		//green strain    E = 0.5 * (dot(Ft, F) - I)
		XMFLOAT3X3 E;
		XMStoreFloat3x3(&E,
			0.5f * (XMMatrixMultiply(XMMatrixTranspose(XMLoadFloat3x3(&F)), XMLoadFloat3x3(&F)) - XMMatrixIdentity()));

		// lame常数
		float mu = 2;
		float la = 2;
		//双点积
		float doubleInner = 0;
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				doubleInner += E(i, j) * E(i, j);
			}
		}
		//迹
		float trE = E(0, 0) + E(1, 1) + E(2, 2);
		//能量
		float energy = doubleInner * mu + la * 0.5 * trE * trE;
		//first piola kirchhoff stress第一PK应力
		//P = F(2μE + λtr(E)I)
		XMFLOAT3X3 Piola;
		XMStoreFloat3x3(&Piola,
			XMMatrixMultiply(XMLoadFloat3x3(&F),
				2 * mu * XMLoadFloat3x3(&E) + la * trE * XMMatrixIdentity()));
		//hessian 矩阵
		XMFLOAT3X3 H;
		XMStoreFloat3x3(&H,
			-volume * XMMatrixMultiply(XMLoadFloat3x3(&Piola), XMMatrixTranspose(XMLoadFloat3x3(&Dminv))));
		XMFLOAT4X3 gradC;
		for (int i = 1; i < 4; ++i) {
			gradC(i, 0) = H(0, i - 1);
			gradC(i, 1) = H(1, i - 1);
			gradC(i, 2) = H(2, i - 1);
		}
		for (int i = 0; i < 3; ++i)
			gradC(0, i) = -H(i, 0) - H(i, 1) - H(i, 2);

		node_force = gradC;


		float sumGradC = 0;
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 3; j++)
			{
				sumGradC += invMass * (gradC(i, j) * gradC(i, j));
			}
		}
		if (sumGradC < 1e-10) return node_vel;


		DirectX::XMFLOAT3X3 dD[12] = {
			DirectX::XMFLOAT3X3(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 0.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, -1.0f),
			DirectX::XMFLOAT3X3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMFLOAT3X3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f) };

		XMFLOAT3X3 dF[12];
		XMFLOAT3X3 dE[12];
		XMFLOAT3X3 dP[12];
		XMFLOAT3X3 dH[12];
		for (int i = 0; i < 12; i++) {
			DirectX::XMMATRIX dDMatrix = DirectX::XMLoadFloat3x3(&dD[i]);
			DirectX::XMMATRIX DminvMatrix = DirectX::XMLoadFloat3x3(&Dminv);
			// dF[i] = np.dot(dD[i],Dminv)
			DirectX::XMStoreFloat3x3(&dF[i], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

			DirectX::XMMATRIX FTMatrix = DirectX::XMLoadFloat3x3(&F);
			DirectX::XMMATRIX dFMatrix = DirectX::XMLoadFloat3x3(&dF[i]);

			DirectX::XMMATRIX dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);
			DirectX::XMMATRIX FTranspose = DirectX::XMMatrixTranspose(FTMatrix);
			//dE[i] = (np.dot(dF[i, :, : ].T, F) + np.dot(F.T, dF[i, :, : ])) * 0.5
			DirectX::XMStoreFloat3x3(&dE[i], (XMMatrixMultiply(dFTranspose, FTMatrix) + XMMatrixMultiply(FTranspose, dFMatrix) * 0.5f));

			float trdE = dE[i](0, 0) + dE[i](1, 1) + dE[i](2, 2);

			DirectX::XMMATRIX EMatrix = DirectX::XMLoadFloat3x3(&E);
			DirectX::XMMATRIX dEMatrix = DirectX::XMLoadFloat3x3(&dE[i]);
			//dP[i] = np.dot(dF[i],2*mu*E + la*trE*np.identity(3)) 
			//dP[i] += np.dot(F,2*mu*dE[i] + la*trdE*np.identity(3))
			DirectX::XMStoreFloat3x3(&dP[i], XMMatrixMultiply(dFMatrix, ((2.0f * mu * EMatrix + la * trE * XMMatrixIdentity()) +
				XMMatrixMultiply(FTMatrix, (2.0f * mu * dEMatrix + la * trdE * XMMatrixIdentity())))));

			DirectX::XMMATRIX DminvTranspose = DirectX::XMMatrixTranspose(DminvMatrix);

			DirectX::XMMATRIX dPMatrix = DirectX::XMLoadFloat3x3(&dP[i]);
			//dH[i] = -volume * np.dot(dP[i], Dminv.T)
			DirectX::XMStoreFloat3x3(&dH[i], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
		}

		float K[12][12];
		for (int n = 0; n < 4; n++) {
			for (int d = 0; d < 3; d++) {
				int idx = n * 3 + d;
				K[3][idx] = dH[idx](0, 0);
				K[4][idx] = dH[idx](1, 0);
				K[5][idx] = dH[idx](2, 0);

				K[6][idx] = dH[idx](0, 1);
				K[7][idx] = dH[idx](1, 1);
				K[8][idx] = dH[idx](2, 1);

				K[9][idx] = dH[idx](0, 2);
				K[10][idx] = dH[idx](1, 2);
				K[11][idx] = dH[idx](2, 2);

				K[0][idx] = -dH[idx](0, 0) - dH[idx](0, 1) - dH[idx](0, 2);
				K[1][idx] = -dH[idx](1, 0) - dH[idx](1, 1) - dH[idx](1, 2);
				K[2][idx] = -dH[idx](2, 0) - dH[idx](2, 1) - dH[idx](2, 2);
			}
		}

		float A[12][12];
		for (int i = 0; i < 12; i++) {
			for (int j = 0; j < 12; j++) {
				if (i == j) {
					A[i][j] = 1.0 - K[i][j] * dt * dt / mass;
				}
				else {
					A[i][j] = -K[i][j] * dt * dt / mass;
				}
			}
		}

		float b[12];
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 3; ++j) {
				b[i * 3 + j] = node_vel(i, j) + dt / mass * node_force(i, j);
			}
		}

		float invA[12][12];
		Gaussian_elimination(A, invA);
		float x[12] = { 0 };
		for (int i = 0; i < 12; ++i) {
			for (int j = 0; j < 12; ++j)
			{
				x[i] += invA[i][j] * b[j];
			}
		}

		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 3; ++j)
			{
				node_vel(i, j) = x[i * 3 + j];
			}
		}
	//}

	return node_vel;
}

void mulMatrix(float a[][12], int r1, int c1, float b[], int r2, float* res)
{
	if (c1 != r2)
		return;
	else {
		for (int i = 0; i < r1; i++)
		{
			res[i] = 0;
			for (int t = 0; t < c1; t++)
				res[i] += a[i][t] * b[t];
			
		}
	}
}
void addMatrix(float a[], float b[], int r, float* res)
{
	for (int i = 0; i < r; i++)
		res[i] = a[i] + b[i];
}

/*
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
*/