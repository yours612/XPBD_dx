#pragma once
#include "stdafx.h"

using Microsoft::WRL::ComPtr;

// 在MeshGeometry中定义了一个几何体的子物体。这用于混合物体的情况。
struct SubmeshGeometry {
	UINT vertexCount = 0; // 顶点数量
	UINT IndexCount = 0;// 索引数量
	UINT StartIndexLocation = 0;// 基准索引位置
	INT BaseVertexLocation = 0;// 基准顶点位置
	BoundingBox BoundingBox; //包围盒
};

// 几何体
struct MeshGeometry {
	// 给MeshGeometry一个名字，这样我们可以通过关联容器等方式对它进行查找
	std::string Name;

	// 四面体序列
	vector<int32_t> Tet;

	// 系统变量内存的复制样本。我们使用Blob因为vertex/index可以用自定义结构体声明
	ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	// 上传到GPU的顶点/索引资源
	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	// 顶点/索引的上传堆资源
	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// 缓冲区相关数据
	UINT VertexByteStride = 0;// 顶点缓冲区单个byte
	UINT VertexBufferByteSize = 0;// 顶点缓冲区总byte
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;// 索引数据格式
	UINT IndexBufferByteSize = 0;// 索引数据总byte

	// 一个MesahGeomotry可以用全局顶点/索引缓冲区中存储多个几何体
	// 使用这个容器来定义Submesh的几何图形，我们就可以利用submesh的name来查找它进行单独绘制
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;// Submesh的单独绘制

	// 获取顶点缓冲视图
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const {
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;

		return vbv;
	}
	
	// 获取索引缓冲视图
	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const {
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	// 我们可以在将数据上传到GPU后释放掉上传堆的内存
	void DisposeUploaders() {
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};
