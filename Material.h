#pragma once
#include "stdafx.h"

using namespace DirectX;

struct MaterialConstants {
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };// ���ʷ�����
	XMFLOAT3 FresnelR0 = { 0.01, 0.01f, 0.01f };// ��������RF(0)ֵ�����ʵķ�������
	float Roughness = 0.25f;// ���ʴֲڳ̶�

	XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();// ������λ�ƾ���
};

struct Material {
	std::string Name;

	int MatCBIndex = -1;// �������ݲų���������������ƫ��

	int DiffuseSrvHeapIndex = -1;// ������SRV�ѵ�ƫ��

	int NumFramesDirty = 3;// ���¼���

	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };// ���ʷ�����
	XMFLOAT3 FresnelR0 = { 0.01, 0.01f, 0.01f };// ��������RF(0)ֵ�����ʵķ�������
	float Roughness = 0.25;// ���ʴֲڳ̶�

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};