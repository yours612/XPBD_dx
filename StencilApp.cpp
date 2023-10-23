#include "D3DApp.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "MeshGeometry.h"
#include "GeometryGenerator.h"
#include "FrameResource.h"
#include "Material.h"
#include "Texture.h"
#include "LoadModel.h"
#include "FEM.h"
#include "XPBD.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

int delayNum = 0;

// 帧资源循环列表数量
const int gNumFrameResources = 3;

// XPBD用
bunnyMesh mesh;
XPBD_Softbody XPBD;// (mesh, 0.0f, 0.0f);
vector<Vertex> vertices_xpbd; // 所有顶点
vector<int32_t> indices_xpbd; // 顶点序列(渲染用)

//拖动用
XMVECTOR rayOrigin;
XMVECTOR rayDir;
XMMATRIX ToLocal;
float Tmin;
bool isMoving = false;

// 渲染项
struct RenderItem {
	RenderItem() = default;

	//是否支持物理系统
	bool hasPhysical = false;

	//该几何体的世界矩阵
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	// 材质偏移
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag表明这个物体数据需要通过常量缓冲区更新计数
	int NumFramesDirty = gNumFrameResources;

	BoundingBox bound; //包围盒

	//该几何体的常量数据在objConstantBuffer中的索引
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	//该几何体的图元拓扑类型
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	//该几何体的绘制三参数
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Mirrors,
	Reflected,
	Transparent,
	Shadow,
	Count
};

class StencilApp : public D3DApp {
public:

	StencilApp();
	~StencilApp();

	virtual bool Init(HINSTANCE hInstance, int nShowCmd) override;

private:

	virtual void Draw(_GameTimer::GameTimer& gt) override;
	virtual void Update(_GameTimer::GameTimer& gt) override;

	void OnKeyboardInput();
	void UpdateCamera();
	void AnimateMaterials();
	void UpdateObjectCBs();
	void UpdateMaterialCBs();
	void UpdateMainPassCB();
	void UpdateFEMPhysical(_GameTimer::GameTimer& gt);
	void UpdateXPBDPhysical();
	void FEM_STVK(_GameTimer::GameTimer& gt);
	void FEM_Neohookean();
	void Jacobi(float A[][12], float b[], float x[]);
	void NewtonIteration(float A[][12], float b[], float x[], int maxIterations, float tolerance);
	void delayFunction();
	void XPBD_Simulate();

	//测试碰撞
	void UpdateCollision();

	void BuildFEMModel();// 构建FEM弹性体模型 
	void BuildXPBDModel(); // 构建XPBD模型

	void BuildRoomGeometry();// 构建陆地几何体
	void BuildSkullGeometry();// 构建
	void BuildBoxGeometry();
	void BuildBox2Geometry();
	void BuildCapsule();
	void BuildLiver();

	void LoadTexture();// 加载纹理
	void BuildDescriptorHeaps();// 创建描述符堆和SRV
	void BuildRootSignature();// 创建根签名
	void BuildShadersAndInputLayout();
	void BuildPSO();// 构建渲染流水线状态
	void BuildFrameResources();// 构建帧资源
	void BuildRenderItems();// 构建渲染项
	void BuildMaterials();// 构建材质
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);// 绘制渲染项

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	// 鼠标事件
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	// 窗口尺寸更改事件
	virtual void OnResize() override;

	//拾取
	virtual void Pick(int x, int y) override;
	//XMFLOAT3 ScreenToWorld(int x, int y);

private:

	// 帧资源
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;// SRV堆

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;// 常量缓冲描述符堆

	// 常量上传堆
	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<PassConstants>> mPassCB = nullptr;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;// 根签名

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;// 输入布局

	// 全部渲染项的集合
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<std::unique_ptr<RenderItem>> mAllFEMRitems;
	RenderItem* mSkullRitem = nullptr;
	RenderItem* mFEMRitem = nullptr;
	RenderItem* mXPBDRitem = nullptr;
	RenderItem* mBoxRitem = nullptr;
	RenderItem* mBox2Ritem = nullptr;
	RenderItem* mCapsuleRitem = nullptr;
	// RenderItem* mReflectedSkullRitem = nullptr;
	// RenderItem* mShadowedSkullRitem = nullptr;


	// 传递给PSO的渲染项
	// std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<RenderItem*> mOpaqueRitems;

	// 主Pass常量数据
	PassConstants mMainPassCB;
	UINT mPassCbvOffset = 0;

	ComPtr<ID3D12PipelineState> mPSO = nullptr;// 渲染流水线状态

	// 摄像机位置
	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	// mvp矩阵
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	// 球面坐标参数
	float mTheta = 1.5f * XM_PI;
	float mPhi = 1.0f; //XM_PIDIV2 - 0.1f;
	float mRadius = 20.0f;

	// 太阳位置
	float mSunTheta = 1.25 * XM_PI;
	float mSunPhi = XM_PIDIV4;

	POINT mLastMousePos;// 记录按下鼠标位置

	unordered_map<std::string, std::unique_ptr<::ofstream>> mOfs;// 调试用

	// FEM用
	float mu = 10; // 8
	float la = 10; //lame常数2000
	float damp = 0.99f; // 阻尼
	float mass = 1.0f; //
	float dt = gt.DeltaTime();
	vector<XMFLOAT3> X; // 所有顶点位置
	vector<vector<int>> X2Ver; // 每个位置对应的顶点
	vector<Vertex> vertices; // 所有顶点，每个四面体12个顶点
	vector<int32_t> indices; // 顶点序列(渲染用)
	vector<int32_t> indicesForPick; //顶点序列(拾取用)
	vector<int32_t> Tet; // 四面体数组
	vector<XMFLOAT3> Vel; // 速度数组
	vector<XMFLOAT3> Force; // 力数组
	vector<XMFLOAT3X3> Dminv; //Dm的逆（初始状态）
	int howToGetPos = 1; //0为显示，1为隐式
	bool isStable = false;
	bool startSimulation = false;
	float maxVel = 0.3f;
	bool ex = false;
	int pickVertex = -1;
	XMFLOAT3 pickForce = {0,0,0};
	//XPBD用
	//XPBD_Softbody XPBD;
	//XPBD_Softbody x(, 100, 0);
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int nShowCmd) {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try {
		StencilApp theApp;
		if (!theApp.Init(hInstance, nShowCmd))
			return 0;

		return theApp.Run();
	}
	catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

StencilApp::StencilApp() {

}

StencilApp::~StencilApp() {

}

void GetBoundingBox(BoundingBox& bound, vector<XMFLOAT3> X) {
	//创建包围盒
	XMFLOAT3 vMinf3(0x3f3f3f3f, 0x3f3f3f3f, 0x3f3f3f3f);
	XMFLOAT3 vMaxf3(-0x3f3f3f3f, -0x3f3f3f3f, -0x3f3f3f3f);
	//转换到XMVECTOR
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);
	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	//包围盒实例

	for (int i = 0; i < X.size(); ++i) {
		XMVECTOR P = XMLoadFloat3(&X[i]);
		// 计算AABB包围盒两个边界点,计算vMax和vMin(类似Min和Max函数)
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	// 构建BoundingBox
	XMStoreFloat3(&bound.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
}

bool StencilApp::Init(HINSTANCE hInstance, int nShowCmd) {
	if (!D3DApp::Init(hInstance, nShowCmd))
		return false;

	// reset命令列表离为后面初始化做准备
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	LoadTexture();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildRoomGeometry();

	BuildBoxGeometry();
	BuildBox2Geometry();
	BuildCapsule();
	BuildLiver();
	BuildXPBDModel();
	BuildFEMModel();
	BuildSkullGeometry();

	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSO();

	// 执行初始化命令
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 等待直到初始化命令完成
	FlushCommandQueue();

	return true;
}

// 渲染
void StencilApp::Draw(_GameTimer::GameTimer& gt) {
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// 重复使用记录命令的相关内存
	ThrowIfFailed(cmdListAlloc->Reset());
	// 复用命令列表及其内存
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO.Get()));

	// 设置视口和裁剪矩形
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// 设置根签名
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// 将后台缓冲资源从呈现状态转换到渲染目标状态（即准备接收图像渲染）
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),// 转换资源为后台缓冲区资源
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));// 从呈现到渲染目标转换

	// 清除后台缓冲区和深度缓冲区，并赋值
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightSteelBlue, 0, nullptr);// 清除RT背景色为暗红，并且不设置裁剪矩形
	mCommandList->ClearDepthStencilView(DepthStencilView(),	// DSV描述符句柄
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,	// FLAG
		1.0f,	// 默认深度值
		0,	// 默认模板值
		0,	// 裁剪矩形数量
		nullptr);	// 裁剪矩形指针

	// 指定将要渲染的缓冲区，即指定RTV和DSV
	mCommandList->OMSetRenderTargets(1,// 待绑定的RTV数量
		&CurrentBackBufferView(),	// 指向RTV数组的指针
		true,	// RTV对象在堆内存中是连续存放的
		&DepthStencilView());	// 指向DSV的指针

	// 设置SRV描述符堆
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// 绑定passCB根描述符
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	// 绘制渲染项
	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

	// 等到渲染完成，将后台缓冲区的状态改成呈现状态，使其之后推到前台缓冲区显示。
	// 完了，关闭命令列表，等待传入命令队列。
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));// 从渲染目标到呈现
	// 完成命令的记录关闭命令列表
	ThrowIfFailed(mCommandList->Close());

	// 等CPU将命令都准备好后，需要将待执行的命令列表加入GPU的命令队列。
	// 使用的是ExecuteCommandLists函数。
	ID3D12CommandList* commandLists[] = { mCommandList.Get() };// 声明并定义命令列表数组
	mCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);// 将命令从命令列表传至命令队列

	// 然后交换前后台缓冲区索引（这里的算法是1变0，0变1，为了让后台缓冲区索引永远为0）。
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// 将当前帧的CPU的Fence++
	mCurrFrameResource->FenceCPU = ++mCurrentFence;

	// 当GPU处理完命令后，将GPU端Fence++
	// 实质是添加一条命令，且在最后执行
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

// 每一帧的操作
void StencilApp::Update(_GameTimer::GameTimer& gt) {
	// 检测键盘按键
	OnKeyboardInput();
	// 更新摄像机
	UpdateCamera();

	// 同步CPU和GPU
	// 每帧遍历一个帧资源（多帧的话就是环形遍历）
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// 如果GPU端围栏值小于当前帧CPU端围栏值，即CPU速度快于GPU，则令CPU等待
	if (mCurrFrameResource->FenceCPU != 0 // 特殊处理前三帧
		&& mFence->GetCompletedValue() < mCurrFrameResource->FenceCPU) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->FenceCPU, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	// 更新常量数据
	AnimateMaterials();
	UpdateObjectCBs();
	UpdateMaterialCBs();
	UpdateMainPassCB();
	//UpdateCollision();

	UpdateXPBDPhysical();
	if (startSimulation) {
		//UpdateFEMPhysical(gt);
		
	}
		

}

XMFLOAT3 MatrixMul(XMFLOAT3 A, XMFLOAT3X3 B) {
	XMFLOAT3 result;

	result.x = A.x * B._11 + A.y * B._21 + A.z * B._31;
	result.y = A.x * B._12 + A.y * B._22 + A.z * B._32;
	result.z = A.x * B._13 + A.y * B._23 + A.z * B._33;

	return result;
}
float MatrixDot(XMFLOAT3 A, XMFLOAT3 B) {
	float result;

	result = A.x * B.x + A.y * B.y + A.z * B.z;

	return result;
}
XMFLOAT3 MatrixMul(XMFLOAT3 A, XMFLOAT3 B) {
	XMFLOAT3 result;

	result.x = A.x * B.x;
	result.y = A.y * B.y;
	result.z = A.z * B.z;

	return result;
}

void StencilApp::UpdateCollision() {
	for (int i = 0; i < XPBD.numVerts; ++i) {
		if (XPBD.invMass[i] == 0.0) continue;
		XMFLOAT3 p = XPBD.Xpos[i];
		XMFLOAT3 N = vertices_xpbd[i].Normal;
		float Nlen = sqrt(N.x * N.x + N.y * N.y + N.z * N.z);
		//N = { N.x / Nlen, N.y / Nlen, N.z / Nlen };
		float Dist = 0.15f;
		//p = { p.x - N.x * Dist, p.y - N.y * Dist, p.z - N.z * Dist };

		float sphereRadius = 0.25f;

		XMFLOAT3X3 worldRotateM_Box;
		XMStoreFloat3x3(&worldRotateM_Box, XMLoadFloat4x4(&mBoxRitem->World));
		XMFLOAT3X3 worldRotateMinv_Box;
		XMStoreFloat3x3(&worldRotateMinv_Box, XMMatrixInverse(nullptr, XMLoadFloat3x3(&worldRotateM_Box)));
		p = {p.x - mBoxRitem->World._41, p.y - mBoxRitem->World._42, p.z - mBoxRitem->World._43};
		//p = { p.x - N.x * Dist, p.y - N.y * Dist, p.z - N.z * Dist };
		p = MatrixMul(p, worldRotateMinv_Box);

	//与长方体检测
		XMFLOAT3 h = { 2,0.25f,0.25f };
		XMFLOAT3 v = { abs(p.x), abs(p.y), abs(p.z)};
		XMFLOAT3 u = { v.x - h.x,  v.y - h.y, v.z - h.z };
		XMFLOAT3 prevU = u;
		if (u.x < 0) u.x = 0;
		if (u.y < 0) u.y = 0;
		if (u.z < 0) u.z = 0;

		//用于碰撞之后
		XMFLOAT3 _v;
		if (v.x != p.x) _v.x = -1.0f;
		else _v.x = 1.0f;
		if (v.y != p.y) _v.y = -1.0f;
		else _v.y = 1.0f;
		if (v.z != p.z) _v.z = -1.0f;
		else _v.z = 1.0f;
		XMFLOAT3 _u = { u.x*_v.x,  u.y*_v.y, u.z*_v.z };
		prevU = { prevU.x * _v.x, prevU.y * _v.y, prevU.z * _v.z };

		float len = sqrt(u.x * u.x + u.y * u.y + u.z * u.z);
		if (len <= sphereRadius && len > 0) {
			p = { p.x + (sphereRadius - len) * _u.x / len ,
							p.y + (sphereRadius - len) * _u.y / len ,
							p.z + (sphereRadius - len) * _u.z / len };
		}
		// 穿透之后挤出
		else if (len <= 0) {
			float min = abs(prevU.x);
			float r = sphereRadius;
			if (abs(prevU.y) < min) min = abs(prevU.y);
			if (abs(prevU.z) < min) min = abs(prevU.z);

			//if (min >= 0)  r = -r;
			if (min == abs(prevU.x)){
				if (prevU.x >= 0) {
					r = -r; min = -min;
				}
				p.x += r + min;
			}
			else if (min == abs(prevU.y)) { 
				if (prevU.y >= 0) {
					r = -r; min = -min;
				}
				p.y += r + min; 
			}
			else { 
				if (prevU.z >= 0) {
					r = -r; min = -min;
				}
				p.z += r + min; 
			}
			
		
		} 
		p = MatrixMul(p, worldRotateM_Box);
		
		p = { p.x + mBoxRitem->World._41, p.y + mBoxRitem->World._42, p.z + mBoxRitem->World._43 };
		//p = { p.x + N.x * Dist, p.y + N.y * Dist, p.z + N.z * Dist };

		XPBD.Xpos[i] = p;
	// --------------------------------------------------------
	//与胶囊体检测
		XMFLOAT3 P = XPBD.Xpos[i];
		XMFLOAT3X3 worldRotateM_Capsule;
		XMStoreFloat3x3(&worldRotateM_Capsule, XMLoadFloat4x4(&mCapsuleRitem->World));

		XMFLOAT3 capusleStart = MatrixMul({0,1,0}, worldRotateM_Capsule);
		capusleStart = { capusleStart.x + mCapsuleRitem->World._41,
								capusleStart.y + mCapsuleRitem->World._42,
								capusleStart.z + mCapsuleRitem->World._43 };	
		XMFLOAT3 capusleEnd = MatrixMul({ 0,-1,0 }, worldRotateM_Capsule);
		capusleEnd = { capusleEnd.x + mCapsuleRitem->World._41,
								capusleEnd.y + mCapsuleRitem->World._42,
								capusleEnd.z + mCapsuleRitem->World._43 };
		float capusleRadius = 0.5f;
		XMFLOAT3 minDir;

		XMFLOAT3 SP = { P.x - capusleStart.x, P.y - capusleStart.y, P.z - capusleStart.z };
		XMFLOAT3 SE = { capusleEnd.x - capusleStart.x, capusleEnd.y - capusleStart.y, capusleEnd.z - capusleStart.z };

		float dot = MatrixDot(SP, SE);
		if (dot <= 0) {
			minDir = SP;
		}
		else if (dot < MatrixDot(SE, SE)) {
			XMFLOAT3 SEDir = {SE.x / sqrt(MatrixDot(SE,SE)), SE.y / sqrt(MatrixDot(SE,SE)), SE.z / sqrt(MatrixDot(SE,SE)) };
			XMFLOAT3 SP2SE = MatrixMul(SP, SEDir);
			minDir = { SP.x - SP2SE.x,SP.y - SP2SE.y,SP.z - SP2SE.z };
		}
		else
		{
			XMFLOAT3 EP = { P.x - capusleEnd.x, P.y - capusleEnd.y, P.z - capusleEnd.z };
			minDir = EP;
		}

		//if (MatrixDot(minDir, minDir) <= (sphereRadius + capusleRadius) * (sphereRadius + capusleRadius)) {
			float min = sqrt(MatrixDot(minDir, minDir));
			len = min - capusleRadius;
			if (MatrixDot(minDir, minDir) < capusleRadius * capusleRadius) //圆心都在胶囊体内
			{
				float deltaLen = capusleRadius - min + sphereRadius;
				XMFLOAT3 Dir = { minDir.x / min * deltaLen, minDir.y / min * deltaLen, minDir.z / min * deltaLen };
				P = { P.x + Dir.x, P.y + Dir.y, P.z + Dir.z};
			}
			else if (MatrixDot(minDir, minDir) <= (sphereRadius + capusleRadius) * (sphereRadius + capusleRadius)){
				/*
				float deltaLen = min - capusleRadius;
				float ratio = (deltaLen + min) / min;
				XMFLOAT3 Dir = { minDir.x * ratio, minDir.y * ratio, minDir.z * ratio };
				P = { P.x + Dir.x - minDir.x, P.y + Dir.y - minDir.y, P.z + Dir.z - minDir.z };
				*/
				float deltaLen = sphereRadius + capusleRadius - min;
				XMFLOAT3 Dir = { minDir.x / min * deltaLen, minDir.y / min * deltaLen, minDir.z / min * deltaLen };
				P = { P.x + Dir.x , P.y + Dir.y , P.z + Dir.z };
			}
		//}
		XPBD.Xpos[i] = P;
	}
}

