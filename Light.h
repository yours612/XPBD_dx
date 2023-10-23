#pragma once

#include "stdafx.h"

using namespace DirectX;

#define MaxLight 16

struct Light {
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };// ��Դ��ɫ
	float FalloffStart = 1.0f;// ���Դ�;۹�ƵĿ�ʼ˥������
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// �����;۹�Ƶķ�������
	float FalloffEnd = 10.0f;// ���;۹�Ƶ�˥����������
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };// ���;۹�Ƶ�����
	float SpotPower = 64.0f;// �۹�������еĲ���
};