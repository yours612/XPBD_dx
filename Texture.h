#pragma once
#include "stdafx.h"
struct Texture
{
	std::string Name;// 纹理的名称

	std::wstring Filename;// 纹理所在路径的目录名

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;// 纹理最后能让GPU访问的资源
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;// 上传堆
};