// 检测键盘按下
void StencilApp::OnKeyboardInput() {
	const float dt = gt.DeltaTime();// 保证平滑过渡

	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
		mSunTheta -= 1.0f * dt;

	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
		mSunTheta += 1.0f * dt;

	if (GetAsyncKeyState(VK_UP) & 0x8000)
		mSunPhi -= 1.0f * dt;

	if (GetAsyncKeyState(VK_DOWN) & 0x8000)
		mSunPhi += 1.0f * dt;

	mSunPhi = MathHelper::Clamp(mSunPhi, 0.1f, XM_PIDIV2);

	if (GetAsyncKeyState('W') & 0x8000) {
		mBoxRitem->World._42 += 0.01f;
	}
	if (GetAsyncKeyState('S') & 0x8000) {
		mBoxRitem->World._42 -= 0.01f;
	}
	if (GetAsyncKeyState('A') & 0x8000) {
		mBoxRitem->World._41 -= 0.01f;
	}
	if (GetAsyncKeyState('D') & 0x8000) {
		mBoxRitem->World._41 += 0.01f;
	}
	if (GetAsyncKeyState('U') & 0x8000) {
		mCapsuleRitem->World._42 += 0.01f;
	}
	if (GetAsyncKeyState('J') & 0x8000) {
		mCapsuleRitem->World._42 -= 0.01f;
	}
	if (GetAsyncKeyState('H') & 0x8000) {
		mCapsuleRitem->World._41 -= 0.01f;
	}
	if (GetAsyncKeyState('K') & 0x8000) {
		mCapsuleRitem->World._41 += 0.01f;
	}

	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
		startSimulation = TRUE;
}

// 更新摄像机
void StencilApp::UpdateCamera() {
	// 转换球面坐标
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// 构建视图矩阵
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

// 纹理位移动画
void StencilApp::AnimateMaterials() {

}

// 更新PerObject常量
void StencilApp::UpdateObjectCBs() {
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems) {
		if (e->NumFramesDirty > 0) {
			if (e.get()->Geo->Name == "skullGeo" && e->NumFramesDirty % 3 == 0)
			{
				XMMATRIX translationMatrix = XMMatrixTranslation(10.0f, 0, 0);
				XMMATRIX worldMatrix = XMLoadFloat4x4(&e->World);
				worldMatrix = XMMatrixMultiply(worldMatrix, translationMatrix);
				XMStoreFloat4x4(&e->World, worldMatrix);
			}
			//更新Box的世界坐标
			if (e.get()->Geo->Name == "boxGeo" ) {
				e->World = mBoxRitem->World;
			}
			if (e.get()->Geo->Name == "box2Geo") {
				e->World = mBox2Ritem->World;
			}
			if (e.get()->Geo->Name == "capsuleGeo") {
				e->World = mCapsuleRitem->World;
			}

			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			// 将数据拷贝至PGU缓存
			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// 计数-1
			e->NumFramesDirty--;
		}
		else {
			if (e.get()->Geo->Name == "boxGeo") {
				e->World = mBoxRitem->World;
				XMMATRIX world = XMLoadFloat4x4(&e->World);

				ObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
				currObjectCB->CopyData(e->ObjCBIndex, objConstants);
			}
			if (e.get()->Geo->Name == "box2Geo") {
				e->World = mBox2Ritem->World;
				XMMATRIX world = XMLoadFloat4x4(&e->World);

				ObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
				currObjectCB->CopyData(e->ObjCBIndex, objConstants);
			}
			if (e.get()->Geo->Name == "capsuleGeo") {
				e->World = mCapsuleRitem->World;
				XMMATRIX world = XMLoadFloat4x4(&e->World);

				ObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
				currObjectCB->CopyData(e->ObjCBIndex, objConstants);
			}
		}
	}
}

// 上传材质常量
void StencilApp::UpdateMaterialCBs() {
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials) {
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;

			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			mat->NumFramesDirty--;// 更新计数
		}
	}
}

// 更新MainPass常量
void StencilApp::UpdateMainPassCB() {
	// 赋值viewProj参数
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));

	// 赋值light参数
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.AmbientLight = { 0.25f,0.25f,0.35f,1.0f };
	mMainPassCB.TotalTime = gt.TotalTime();

	// 球面坐标转换笛卡尔坐标
	XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, mSunTheta, mSunPhi);

	XMStoreFloat3(&mMainPassCB.Lights[0].Direction, lightDir);
	mMainPassCB.Lights[0].Strength = { 1.0f, 1.0f, 0.9f };

	// 将perPass常量数据传给缓冲区
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void StencilApp::UpdateFEMPhysical(_GameTimer::GameTimer& gt) {
	auto FEM_CB = mCurrFrameResource->FEMmodelVB.get();
	Vertex* vertexList = nullptr;
	vertexList = reinterpret_cast<Vertex*>(mFEMRitem->Geo->VertexBufferCPU->GetBufferPointer());

	BoundingBox boundingbox; 
	GetBoundingBox(boundingbox, X);
	mFEMRitem->bound = boundingbox;

	//FEM_STVK(gt);
	FEM_Neohookean();

	for (int tet = 0, vertex_number = 0; tet < Tet.size() / 4; tet++) {
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 0]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 2]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 1]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 0]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 3]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 2]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 0]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 1]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 3]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 1]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 2]];
		vertexList[vertex_number++].Pos = X[Tet[tet * 4 + 3]];
	}

	for (int t = 0; t < Tet.size(); t++)
	{
		vertexList[indices[t * 3 + 0]].Normal = { 0, 0, 0 };
		vertexList[indices[t * 3 + 1]].Normal = { 0, 0, 0 };
		vertexList[indices[t * 3 + 2]].Normal = { 0, 0, 0 };
		XMFLOAT3 v1 = vertexList[indices[t * 3 + 0]].Pos;
		XMFLOAT3 v2 = vertexList[indices[t * 3 + 1]].Pos;
		XMFLOAT3 v3 = vertexList[indices[t * 3 + 2]].Pos;

		XMVECTOR edge1 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v2));
		XMVECTOR edge2 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v3));
		XMVECTOR normal = XMVector3Normalize(DirectX::XMVector3Cross(edge1, edge2));
		XMFLOAT3 Normal;
		XMStoreFloat3(&Normal, normal);

		vertexList[indices[t * 3 + 0]].Normal.x += Normal.x;
		vertexList[indices[t * 3 + 0]].Normal.y += Normal.y;
		vertexList[indices[t * 3 + 0]].Normal.z += Normal.z;

		vertexList[indices[t * 3 + 1]].Normal.x += Normal.x;
		vertexList[indices[t * 3 + 1]].Normal.y += Normal.y;
		vertexList[indices[t * 3 + 1]].Normal.z += Normal.z;

		vertexList[indices[t * 3 + 2]].Normal.x += Normal.x;
		vertexList[indices[t * 3 + 2]].Normal.y += Normal.y;
		vertexList[indices[t * 3 + 2]].Normal.z += Normal.z;
	}


	for (int i = 0; i < mFEMRitem->Geo->DrawArgs["FEM"].vertexCount; ++i) {
		FEM_CB->CopyData(i, vertexList[i]);
	}
	mFEMRitem->Geo->VertexBufferGPU = FEM_CB->Resource();
}

void StencilApp::UpdateXPBDPhysical() {
	auto XPBD_CB = mCurrFrameResource->XPBDmodelVB.get();
	Vertex* vertexList = nullptr;
	vertexList = reinterpret_cast<Vertex*>(mXPBDRitem->Geo->VertexBufferCPU->GetBufferPointer());

	BoundingBox boundingbox;
	GetBoundingBox(boundingbox, XPBD.Xpos);
	mXPBDRitem->bound = boundingbox;
	//cout << boundingbox.Center.x << "\n";

	XPBD_Simulate();

	for (int i = 0; i < XPBD.numVerts; i++) {
		vertexList[i].Pos = XPBD.Xpos[i];
		vertexList[i].Normal = { 0,0,0 };
	}
	for (int t = 0; t < mesh.tetSurfaceTriIds.size() / 3; t++)
	{
		XMFLOAT3 v1 = vertexList[indices_xpbd[t * 3 + 0]].Pos;
		XMFLOAT3 v2 = vertexList[indices_xpbd[t * 3 + 1]].Pos;
		XMFLOAT3 v3 = vertexList[indices_xpbd[t * 3 + 2]].Pos;

		XMVECTOR edge1 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v2));
		XMVECTOR edge2 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v3));
		XMVECTOR normal = XMVector3Normalize(DirectX::XMVector3Cross(edge1, edge2));
		XMFLOAT3 Normal;
		XMStoreFloat3(&Normal, normal);

		vertexList[indices_xpbd[t * 3 + 0]].Normal.x += Normal.x;
		vertexList[indices_xpbd[t * 3 + 0]].Normal.y += Normal.y;
		vertexList[indices_xpbd[t * 3 + 0]].Normal.z += Normal.z;

		vertexList[indices_xpbd[t * 3 + 1]].Normal.x += Normal.x;
		vertexList[indices_xpbd[t * 3 + 1]].Normal.y += Normal.y;
		vertexList[indices_xpbd[t * 3 + 1]].Normal.z += Normal.z;

		vertexList[indices_xpbd[t * 3 + 2]].Normal.x += Normal.x;
		vertexList[indices_xpbd[t * 3 + 2]].Normal.y += Normal.y;
		vertexList[indices_xpbd[t * 3 + 2]].Normal.z += Normal.z;
	}


	for (int i = 0; i < mXPBDRitem->Geo->DrawArgs["XPBD"].vertexCount; ++i) {
		XPBD_CB->CopyData(i, vertexList[i]);
	}
	mXPBDRitem->Geo->VertexBufferGPU = XPBD_CB->Resource();
}

void StencilApp::delayFunction( ){//vector<XMFLOAT3> Vel) {
		// 这里放置你想要每隔两秒执行一次的代码
	cout << mFEMRitem->World._11 << "  " << mFEMRitem->World._12 << "  " << mFEMRitem->World._13 << " " << mFEMRitem->World._14 << "\n";
	cout << mFEMRitem->World._21 << "  " << mFEMRitem->World._22 << "  " << mFEMRitem->World._23 << " " << mFEMRitem->World._24 << "\n";
	cout << mFEMRitem->World._31 << "  " << mFEMRitem->World._32 << "  " << mFEMRitem->World._33 << " " << mFEMRitem->World._34 << "\n";

	//cout << Vel[4].x << "  " << Vel[4].y << " " << Vel[4].z << "\n";
	//cout << "\n";
}

