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
	// ά������
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();

public: 

	static D3DApp* GetApp();

	int Run();// ��Ϣѭ��
	
	virtual bool Init(HINSTANCE hInstance, int nShowCmd);// ��ʼ�����ں�D3D
	virtual LRESULT CALLBACK MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);// ���ڹ���

	HWND GetMainWnd() { return mhMainWnd; };

protected:

	bool InitWindow(HINSTANCE hInstance, int nShowCmd);// ��ʼ������
	void CreateMyMenu(); // �����˵�
	void ChangeMenuWord(wstring str);
	bool InitDirect3D();// ��ʼ��D3D

	virtual void Draw(_GameTimer::GameTimer& gt) = 0;
	virtual void Update(_GameTimer::GameTimer& gt) = 0;

	void CreateDevice();// �����豸
	void CreateFence();// ����Χ��
	void GetDescriptorSize();// �õ���������С
	void SetMSAA();// ���MSAA����֧��
	void CreateCommandObject();// �����������
	void CreateSwapChain();// ����������
	void CreateRtvAndDsvDescriptorHeaps();// ������������
	void CreateRTV();// ������ȾĿ����ͼ
	void CreateDSV();// �������/ģ����ͼ
	void CreateViewPortAndScissorRect();// ������ͼ�Ͳü��ռ����

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void FlushCommandQueue();// ʵ��Χ��
	void CalculateFrameState();// ����fps��mspfz

	// ����¼�
	virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
	virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
	virtual void OnMouseMove(WPARAM btnState, int x, int y) {}

	// ���ڳߴ�ı��¼�
	virtual void OnResize();

	//ʰȡ
	virtual void Pick(int x, int y) {}

protected:

	static D3DApp* mApp;

	HWND mhMainWnd = 0;// ���ھ��
	HMENU menuWnd = 0; //�˵����

	static const int SwapChainBufferCount = 2;// ����������

	//ָ��ӿںͱ�������
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

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};// ��������

	UINT64 mCurrentFence = 0;// ��ǰΧ��ֵ

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	UINT mRtvDescriptorSize = 0;// ��ȾĿ����ͼ��С
	UINT dsvDescriptorSize = 0;// ���/ģ��Ŀ����ͼ��С
	UINT cbv_srv_uavDescriptorSize = 0;// ����������ͼ��С

	_GameTimer::GameTimer gt;// ��Ϸʱ��ʵ��

	UINT mCurrBackBuffer = 0;

	bool m4xMsaaState = false;// �Ƿ���4X MSAA
	UINT m4xMsaaQuality = 0;// 4xMSAA����
	
	std::wstring mMainWndCaption = L"StencilDemo";// ��������
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 1280;
	int mClientHeight = 800;

	// ��Ϸ/����״̬
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled
};