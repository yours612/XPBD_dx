// stdafx.h：用来include那些基础框架所需要的头文件
// 和那些经常使用但很少更改的头文件
#pragma once

// 排除windows头文件的重定义
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>

#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <MathHelper.h>
#include "d3dx12.h"
#include "DDSTextureLoader.h"

#include <wrl.h>
#include <shellapi.h>

#include <memory>

#include <string>
#include <array>
#include <vector>
#include <unordered_map>

#include <fstream>
#include <iostream>