void GetDH(XMFLOAT3X3 dD[12], XMFLOAT3X3 dF[12], XMFLOAT3X3 dP[12], XMFLOAT3X3 dH[12],
	int tet, vector<XMFLOAT3X3> Dminv, XMFLOAT3X3 F, XMFLOAT3X3 FinvT, XMFLOAT3X3 Finv,
	bool isStable, float mu, float la, XMVECTOR J, float LogJ, float volume) {

	// 0--------------------------------------------------
	DirectX::XMMATRIX dDMatrix = DirectX::XMLoadFloat3x3(&dD[0]);
	DirectX::XMMATRIX DminvMatrix = DirectX::XMLoadFloat3x3(&Dminv[tet]);
	// dF[i] = np.dot(dD[i],Dminv)
	DirectX::XMStoreFloat3x3(&dF[0], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	DirectX::XMMATRIX FMatrix = DirectX::XMLoadFloat3x3(&F);
	DirectX::XMMATRIX dFMatrix = DirectX::XMLoadFloat3x3(&dF[0]);
	DirectX::XMMATRIX FinvTMatrix = DirectX::XMLoadFloat3x3(&FinvT);
	DirectX::XMMATRIX FinvMatrix = DirectX::XMLoadFloat3x3(&Finv);

	DirectX::XMMATRIX dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);
	DirectX::XMMATRIX FTranspose = DirectX::XMMatrixTranspose(FMatrix);

	XMFLOAT3X3 term;
	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[0], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[0], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}
	//*/

	DirectX::XMMATRIX DminvTranspose = DirectX::XMMatrixTranspose(DminvMatrix);

	DirectX::XMMATRIX dPMatrix = DirectX::XMLoadFloat3x3(&dP[0]);
	//dH[i] = -volume * np.dot(dP[i], Dminv.T)
	DirectX::XMStoreFloat3x3(&dH[0], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));

	// 1---------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[1]);
	DirectX::XMStoreFloat3x3(&dF[1], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[1]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[1], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[1], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[1]);
	XMStoreFloat3x3(&dH[1], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 2-----------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[2]);
	DirectX::XMStoreFloat3x3(&dF[2], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[2]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[2], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[2], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[2]);
	XMStoreFloat3x3(&dH[2], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 3--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[3]);
	DirectX::XMStoreFloat3x3(&dF[3], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[3]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[3], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[3], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[3]);
	XMStoreFloat3x3(&dH[3], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 4--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[4]);
	DirectX::XMStoreFloat3x3(&dF[4], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[4]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[4], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[4], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[4]);
	XMStoreFloat3x3(&dH[4], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 5--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[5]);
	DirectX::XMStoreFloat3x3(&dF[5], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[5]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[5], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[5], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[5]);
	XMStoreFloat3x3(&dH[5], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 6--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[6]);
	DirectX::XMStoreFloat3x3(&dF[6], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[6]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[6], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[6], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[6]);
	XMStoreFloat3x3(&dH[6], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 7--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[7]);
	DirectX::XMStoreFloat3x3(&dF[7], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[7]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[7], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[7], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[7]);
	XMStoreFloat3x3(&dH[7], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 8--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[8]);
	DirectX::XMStoreFloat3x3(&dF[8], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[8]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[8], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[8], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[8]);
	XMStoreFloat3x3(&dH[8], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 9--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[9]);
	DirectX::XMStoreFloat3x3(&dF[9], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[9]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[9], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[9], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[9]);
	XMStoreFloat3x3(&dH[9], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 10--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[10]);
	DirectX::XMStoreFloat3x3(&dF[10], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[10]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {

		XMStoreFloat3x3(&dP[10], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[10], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[10]);
	XMStoreFloat3x3(&dH[10], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
	// 11--------------------------------------------------------------------------------------
	dDMatrix = DirectX::XMLoadFloat3x3(&dD[11]);
	DirectX::XMStoreFloat3x3(&dF[11], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

	dFMatrix = DirectX::XMLoadFloat3x3(&dF[11]);
	dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);

	XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

	if (!isStable) {
		//mu * XMLoadFloat3x3(&F) - mu * XMLoadFloat3x3(&FinvT) + la * LogJ * XMLoadFloat3x3(&FinvT));
		XMStoreFloat3x3(&dP[11], mu * dFMatrix
			+
			(mu - la * LogJ) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			la * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);

	}
	else {
		///*
		XMStoreFloat3x3(&dP[11], mu * dFMatrix
			+
			(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
			+
			(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
		);
	}

	dPMatrix = DirectX::XMLoadFloat3x3(&dP[11]);
	XMStoreFloat3x3(&dH[11], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
}
void GetK(float K[][12], XMFLOAT3X3 dH[12]) {
	int idx = 0;
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
	idx = 1;
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
	idx = 2;
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
	idx = 3;
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
	idx = 4;
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
	idx = 5;
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
	idx = 6;
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
	idx = 7;
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
	idx = 8;
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
	idx = 9;
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
	idx = 10;
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
	idx = 11;
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
void GetA(float A[][12], float K[][12], float mass, float DeltaTime) {
	A[0][0] = 1.0 - K[0][0] * DeltaTime * DeltaTime / mass;
	A[0][1] = -K[0][1] * DeltaTime * DeltaTime / mass;
	A[0][2] = -K[0][2] * DeltaTime * DeltaTime / mass;
	A[0][3] = -K[0][3] * DeltaTime * DeltaTime / mass;
	A[0][4] = -K[0][4] * DeltaTime * DeltaTime / mass;
	A[0][5] = -K[0][5] * DeltaTime * DeltaTime / mass;
	A[0][6] = -K[0][6] * DeltaTime * DeltaTime / mass;
	A[0][7] = -K[0][7] * DeltaTime * DeltaTime / mass;
	A[0][8] = -K[0][8] * DeltaTime * DeltaTime / mass;
	A[0][9] = -K[0][9] * DeltaTime * DeltaTime / mass;
	A[0][10] = -K[0][10] * DeltaTime * DeltaTime / mass;
	A[0][11] = -K[0][11] * DeltaTime * DeltaTime / mass;

	A[1][0] = -K[1][0] * DeltaTime * DeltaTime / mass;
	A[1][1] = 1.0 - K[1][1] * DeltaTime * DeltaTime / mass;
	A[1][2] = -K[1][2] * DeltaTime * DeltaTime / mass;
	A[1][3] = -K[1][3] * DeltaTime * DeltaTime / mass;
	A[1][4] = -K[1][4] * DeltaTime * DeltaTime / mass;
	A[1][5] = -K[1][5] * DeltaTime * DeltaTime / mass;
	A[1][6] = -K[1][6] * DeltaTime * DeltaTime / mass;
	A[1][7] = -K[1][7] * DeltaTime * DeltaTime / mass;
	A[1][8] = -K[1][8] * DeltaTime * DeltaTime / mass;
	A[1][9] = -K[1][9] * DeltaTime * DeltaTime / mass;
	A[1][10] = -K[1][10] * DeltaTime * DeltaTime / mass;
	A[1][11] = -K[1][11] * DeltaTime * DeltaTime / mass;

	A[2][0] = -K[2][0] * DeltaTime * DeltaTime / mass;
	A[2][1] = -K[2][1] * DeltaTime * DeltaTime / mass;
	A[2][2] = 1.0 - K[2][2] * DeltaTime * DeltaTime / mass;
	A[2][3] = -K[2][3] * DeltaTime * DeltaTime / mass;
	A[2][4] = -K[2][4] * DeltaTime * DeltaTime / mass;
	A[2][5] = -K[2][5] * DeltaTime * DeltaTime / mass;
	A[2][6] = -K[2][6] * DeltaTime * DeltaTime / mass;
	A[2][7] = -K[2][7] * DeltaTime * DeltaTime / mass;
	A[2][8] = -K[2][8] * DeltaTime * DeltaTime / mass;
	A[2][9] = -K[2][9] * DeltaTime * DeltaTime / mass;
	A[2][10] = -K[2][10] * DeltaTime * DeltaTime / mass;
	A[2][11] = -K[2][11] * DeltaTime * DeltaTime / mass;

	A[3][0] = -K[3][0] * DeltaTime * DeltaTime / mass;
	A[3][1] = -K[3][1] * DeltaTime * DeltaTime / mass;
	A[3][2] = -K[3][2] * DeltaTime * DeltaTime / mass;
	A[3][3] = 1.0 - K[3][3] * DeltaTime * DeltaTime / mass;
	A[3][4] = -K[3][4] * DeltaTime * DeltaTime / mass;
	A[3][5] = -K[3][5] * DeltaTime * DeltaTime / mass;
	A[3][6] = -K[3][6] * DeltaTime * DeltaTime / mass;
	A[3][7] = -K[3][7] * DeltaTime * DeltaTime / mass;
	A[3][8] = -K[3][8] * DeltaTime * DeltaTime / mass;
	A[3][9] = -K[3][9] * DeltaTime * DeltaTime / mass;
	A[3][10] = -K[3][10] * DeltaTime * DeltaTime / mass;
	A[3][11] = -K[3][11] * DeltaTime * DeltaTime / mass;

	A[4][0] = -K[4][0] * DeltaTime * DeltaTime / mass;
	A[4][1] = -K[4][1] * DeltaTime * DeltaTime / mass;
	A[4][2] = -K[4][2] * DeltaTime * DeltaTime / mass;
	A[4][3] = -K[4][3] * DeltaTime * DeltaTime / mass;
	A[4][4] = 1.0 - K[4][4] * DeltaTime * DeltaTime / mass;
	A[4][5] = -K[4][5] * DeltaTime * DeltaTime / mass;
	A[4][6] = -K[4][6] * DeltaTime * DeltaTime / mass;
	A[4][7] = -K[4][7] * DeltaTime * DeltaTime / mass;
	A[4][8] = -K[4][8] * DeltaTime * DeltaTime / mass;
	A[4][9] = -K[4][9] * DeltaTime * DeltaTime / mass;
	A[4][10] = -K[4][10] * DeltaTime * DeltaTime / mass;
	A[4][11] = -K[4][11] * DeltaTime * DeltaTime / mass;

	A[5][0] = -K[5][0] * DeltaTime * DeltaTime / mass;
	A[5][1] = -K[5][1] * DeltaTime * DeltaTime / mass;
	A[5][2] = -K[5][2] * DeltaTime * DeltaTime / mass;
	A[5][3] = -K[5][3] * DeltaTime * DeltaTime / mass;
	A[5][4] = -K[5][4] * DeltaTime * DeltaTime / mass;
	A[5][5] = 1.0 - K[5][5] * DeltaTime * DeltaTime / mass;
	A[5][6] = -K[5][6] * DeltaTime * DeltaTime / mass;
	A[5][7] = -K[5][7] * DeltaTime * DeltaTime / mass;
	A[5][8] = -K[5][8] * DeltaTime * DeltaTime / mass;
	A[5][9] = -K[5][9] * DeltaTime * DeltaTime / mass;
	A[5][10] = -K[5][10] * DeltaTime * DeltaTime / mass;
	A[5][11] = -K[5][11] * DeltaTime * DeltaTime / mass;

	A[6][0] = -K[6][0] * DeltaTime * DeltaTime / mass;
	A[6][1] = -K[6][1] * DeltaTime * DeltaTime / mass;
	A[6][2] = -K[6][2] * DeltaTime * DeltaTime / mass;
	A[6][3] = -K[6][3] * DeltaTime * DeltaTime / mass;
	A[6][4] = -K[6][4] * DeltaTime * DeltaTime / mass;
	A[6][5] = -K[6][5] * DeltaTime * DeltaTime / mass;
	A[6][6] = 1.0 - K[6][6] * DeltaTime * DeltaTime / mass;
	A[6][7] = -K[6][7] * DeltaTime * DeltaTime / mass;
	A[6][8] = -K[6][8] * DeltaTime * DeltaTime / mass;
	A[6][9] = -K[6][9] * DeltaTime * DeltaTime / mass;
	A[6][10] = -K[6][10] * DeltaTime * DeltaTime / mass;
	A[6][11] = -K[6][11] * DeltaTime * DeltaTime / mass;

	A[7][0] = -K[7][0] * DeltaTime * DeltaTime / mass;
	A[7][1] = -K[7][1] * DeltaTime * DeltaTime / mass;
	A[7][2] = -K[7][2] * DeltaTime * DeltaTime / mass;
	A[7][3] = -K[7][3] * DeltaTime * DeltaTime / mass;
	A[7][4] = -K[7][4] * DeltaTime * DeltaTime / mass;
	A[7][5] = -K[7][5] * DeltaTime * DeltaTime / mass;
	A[7][6] = -K[7][6] * DeltaTime * DeltaTime / mass;
	A[7][7] = 1.0 - K[7][7] * DeltaTime * DeltaTime / mass;
	A[7][8] = -K[7][8] * DeltaTime * DeltaTime / mass;
	A[7][9] = -K[7][9] * DeltaTime * DeltaTime / mass;
	A[7][10] = -K[7][10] * DeltaTime * DeltaTime / mass;
	A[7][11] = -K[7][11] * DeltaTime * DeltaTime / mass;

	A[8][0] = -K[8][0] * DeltaTime * DeltaTime / mass;
	A[8][1] = -K[8][1] * DeltaTime * DeltaTime / mass;
	A[8][2] = -K[8][2] * DeltaTime * DeltaTime / mass;
	A[8][3] = -K[8][3] * DeltaTime * DeltaTime / mass;
	A[8][4] = -K[8][4] * DeltaTime * DeltaTime / mass;
	A[8][5] = -K[8][5] * DeltaTime * DeltaTime / mass;
	A[8][6] = -K[8][6] * DeltaTime * DeltaTime / mass;
	A[8][7] = -K[8][7] * DeltaTime * DeltaTime / mass;
	A[8][8] = 1.0 - K[8][8] * DeltaTime * DeltaTime / mass;
	A[8][9] = -K[8][9] * DeltaTime * DeltaTime / mass;
	A[8][10] = -K[8][10] * DeltaTime * DeltaTime / mass;
	A[8][11] = -K[8][11] * DeltaTime * DeltaTime / mass;

	A[9][0] = -K[9][0] * DeltaTime * DeltaTime / mass;
	A[9][1] = -K[9][1] * DeltaTime * DeltaTime / mass;
	A[9][2] = -K[9][2] * DeltaTime * DeltaTime / mass;
	A[9][3] = -K[9][3] * DeltaTime * DeltaTime / mass;
	A[9][4] = -K[9][4] * DeltaTime * DeltaTime / mass;
	A[9][5] = -K[9][5] * DeltaTime * DeltaTime / mass;
	A[9][6] = -K[9][6] * DeltaTime * DeltaTime / mass;
	A[9][7] = -K[9][7] * DeltaTime * DeltaTime / mass;
	A[9][8] = -K[9][8] * DeltaTime * DeltaTime / mass;
	A[9][9] = 1.0 - K[9][9] * DeltaTime * DeltaTime / mass;
	A[9][10] = -K[9][10] * DeltaTime * DeltaTime / mass;
	A[9][11] = -K[9][11] * DeltaTime * DeltaTime / mass;

	A[10][0] = -K[10][0] * DeltaTime * DeltaTime / mass;
	A[10][1] = -K[10][1] * DeltaTime * DeltaTime / mass;
	A[10][2] = -K[10][2] * DeltaTime * DeltaTime / mass;
	A[10][3] = -K[10][3] * DeltaTime * DeltaTime / mass;
	A[10][4] = -K[10][4] * DeltaTime * DeltaTime / mass;
	A[10][5] = -K[10][5] * DeltaTime * DeltaTime / mass;
	A[10][6] = -K[10][6] * DeltaTime * DeltaTime / mass;
	A[10][7] = -K[10][7] * DeltaTime * DeltaTime / mass;
	A[10][8] = -K[10][8] * DeltaTime * DeltaTime / mass;
	A[10][9] = -K[10][9] * DeltaTime * DeltaTime / mass;
	A[10][10] = 1.0 - K[10][10] * DeltaTime * DeltaTime / mass;
	A[10][11] = -K[10][11] * DeltaTime * DeltaTime / mass;

	A[11][0] = -K[11][0] * DeltaTime * DeltaTime / mass;
	A[11][1] = -K[11][1] * DeltaTime * DeltaTime / mass;
	A[11][2] = -K[11][2] * DeltaTime * DeltaTime / mass;
	A[11][3] = -K[11][3] * DeltaTime * DeltaTime / mass;
	A[11][4] = -K[11][4] * DeltaTime * DeltaTime / mass;
	A[11][5] = -K[11][5] * DeltaTime * DeltaTime / mass;
	A[11][6] = -K[11][6] * DeltaTime * DeltaTime / mass;
	A[11][7] = -K[11][7] * DeltaTime * DeltaTime / mass;
	A[11][8] = -K[11][8] * DeltaTime * DeltaTime / mass;
	A[11][9] = -K[11][9] * DeltaTime * DeltaTime / mass;
	A[11][10] = -K[11][10] * DeltaTime * DeltaTime / mass;
	A[11][11] = 1.0 - K[11][11] * DeltaTime * DeltaTime / mass;
}
void GetB(float b[12], vector<XMFLOAT3> Vel, vector<int> Tet, int tet, vector<XMFLOAT3> Force
, float mass, float DeltaTime) {
	b[0] = Vel[Tet[tet * 4 + 0]].x + DeltaTime / mass * Force[Tet[tet * 4 + 0]].x;
	b[1] = Vel[Tet[tet * 4 + 0]].y + DeltaTime / mass * Force[Tet[tet * 4 + 0]].y;
	b[2] = Vel[Tet[tet * 4 + 0]].z + DeltaTime / mass * Force[Tet[tet * 4 + 0]].z;
	b[3] = Vel[Tet[tet * 4 + 1]].x + DeltaTime / mass * Force[Tet[tet * 4 + 1]].x;
	b[4] = Vel[Tet[tet * 4 + 1]].y + DeltaTime / mass * Force[Tet[tet * 4 + 1]].y;
	b[5] = Vel[Tet[tet * 4 + 1]].z + DeltaTime / mass * Force[Tet[tet * 4 + 1]].z;
	b[6] = Vel[Tet[tet * 4 + 2]].x + DeltaTime / mass * Force[Tet[tet * 4 + 2]].x;
	b[7] = Vel[Tet[tet * 4 + 2]].y + DeltaTime / mass * Force[Tet[tet * 4 + 2]].y;
	b[8] = Vel[Tet[tet * 4 + 2]].z + DeltaTime / mass * Force[Tet[tet * 4 + 2]].z;
	b[9] = Vel[Tet[tet * 4 + 3]].x + DeltaTime / mass * Force[Tet[tet * 4 + 3]].x;
	b[10] = Vel[Tet[tet * 4 + 3]].y + DeltaTime / mass * Force[Tet[tet * 4 + 3]].y;
	b[11] = Vel[Tet[tet * 4 + 3]].z + DeltaTime / mass * Force[Tet[tet * 4 + 3]].z;
}
void GetNextVel(float x[12], vector<XMFLOAT3> Vel, vector<XMFLOAT3> NextVel, vector<int> Tet, int tet, vector<bool> ifAddGravity) {
	if (x[0] < 1e-3 && x[0] > -1e-3) x[0] = 0;
	float subx_0 = x[0] - Vel[Tet[tet * 4 + 0]].x;
	if (x[1] < 1e-3 && x[1] > -1e-3) x[1] = 0;
	float suby_0 = x[1] - Vel[Tet[tet * 4 + 0]].y;
	if (x[2] < 1e-3 && x[2] > -1e-3) x[2] = 0;
	float subz_0 = x[2] - Vel[Tet[tet * 4 + 0]].z;
	if (ifAddGravity[Tet[tet * 4 + 0]] == false) {
		NextVel[Tet[tet * 4 + 0]].y += -0.0098;
		ifAddGravity[Tet[tet * 4 + 0]] = true;
	}
	NextVel[Tet[tet * 4 + 0]].x += subx_0;
	NextVel[Tet[tet * 4 + 0]].y += suby_0;
	NextVel[Tet[tet * 4 + 0]].z += subz_0;

	if (x[3] < 1e-3 && x[3] > -1e-3) x[3] = 0;
	float subx_1 = x[3] - Vel[Tet[tet * 4 + 1]].x;
	if (x[4] < 1e-3 && x[4] > -1e-3) x[4] = 0;
	float suby_1 = x[4] - Vel[Tet[tet * 4 + 1]].y;
	if (x[5] < 1e-3 && x[5] > -1e-3) x[5] = 0;
	float subz_1 = x[5] - Vel[Tet[tet * 4 + 1]].z;
	if (ifAddGravity[Tet[tet * 4 + 1]] == false) {
		NextVel[Tet[tet * 4 + 1]].y += -0.0098;
		ifAddGravity[Tet[tet * 4 + 1]] = true;
	}
	NextVel[Tet[tet * 4 + 1]].x += subx_1;
	NextVel[Tet[tet * 4 + 1]].y += suby_1;
	NextVel[Tet[tet * 4 + 1]].z += subz_1;

	if (x[6] < 1e-3 && x[6] > -1e-3) x[6] = 0;
	float subx_2 = x[6] - Vel[Tet[tet * 4 + 2]].x;
	if (x[7] < 1e-3 && x[7] > -1e-3) x[7] = 0;
	float suby_2 = x[7] - Vel[Tet[tet * 4 + 2]].y;
	if (x[8] < 1e-3 && x[8] > -1e-3) x[8] = 0;
	float subz_2 = x[8] - Vel[Tet[tet * 4 + 2]].z;
	if (ifAddGravity[Tet[tet * 4 + 2]] == false) {
		NextVel[Tet[tet * 4 + 2]].y += -0.0098;
		ifAddGravity[Tet[tet * 4 + 2]] = true;
	}
	NextVel[Tet[tet * 4 + 2]].x += subx_2;
	NextVel[Tet[tet * 4 + 2]].y += suby_2;
	NextVel[Tet[tet * 4 + 2]].z += subz_2;

	if (x[9] < 1e-3 && x[9] > -1e-3) x[9] = 0;
	float subx_3 = x[9] - Vel[Tet[tet * 4 + 3]].x;
	if (x[10] < 1e-3 && x[10] > -1e-3) x[10] = 0;
	float suby_3 = x[10] - Vel[Tet[tet * 4 + 3]].y;
	if (x[11] < 1e-3 && x[11] > -1e-3) x[11] = 0;
	float subz_3 = x[11] - Vel[Tet[tet * 4 + 3]].z;
	if (ifAddGravity[Tet[tet * 4 + 3]] == false) {
		NextVel[Tet[tet * 4 + 3]].y += -0.0098;
		ifAddGravity[Tet[tet * 4 + 3]] = true;
	}
	NextVel[Tet[tet * 4 + 3]].x += subx_3;
	NextVel[Tet[tet * 4 + 3]].y += suby_3;
	NextVel[Tet[tet * 4 + 3]].z += subz_3;
}

void StencilApp::Jacobi(float A[][12], float b[], float x[]) {
	//Ax = b
	//A = L+D+U
	//DX = b - (L+U)X
	//X^k+1 = B X^k + fj
	//    B = D-1(L+U)
	//    fj = D-1b

	//计算矩阵B（雅可比矩阵）
	float B[12][12];
	///*
	B[0][0] = 0;
	B[0][1] = -A[0][1] / A[0][0];
	B[0][2] = -A[0][2] / A[0][0];
	B[0][3] = -A[0][3] / A[0][0];
	B[0][4] = -A[0][4] / A[0][0];
	B[0][5] = -A[0][5] / A[0][0];
	B[0][6] = -A[0][6] / A[0][0];
	B[0][7] = -A[0][7] / A[0][0];
	B[0][8] = -A[0][8] / A[0][0];
	B[0][9] = -A[0][9] / A[0][0];
	B[0][10] = -A[0][10] / A[0][0];
	B[0][11] = -A[0][11] / A[0][0];

	B[1][0] = -A[1][0] / A[1][1];
	B[1][1] = 0;
	B[1][2] = -A[1][2] / A[1][1];
	B[1][3] = -A[1][3] / A[1][1];
	B[1][4] = -A[1][4] / A[1][1];
	B[1][5] = -A[1][5] / A[1][1];
	B[1][6] = -A[1][6] / A[1][1];
	B[1][7] = -A[1][7] / A[1][1];
	B[1][8] = -A[1][8] / A[1][1];
	B[1][9] = -A[1][9] / A[1][1];
	B[1][10] = -A[1][10] / A[1][1];
	B[1][11] = -A[1][11] / A[1][1];

	B[2][0] = -A[2][0] / A[2][2];
	B[2][1] = -A[2][1] / A[2][2];
	B[2][2] = 0;
	B[2][3] = -A[2][3] / A[2][2];
	B[2][4] = -A[2][4] / A[2][2];
	B[2][5] = -A[2][5] / A[2][2];
	B[2][6] = -A[2][6] / A[2][2];
	B[2][7] = -A[2][7] / A[2][2];
	B[2][8] = -A[2][8] / A[2][2];
	B[2][9] = -A[2][9] / A[2][2];
	B[2][10] = -A[2][10] / A[2][2];
	B[2][11] = -A[2][11] / A[2][2];

	B[3][0] = -A[3][0] / A[3][3];
	B[3][1] = -A[3][1] / A[3][3];
	B[3][2] = -A[3][2] / A[3][3];
	B[3][3] = 0;
	B[3][4] = -A[3][4] / A[3][3];
	B[3][5] = -A[3][5] / A[3][3];
	B[3][6] = -A[3][6] / A[3][3];
	B[3][7] = -A[3][7] / A[3][3];
	B[3][8] = -A[3][8] / A[3][3];
	B[3][9] = -A[3][9] / A[3][3];
	B[3][10] = -A[3][10] / A[3][3];
	B[3][11] = -A[3][11] / A[3][3];

	B[4][0] = -A[4][0] / A[4][4];
	B[4][1] = -A[4][1] / A[4][4];
	B[4][2] = -A[4][2] / A[4][4];
	B[4][3] = -A[4][3] / A[4][4];
	B[4][4] = 0;
	B[4][5] = -A[4][5] / A[4][4];
	B[4][6] = -A[4][6] / A[4][4];
	B[4][7] = -A[4][7] / A[4][4];
	B[4][8] = -A[4][8] / A[4][4];
	B[4][9] = -A[4][9] / A[4][4];
	B[4][10] = -A[4][10] / A[4][4];
	B[4][11] = -A[4][11] / A[4][4];

	B[5][0] = -A[5][0] / A[5][5];
	B[5][1] = -A[5][1] / A[5][5];
	B[5][2] = -A[5][2] / A[5][5];
	B[5][3] = -A[5][3] / A[5][5];
	B[5][4] = -A[5][4] / A[5][5];
	B[5][5] = 0;
	B[5][6] = -A[5][6] / A[5][5];
	B[5][7] = -A[5][7] / A[5][5];
	B[5][8] = -A[5][8] / A[5][5];
	B[5][9] = -A[5][9] / A[5][5];
	B[5][10] = -A[5][10] / A[5][5];
	B[5][11] = -A[5][11] / A[5][5];

	B[6][0] = -A[6][0] / A[6][6];
	B[6][1] = -A[6][1] / A[6][6];
	B[6][2] = -A[6][2] / A[6][6];
	B[6][3] = -A[6][3] / A[6][6];
	B[6][4] = -A[6][4] / A[6][6];
	B[6][5] = -A[6][5] / A[6][6];
	B[6][6] = 0;
	B[6][7] = -A[6][7] / A[6][6];
	B[6][8] = -A[6][8] / A[6][6];
	B[6][9] = -A[6][9] / A[6][6];
	B[6][10] = -A[6][10] / A[6][6];
	B[6][11] = -A[6][11] / A[6][6];

	B[7][0] = -A[7][0] / A[7][7];
	B[7][1] = -A[7][1] / A[7][7];
	B[7][2] = -A[7][2] / A[7][7];
	B[7][3] = -A[7][3] / A[7][7];
	B[7][4] = -A[7][4] / A[7][7];
	B[7][5] = -A[7][5] / A[7][7];
	B[7][6] = -A[7][6] / A[7][7];
	B[7][7] = 0;
	B[7][8] = -A[7][8] / A[7][7];
	B[7][9] = -A[7][9] / A[7][7];
	B[7][10] = -A[7][10] / A[7][7];
	B[7][11] = -A[7][11] / A[7][7];

	B[8][0] = -A[8][0] / A[8][8];
	B[8][1] = -A[8][1] / A[8][8];
	B[8][2] = -A[8][2] / A[8][8];
	B[8][3] = -A[8][3] / A[8][8];
	B[8][4] = -A[8][4] / A[8][8];
	B[8][5] = -A[8][5] / A[8][8];
	B[8][6] = -A[8][6] / A[8][8];
	B[8][7] = -A[8][7] / A[8][8];
	B[8][8] = 0;
	B[8][9] = -A[8][9] / A[8][8];
	B[8][10] = -A[8][10] / A[8][8];
	B[8][11] = -A[8][11] / A[8][8];

	B[9][0] = -A[9][0] / A[9][9];
	B[9][1] = -A[9][1] / A[9][9];
	B[9][2] = -A[9][2] / A[9][9];
	B[9][3] = -A[9][3] / A[9][9];
	B[9][4] = -A[9][4] / A[9][9];
	B[9][5] = -A[9][5] / A[9][9];
	B[9][6] = -A[9][6] / A[9][9];
	B[9][7] = -A[9][7] / A[9][9];
	B[9][8] = -A[9][8] / A[9][9];
	B[9][9] = 0;
	B[9][10] = -A[9][10] / A[9][9];
	B[9][11] = -A[9][11] / A[9][9];

	B[10][0] = -A[10][0] / A[10][10];
	B[10][1] = -A[10][1] / A[10][10];
	B[10][2] = -A[10][2] / A[10][10];
	B[10][3] = -A[10][3] / A[10][10];
	B[10][4] = -A[10][4] / A[10][10];
	B[10][5] = -A[10][5] / A[10][10];
	B[10][6] = -A[10][6] / A[10][10];
	B[10][7] = -A[10][7] / A[10][10];
	B[10][8] = -A[10][8] / A[10][10];
	B[10][9] = -A[10][9] / A[10][10];
	B[10][10] = 0;
	B[10][11] = -A[10][11] / A[10][10];

	B[11][0] = -A[11][0] / A[11][11];
	B[11][1] = -A[11][1] / A[11][11];
	B[11][2] = -A[11][2] / A[11][11];
	B[11][3] = -A[11][3] / A[11][11];
	B[11][4] = -A[11][4] / A[11][11];
	B[11][5] = -A[11][5] / A[11][11];
	B[11][6] = -A[11][6] / A[11][11];
	B[11][7] = -A[11][7] / A[11][11];
	B[11][8] = -A[11][8] / A[11][11];
	B[11][9] = -A[11][9] / A[11][11];
	B[11][10] = -A[11][10] / A[11][11];
	B[11][11] = 0;
	//*/
	/*
	for (int i = 0; i < 12; ++i) {
		for (int j = 0; j < 12; ++j) {
			if (i != j)
				B[i][j] = -A[i][j] / A[i][i];
			else
				B[i][j] = 0;
		}
	}
	*/

	//计算fj
	float fj[12] = { 0 };
	fj[0] = b[0] / A[0][0];
	fj[1] = b[1] / A[1][1];
	fj[2] = b[2] / A[2][2];
	fj[3] = b[3] / A[3][3];
	fj[4] = b[4] / A[4][4];
	fj[5] = b[5] / A[5][5];
	fj[6] = b[6] / A[6][6];
	fj[7] = b[7] / A[7][7];
	fj[8] = b[8] / A[8][8];
	fj[9] = b[9] / A[9][9];
	fj[10] = b[10] / A[10][10];
	fj[11] = b[11] / A[11][11];
	
	/*
	for (int i = 0; i < 12; ++i) {
		fj[i] = b[i] / A[i][i];
	}
	*/

	//第一次迭代
	float res[12] = { 0 };
	mulMatrix(B, 12, 12, x, 12, res);
	addMatrix(res, fj, 12, res);
	//迭代
	for (int i = 1; i < 10; ++i) {
		x[0] = res[0];
		x[1] = res[1];
		x[2] = res[2];
		x[3] = res[3];
		x[4] = res[4];
		x[5] = res[5];
		x[6] = res[6];
		x[7] = res[7];
		x[8] = res[8];
		x[9] = res[9];
		x[10] = res[10];
		x[11] = res[11];
		/*
		for (int j = 0; j < 12; ++j) {
			x[j] = res[j];
			
		}
		*/
		mulMatrix(B, 12, 12, x, 12, res);
		addMatrix(res, fj, 12, res);
	}
}
void ComputePartialDerivatives(float A[][12], const float* x, int i, float epsilon, float* derivatives) {
	float x_perturbed[12];
	for (int j = 0; j < 12; ++j) {
		x_perturbed[j] = x[j];
	}
	x_perturbed[i] += epsilon;

	for (int j = 0; j < 12; ++j) {
		derivatives[j] = (A[j][i] * x_perturbed[i] - A[j][i] * x[i]) / epsilon;
	}
}
// Compute the Jacobian matrix
void ComputeJacobian(float A[][12], float x[], float epsilon, float jacobian[][12]) {
	for (int i = 0; i < 12; ++i) {
		float partial_derivatives[12];
		ComputePartialDerivatives(A, x, i, epsilon, partial_derivatives);

		for (int j = 0; j < 12; ++j) {
			jacobian[i][j] = partial_derivatives[j];
		}
	}
}
void SolveLinearSystem(float A[][12], float b[], float x[], int maxIterations, float tolerance) {
	for (int iteration = 0; iteration < maxIterations; ++iteration) {
		float maxResidual = 0.0;

		for (int i = 0; i < 12; ++i) {
			float sum = 0.0;
			for (int j = 0; j < 12; ++j) {
				if (i != j) {
					sum += A[i][j] * x[j];
				}
			}

			float newX = (b[i] - sum) / A[i][i];
			maxResidual = max(maxResidual, std::abs(newX - x[i]));
			x[i] = newX;
		}

		if (maxResidual < tolerance) {
			return;
		}
	}

}
void StencilApp::NewtonIteration(float A[][12], float b[], float x[], int maxIterations, float tolerance) {
	float epsilon = 1e-6;

	// Main iteration loop
	for (int iteration = 0; iteration < maxIterations; ++iteration) {
		float residual[12];

		// Calculate residual: A * x - b
		for (int i = 0; i < 12; ++i) {
			residual[i] = 0.0;
			for (int j = 0; j < 12; ++j) {
				residual[i] += A[i][j] * x[j];
			}
			residual[i] -= b[i];
		}

		float jacobian[12][12];
		ComputeJacobian(A, x, epsilon, jacobian);

		float delta_x[12];
		SolveLinearSystem(jacobian, residual, delta_x, maxIterations, tolerance);

		// Update x with delta_x
		for (int i = 0; i < 12; ++i) {
			x[i] -= delta_x[i];
		}

		// Check for convergence based on norm of delta_x
		float deltaNorm = 0.0;
		for (int i = 0; i < 12; ++i) {
			deltaNorm += delta_x[i] * delta_x[i];
		}
		deltaNorm = sqrt(deltaNorm);

		if (deltaNorm < tolerance) {
			return;
		}
	}
}

void StencilApp::FEM_STVK(_GameTimer::GameTimer& gt) {
	for (int l = 0; l < 2; ++l) {
		vector<bool> ifAddGravity;
		ifAddGravity.resize(X.size(), false);

		if (GetAsyncKeyState(VK_SPACE)) {
			for (int i = 0; i < X.size(); i++)
				Vel[i].y += 0.01f;
		}

		for (int i = 0; i < X.size(); ++i) {
			Force[i] = { 0, 0, 0 };
		}
		if (pickVertex != -1) {
			Force[pickVertex] = pickForce;
		}


		if (delayNum % 1000 == 0) cout << pickVertex << "\n";

		/**/
		vector<XMFLOAT3> NextVel;
		NextVel.resize(Vel.size());
		for (int i = 0; i < X.size(); i++) {
			NextVel[i] = { 0, 0, 0 };
		}

		for (int tet = 0; tet < Tet.size() / 4; ++tet) {
			XMFLOAT3X3 dm = Build_Edge_Matrix(tet, X, Tet);
			XMFLOAT3X3 Test = Dminv[tet];
			XMStoreFloat3x3(&Test, XMMatrixInverse(nullptr, XMLoadFloat3x3(&Dminv[tet])));

			XMVECTOR det = XMMatrixDeterminant(XMLoadFloat3x3(&dm));
			float volume = XMVectorGetX(det) * 0.1666667; //体积

			//deformation gradient 变形梯度
			XMFLOAT3X3 F;
			XMStoreFloat3x3(&F, XMMatrixMultiply(XMLoadFloat3x3(&dm), XMLoadFloat3x3(&Dminv[tet])));
			if (F(0, 0) - 1 <= 1e-5 && F(0, 0) - 1 >= -1e-5) F(0, 0) = 1.0f;
			if (std::abs(F(0, 0)) < 1e-5) F(0, 0) = 0;
			if (F(0, 1) - 1 <= 1e-5 && F(0, 1) - 1 >= -1e-5) F(0, 1) = 1.0f;
			if (std::abs(F(0, 1)) < 1e-5) F(0, 1) = 0;
			if (F(0, 2) - 1 <= 1e-5 && F(0, 2) - 1 >= -1e-5) F(0, 2) = 1.0f;
			if (std::abs(F(0, 2)) < 1e-5) F(0, 2) = 0;
			if (F(1, 0) - 1 <= 1e-5 && F(1, 0) - 1 >= -1e-5) F(1, 0) = 1.0f;
			if (std::abs(F(1, 0)) < 1e-5) F(1, 0) = 0;
			if (F(1, 1) - 1 <= 1e-5 && F(1, 1) - 1 >= -1e-5) F(1, 1) = 1.0f;
			if (std::abs(F(1, 1)) < 1e-5) F(1, 1) = 0;
			if (F(1, 2) - 1 <= 1e-5 && F(1, 2) - 1 >= -1e-5) F(1, 2) = 1.0f;
			if (std::abs(F(1, 2)) < 1e-5) F(1, 2) = 0;
			if (F(2, 0) - 1 <= 1e-5 && F(2, 0) - 1 >= -1e-5) F(2, 0) = 1.0f;
			if (std::abs(F(2, 0)) < 1e-5) F(2, 0) = 0;
			if (F(2, 1) - 1 <= 1e-5 && F(2, 1) - 1 >= -1e-5) F(2, 1) = 1.0f;
			if (std::abs(F(2, 1)) < 1e-5) F(2, 1) = 0;
			if (F(2, 2) - 1 <= 1e-5 && F(2, 2) - 1 >= -1e-5) F(2, 2) = 1.0f;
			if (std::abs(F(2, 2)) < 1e-5) F(2, 2) = 0;
			/*
			for (int i = 0; i < 3; ++i) {
				for (int j = 0; j < 3; ++j) {
					if (F(i, j) - 1 <= 1e-5 && F(i, j) - 1 >= -1e-5) F(i, j) = 1.0f;

					if (std::abs(F(i, j)) < 1e-5) F(i, j) = 0;
				}
			}
			*/

			//green strain    E = 0.5 * (dot(Ft, F) - I)
			XMFLOAT3X3 E;
			XMStoreFloat3x3(&E,
				0.5f * (XMMatrixMultiply(XMMatrixTranspose(XMLoadFloat3x3(&F)), XMLoadFloat3x3(&F)) - XMMatrixIdentity()));
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
				-volume * XMMatrixMultiply(XMLoadFloat3x3(&Piola), XMMatrixTranspose(XMLoadFloat3x3(&Dminv[tet]))));
			XMFLOAT3 f1 = { H(0,0), H(1,0), H(2,0) };
			XMFLOAT3 f2 = { H(0,1), H(1,1), H(2,1) };
			XMFLOAT3 f3 = { H(0,2), H(1,2), H(2,2) };
			XMFLOAT3 f0 = { -f1.x - f2.x - f3.x, -f1.y - f2.y - f3.y, -f1.z - f2.z - f3.z };

			Force[Tet[tet * 4 + 0]].x += f0.x; Force[Tet[tet * 4 + 0]].y += f0.y; Force[Tet[tet * 4 + 0]].z += f0.z;
			Force[Tet[tet * 4 + 1]].x += f1.x; Force[Tet[tet * 4 + 1]].y += f1.y; Force[Tet[tet * 4 + 1]].z += f1.z;
			Force[Tet[tet * 4 + 2]].x += f2.x; Force[Tet[tet * 4 + 2]].y += f2.y; Force[Tet[tet * 4 + 2]].z += f2.z;
			Force[Tet[tet * 4 + 3]].x += f3.x; Force[Tet[tet * 4 + 3]].y += f3.y; Force[Tet[tet * 4 + 3]].z += f3.z;
			for (int i = 0; i < 4; ++i) {
				if (Force[Tet[tet * 4 + i]].x > 5.0) Force[Tet[tet * 4 + i]].x = 5;
				else if (Force[Tet[tet * 4 + i]].x < -5.0) Force[Tet[tet * 4 + i]].x = -5;
				if (Force[Tet[tet * 4 + i]].y > 5.0) Force[Tet[tet * 4 + i]].y = 5;
				else if (Force[Tet[tet * 4 + i]].y < -5.0) Force[Tet[tet * 4 + i]].y = -5;
				if (Force[Tet[tet * 4 + i]].z > 5.0) Force[Tet[tet * 4 + i]].z = 5;
				else if (Force[Tet[tet * 4 + i]].z < -5.0) Force[Tet[tet * 4 + i]].z = -5;
			}

			switch (howToGetPos)
			{
				// 隐式积分
			case 1: {// 隐式积分需要
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
					DirectX::XMMATRIX DminvMatrix = DirectX::XMLoadFloat3x3(&Dminv[tet]);
					// dF[i] = np.dot(dD[i],Dminv)
					DirectX::XMStoreFloat3x3(&dF[i], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

					DirectX::XMMATRIX FMatrix = DirectX::XMLoadFloat3x3(&F);
					DirectX::XMMATRIX dFMatrix = DirectX::XMLoadFloat3x3(&dF[i]);

					DirectX::XMMATRIX dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);
					DirectX::XMMATRIX FTranspose = DirectX::XMMatrixTranspose(FMatrix);
					//dE[i] = (np.dot(dF[i, :, : ].T, F) + np.dot(F.T, dF[i, :, : ])) * 0.5
					DirectX::XMStoreFloat3x3(&dE[i], (XMMatrixMultiply(dFTranspose, FMatrix) + XMMatrixMultiply(FTranspose, dFMatrix)) * 0.5f);

					float trdE = dE[i](0, 0) + dE[i](1, 1) + dE[i](2, 2);

					DirectX::XMMATRIX EMatrix = DirectX::XMLoadFloat3x3(&E);
					DirectX::XMMATRIX dEMatrix = DirectX::XMLoadFloat3x3(&dE[i]);
					//dP[i] = np.dot(dF[i],2*mu*E + la*trE*np.identity(3)) 
					//dP[i] += np.dot(F,2*mu*dE[i] + la*trdE*np.identity(3))
					XMStoreFloat3x3(&dP[i], XMMatrixMultiply(dFMatrix, 2.0f * mu * EMatrix + la * trE * XMMatrixIdentity()) +
						XMMatrixMultiply(FMatrix, 2.0f * mu * dEMatrix + la * trdE * XMMatrixIdentity()));

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
							A[i][j] = 1.0 - K[i][j] * gt.DeltaTime() * gt.DeltaTime() / mass;
						}
						else {
							A[i][j] = -K[i][j] * gt.DeltaTime() * gt.DeltaTime() / mass;
						}
					}
				}

				float b[12];
				for (int i = 0; i < 4; ++i) {
					b[i * 3 + 0] = Vel[Tet[tet * 4 + i]].x + gt.DeltaTime() / mass * Force[Tet[tet * 4 + i]].x;
					b[i * 3 + 1] = Vel[Tet[tet * 4 + i]].y + gt.DeltaTime() / mass * Force[Tet[tet * 4 + i]].y;
					b[i * 3 + 2] = Vel[Tet[tet * 4 + i]].z + gt.DeltaTime() / mass * Force[Tet[tet * 4 + i]].z;
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
					//Vel[Tet[tet * 4 + i]].x += x[i * 3 + 0];
					//Vel[Tet[tet * 4 + i]].y += x[i * 3 + 1];
					//Vel[Tet[tet * 4 + i]].z += x[i * 3 + 2];
					if (x[i * 3 + 0] < 1e-3 && x[i * 3 + 0] > -1e-3) x[i * 3 + 0] = 0;
					float subx = x[i * 3 + 0] - Vel[Tet[tet * 4 + i]].x;
					if (x[i * 3 + 1] < 1e-3 && x[i * 3 + 1] > -1e-3) x[i * 3 + 1] = 0;
					float suby = x[i * 3 + 1] - Vel[Tet[tet * 4 + i]].y;
					if (x[i * 3 + 2] < 1e-3 && x[i * 3 + 2] > -1e-3) x[i * 3 + 2] = 0;
					float subz = x[i * 3 + 2] - Vel[Tet[tet * 4 + i]].z;

					if (ifAddGravity[Tet[tet * 4 + i]] == false) {
						NextVel[Tet[tet * 4 + i]].y += -0.0098;
						ifAddGravity[Tet[tet * 4 + i]] = true;
					}

					NextVel[Tet[tet * 4 + i]].x += subx;
					if (abs(NextVel[Tet[tet * 4 + i]].x) < 1e-6) { 
						NextVel[Tet[tet * 4 + i]].x = 0; }
					NextVel[Tet[tet * 4 + i]].y += suby;
					if (abs(NextVel[Tet[tet * 4 + i]].y) < 1e-6) NextVel[Tet[tet * 4 + i]].y = 0;
					NextVel[Tet[tet * 4 + i]].z += subz;
					if (abs(NextVel[Tet[tet * 4 + i]].z) < 1e-6) NextVel[Tet[tet * 4 + i]].z = 0;
				}
			}
				  break;
			default: //显示积分
				break;
			}
		}



		// Update V[], damping V[]
		for (int i = 0; i < X.size(); ++i) {
			if (howToGetPos == 0) {
				Vel[i].x += gt.DeltaTime() * Force[i].x;
				Vel[i].y += gt.DeltaTime() * Force[i].y;
				Vel[i].z += gt.DeltaTime() * Force[i].z;
			}
			else {
				Vel[i].x += NextVel[i].x;
				Vel[i].y += NextVel[i].y;
				Vel[i].z += NextVel[i].z;

			}
			
			
			if (Vel[i].x > 1) Vel[i].x = 1;
			else if (Vel[i].x < -1) Vel[i].x = -1;
			if (Vel[i].y > 1) Vel[i].y = 1;
			else if (Vel[i].y < -1) Vel[i].y = -1;
			if (Vel[i].z > 1) Vel[i].z = 1;
			else if (Vel[i].z < -1) Vel[i].z = -1;
			
			
			Vel[i].x *= damp; Vel[i].y *= damp; Vel[i].z *= damp;
		}

		// 调用周期性函数
		//if (delayNum % 1000 == 0) delayFunction(Vel);
		delayNum++;

		//求位置 
		for (int i = 0; i < X.size(); ++i) {
			float length;
			XMStoreFloat(&length, XMVector3Length(XMLoadFloat3(&Vel[i])));
			if (length < 0.009f) {
				Vel[i].x = Vel[i].y = Vel[i].z = 0.0f;
			}
			if (delayNum % 1000 == 0) {
				cout << "落地前：\n";
				//delayFunction(Vel);
			}

			//if (i != 0) {
			X[i].x += gt.DeltaTime() * Vel[i].x;
			X[i].y += gt.DeltaTime() * Vel[i].y;
			X[i].z += gt.DeltaTime() * Vel[i].z;
			//}
			

			//碰撞检测
			float floor = -3.5f;
			XMFLOAT3 floorN = { 0, 1, 0 };
			if (X[i].y < floor) {
				X[i].y = floor;

				if (delayNum % 1000 == 0) {
					cout << "\n";
					cout << "落地后：\n";
				}
				if (Vel[i].x * floorN.x + Vel[i].y * floorN.y + Vel[i].z * floorN.z < 0) {
					XMVECTOR Vn = XMVector3Dot(XMLoadFloat3(&Vel[i]), XMLoadFloat3(&floorN)) * XMLoadFloat3(&floorN);
					XMVECTOR Vt = XMLoadFloat3(&Vel[i]) - Vn;

					float mu_N = 0.35f; //摩擦系数
					float a = 0.95f; //比例系数，用于调整垂直于法向量的速度分量
					Vn *= -mu_N;
					Vt *= a;

					XMStoreFloat3(&Vel[i], { Vel[i].x * 0.4f, Vel[i].y * -0.5f, Vel[i].z * 0.5f });
					//if (delayNum % 1000 == 0) delayFunction(Vel);
				}
			}
		}
	}
}
void StencilApp::FEM_Neohookean() {
	for (int l = 0; l < 3; ++l) {
		int Size = X.size();
		
		vector<bool> ifAddGravity;
		ifAddGravity.resize(Size, false);

		if (GetAsyncKeyState(VK_SPACE)) {
			for (int i = 0; i < Size; i++)
				Vel[i].y += 0.01f;
		}

		Force.resize(Size);
		XMFLOAT3 F = { 0.0f,0.0f,0.0f };
		std::fill(Force.begin(), Force.end(), F);
		/*
		for (int i = 0; i < Size; i++) {
			Force[i] = { 0, 0, 0 };
		}
		*/
		if (pickVertex != -1) {
			Force[pickVertex] = pickForce;
		}

		vector<XMFLOAT3> NextVel;
		NextVel.resize(Size);
		std::fill(NextVel.begin(), NextVel.end(), F);
			
		for (int tet = 0; tet < Tet.size() / 4; ++tet) {
			XMFLOAT3X3 dm = Build_Edge_Matrix(tet, X, Tet);
			//XMFLOAT3X3 Test = Dminv[tet];
			//XMStoreFloat3x3(&Test, XMMatrixInverse(nullptr, XMLoadFloat3x3(&Dminv[tet])));

			XMVECTOR det = XMMatrixDeterminant(XMLoadFloat3x3(&dm));
			float volume = XMVectorGetX(det) * 0.1666667; //体积

			//deformation gradient 变形梯度
			XMFLOAT3X3 F;
			XMStoreFloat3x3(&F, XMMatrixMultiply(XMLoadFloat3x3(&dm), XMLoadFloat3x3(&Dminv[tet])));
			if (F(0, 0) - 1 <= 1e-5 && F(0, 0) - 1 >= -1e-5) F(0, 0) = 1.0f;
			if (std::abs(F(0, 0)) < 1e-5) F(0, 0) = 0;
			if (F(0, 1) - 1 <= 1e-5 && F(0, 1) - 1 >= -1e-5) F(0, 1) = 1.0f;
			if (std::abs(F(0, 1)) < 1e-5) F(0, 1) = 0;
			if (F(0, 2) - 1 <= 1e-5 && F(0, 2) - 1 >= -1e-5) F(0, 2) = 1.0f;
			if (std::abs(F(0, 2)) < 1e-5) F(0, 2) = 0;
			if (F(1, 0) - 1 <= 1e-5 && F(1, 0) - 1 >= -1e-5) F(1, 0) = 1.0f;
			if (std::abs(F(1, 0)) < 1e-5) F(1, 0) = 0;
			if (F(1, 1) - 1 <= 1e-5 && F(1, 1) - 1 >= -1e-5) F(1, 1) = 1.0f;
			if (std::abs(F(1, 1)) < 1e-5) F(1, 1) = 0;
			if (F(1, 2) - 1 <= 1e-5 && F(1, 2) - 1 >= -1e-5) F(1, 2) = 1.0f;
			if (std::abs(F(1, 2)) < 1e-5) F(1, 2) = 0;
			if (F(2, 0) - 1 <= 1e-5 && F(2, 0) - 1 >= -1e-5) F(2, 0) = 1.0f;
			if (std::abs(F(2, 0)) < 1e-5) F(2, 0) = 0;
			if (F(2, 1) - 1 <= 1e-5 && F(2, 1) - 1 >= -1e-5) F(2, 1) = 1.0f;
			if (std::abs(F(2, 1)) < 1e-5) F(2, 1) = 0;
			if (F(2, 2) - 1 <= 1e-5 && F(2, 2) - 1 >= -1e-5) F(2, 2) = 1.0f;
			if (std::abs(F(2, 2)) < 1e-5) F(2, 2) = 0;
			/*
			for (int i = 0; i < 3; ++i) {
				for (int j = 0; j < 3; ++j) {
					if (F(i, j) - 1 <= 1e-5 && F(i, j) - 1 >= -1e-5) F(i, j) = 1.0f;

					if (std::abs(F(i, j)) < 1e-5) F(i, j) = 0;
				}
			}
			*/

			XMFLOAT3X3 Finv;
			XMStoreFloat3x3(&Finv, XMMatrixInverse(nullptr, XMLoadFloat3x3(&F)));
			XMFLOAT3X3 FinvT;
			XMStoreFloat3x3(&FinvT, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat3x3(&F))));
			XMFLOAT3X3 FtF;
			XMStoreFloat3x3(&FtF, XMMatrixMultiply(XMMatrixTranspose(XMLoadFloat3x3(&F)), XMLoadFloat3x3(&F)));

			XMVECTOR J = XMMatrixDeterminant(XMLoadFloat3x3(&F));
			float LogJ = std::log(XMVectorGetX(J));

			XMFLOAT3X3 Piola;
			//第一不变量
			float Ic = FtF(0, 0) + FtF(1, 1) + FtF(2, 2);
			//可压缩 neohookean 能量
			if (!isStable) {
				float energy = mu * 0.5 * (Ic - 3) - mu * LogJ + la * 0.5 * LogJ * LogJ;
				//第一 piola kirchhoff 应力	
				XMStoreFloat3x3(&Piola,
					mu * XMLoadFloat3x3(&F) - mu * XMLoadFloat3x3(&FinvT) + la * LogJ * XMLoadFloat3x3(&FinvT));
			}
			//Stable Neo-Hookean
			else {
				float ar = 1 + (3 * mu) / (4 * la);
				float energy1 = mu * 0.5 * (Ic - 3) + la * 0.5 * (XMVectorGetX(J) - ar) * (XMVectorGetX(J) - ar);
				//XMFLOAT3X3 Piola1;
				XMStoreFloat3x3(&Piola,
					//mu * XMLoadFloat3x3(&F) + la * (XMVectorGetX(J) - ar) * XMLoadFloat3x3(&Finv) * XMVectorGetX(J));
					mu * XMLoadFloat3x3(&F) + (la * LogJ - mu + 0.1 * (XMVectorGetX(J) - 1)) * XMLoadFloat3x3(&FinvT));
			}
				
			//hessian 矩阵
			XMFLOAT3X3 H;
			XMStoreFloat3x3(&H,
				-volume * XMMatrixMultiply(XMLoadFloat3x3(&Piola), XMMatrixTranspose(XMLoadFloat3x3(&Dminv[tet]))));
			XMFLOAT3 f1 = { H(0,0), H(1,0), H(2,0) };
			XMFLOAT3 f2 = { H(0,1), H(1,1), H(2,1) };
			XMFLOAT3 f3 = { H(0,2), H(1,2), H(2,2) };
			XMFLOAT3 f0 = { -f1.x - f2.x - f3.x, -f1.y - f2.y - f3.y, -f1.z - f2.z - f3.z };

			Force[Tet[tet * 4 + 0]].x += f0.x; Force[Tet[tet * 4 + 0]].y += f0.y; Force[Tet[tet * 4 + 0]].z += f0.z;
			Force[Tet[tet * 4 + 1]].x += f1.x; Force[Tet[tet * 4 + 1]].y += f1.y; Force[Tet[tet * 4 + 1]].z += f1.z;
			Force[Tet[tet * 4 + 2]].x += f2.x; Force[Tet[tet * 4 + 2]].y += f2.y; Force[Tet[tet * 4 + 2]].z += f2.z;
			Force[Tet[tet * 4 + 3]].x += f3.x; Force[Tet[tet * 4 + 3]].y += f3.y; Force[Tet[tet * 4 + 3]].z += f3.z;
			///*
			for (int i = 0; i < 4; ++i) {
				if (Force[Tet[tet * 4 + i]].x > 5.0) 
				{ Force[Tet[tet * 4 + i]].x = 5; };
				if (Force[Tet[tet * 4 + i]].y > 5.0) Force[Tet[tet * 4 + i]].y = 5;
				if (Force[Tet[tet * 4 + i]].z > 5.0) Force[Tet[tet * 4 + i]].z = 5;
			}
			//*/
			switch (howToGetPos)
			{
				// 隐式积分
			case 1: {// 隐式积分需要
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
				XMFLOAT3X3 dP[12];
				XMFLOAT3X3 dH[12];

				GetDH(dD, dF, dP, dH, tet, Dminv, F, FinvT, Finv, isStable, mu, la, J, LogJ, volume);
				/*
				for (int i = 0; i < 12; i++) {
					DirectX::XMMATRIX dDMatrix = DirectX::XMLoadFloat3x3(&dD[i]);
					DirectX::XMMATRIX DminvMatrix = DirectX::XMLoadFloat3x3(&Dminv[tet]);
					// dF[i] = np.dot(dD[i],Dminv)
					DirectX::XMStoreFloat3x3(&dF[i], DirectX::XMMatrixMultiply(dDMatrix, DminvMatrix));

					DirectX::XMMATRIX FMatrix = DirectX::XMLoadFloat3x3(&F);
					DirectX::XMMATRIX dFMatrix = DirectX::XMLoadFloat3x3(&dF[i]);
					DirectX::XMMATRIX FinvTMatrix = DirectX::XMLoadFloat3x3(&FinvT);
					DirectX::XMMATRIX FinvMatrix = DirectX::XMLoadFloat3x3(&Finv);

					DirectX::XMMATRIX dFTranspose = DirectX::XMMatrixTranspose(dFMatrix);
					DirectX::XMMATRIX FTranspose = DirectX::XMMatrixTranspose(FMatrix);

					//dP[i] = mu * dF[i]
					//dP[i] += (mu - la * logJ) * np.dot(np.dot(FinvT, dF[i].T), FinvT)
					//dP[i] += la * (term[0,0] + term[1,1] + term[2,2]) * FinvT
					XMFLOAT3X3 term;
					XMStoreFloat3x3(&term, XMMatrixMultiply(FinvMatrix, dFMatrix));

					if (!isStable) {
						
						XMStoreFloat3x3(&dP[i], mu* dFMatrix
							+
							(mu -la*LogJ)* XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
							+
							la * (term(0,0)+term(1,1)+term(2,2)) * FinvTMatrix
						);
						
					}
					else {
						//
						XMStoreFloat3x3(&dP[i], mu * dFMatrix
							+
							(la * LogJ - mu - 0.1 + 0.1 * XMVectorGetX(J)) * XMMatrixMultiply(XMMatrixMultiply(FinvTMatrix, dFTranspose), FinvTMatrix)
							+
							(0.1 + la) * (term(0, 0) + term(1, 1) + term(2, 2)) * FinvTMatrix
						);
					}
					//

					DirectX::XMMATRIX DminvTranspose = DirectX::XMMatrixTranspose(DminvMatrix);

					DirectX::XMMATRIX dPMatrix = DirectX::XMLoadFloat3x3(&dP[i]);
					//dH[i] = -volume * np.dot(dP[i], Dminv.T)
					DirectX::XMStoreFloat3x3(&dH[i], -volume * XMMatrixMultiply(dPMatrix, DminvTranspose));
				}
				*/
				float K[12][12];
				GetK(K, dH);
				/*
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
				*/
				float A[12][12];
				GetA(A, K, mass, gt.DeltaTime());
				/*
				for (int i = 0; i < 12; i++) {
					for (int j = 0; j < 12; j++) {
						if (i == j) {
							A[i][j] = 1.0 - K[i][j] * gt.DeltaTime() * gt.DeltaTime() / mass;
						}
						else {
							A[i][j] = -K[i][j] * gt.DeltaTime() * gt.DeltaTime() / mass;
						}
					}
				}
				*/
				float b[12];
				GetB(b, Vel, Tet, tet, Force, mass, gt.DeltaTime());
				/*
				for (int i = 0; i < 4; ++i) {
					b[i * 3 + 0] = Vel[Tet[tet * 4 + i]].x + gt.DeltaTime() / mass * Force[Tet[tet * 4 + i]].x;
					b[i * 3 + 1] = Vel[Tet[tet * 4 + i]].y + gt.DeltaTime() / mass * Force[Tet[tet * 4 + i]].y;
					b[i * 3 + 2] = Vel[Tet[tet * 4 + i]].z + gt.DeltaTime() / mass * Force[Tet[tet * 4 + i]].z;
				}
				*/
				float x[12] = { 0 };
				StencilApp::Jacobi(A, b, x);
				//---------------------------------------

				//Ax = b  ->  x = A-1*b
				/*
				float invA[12][12];
				Gaussian_elimination(A, invA);
				//float x[12] = { 0 };
				for (int i = 0; i < 12; ++i) {
					for (int j = 0; j < 12; ++j)
					{
						x[i] += invA[i][j] * b[j];
					}
				}
				*/

				///*
				for (int i = 0; i < 4; ++i) {
					/*
					float subx = x[i * 3 + 0] - Vel[Tet[tet * 4 + i]].x;
					float suby = x[i * 3 + 1] - Vel[Tet[tet * 4 + i]].y;
					float subz = x[i * 3 + 2] - Vel[Tet[tet * 4 + i]].z;
					if (abs(subx) < 1e-11) subx = 0;
					if (abs(suby) < 1e-11) suby = 0;
					if (abs(subz) < 1e-11) subz = 0;
					*/
					if (x[i * 3 + 0] < 1e-3 && x[i * 3 + 0] > -1e-3) x[i * 3 + 0] = 0;
					float subx = x[i * 3 + 0] - Vel[Tet[tet * 4 + i]].x;
					if (x[i * 3 + 1] < 1e-3 && x[i * 3 + 1] > -1e-3) x[i * 3 + 1] = 0;
					float suby = x[i * 3 + 1] - Vel[Tet[tet * 4 + i]].y;
					if (x[i * 3 + 2] < 1e-3 && x[i * 3 + 2] > -1e-3) x[i * 3 + 2] = 0;
					float subz = x[i * 3 + 2] - Vel[Tet[tet * 4 + i]].z;

					if (ifAddGravity[Tet[tet * 4 + i]] == false) {
						NextVel[Tet[tet * 4 + i]].y += -0.0098;
						ifAddGravity[Tet[tet * 4 + i]] = true;
					}

					NextVel[Tet[tet * 4 + i]].x += subx;
					NextVel[Tet[tet * 4 + i]].y += suby;
					NextVel[Tet[tet * 4 + i]].z += subz;
				}
				//*/
			}
				  break;
			default: //显示积分
				break;
			}


		}

		// 调用周期性函数
		if (delayNum % 1000 == 0) delayFunction();
		delayNum++;

		// 更新速度
		for (int i = 0; i < Size; ++i) {
			if (howToGetPos == 0) {
				Vel[i].x += gt.DeltaTime() * Force[i].x;
				Vel[i].y += gt.DeltaTime() * Force[i].y;
				Vel[i].z += gt.DeltaTime() * Force[i].z;
			}
			else {
				Vel[i].x += NextVel[i].x;
				Vel[i].y += NextVel[i].y;
				Vel[i].z += NextVel[i].z;
			}

			if (Vel[i].x > 1) Vel[i].x = 1;
			else if (Vel[i].x < -1) Vel[i].x = -1;
			if (Vel[i].y > 1) Vel[i].y = 1;
			else if (Vel[i].y < -1) Vel[i].y = -1;
			if (Vel[i].z > 1) Vel[i].z = 1;
			else if (Vel[i].z < -1) Vel[i].z = -1;

			Vel[i].x *= damp; Vel[i].y *= damp; Vel[i].z *= damp;
		}
		//求位置 
		for (int i = 0; i < Size; ++i) {
			float length;
			XMStoreFloat(&length, XMVector3Length(XMLoadFloat3(&Vel[i])));
			if (length < 0.001f) {
				Vel[i].x = Vel[i].y = Vel[i].z = 0.0f;
			}
			if (abs(Vel[i].x) < 0.0001) Vel[i].x = 0;
			if (abs(Vel[i].y) < 0.0001) Vel[i].y = 0;
			if (abs(Vel[i].z) < 0.0001) Vel[i].z = 0;

			//if (i != 0){//&& i != 3 && i != 6 && i != 8
				//&& i != 11 && i != 16 && i != 18 && i != 14) {
			X[i].x += gt.DeltaTime() * Vel[i].x;
			X[i].y += gt.DeltaTime() * Vel[i].y;
			X[i].z += gt.DeltaTime() * Vel[i].z;
			//}

			//碰撞检测
			float floor = -3.5f;
			XMFLOAT3 floorN = { 0, 1, 0 };
			if (X[i].y < floor) {
				X[i].y = floor;
				if (Vel[i].x * floorN.x + Vel[i].y * floorN.y + Vel[i].z * floorN.z < 0) {
					XMVECTOR Vn = XMVector3Dot(XMLoadFloat3(&Vel[i]), XMLoadFloat3(&floorN)) * XMLoadFloat3(&floorN);
					XMVECTOR Vt = XMLoadFloat3(&Vel[i]) - Vn;

					float mu_N = 0.65f; //摩擦系数
					float a = 0.65f; //比例系数，用于调整垂直于法向量的速度分量
					Vn *= -mu_N;
					Vt *= a;
					//XMStoreFloat3(&Vel[i], Vn + Vt);
					XMStoreFloat3(&Vel[i], { Vel[i].x * 0.4f, Vel[i].y * -0.5f, Vel[i].z * 0.5f });
				}
			}
		}
	}
}
void StencilApp::XPBD_Simulate() {
	if (GetAsyncKeyState(VK_SPACE)) {
		for (int i = 0; i < XPBD.Xpos.size(); i++)
			XPBD.Xpos[i].y += 0.5f;
	}
	
	float dt = 1.0f / 600.0f;
	for (int step = 0; step < 1; step++) {	
		if (pickVertex != -1 && isMoving) {
			//for (int i = 0; i < XPBD.numVerts; i++) {
				//if (XPBD.invMass[pickVertex] == 0.0)
					//continue;
			XPBD.invMass[pickVertex] = 0.0;
			///*
			XPBD.Xpos[pickVertex].x = pickForce.x;
			XPBD.Xpos[pickVertex].y = pickForce.y;
			XPBD.Xpos[pickVertex].z = pickForce.z;
		}

		XPBD.preSolve(dt, { 0, -100.0f, 0 });
		UpdateCollision();
		XPBD.solve(dt);
		
		XPBD.postSolve(dt);
	}
	//cout << pickVertex << "\n";
	
}

