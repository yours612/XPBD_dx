#pragma once
#include "stdafx.h"
struct Texture
{
	std::string Name;// ���������

	std::wstring Filename;// ��������·����Ŀ¼��

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;// �����������GPU���ʵ���Դ
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;// �ϴ���
};
