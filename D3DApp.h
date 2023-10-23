#pragma once

#include "stdafx.h"
#include "DxException.h"
#include "GameTimer.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

#define IDM_INSTRUCTION 0

class D3DApp {
protected:

	D3DApp();
	// 维护单例
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();

public: 

	static D3DApp* GetApp();

	int Run();// 消息循环
	
	virtual bool Init(HINSTANCE hInstance, int nShowCmd);// 初始化窗口和D3D
	virtual LRESULT CALLBACK MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);// 窗口过程

	HWND GetMainWnd() { return mhMainWnd; };

protected:

	bool InitWindow(HINSTANCE hInstance, int nShowCmd);// 初始化窗口
	void CreateMyMenu(); // 创建菜单
	void ChangeMenuWord(wstring str);
	bool InitDirect3D();// 初始化D3D

	virtual void Draw(_GameTimer::GameTimer& gt) = 0;
	virtual void Update(_GameTimer::GameTimer& gt) = 0;

	void CreateDevice();// 创建设备
	void CreateFence();// 创建围栏
	void GetDescriptorSize();// 得到描述符大小
	void SetMSAA();// 检测MSAA质量支持
	void CreateCommandObject();// 创建命令队列
	void CreateSwapChain();// 创建交换链
	void CreateRtvAndDsvDescriptorHeaps();// 创建描述符堆
	void CreateRTV();// 创建渲染目标视图
	void CreateDSV();// 创建深度/模板视图
	void CreateViewPortAndScissorRect();// 创建视图和裁剪空间矩形

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void FlushCommandQueue();// 实现围栏
	void CalculateFrameState();// 计算fps和mspfz

	// 鼠标事件
	virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
	virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
	virtual void OnMouseMove(WPARAM btnState, int x, int y) {}

	// 窗口尺寸改变事件
	virtual void OnResize();

	//拾取
	virtual void Pick(int x, int y) {}

protected:

	static D3DApp* mApp;

	HWND mhMainWnd = 0;// 窗口句柄
	HMENU menuWnd = 0; //菜单句柄

	static const int SwapChainBufferCount = 2;// 交换链数量

	//指针接口和变量声明
	ComPtr<ID3D12Device> mD3dDevice;
	ComPtr<IDXGIFactory4> dxgiFactory;
	ComPtr<ID3D12Fence> mFence;
	ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	ComPtr<ID3D12CommandQueue> mCommandQueue;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
	ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	ComPtr<IDXGISwapChain> mSwapChain;
	ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};// 描述队列

	UINT64 mCurrentFence = 0;// 当前围栏值

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;// 渲染目标视图大小
	UINT dsvDescriptorSize = 0;// 深度/模板目标视图大小
	UINT cbv_srv_uavDescriptorSize = 0;// 常量缓冲视图大小

	_GameTimer::GameTimer gt;// 游戏时间实例

	UINT mCurrBackBuffer = 0;

	bool m4xMsaaState = false;// 是否开启4X MSAA
	UINT m4xMsaaQuality = 0;// 4xMSAA质量
	
	std::wstring mMainWndCaption = L"StencilDemo";// 窗口名称
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 1280;
	int mClientHeight = 800;

	// 游戏/窗口状态
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled
};