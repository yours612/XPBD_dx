#include "D3DApp.h"



D3DApp::D3DApp() {
	assert(mApp == nullptr);
	mApp = this;
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp* D3DApp::GetApp() {
	return mApp;
}

D3DApp::~D3DApp() {
	if (mD3dDevice != nullptr)
		FlushCommandQueue();
}

// ��ʼ�����ں�D3D
bool D3DApp::Init(HINSTANCE hInstance, int nShowCmd) {
	if (!InitWindow(hInstance, nShowCmd))
		return false;
	else if (!InitDirect3D())
		return false;

	// ����RTV DSV ViewPort ScissorRect
	OnResize();

	return true;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}


// ��������
bool D3DApp::InitWindow(HINSTANCE hInstance, int nShowCmd) {
	AllocConsole();
	FILE* stream;
	freopen_s(&stream, "CON", "r", stdin);//�ض���������
	freopen_s(&stream, "CON", "w", stdout);//�ض���������

	// ���ڳ�ʼ�������ṹ��(WNDCLASS)
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;	// ����������߸ı䣬�����»��ƴ���
	wc.lpfnWndProc = MainWndProc;	// ָ�����ڹ���
	wc.cbClsExtra = 0;	// �����������ֶ���Ϊ��ǰӦ�÷��������ڴ�ռ䣨���ﲻ���䣬������0��
	wc.cbWndExtra = 0;	// �����������ֶ���Ϊ��ǰӦ�÷��������ڴ�ռ䣨���ﲻ���䣬������0��
	wc.hInstance = hInstance;	// Ӧ�ó���ʵ���������WinMain���룩
	wc.hIcon = LoadIcon(0, IDC_ARROW);	//ʹ��Ĭ�ϵ�Ӧ�ó���ͼ��
	wc.hCursor = LoadCursor(0, IDC_ARROW);	//ʹ�ñ�׼�����ָ����ʽ
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);	//ָ���˰�ɫ������ˢ���
	wc.lpszMenuName = 0;	//û�в˵���
	wc.lpszClassName = L"MainWnd";	//������
	// ������ע��ʧ��
	if (!RegisterClass(&wc)) {
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return 0;
	}

	// ������ע��ɹ�
	RECT R = { 0, 0, mClientWidth, mClientHeight };	// �ü�����
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);	//���ݴ��ڵĿͻ�����С���㴰�ڵĴ�С
	int width = R.right - R.left;
	int hight = R.bottom - R.top;

	// ��������,���ز���ֵ
	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 
		mClientWidth/5, mClientHeight/6, width, hight, 0, menuWnd, hInstance, 0);
	// ���ڴ���ʧ��
	if (!mhMainWnd) {
		MessageBox(0, L"CreatWindow Failed", 0, 0);
		return 0;
	}
	// ���ڴ����ɹ�,����ʾ�����´���
	ShowWindow(mhMainWnd, nShowCmd);
	UpdateWindow(mhMainWnd);

	return true;
}

// ��ʼ��D3D
bool D3DApp::InitDirect3D() {
	/*����D3D12���Բ�*/
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif

	CreateDevice();
	CreateFence();
	GetDescriptorSize();
	SetMSAA();
	CreateCommandObject();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();
	
	// ��Init��ִ��OnResize
	// CreateRTV();
	// CreateDSV();
	// CreateViewPortAndScissorRect();

	return true;
}