//构造肝脏XPBD
void StencilApp::BuildLiver() {
	vector<float> vertsPos;
	vector<int> tetId;
	vector<int> tetEdgeIds;
	vector<int> tetSurfaceTriIds;
	//读ele
	ifstream file("Models/Liver/singleLiver.1.ele");
	if (!file)
	{
		return;
	}
	string fileContent((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
	file.close();

	vector<string> Strings = SplitString(fileContent, ' ');
	int tet_number = stoi(Strings[0]);
	tetId.resize(tet_number * 4);
	for (int tet = 0; tet < tet_number; ++tet){
		tetId[tet * 4 + 0] = std::stoi(Strings[(double)tet * 5 + 4]);
		tetId[tet * 4 + 1] = std::stoi(Strings[(double)tet * 5 + 5]);
		tetId[tet * 4 + 2] = std::stoi(Strings[(double)tet * 5 + 6]);
		tetId[tet * 4 + 3] = std::stoi(Strings[(double)tet * 5 + 7]);
	}
	//读node
	ifstream file1("Models/Liver/singleLiver.1.node");
	if (!file1)
	{
		return;
	}
	string fileContent1((istreambuf_iterator<char>(file1)), istreambuf_iterator<char>());
	file1.close();

	Strings = SplitString(fileContent1, ' ');
	int xNumber = stoi(Strings[0]);
	vertsPos.resize(xNumber * 3);
	for (int i = 0; i < xNumber; ++i) {
		vertsPos[i * 3 + 0] = stod(Strings[i * 4 + 5]);
		vertsPos[i * 3 + 1] = stod(Strings[i * 4 + 6]);
		vertsPos[i * 3 + 2] = stod(Strings[i * 4 + 7]);
	}
	//读egde
	ifstream file2("Models/Liver/singleLiver.1.edge");
	if (!file2)
	{
		return;
	}
	string fileContent2((istreambuf_iterator<char>(file2)), istreambuf_iterator<char>());
	file2.close();

	Strings = SplitString(fileContent2, ' ');
	int eNum = stoi(Strings[0]);
	tetEdgeIds.resize(eNum * 2);
	for (int i = 0; i < eNum; ++i) {
		tetEdgeIds[i * 2 + 0] = stoi(Strings[(double)i * 4 + 3]);
		tetEdgeIds[i * 2 + 1] = stoi(Strings[(double)i * 4 + 4]);
	}
	//读face
	ifstream file3("Models/Liver/singleLiver.1.face");
	if (!file3)
	{
		return;
	}
	string fileContent3((istreambuf_iterator<char>(file3)), istreambuf_iterator<char>());
	file3.close();

	Strings = SplitString(fileContent3, ' ');
	int fNum = stoi(Strings[0]);
	tetSurfaceTriIds.resize(fNum * 3);
	for (int i = 0; i < fNum; ++i) {
		tetSurfaceTriIds[i * 3 + 0] = stoi(Strings[(double)i * 5 + 3]);
		tetSurfaceTriIds[i * 3 + 2] = stoi(Strings[(double)i * 5 + 4]);
		tetSurfaceTriIds[i * 3 + 1] = stoi(Strings[(double)i * 5 + 5]);
	}

	mesh.tetId = tetId;
	mesh.vertsPos = vertsPos;
	mesh.tetEdgeIds = tetEdgeIds;
	mesh.tetSurfaceTriIds = tetSurfaceTriIds;
}

// 创建FEM弹性体模型
void StencilApp::BuildFEMModel() {
	ReadEleFile("Models/cubeHigh.ele", Tet);
	ReadNodeFile("Models/cubeHigh.node", vertices, Tet, X, Dminv, indices, X2Ver);
	ReadFaceFile("Models/cubeHigh.face", indicesForPick);

	Force.resize(X.size());
	Vel.resize(X.size());

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "FEMGeo";
	geo->Tet = Tet;

	// 顶点索引数据从CPU上传到GPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	// 更新几何体实例相关数据
	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	BoundingBox bound;
	GetBoundingBox(bound, X);

	SubmeshGeometry submesh;
	submesh.vertexCount = (UINT)vertices.size();
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.BoundingBox = bound;

	geo->DrawArgs["FEM"] = submesh;
	mGeometries[geo->Name] = std::move(geo);
}
// 构建XPBD模型
void StencilApp::BuildXPBDModel() {	
	cout << "鼠标中键点击模型拖动" << "\n";
	
	XPBD_Softbody _XPBD(mesh, 0, 0.0);
	XPBD = _XPBD;
	// 一个四面体4个顶点
	vertices_xpbd.resize(XPBD.numVerts);
	for (int i = 0; i < XPBD.numVerts; ++i) {
		vertices_xpbd[i].Pos = XPBD.Xpos[i];
		//vertices_xpbd[i].Normal = { 0,0,0 };
	}
	indices_xpbd.resize(mesh.tetSurfaceTriIds.size());
	for (int i = 0; i < mesh.tetSurfaceTriIds.size() / 3; ++i) {
		indices_xpbd[i*3] = XPBD.Mesh.tetSurfaceTriIds[i*3];
		indices_xpbd[i * 3+1] = XPBD.Mesh.tetSurfaceTriIds[i * 3+1];
		indices_xpbd[i * 3+2] = XPBD.Mesh.tetSurfaceTriIds[i * 3+2];
		//求法线
		XMFLOAT3 v1 = vertices_xpbd[indices_xpbd[i * 3 + 0]].Pos;
		XMFLOAT3 v2 = vertices_xpbd[indices_xpbd[i * 3 + 1]].Pos;
		XMFLOAT3 v3 = vertices_xpbd[indices_xpbd[i * 3 + 2]].Pos;

		XMVECTOR edge1 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v2));
		XMVECTOR edge2 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v3));
		XMVECTOR normal = XMVector3Normalize(DirectX::XMVector3Cross(edge1, edge2));
		XMFLOAT3 Normal;
		XMStoreFloat3(&Normal, normal);

		vertices_xpbd[indices_xpbd[i * 3]].Normal.x += Normal.x;
		vertices_xpbd[indices_xpbd[i * 3]].Normal.y += Normal.y;
		vertices_xpbd[indices_xpbd[i * 3]].Normal.z += Normal.z;

		vertices_xpbd[indices_xpbd[i * 3 + 1]].Normal.x += Normal.x;
		vertices_xpbd[indices_xpbd[i * 3 + 1]].Normal.y += Normal.y;
		vertices_xpbd[indices_xpbd[i * 3 + 1]].Normal.z += Normal.z;

		vertices_xpbd[indices_xpbd[i * 3 + 2]].Normal.x += Normal.x;
		vertices_xpbd[indices_xpbd[i * 3 + 2]].Normal.y += Normal.y;
		vertices_xpbd[indices_xpbd[i * 3 + 2]].Normal.z += Normal.z;


	}

	const UINT vbByteSize = (UINT)vertices_xpbd.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices_xpbd.size() * sizeof(int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "XPBDGeo";
	//geo->Tet = Tet;

	// 顶点索引数据从CPU上传到GPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices_xpbd.data(), vbByteSize);
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices_xpbd.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), vertices_xpbd.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), indices_xpbd.data(), ibByteSize, geo->IndexBufferUploader);

	// 更新几何体实例相关数据
	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	BoundingBox bound;
	GetBoundingBox(bound, XPBD.Xpos);

	SubmeshGeometry submesh;
	submesh.vertexCount = (UINT)vertices_xpbd.size();
	submesh.IndexCount = (UINT)indices_xpbd.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.BoundingBox = bound;

	geo->DrawArgs["XPBD"] = submesh;
	mGeometries[geo->Name] = std::move(geo);
}
// 创建BOX
void StencilApp::BuildBoxGeometry()
{
	GeometryGenerator geoGen;
	//GeometryGenerator::MeshData box;
	GeometryGenerator::MeshData box = geoGen.CreateBox(4.0f, 0.5f, 0.5f, 0);

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.vertexCount = (UINT)vertices.size();
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries["boxGeo"] = std::move(geo);
}
void StencilApp::BuildBox2Geometry()
{
	GeometryGenerator geoGen;
	//GeometryGenerator::MeshData box;
	GeometryGenerator::MeshData box = geoGen.CreateBox(4.0f, 2.0f, 2.0f, 0);

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "box2Geo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.vertexCount = (UINT)vertices.size();
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box2"] = submesh;

	mGeometries["box2Geo"] = std::move(geo);
}
//创建胶囊体
void StencilApp::BuildCapsule() {
	std::ifstream fin("Models/Capsule.txt");
	string s;
	int i = 0;

	if (!fin)
	{
		MessageBox(0, L"not found.", 0, 0);
		return;
	}

	std::vector<Vertex> vertices;
	std::vector<XMFLOAT3> vns;
	std::vector<std::int32_t> indices;

	while (fin >> s) {
		switch (s[0]) {
		case 'f':
		{
			string x;
			int v;
			int f1,f2,f3;
			fin >> f1 >> x >> f2 >> x >> f3 >> x;
			v = x[2] - '0';
			f1 -= 1; f2 -= 1; f3 -= 1; v -= 1;
			indices.push_back(f1); indices.push_back(f2); indices.push_back(f3);
			//vertices[f1].Normal.x += vns[v].x; vertices[f1].Normal.y += vns[v].y; vertices[f1].Normal.z += vns[v].z;
			//vertices[f2].Normal.x += vns[v].x; vertices[f2].Normal.y += vns[v].y; vertices[f2].Normal.z += vns[v].z;
			//vertices[f3].Normal.x += vns[v].x; vertices[f3].Normal.y += vns[v].y; vertices[f3].Normal.z += vns[v].z;
		}
		break;

		case 'v':
		{
			if (s.size() == 1) {
				Vertex v;
				fin >> v.Pos.x >> v.Pos.y >> v.Pos.z;
				v.Normal = { 0,0,0 };
				vertices.push_back(v);
			}
			else {
				if (s[1] == 'n') {
					XMFLOAT3 vn;
					fin >> vn.x >> vn.y >> vn.z;
					vns.push_back(vn);

					//fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
					//++i;
				}
			}
		}
		break;

		default:
		break;

		}
	}

	for (int i = 0; i < indices.size() / 3; ++i) {
		//int i1 = indices[i * 3];
		//int i2 = indices[i * 3 + 1];
		//int i3 = indices[i * 3 + 2];
		//求法线
		XMFLOAT3 v1 = vertices[indices[i * 3 + 0]].Pos;
		XMFLOAT3 v2 = vertices[indices[i * 3 + 1]].Pos;
		XMFLOAT3 v3 = vertices[indices[i * 3 + 2]].Pos;

		XMVECTOR edge1 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v2));
		XMVECTOR edge2 = XMVectorSubtract(DirectX::XMLoadFloat3(&v1), DirectX::XMLoadFloat3(&v3));
		XMVECTOR normal = XMVector3Normalize(DirectX::XMVector3Cross(edge1, edge2));
		XMFLOAT3 Normal;
		XMStoreFloat3(&Normal, normal);

		vertices[indices[i * 3]].Normal.x += Normal.x;
		vertices[indices[i * 3]].Normal.y += Normal.y;
		vertices[indices[i * 3]].Normal.z += Normal.z;

		vertices[indices[i * 3 + 1]].Normal.x += Normal.x;
		vertices[indices[i * 3 + 1]].Normal.y += Normal.y;
		vertices[indices[i * 3 + 1]].Normal.z += Normal.z;

		vertices[indices[i * 3 + 2]].Normal.x += Normal.x;
		vertices[indices[i * 3 + 2]].Normal.y += Normal.y;
		vertices[indices[i * 3 + 2]].Normal.z += Normal.z;
	}

	for (int i = 0; i < vertices.size(); ++i) {
		XMFLOAT3 X = vertices[i].Normal;
		XMFLOAT3 Y = X;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "capsuleGeo";

	// 顶点索引数据从CPU上传到GPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	// 更新几何体实例相关数据
	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["capsule"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}
// 创建几何体
void StencilApp::BuildRoomGeometry() {
	// Create and specify geometry.  For this sample we draw a floor
// and a wall with a mirror on it.  We put the floor, wall, and
// mirror geometry in one vertex buffer.
//
//   |--------------|
//   |              |
//   |----|----|----|
//   |Wall|Mirr|Wall|
//   |    | or |    |
//   /--------------/
//  /   Floor      /
// /--------------/

	// 构建顶点数据
	std::array<Vertex, 20> vertices =
	{
		// Floor: Observe we tile texture coordinates.
		Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
		Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

		// Wall: Observe we tile texture coordinates, and that we
		// leave a gap in the middle for the mirror.
		Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// Mirror
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	// 构建索引数据
	std::array<std::int16_t, 30> indices =
	{
		// Floor
		0, 1, 2,
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	// 构建子物体
	SubmeshGeometry floorSubmesh;
	floorSubmesh.vertexCount = 4;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.vertexCount = 12;
	wallSubmesh.IndexCount = 18;
	wallSubmesh.StartIndexLocation = 6;
	wallSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.vertexCount = 4;
	mirrorSubmesh.IndexCount = 6;
	mirrorSubmesh.StartIndexLocation = 24;
	mirrorSubmesh.BaseVertexLocation = 0;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "roomGeo";

	// 顶点/索引数据从CPU上传到GPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU)); //创建一个Direct3Dblob对象，用于存储顶点缓冲区数据
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);//将顶点数据从'顶点'容器复制到blob的缓冲区中（CPU中）。

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	// 更新几何体实例相关数据
	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["floor"] = floorSubmesh;
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}
// 导入模型
void StencilApp::BuildSkullGeometry() {
	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	// 读取顶点/索引数据
	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].TexC = { 0.0f, 0.0f };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	// 顶点索引数据从CPU上传到GPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mD3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	// 更新几何体实例相关数据
	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

// 读取DDS纹理
void StencilApp::LoadTexture() {
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"Textures/bricks3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mD3dDevice.Get(),
		mCommandList.Get(), bricksTex->Filename.c_str(),
		bricksTex->Resource, bricksTex->UploadHeap));

	auto checkboardTex = std::make_unique<Texture>();
	checkboardTex->Name = "checkboardTex";
	checkboardTex->Filename = L"Textures/checkboard.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mD3dDevice.Get(),
		mCommandList.Get(), checkboardTex->Filename.c_str(),
		checkboardTex->Resource, checkboardTex->UploadHeap));

	auto iceTex = std::make_unique<Texture>();
	iceTex->Name = "iceTex";
	iceTex->Filename = L"Textures/ice.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mD3dDevice.Get(),
		mCommandList.Get(), iceTex->Filename.c_str(),
		iceTex->Resource, iceTex->UploadHeap));

	auto white1x1Tex = std::make_unique<Texture>();
	white1x1Tex->Name = "white1x1Tex";
	white1x1Tex->Filename = L"Textures/white1x1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mD3dDevice.Get(),
		mCommandList.Get(), white1x1Tex->Filename.c_str(),
		white1x1Tex->Resource, white1x1Tex->UploadHeap));

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[checkboardTex->Name] = std::move(checkboardTex);
	mTextures[iceTex->Name] = std::move(iceTex);
	mTextures[white1x1Tex->Name] = std::move(white1x1Tex);
}
// 创建描述符堆和SRV（着色器资源视图）——存放纹理资源
void StencilApp::BuildDescriptorHeaps() {
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 4 + 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(mD3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = mTextures["bricksTex"]->Resource;
	auto checkboardTex = mTextures["checkboardTex"]->Resource;
	auto iceTex = mTextures["iceTex"]->Resource;
	auto white1x1Tex = mTextures["white1x1Tex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	mD3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbv_srv_uavDescriptorSize);

	srvDesc.Format = checkboardTex->GetDesc().Format;
	mD3dDevice->CreateShaderResourceView(checkboardTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbv_srv_uavDescriptorSize);

	srvDesc.Format = iceTex->GetDesc().Format;
	mD3dDevice->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbv_srv_uavDescriptorSize);

	srvDesc.Format = white1x1Tex->GetDesc().Format;
	mD3dDevice->CreateShaderResourceView(white1x1Tex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, cbv_srv_uavDescriptorSize);

	srvDesc.Format = white1x1Tex->GetDesc().Format;
	mD3dDevice->CreateShaderResourceView(white1x1Tex.Get(), &srvDesc, hDescriptor);
}
// 创建根签名
void StencilApp::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable;// 材质描述符表
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,// 描述符类型
		1, // 描述符数量
		0);// 描述符绑定的寄存槽编号

	// 根参数可以是描述符表、根描述符、根常量
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// 创建根描述符
	// 性能提示：寄存器编号从最频繁到不频繁
	slotRootParameter[0].InitAsDescriptorTable(1, // Range数量
		&texTable,// Range指针
		D3D12_SHADER_VISIBILITY_PIXEL);// 该资源只在像素着色器可读
	slotRootParameter[1].InitAsConstantBufferView(0);// ObjectConstants
	slotRootParameter[2].InitAsConstantBufferView(1);// PassConstants
	slotRootParameter[3].InitAsConstantBufferView(2);// MaterialConstants

	auto staticSamplers = GetStaticSamplers();

	// 根签名是根参数的数组
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(),// 静态采样器数量
		staticSamplers.data(),// 静态采样器指针
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);// 选择输入汇编器

	// 用单个寄存槽来创建一个根签名，该槽位指向一个仅含有单个常量缓冲区描述符区域
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	// 创建根签名与序列化内存绑定
	ThrowIfFailed(mD3dDevice->CreateRootSignature(
		0,// 适配器数量
		serializedRootSig->GetBufferPointer(),// 根签名绑定的序列化内存指针
		serializedRootSig->GetBufferSize(),// 根签名绑定的序列化内存byte
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));// 根签名COM ID
}
// 构建输入布局 编译Shaders
void StencilApp::BuildShadersAndInputLayout() {
	HRESULT hr = S_OK;

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}
// 构建渲染流水线状态
void StencilApp::BuildPSO() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };// 输入布局描述  
	psoDesc.pRootSignature = mRootSignature.Get();// 与此PSO绑定的根签名指针
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};// 待绑定的顶点着色器
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};// 待绑定的像素着色器
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);// 光栅化状态
	//psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 不剔除
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);// 混合状态
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);// 深度/模板测试状态
	psoDesc.SampleMask = UINT_MAX;// 每个采样点的采样情况
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;// 图元拓扑类型
	psoDesc.NumRenderTargets = 1;// 渲染目标数量
	psoDesc.RTVFormats[0] = mBackBufferFormat;// 渲染目标的格式
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;// 多重采样数量
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;// 多重采样级别
	psoDesc.DSVFormat = mDepthStencilFormat;// 深度/模板缓冲区格式
	ThrowIfFailed(mD3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}
