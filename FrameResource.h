#pragma once

#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "Material.h"
#include "Light.h"

using namespace DirectX::PackedVector;
using namespace DirectX;

// 定义顶点结构体
struct Vertex {
	Vertex() = default;
	Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v) :
		Pos(x, y, z),
		Normal(nx, ny, nz),
		TexC(u, v) {}
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 TexC;

	XMFLOAT3 Velocity;
	XMFLOAT3 Force;
};

// 单个物体的物体常量数据（不变的）
struct ObjectConstants {
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();// UV变化矩阵
};

// perPass的常量数据（每帧变化）
struct PassConstants {
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();

	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };// 观察位置
	float TotalTime = 0.0f;// 总时间
	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };// 环境光
	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light Lights[MaxLight];
};

struct FrameResource {
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT FEM_objVerNum, UINT XPBD_objVerNum);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator = (const FrameResource& rhs) = delete;
	~FrameResource();

	// 每帧资源都需要独立的命令分配器
	ComPtr<ID3D12CommandAllocator> CmdListAlloc;
	
	// 每帧都需要单独的资源缓冲区
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;

	std::unique_ptr<UploadBuffer<Vertex>> FEMmodelVB = nullptr;
	std::unique_ptr<UploadBuffer<Vertex>> XPBDmodelVB = nullptr;

	// CPU端的围栏值
	UINT64 FenceCPU = 0;
};