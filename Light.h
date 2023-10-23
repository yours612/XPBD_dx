#pragma once

#include "stdafx.h"

using namespace DirectX;

#define MaxLight 16

struct Light {
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };// 光源颜色
	float FalloffStart = 1.0f;// 点光源和聚光灯的开始衰减距离
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// 方向光和聚光灯的方向向量
	float FalloffEnd = 10.0f;// 点光和聚光灯的衰减结束距离
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };// 点光和聚光灯的坐标
	float SpotPower = 64.0f;// 聚光灯因子中的参数
};