// 构建帧资源
void StencilApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mD3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), 
			(UINT)mFEMRitem->Geo->DrawArgs["FEM"].vertexCount, 
			(UINT)mXPBDRitem->Geo->DrawArgs["XPBD"].vertexCount));
	}
}
// 构建渲染项
void StencilApp::BuildRenderItems() {
	auto floorRitem = std::make_unique<RenderItem>();
	// floorRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&(floorRitem->World), DirectX::XMMatrixScaling(3.0f, 1.0f, 3.0f));
	floorRitem->TexTransform = MathHelper::Identity4x4();
	floorRitem->ObjCBIndex = 0;
	floorRitem->Mat = mMaterials["checkertile"].get();
	floorRitem->Geo = mGeometries["roomGeo"].get();
	floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;

	/*
	auto wallsRitem = std::make_unique<RenderItem>();
	// wallsRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&(wallsRitem->World), DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) * DirectX::XMMatrixTranslation(0.0f, -2.0f, 5.0f));
	wallsRitem->TexTransform = MathHelper::Identity4x4();
	wallsRitem->ObjCBIndex = 1;
	wallsRitem->Mat = mMaterials["bricks"].get();
	wallsRitem->Geo = mGeometries["roomGeo"].get();
	wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].IndexCount;
	wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
	*/

	
	auto FEMRitem = std::make_unique<RenderItem>();
	// skullRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&FEMRitem->World,
		DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f) * DirectX::XMMatrixTranslation(0.0f, 5.0f, 0.0f));
	FEMRitem->TexTransform = MathHelper::Identity4x4();
	FEMRitem->ObjCBIndex = 1;
	FEMRitem->Mat = mMaterials["FEM_Mat"].get();
	FEMRitem->Geo = mGeometries["FEMGeo"].get();
	FEMRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FEMRitem->IndexCount = FEMRitem->Geo->DrawArgs["FEM"].IndexCount;
	FEMRitem->StartIndexLocation = FEMRitem->Geo->DrawArgs["FEM"].StartIndexLocation;
	FEMRitem->BaseVertexLocation = FEMRitem->Geo->DrawArgs["FEM"].BaseVertexLocation;
	FEMRitem->bound = FEMRitem->Geo->DrawArgs["FEM"].BoundingBox;
	FEMRitem->hasPhysical = true;
	mFEMRitem = FEMRitem.get();
	
	
	auto XPBDRitem = std::make_unique<RenderItem>();
	// mirrorRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&XPBDRitem->World, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) * DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	XPBDRitem->TexTransform = MathHelper::Identity4x4();
	XPBDRitem->ObjCBIndex = 0;
	XPBDRitem->Mat = mMaterials["FEM_Mat"].get();
	XPBDRitem->Geo = mGeometries["XPBDGeo"].get();
	XPBDRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	XPBDRitem->IndexCount = XPBDRitem->Geo->DrawArgs["XPBD"].IndexCount;
	XPBDRitem->StartIndexLocation = XPBDRitem->Geo->DrawArgs["XPBD"].StartIndexLocation;
	XPBDRitem->BaseVertexLocation = XPBDRitem->Geo->DrawArgs["XPBD"].BaseVertexLocation;
	XPBDRitem->bound = XPBDRitem->Geo->DrawArgs["XPBD"].BoundingBox;
	XPBDRitem->hasPhysical = true;
	mXPBDRitem = XPBDRitem.get();

	/*auto mirrorRitem = std::make_unique<RenderItem>();
	// mirrorRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&(mirrorRitem->World), DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) * DirectX::XMMatrixTranslation(0.0f, -2.0f, 5.0f));
	mirrorRitem->TexTransform = MathHelper::Identity4x4();
	mirrorRitem->ObjCBIndex = 3;
	mirrorRitem->Mat = mMaterials["icemirror"].get();
	mirrorRitem->Geo = mGeometries["roomGeo"].get();
	mirrorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].IndexCount;
	mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].StartIndexLocation;
	mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].BaseVertexLocation;


	auto skullRitem = std::make_unique<RenderItem>();
	// skullRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&skullRitem->World,
		DirectX::XMMatrixScaling(0.5f, 0.5f, 0.5f) * DirectX::XMMatrixTranslation(0.0f, -1.5f, 0.0f) * DirectX::XMMatrixRotationY(1.57f));
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 4;
	skullRitem->Mat = mMaterials["skullMat"].get();
	skullRitem->Geo = mGeometries["skullGeo"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	mSkullRitem = skullRitem.get();
	*/
	
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World,
		XMMatrixRotationX(XMConvertToRadians(0.0f))* DirectX::XMMatrixTranslation(0.0f, 5.0f, 0.0f));
	boxRitem->TexTransform = MathHelper::Identity4x4();
	boxRitem->ObjCBIndex = 1;
	boxRitem->Mat = mMaterials["skullMat"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mBoxRitem = boxRitem.get();

	auto box2Ritem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&box2Ritem->World,
		DirectX::XMMatrixTranslation(0.0f, -20.0f, 0.0f));// * XMMatrixRotationY(XMConvertToRadians(45.0f)));
	box2Ritem->TexTransform = MathHelper::Identity4x4();
	box2Ritem->ObjCBIndex = 2;
	box2Ritem->Mat = mMaterials["skullMat"].get();
	box2Ritem->Geo = mGeometries["box2Geo"].get();
	box2Ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	box2Ritem->IndexCount = box2Ritem->Geo->DrawArgs["box2"].IndexCount;
	box2Ritem->StartIndexLocation = box2Ritem->Geo->DrawArgs["box2"].StartIndexLocation;
	box2Ritem->BaseVertexLocation = box2Ritem->Geo->DrawArgs["box2"].BaseVertexLocation;
	mBox2Ritem = box2Ritem.get();

	auto CapsuleRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CapsuleRitem->World,
		XMMatrixRotationZ(XMConvertToRadians(90.0f)) * DirectX::XMMatrixTranslation(0.0f, 5.0f, 0.0f));
	CapsuleRitem->TexTransform = MathHelper::Identity4x4();
	CapsuleRitem->ObjCBIndex = 3;
	CapsuleRitem->Mat = mMaterials["skullMat"].get();
	CapsuleRitem->Geo = mGeometries["capsuleGeo"].get();
	CapsuleRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CapsuleRitem->IndexCount = CapsuleRitem->Geo->DrawArgs["capsule"].IndexCount;
	CapsuleRitem->StartIndexLocation = CapsuleRitem->Geo->DrawArgs["capsule"].StartIndexLocation;
	CapsuleRitem->BaseVertexLocation = CapsuleRitem->Geo->DrawArgs["capsule"].BaseVertexLocation;
	mCapsuleRitem = CapsuleRitem.get();

	//mAllRitems.push_back(std::move(floorRitem));
	//mAllRitems.push_back(std::move(wallsRitem));
	//mAllRitems.push_back(std::move(FEMRitem));
	mAllRitems.push_back(std::move(XPBDRitem));
	mAllRitems.push_back(std::move(boxRitem));
	mAllRitems.push_back(std::move(box2Ritem));
	mAllRitems.push_back(std::move(CapsuleRitem));
	mAllFEMRitems.push_back(std::move(FEMRitem));
	//mAllRitems.push_back(std::move(mirrorRitem));

	//mAllRitems.push_back(std::move(skullRitem));
	//mAllRitems.push_back(std::move(boxRitem));

	for (auto& e : mAllRitems) {
		mOpaqueRitems.push_back(e.get());
	}
}
// 创建材质
void StencilApp::BuildMaterials() {
	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkertile = std::make_unique<Material>();
	checkertile->Name = "checkertile";
	checkertile->MatCBIndex = 1;
	checkertile->DiffuseSrvHeapIndex = 1;
	checkertile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	checkertile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	checkertile->Roughness = 0.3f;

	auto icemirror = std::make_unique<Material>();
	icemirror->Name = "icemirror";
	icemirror->MatCBIndex = 2;
	icemirror->DiffuseSrvHeapIndex = 2;
	icemirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
	icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto FEM_Mat = std::make_unique<Material>();
	FEM_Mat->Name = "FEM_Mat";
	FEM_Mat->MatCBIndex = 4;
	FEM_Mat->DiffuseSrvHeapIndex = 3;
	FEM_Mat->DiffuseAlbedo = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);
	FEM_Mat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	FEM_Mat->Roughness = 0.8f;

	mMaterials["bricks"] = std::move(bricks);
	mMaterials["checkertile"] = std::move(checkertile);
	mMaterials["icemirror"] = std::move(icemirror);

	mMaterials["skullMat"] = std::move(skullMat);

	mMaterials["FEM_Mat"] = std::move(FEM_Mat);
}
// 绘制渲染项
void StencilApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBBytesize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	for (size_t i = 0; i < ritems.size(); ++i) {
		auto ri = ritems[i];

		// 设置顶点缓冲区
		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		// 设置索引缓冲区
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		// 设置图元拓扑
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);


		// 设置描述符表，将纹理资源与流水线绑定
		CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		texHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, cbv_srv_uavDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, texHandle);

		// 设置根描述符，将根描述符与资源绑定
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBBytesize;

		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);// 根参数索引
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);// 根参数索引

		// 绘制顶点实例
		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

