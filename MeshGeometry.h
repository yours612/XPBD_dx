#pragma once
#include "stdafx.h"

using Microsoft::WRL::ComPtr;

// ��MeshGeometry�ж�����һ��������������塣�����ڻ������������
struct SubmeshGeometry {
	UINT vertexCount = 0; // ��������
	UINT IndexCount = 0;// ��������
	UINT StartIndexLocation = 0;// ��׼����λ��
	INT BaseVertexLocation = 0;// ��׼����λ��
	BoundingBox BoundingBox; //��Χ��
};

// ������
struct MeshGeometry {
	// ��MeshGeometryһ�����֣��������ǿ���ͨ�����������ȷ�ʽ�������в���
	std::string Name;

	// ����������
	vector<int32_t> Tet;

	// ϵͳ�����ڴ�ĸ�������������ʹ��Blob��Ϊvertex/index�������Զ���ṹ������
	ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	// �ϴ���GPU�Ķ���/������Դ
	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	// ����/�������ϴ�����Դ
	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// �������������
	UINT VertexByteStride = 0;// ���㻺��������byte
	UINT VertexBufferByteSize = 0;// ���㻺������byte
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;// �������ݸ�ʽ
	UINT IndexBufferByteSize = 0;// ����������byte

	// һ��MesahGeomotry������ȫ�ֶ���/�����������д洢���������
	// ʹ���������������Submesh�ļ���ͼ�Σ����ǾͿ�������submesh��name�����������е�������
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;// Submesh�ĵ�������

	// ��ȡ���㻺����ͼ
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const {
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;

		return vbv;
	}
	
	// ��ȡ����������ͼ
	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const {
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	// ���ǿ����ڽ������ϴ���GPU���ͷŵ��ϴ��ѵ��ڴ�
	void DisposeUploaders() {
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};