// ���ڹ���
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	// ��Ϣ����
	switch (msg) {
	// ��갴������ʱ�Ĵ����������ң�
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	// wParamΪ�������������룬lParamΪϵͳ�����Ĺ����Ϣ
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	// ��갴��̧��ʱ�Ĵ����������ң�
	case WM_LBUTTONUP:
		break;
	case WM_MBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_RBUTTONUP:
		break;

	// ����ƶ��Ĵ���
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;


	case WM_MOUSELEAVE:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	// �����ڱ�����ʱ����ֹ��Ϣѭ��
	case WM_DESTROY:
		PostQuitMessage(0);	//��ֹ��Ϣѭ����������WM_QUIT��Ϣ
		return 0;

	// �����ڳߴ緢���任ʱ
	case WM_SIZE:
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (mD3dDevice) {
			//�����С��,����ͣ��Ϸ��������С�������״̬
			if (wParam == SIZE_MINIMIZED) {
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED) {
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED) {
				if (mMinimized) {
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				else if (mMaximized) {
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}

				else if (mResizing) {
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else {
					OnResize();
				}
			}
		}
		return 0;

	default:
		break;
	}
	// ������û�д������Ϣת����Ĭ�ϵĴ��ڹ���
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ��Ϣѭ��������"һ��������Ϣѭ��"
// Run()ÿ����һ�� ����һ֡
int D3DApp::Run() {
	// ������Ϣ�ṹ��
	MSG msg = { 0 };
	// ÿ����Ϣѭ������һ�μ�����
	gt.Reset();
	// ���PeekMessage����������0��˵��û�н��ܵ�WM_QUIT
	while (msg.message != WM_QUIT) {
		// ����д�����Ϣ�ͽ��д���
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) // PeekMessage�������Զ����msg�ṹ��Ԫ��
		{
			TranslateMessage(&msg);	// ���̰���ת�������������Ϣת��Ϊ�ַ���Ϣ
			DispatchMessage(&msg);	// ����Ϣ���ɸ���Ӧ�Ĵ��ڹ���
		}
		// �����ִ�ж�������Ϸ�߼�
		else {
			gt.Tick();// ����ÿ��֡���ʱ�� ����ȡ���ڵ�ʱ��
			if (!gt.IsStopped()) {
				CalculateFrameState();
				Update(gt);
				Draw(gt);
			}
			else {
				Sleep(100);
			}
		}
	}
	return (int)msg.wParam;
}

// �����豸
void D3DApp::CreateDevice() {
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));
	ThrowIfFailed(D3D12CreateDevice(nullptr, // �˲����������Ϊnullptr����ʹ����������
		D3D_FEATURE_LEVEL_12_0,		// Ӧ�ó�����ҪӲ����֧�ֵ���͹��ܼ���
		IID_PPV_ARGS(&mD3dDevice)));	// ���������豸
}

// ����Χ��
void D3DApp::CreateFence() {
	ThrowIfFailed(mD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
}


// ��ȡ��������С �����ҵ��ѵ�ƫ��
void D3DApp::GetDescriptorSize() {
	// UINT 
	mRtvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);// ��ȾĿ�껺������������С
	// UINT 
	dsvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);// ���ģ�建������������С
	// UINT 
	cbv_srv_uavDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);// ������������������С
}

// ���MSAA����֧��
// ע�⣺�˴���ʹ��MSAA��������������Ϊ1��������������
void D3DApp::SetMSAA() {
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// UNORM�ǹ�һ��������޷�������
	msQualityLevels.SampleCount = 1;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;// û���κ�ѡ��֧��
	msQualityLevels.NumQualityLevels = 0;
	// ��������Ƿ�֧�����MSAA���� ��ע�⣺�ڶ������������������������
	ThrowIfFailed(mD3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;

	// �����֧��֧�����MSAA��������Check�������ص�NumQualityLevels = 0
	assert(msQualityLevels.NumQualityLevels > 0);
}

// �����������
void D3DApp::CreateCommandObject() {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};// ��������

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;// ָ��GPU����ִ�е����������ֱ�������б�δ�̳��κ�GPU״̬
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;// Ĭ���������
	ThrowIfFailed(mD3dDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&mCommandQueue)));// �����������
	ThrowIfFailed(mD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectCmdListAlloc)));// ������������� &cmdAllocator�ȼ���cmdAllocator.GetAddressOf
	// ���������б�
	ThrowIfFailed(mD3dDevice->CreateCommandList(0, // ����ֵΪ0����GPU
		D3D12_COMMAND_LIST_TYPE_DIRECT, // �����б�����
		mDirectCmdListAlloc.Get(),	// ����������ӿ�ָ��
		nullptr,	// ��ˮ��״̬����PSO�����ﲻ���ƣ����Կ�ָ��
		IID_PPV_ARGS(&mCommandList)));	// ���ش����������б�
	mCommandList->Close();	// ���������б�ǰ���뽫��ر�
}