// 获取不同的静态采样器
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> StencilApp::GetStaticSamplers() {
	// Applications usually only need a handful of samplers.  So just define them all up front
// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

// 按下鼠标事件
void StencilApp::OnMouseDown(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0)
	{
		mLastMousePos.x = x;
		mLastMousePos.y = y;

		SetCapture(mhMainWnd);
	}
	else if ((btnState & MK_MBUTTON) != 0)
	{
		Pick(x, y);
	}
}

// 松开鼠标事件
void StencilApp::OnMouseUp(WPARAM btnState, int x, int y) {
	
	//if ((btnState & MK_MBUTTON) != 0) // 中键
	//{
	if (pickVertex != -1) {
		pickForce = { 0,0,0 };
		XPBD.invMass[pickVertex] = XPBD.pickMass;
		pickVertex = -1;
		isMoving = false;
	}
	//}
	//else if ((btnState & MK_LBUTTON) != 0) cout << "松开zuo键" << "\n";
	ReleaseCapture();	// 按键抬起后释放鼠标捕获
}

// 移动鼠标事件
void StencilApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0)// 如果在左键按下状态
	{
		// 将鼠标的移动距离换算成弧度，0.25为调节阈值
		float dx = XMConvertToRadians(static_cast<float>(mLastMousePos.x - x) * 0.25f);
		float dy = XMConvertToRadians(static_cast<float>(mLastMousePos.y - y) * 0.25f);

		// 计算鼠标没有松开前的累计弧度
		mTheta += dx;
		mPhi += dy;

		// 限制角度phi的范围在（0.1， Pi-0.1）
		mPhi = MathHelper::Clamp(mPhi, 0.1f, 3.1415f - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)// 如果在右键按下状态
	{
		// 将鼠标的移动距离换算成缩放大小，0.2为调节阈值
		float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);
		// 根据鼠标输入更新摄像机可视范围半径
		mRadius += dx - dy;
		//限制可视范围半径
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}
	else if ((btnState & MK_MBUTTON) != 0) // 中键
	{
		if (pickVertex != -1) {
			XMFLOAT4X4 P;
			P = mProj;
			//屏幕坐标转换为投影窗口坐标
			float xv = (2.f * x / mClientWidth - 1.f) / P(0, 0);
			float yv = (-2.f * y / mClientHeight + 1.f) / P(1, 1);
			//cout << xv << " " << yv << "\n";
			rayDir = XMVectorSet(xv, yv, 1.f, 0.f);
			rayDir = XMVector3TransformNormal(rayDir, ToLocal);
			rayDir = XMVector3Normalize(rayDir);

			XMVECTOR intersectionPoint = rayOrigin + Tmin * rayDir;
			XMFLOAT3 intersectionPos;
			XMStoreFloat3(&intersectionPos, intersectionPoint);

			float dx = 1.5f * static_cast<float>(x - mLastMousePos.x);
			float dy = 1.5f * static_cast<float>(mLastMousePos.y - y);

			//pickForce.x = dx; pickForce.x = MathHelper::Clamp(pickForce.x, -15.0f, 15.0f);
			//pickForce.y = dy; pickForce.y = MathHelper::Clamp(pickForce.y, -15.0f, 15.0f);
			//if (pickForce.x > 0 && pickForce.y)

			// 将屏幕坐标变化转换为世界坐标变化
			XMFLOAT3 worldDelta;
			XMVECTOR screenDelta = XMVectorSet(dx, dy, 0.0f, 0.0f);
			XMVECTOR worldDeltaVec = XMVector4Transform(screenDelta, XMMatrixInverse(nullptr, XMLoadFloat4x4(&mView)));
			XMStoreFloat3(&worldDelta, worldDeltaVec);

			pickForce.x = intersectionPos.x; //pickForce.x = MathHelper::Clamp(pickForce.x, -5.0f, 5.0f);
			pickForce.y = intersectionPos.y; //pickForce.y = MathHelper::Clamp(pickForce.y, -5.0f, 5.0f);
			pickForce.z = intersectionPos.z; //pickForce.z = MathHelper::Clamp(pickForce.z, -5.0f, 5.0f);

			isMoving = true;
		}
		//cout << pickForce.x << "\n";
	}
	// 将当前鼠标坐标赋值给“上一次鼠标坐标”，为下一次鼠标操作提供先前值
	mLastMousePos.x = x;
	mLastMousePos.y = y;

}

