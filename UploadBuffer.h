// 获得不同类型的上传堆资源(GPU)
#pragma once
#include "stdafx.h"
#include "d3dUtil.h"

using Microsoft::WRL::ComPtr;

template<typename T>
class UploadBuffer {
public:

	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer):
		mIsConstantBuffer(isConstantBuffer) {
		// 判断资源byte
		mElementByteSize = sizeof(T);
		if (isConstantBuffer)
			mElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));

		// 创建上传堆
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploaderBuffer)));

		// 获得指向欲更新资源数据的指针
		ThrowIfFailed(mUploaderBuffer->Map(0,// 子资源索引
			nullptr,// 内存映射范围，nullptr对整个资源进行映射
			reinterpret_cast<void**>(&mMappedData)));// [out]借助双重指针，返回映射资源数据的目标内存块
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

	// 析构时取消映射并释放内存
	~UploadBuffer() {
		if (mUploaderBuffer != nullptr)
			mUploaderBuffer->Unmap(0,// 子资源索引
				nullptr);// 取消映射范围
		mMappedData = nullptr;
	}

	// 返回资源指针
	ID3D12Resource* Resource() const {
		return mUploaderBuffer.Get();
	}

	// 将CPU上的数据复制到GPU上的缓冲区中
	void CopyData(int elementIndex, const T& data) {
		memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
	}

private:
	ComPtr<ID3D12Resource> mUploaderBuffer;
	BYTE* mMappedData = nullptr;
	
	UINT mElementByteSize = 0;
	bool mIsConstantBuffer = false;
};