// ����������
void D3DApp::CreateSwapChain() {
	mSwapChain.Reset();
	// ����������
	DXGI_SWAP_CHAIN_DESC swapChainDesc;// �����������ṹ��
	swapChainDesc.BufferDesc.Width = mClientWidth;	// �������ֱ��ʵĿ��
	swapChainDesc.BufferDesc.Height = mClientHeight;	// �������ֱ��ʵĸ߶�
	swapChainDesc.BufferDesc.Format = mBackBufferFormat;	// ����������ʾ��ʽ
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;	// ˢ���ʵķ���
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;	// ˢ���ʵķ�ĸ
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;	// ����ɨ��VS����ɨ��(δָ����)
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;	// ͼ�������Ļ�����죨δָ���ģ�
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// ��������Ⱦ����̨������������Ϊ��ȾĿ�꣩
	swapChainDesc.OutputWindow = mhMainWnd;	// ��Ⱦ���ھ��
	swapChainDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;	// ���ز�������
	swapChainDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;	// ���ز�������
	swapChainDesc.Windowed = true;	// �Ƿ񴰿ڻ�
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	// �̶�д��
	swapChainDesc.BufferCount = SwapChainBufferCount;	// ��̨������������˫���壩
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	// ����Ӧ����ģʽ���Զ�ѡ�������ڵ�ǰ���ڳߴ����ʾģʽ��
	// ����DXGI�ӿ��µĹ����ഴ��������
	ThrowIfFailed(dxgiFactory->CreateSwapChain(mCommandQueue.Get(), &swapChainDesc, mSwapChain.GetAddressOf()));
}

// ���������ѷ�
void D3DApp::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
	// ���ȴ���RTV��
	rtvDescriptorHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(mD3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&mRtvHeap)));
	// Ȼ�󴴽�DSV��
	dsvDescriptorHeapDesc.NumDescriptors = 1;
	dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(mD3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
}

// ������ȾĿ����ͼ������
void D3DApp::CreateRTV() {
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < SwapChainBufferCount; i++) {
		// ��ô��ڽ������еĺ�̨��������Դ
		mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf()));
		// ����RTV
		mD3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(),
			nullptr,	// �ڽ������������Ѿ������˸���Դ�����ݸ�ʽ����������ָ��Ϊ��ָ��
			rtvHeapHandle);	// ����������ṹ�壨�����Ǳ��壬�̳���CD3DX12_CPU_DESCRIPTOR_HANDLE��
		// ƫ�Ƶ����������е���һ��������
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}
}

// �������/ģ�建����������ͼ
void D3DApp::CreateDSV() {
	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;// ����ģ������
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;// ָ����Դά�ȣ����ͣ�ΪTEXTURE2D
	depthStencilDesc.Alignment = 0;// ָ������
	depthStencilDesc.Width = mClientWidth;// ��Դ��
	depthStencilDesc.Height = mClientHeight;// ��Դ��
	depthStencilDesc.DepthOrArraySize = 1;// �������Ϊ1
	depthStencilDesc.MipLevels = 1;// MIPMAP�㼶����
	depthStencilDesc.Format = mDepthStencilFormat;// 24λ��ȣ�8λģ��,���и������͵ĸ�ʽDXGI_FORMAT_R24G8_TYPELESSҲ����ʹ��
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;// ���ز�������
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;// ���ز�������
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;// ָ�������֣����ﲻָ����
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;// ���ģ����Դ��Flag

	// ���������Դ���Ż�ֵ
	D3D12_CLEAR_VALUE optClear;// �����Դ���Ż�ֵ��������������ִ���ٶȣ�CreateCommittedResource�����д��룩
	optClear.Format = mDepthStencilFormat;// 24λ��ȣ�8λģ��,���и������͵ĸ�ʽDXGI_FORMAT_R24G8_TYPELESSҲ����ʹ��
	optClear.DepthStencil.Depth = 1.0f;// ��ʼ���ֵΪ1
	optClear.DepthStencil.Stencil = 0;// ��ʼģ��ֵΪ0
	// ����һ����Դ��һ���ѣ�������Դ�ύ�����У������ģ�������ύ��GPU�Դ��У�
	ThrowIfFailed(mD3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),// �������е����ԣ�����ΪĬ�϶ѡ�
		D3D12_HEAP_FLAG_NONE,// ������ΪĬ�϶ѣ�����д�룩
		&depthStencilDesc,// Flag
		D3D12_RESOURCE_STATE_COMMON,// ���涨���DSV��Դָ��
		&optClear,// ���涨����Ż�ֵָ��
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));// �������ģ����Դ

	// ����DSV(�������DSV���Խṹ�壬�ʹ�����ȾĿ����ͼ��ͬ��RTV��ͨ�����)
	// D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	// dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	// dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	// dsvDesc.Texture2D.MipSlice = 0;
	mD3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), // D3D12_DEPTH_STENCIL_VIEW_DESC����ָ�룬����&dsvDesc������ע�ʹ��룩
		nullptr,// �����ڴ������ģ����Դʱ�Ѿ��������ģ���������ԣ������������ָ��Ϊ��ָ��
		DepthStencilView());// DSV���

	// ����Դ�ӳ�ʼ״̬ת������Ȼ���״̬
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
}

