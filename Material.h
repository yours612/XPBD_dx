#pragma once
#include "stdafx.h"

using namespace DirectX;

struct MaterialConstants {
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };// 材质反射率
	XMFLOAT3 FresnelR0 = { 0.01, 0.01f, 0.01f };// 菲涅尔项RF(0)值，材质的反射属性
	float Roughness = 0.25f;// 材质粗糙程度

	XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();// 纹理动画位移矩阵
};

struct Material {
	std::string Name;

	int MatCBIndex = -1;// 材质数据才常量缓冲区的索引偏移

	int DiffuseSrvHeapIndex = -1;// 材质在SRV堆的偏移

	int NumFramesDirty = 3;// 更新计数

	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };// 材质反射率
	XMFLOAT3 FresnelR0 = { 0.01, 0.01f, 0.01f };// 菲涅尔项RF(0)值，材质的反射属性
	float Roughness = 0.25;// 材质粗糙程度

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};