void StencilApp::OnResize() {
	gt.Stop();
	//startSimulation = false;
	D3DApp::OnResize();

	//构建投影矩阵
	XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * 3.1416f, static_cast<float>(mClientWidth) / mClientHeight, 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, p);
	Sleep(500);
	//startSimulation = true;
	gt.Start();
}

void StencilApp::Pick(int x, int y) {
	//XMVECTOR rayOrigin;
	//XMVECTOR rayDir;

	XMFLOAT4X4 P;
	XMMATRIX V;

	P = mProj;
	V = XMLoadFloat4x4(&mView);

	//屏幕坐标转换为投影窗口坐标
	float xv = (2.f * x / mClientWidth - 1.f) / P(0, 0);
	float yv = (-2.f * y / mClientHeight + 1.f) / P(1, 1);

	bool Picked = false;
	float tmin;
	float triMin;

	//循环所有柔体
	for (auto& ri : mAllRitems)
	{
		if (!ri->hasPhysical) continue;
		auto geo = ri->Geo;

		//射线原点和方向
		rayOrigin = XMVectorSet(0, 0, 0, 1.f);
		rayDir = XMVectorSet(xv, yv, 1.f, 0.f);
		//观察矩阵逆矩阵
		XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V), V);
		//世界矩阵逆矩阵
		XMMATRIX W = XMLoadFloat4x4(&ri->World);
		XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);
		// 将拾取射线变换到网格局部空间 
		XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);
		ToLocal = toLocal;
		//分别用来对点和向量进行变换(乘以变化矩阵)。
		rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
		rayDir = XMVector3TransformNormal(rayDir, toLocal);
		rayDir = XMVector3Normalize(rayDir); // 规范化  

		/*
		BoundingBox Bounds;
		XMVECTOR boundPos1 = XMLoadFloat3(&ri->bound.Center);
		boundPos1 = XMVector3TransformCoord(boundPos1, W);
		XMStoreFloat3(&Bounds.Center, boundPos1);
		XMVECTOR boundPos2 = XMLoadFloat3(&ri->bound.Extents);
		XMStoreFloat3(&Bounds.Extents, boundPos2);
		*/
		tmin = 0;
		/*
		if (mFEMRitem->bound.Intersects(rayOrigin, rayDir, tmin))
		{
			Vertex* vertexList = nullptr;
			vertexList = reinterpret_cast<Vertex*>(mFEMRitem->Geo->VertexBufferCPU->GetBufferPointer());
			//auto vertices = (Vertex*)geo->VertexBufferCPU->GetBufferPointer();
			//auto indices = indicesForPick;
			UINT triCount = indicesForPick.size() / 3;

			XMVECTOR intersectionPoint; // 相交点的位置
			XMVECTOR V0;
			XMVECTOR V1;
			XMVECTOR V2;
			UINT I0 ;
			UINT I1 ;
			UINT I2 ;

			// 查找射线相交的最近三角形，初始把tmin设置为最大值
			tmin = 0x3f3f3f3f;
			for (UINT i = 0; i < triCount; ++i)
			{
				// 三角形索引
				UINT i0 = indicesForPick[i * 3 + 0];
				UINT i1 = indicesForPick[i * 3 + 1];
				UINT i2 = indicesForPick[i * 3 + 2];

				// 三角形顶点
				XMVECTOR v0 = XMLoadFloat3(&X[i0]);
				XMVECTOR v1 = XMLoadFloat3(&X[i1]);
				XMVECTOR v2 = XMLoadFloat3(&X[i2]);

				// 我们对所有三角形相交检测，以便找到最近的一个
				float t = 0.0f;
				if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, t))
				{
					if (t < tmin)
					{
						// 这个是最新的最近三角形
						tmin = t;
						V0 = v0;
						V1 = v1;
						V2 = v2;
						I0 = i0; I1 = i1; I2 = i2;
					}
				}
			}

			// 更新相交点的位置
			intersectionPoint = rayOrigin + tmin * rayDir;
			XMFLOAT3 intersectionPos;
			XMStoreFloat3(&intersectionPos, intersectionPoint);

			if (tmin != 0x3f3f3f3f) {
				// 计算相交点与三个顶点的距离
				float distToV0 = XMVectorGetX(XMVector3Length(XMVectorSubtract(intersectionPoint, V0)));
				float distToV1 = XMVectorGetX(XMVector3Length(XMVectorSubtract(intersectionPoint, V1)));
				float distToV2 = XMVectorGetX(XMVector3Length(XMVectorSubtract(intersectionPoint, V2)));

				// 找出最近的顶点索引
				if (distToV0 < distToV1 && distToV0 < distToV2)
				{
					pickVertex = I0;
				}
				else if (distToV1 < distToV2)
				{
					pickVertex = I1;
				}
				else
				{
					pickVertex = I2;
				}
			}
			wstring str = to_wstring(pickVertex);
			LPCWSTR str1 = str.c_str();
			//MessageBox(NULL, str1, (LPCWSTR)L"Infor", MB_OK);
			//pickVertex = -1;
		}
		*/
		if (mXPBDRitem->bound.Intersects(rayOrigin, rayDir, tmin)) {
			Vertex* vertexList = nullptr;
			vertexList = reinterpret_cast<Vertex*>(mXPBDRitem->Geo->VertexBufferCPU->GetBufferPointer());
			UINT triCount = XPBD.Mesh.tetSurfaceTriIds.size() / 3;

			XMVECTOR intersectionPoint; // 相交点的位置
			XMVECTOR V0;
			XMVECTOR V1;
			XMVECTOR V2;
			UINT I0;
			UINT I1;
			UINT I2;

			// 查找射线相交的最近三角形，初始把tmin设置为最大值
			tmin = 0x3f3f3f3f;
			for (UINT i = 0; i < triCount; ++i)
			{
				// 三角形索引
				UINT i0 = XPBD.Mesh.tetSurfaceTriIds[i * 3 + 0];
				UINT i1 = XPBD.Mesh.tetSurfaceTriIds[i * 3 + 1];
				UINT i2 = XPBD.Mesh.tetSurfaceTriIds[i * 3 + 2];

				// 三角形顶点
				XMVECTOR v0 = XMLoadFloat3(&XPBD.Xpos[i0]);
				XMVECTOR v1 = XMLoadFloat3(&XPBD.Xpos[i1]);
				XMVECTOR v2 = XMLoadFloat3(&XPBD.Xpos[i2]);

				// 我们对所有三角形相交检测，以便找到最近的一个
				float t = 0.0f;
				if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, t))
				{
					if (t < tmin)
					{
						// 这个是最新的最近三角形
						tmin = t;
						V0 = v0;
						V1 = v1;
						V2 = v2;
						I0 = i0; I1 = i1; I2 = i2;
					}
				}
			}

			

			if (tmin != 0x3f3f3f3f) {
				// 更新相交点的位置
				Tmin = tmin;
				intersectionPoint = rayOrigin + tmin * rayDir;
				XMFLOAT3 intersectionPos;
				XMStoreFloat3(&intersectionPos, intersectionPoint);

				// 计算相交点与三个顶点的距离
				float distToV0 = XMVectorGetX(XMVector3Length(XMVectorSubtract(intersectionPoint, V0)));
				float distToV1 = XMVectorGetX(XMVector3Length(XMVectorSubtract(intersectionPoint, V1)));
				float distToV2 = XMVectorGetX(XMVector3Length(XMVectorSubtract(intersectionPoint, V2)));

				// 找出最近的顶点索引
				if (distToV0 < distToV1 && distToV0 < distToV2)
				{
					pickVertex = I0;
				}
				else if (distToV1 < distToV2)
				{
					pickVertex = I1;
				}
				else
				{
					pickVertex = I2;
				}

				if (XPBD.invMass[pickVertex] != 0) {
					XPBD.pickMass = XPBD.invMass[pickVertex];
					cout << "拾取顶点：" << pickVertex << "\n";
				}
				else {
					pickVertex = -1;
				}
			}			
		}
	}
}