// ������/ģ����Դ��״̬
//void D3DApp::resourceBarrierBuild() {
//	cmdList->ResourceBarrier(1,	// Barrier���ϸ���
//		&CD3DX12_RESOURCE_BARRIER::Transition(depthStencilBuffer.Get(),// ��ͼ��COM�ӿ�(?)��ָ��֮ǰ�����/ģ�建�����Ľӿڡ�
//			D3D12_RESOURCE_STATE_COMMON,	// ת��ǰ״̬������ʱ��״̬����CreateCommittedResource�����ж����״̬��
//			D3D12_RESOURCE_STATE_DEPTH_WRITE));	// ת����״̬Ϊ��д������ͼ������һ��D3D12_RESOURCE_STATE_DEPTH_READ��ֻ�ɶ������ͼ
//	// �������������cmdList�󣬽�����������б���������У�Ҳ���Ǵ�CPU����GPU�Ĺ��̡�
//	// ע�⣺�ڴ����������ǰ����ر������б�
//	ThrowIfFailed(cmdList->Close());	// ������������ر�
//	ID3D12CommandList* cmdLists[] = { cmdList.Get() };	// ���������������б�����
//	cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);	// ������������б����������
//}

// ʵ��Χ��
void D3DApp::FlushCommandQueue() {
	mCurrentFence++;	// CPU��������رպ󣬽���ǰΧ��ֵ+1
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));	// ��GPU������CPU���������󣬽�fence�ӿ��е�Χ��ֵ+1����fence->GetCompletedValue()+1
	if (mFence->GetCompletedValue() < mCurrentFence)	// ���С�ڣ�˵��GPUû�д�������������
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, L"FenceSetDone");	// �����¼�
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));// ��Χ���ﵽmCurrentFenceֵ����ִ�е�Signal����ָ���޸���Χ��ֵ��ʱ������eventHandle�¼�
		WaitForSingleObject(eventHandle, INFINITE);// �ȴ�GPU����Χ���������¼���������ǰ�߳�ֱ���¼�������ע���Enent���������ٵȴ���
							   // ���û��Set��Wait���������ˣ�Set��Զ������ã�����Ҳ��û�߳̿��Ի�������̣߳�
		CloseHandle(eventHandle);
	}
}

// �����ӿںͲü�����
void D3DApp::CreateViewPortAndScissorRect() {
	// �ӿ�����
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;
	// �ü��������ã�����������ض������޳���
	// ǰ����Ϊ���ϵ����꣬������Ϊ���µ�����
	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}

// ����֡�ʺ�ÿ֡���ٺ���
void D3DApp::CalculateFrameState() {
	using namespace _GameTimer;
	static int frameCnt = 0;// ��֡��
	static float timeElapsed = 0.0f;//��ʱ��
	frameCnt++;
	if (gt.TotalTime() - timeElapsed >= 1.0f) {
		float fps = frameCnt;// ÿ�����֡
		float mspf = 1000.0f / fps;// ÿ֡���ٺ���

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);
		// ��֡����ʾ�ڴ�����
		std::wstring windowText = mMainWndCaption + L" fps: " + fpsStr + L"    " + L"mspf: " + mspfStr;
		SetWindowText(mhMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer() const {
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const {
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

// ���ڳߴ�ı��¼�
void D3DApp::OnResize() {
	assert(mD3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// �ı���Դǰ��ͬ��
	FlushCommandQueue();
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// �ͷ�֮ǰ����Դ��Ϊ�������´�������׼��
	for (int i = 0; i < SwapChainBufferCount; i++) 
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// ���µ�����̨��������Դ�Ĵ�С
	ThrowIfFailed(mSwapChain->ResizeBuffers(SwapChainBufferCount,
		mClientWidth,
		mClientHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	// ��̨��������������
	mCurrBackBuffer = 0;

	CreateRTV();
	CreateDSV();
	// ִ��Resize����
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// �ȴ�Resize�������
	FlushCommandQueue();

	// �����ӿ�/ͶӰ������Ӧ���ڱ任
	CreateViewPortAndScissorRect();
}
