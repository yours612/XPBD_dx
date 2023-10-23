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

// 初始化窗口和D3D
bool D3DApp::Init(HINSTANCE hInstance, int nShowCmd) {
	if (!InitWindow(hInstance, nShowCmd))
		return false;
	else if (!InitDirect3D())
		return false;

	// 创建RTV DSV ViewPort ScissorRect
	OnResize();

	return true;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}


// 创建窗口
bool D3DApp::InitWindow(HINSTANCE hInstance, int nShowCmd) {
	AllocConsole();
	FILE* stream;
	freopen_s(&stream, "CON", "r", stdin);//重定向输入流
	freopen_s(&stream, "CON", "w", stdout);//重定向输入流

	// 窗口初始化描述结构体(WNDCLASS)
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;	// 当工作区宽高改变，则重新绘制窗口
	wc.lpfnWndProc = MainWndProc;	// 指定窗口过程
	wc.cbClsExtra = 0;	// 借助这两个字段来为当前应用分配额外的内存空间（这里不分配，所以置0）
	wc.cbWndExtra = 0;	// 借助这两个字段来为当前应用分配额外的内存空间（这里不分配，所以置0）
	wc.hInstance = hInstance;	// 应用程序实例句柄（由WinMain传入）
	wc.hIcon = LoadIcon(0, IDC_ARROW);	//使用默认的应用程序图标
	wc.hCursor = LoadCursor(0, IDC_ARROW);	//使用标准的鼠标指针样式
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);	//指定了白色背景画刷句柄
	wc.lpszMenuName = 0;	//没有菜单栏
	wc.lpszClassName = L"MainWnd";	//窗口名
	// 窗口类注册失败
	if (!RegisterClass(&wc)) {
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return 0;
	}

	// 窗口类注册成功
	RECT R = { 0, 0, mClientWidth, mClientHeight };	// 裁剪矩形
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);	//根据窗口的客户区大小计算窗口的大小
	int width = R.right - R.left;
	int hight = R.bottom - R.top;

	// 创建窗口,返回布尔值
	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 
		mClientWidth/5, mClientHeight/6, width, hight, 0, menuWnd, hInstance, 0);
	// 窗口创建失败
	if (!mhMainWnd) {
		MessageBox(0, L"CreatWindow Failed", 0, 0);
		return 0;
	}
	// 窗口创建成功,则显示并更新窗口
	ShowWindow(mhMainWnd, nShowCmd);
	UpdateWindow(mhMainWnd);

	return true;
}

// 初始化D3D
bool D3DApp::InitDirect3D() {
	/*开启D3D12调试层*/
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
	
	// 在Init中执行OnResize
	// CreateRTV();
	// CreateDSV();
	// CreateViewPortAndScissorRect();

	return true;
}

// 窗口过程
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	// 消息处理
	switch (msg) {
	// 鼠标按键按下时的触发（左中右）
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	// wParam为输入的虚拟键代码，lParam为系统反馈的光标信息
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	// 鼠标按键抬起时的触发（左中右）
	case WM_LBUTTONUP:
		break;
	case WM_MBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_RBUTTONUP:
		break;

	// 鼠标移动的触发
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;


	case WM_MOUSELEAVE:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	// 当窗口被销毁时，终止消息循环
	case WM_DESTROY:
		PostQuitMessage(0);	//终止消息循环，并发出WM_QUIT消息
		return 0;

	// 当窗口尺寸发生变换时
	case WM_SIZE:
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (mD3dDevice) {
			//如果最小化,则暂停游戏，调整最小化和最大化状态
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
	// 将上面没有处理的消息转发给默认的窗口过程
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// 消息循环，采用"一种灵活的消息循环"
// Run()每运行一次 代表一帧
int D3DApp::Run() {
	// 定义消息结构体
	MSG msg = { 0 };
	// 每次消息循环重置一次计数器
	gt.Reset();
	// 如果PeekMessage函数不等于0，说明没有接受到WM_QUIT
	while (msg.message != WM_QUIT) {
		// 如果有窗口消息就进行处理
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) // PeekMessage函数会自动填充msg结构体元素
		{
			TranslateMessage(&msg);	// 键盘按键转换，将虚拟键消息转换为字符消息
			DispatchMessage(&msg);	// 把消息分派给相应的窗口过程
		}
		// 否则就执行动画和游戏逻辑
		else {
			gt.Tick();// 计算每两帧间隔时间 并获取现在的时间
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

// 创建设备
void D3DApp::CreateDevice() {
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));
	ThrowIfFailed(D3D12CreateDevice(nullptr, // 此参数如果设置为nullptr，则使用主适配器
		D3D_FEATURE_LEVEL_12_0,		// 应用程序需要硬件所支持的最低功能级别
		IID_PPV_ARGS(&mD3dDevice)));	// 返回所建设备
}

// 创建围栏
void D3DApp::CreateFence() {
	ThrowIfFailed(mD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
}


// 获取描述符大小 用来找到堆的偏移
void D3DApp::GetDescriptorSize() {
	// UINT 
	mRtvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);// 渲染目标缓冲区描述符大小
	// UINT 
	dsvDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);// 深度模板缓冲区描述符大小
	// UINT 
	cbv_srv_uavDescriptorSize = mD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);// 常量缓冲区描述符大小
}

// 检测MSAA质量支持
// 注意：此处不使用MSAA，采样数量设置为1（即不采样）。
void D3DApp::SetMSAA() {
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// UNORM是归一化处理的无符号整数
	msQualityLevels.SampleCount = 1;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;// 没有任何选项支持
	msQualityLevels.NumQualityLevels = 0;
	// 检测驱动是否支持这个MSAA描述 （注意：第二个参数即是输入又是输出）
	ThrowIfFailed(mD3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;

	// 如果不支持支持这个MSAA描述，则Check函数返回的NumQualityLevels = 0
	assert(msQualityLevels.NumQualityLevels > 0);
}

// 创建命令队列
void D3DApp::CreateCommandObject() {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};// 描述队列

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;// 指定GPU可以执行的命令缓冲区，直接命令列表未继承任何GPU状态
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;// 默认命令队列
	ThrowIfFailed(mD3dDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&mCommandQueue)));// 创建命令队列
	ThrowIfFailed(mD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectCmdListAlloc)));// 创建命令分配器 &cmdAllocator等价于cmdAllocator.GetAddressOf
	// 创建命令列表
	ThrowIfFailed(mD3dDevice->CreateCommandList(0, // 掩码值为0，单GPU
		D3D12_COMMAND_LIST_TYPE_DIRECT, // 命令列表类型
		mDirectCmdListAlloc.Get(),	// 命令分配器接口指针
		nullptr,	// 流水线状态对象PSO，这里不绘制，所以空指针
		IID_PPV_ARGS(&mCommandList)));	// 返回创建的命令列表
	mCommandList->Close();	// 重置命令列表前必须将其关闭
}

// 创建交换链
void D3DApp::CreateSwapChain() {
	mSwapChain.Reset();
	// 描述交换链
	DXGI_SWAP_CHAIN_DESC swapChainDesc;// 描述交换链结构体
	swapChainDesc.BufferDesc.Width = mClientWidth;	// 缓冲区分辨率的宽度
	swapChainDesc.BufferDesc.Height = mClientHeight;	// 缓冲区分辨率的高度
	swapChainDesc.BufferDesc.Format = mBackBufferFormat;	// 缓冲区的显示格式
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;	// 刷新率的分子
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;	// 刷新率的分母
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;	// 逐行扫描VS隔行扫描(未指定的)
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;	// 图像相对屏幕的拉伸（未指定的）
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// 将数据渲染至后台缓冲区（即作为渲染目标）
	swapChainDesc.OutputWindow = mhMainWnd;	// 渲染窗口句柄
	swapChainDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;	// 多重采样数量
	swapChainDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;	// 多重采样质量
	swapChainDesc.Windowed = true;	// 是否窗口化
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	// 固定写法
	swapChainDesc.BufferCount = SwapChainBufferCount;	// 后台缓冲区数量（双缓冲）
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	// 自适应窗口模式（自动选择最适于当前窗口尺寸的显示模式）
	// 利用DXGI接口下的工厂类创建交换链
	ThrowIfFailed(dxgiFactory->CreateSwapChain(mCommandQueue.Get(), &swapChainDesc, mSwapChain.GetAddressOf()));
}

// 创建描述堆符
void D3DApp::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
	// 首先创建RTV堆
	rtvDescriptorHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(mD3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&mRtvHeap)));
	// 然后创建DSV堆
	dsvDescriptorHeapDesc.NumDescriptors = 1;
	dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(mD3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
}

// 创建渲染目标视图描述符
void D3DApp::CreateRTV() {
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (int i = 0; i < SwapChainBufferCount; i++) {
		// 获得存于交换链中的后台缓冲区资源
		mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf()));
		// 创建RTV
		mD3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(),
			nullptr,	// 在交换链创建中已经定义了该资源的数据格式，所以这里指定为空指针
			rtvHeapHandle);	// 描述符句柄结构体（这里是变体，继承自CD3DX12_CPU_DESCRIPTOR_HANDLE）
		// 偏移到描述符堆中的下一个缓冲区
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}
}

// 创建深度/模板缓冲区及其视图
void D3DApp::CreateDSV() {
	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;// 描述模板类型
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;// 指定资源维度（类型）为TEXTURE2D
	depthStencilDesc.Alignment = 0;// 指定对齐
	depthStencilDesc.Width = mClientWidth;// 资源宽
	depthStencilDesc.Height = mClientHeight;// 资源高
	depthStencilDesc.DepthOrArraySize = 1;// 纹理深度为1
	depthStencilDesc.MipLevels = 1;// MIPMAP层级数量
	depthStencilDesc.Format = mDepthStencilFormat;// 24位深度，8位模板,还有个无类型的格式DXGI_FORMAT_R24G8_TYPELESS也可以使用
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;// 多重采样数量
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;// 多重采样质量
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;// 指定纹理布局（这里不指定）
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;// 深度模板资源的Flag

	// 描述清除资源的优化值
	D3D12_CLEAR_VALUE optClear;// 清除资源的优化值，提高清除操作的执行速度（CreateCommittedResource函数中传入）
	optClear.Format = mDepthStencilFormat;// 24位深度，8位模板,还有个无类型的格式DXGI_FORMAT_R24G8_TYPELESS也可以使用
	optClear.DepthStencil.Depth = 1.0f;// 初始深度值为1
	optClear.DepthStencil.Stencil = 0;// 初始模板值为0
	// 创建一个资源和一个堆，并将资源提交至堆中（将深度模板数据提交至GPU显存中）
	ThrowIfFailed(mD3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),// 堆所具有的属性，设置为默认堆。
		D3D12_HEAP_FLAG_NONE,// 堆类型为默认堆（不能写入）
		&depthStencilDesc,// Flag
		D3D12_RESOURCE_STATE_COMMON,// 上面定义的DSV资源指针
		&optClear,// 上面定义的优化值指针
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));// 返回深度模板资源

	// 创建DSV(必须填充DSV属性结构体，和创建渲染目标视图不同，RTV是通过句柄)
	// D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	// dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	// dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	// dsvDesc.Texture2D.MipSlice = 0;
	mD3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), // D3D12_DEPTH_STENCIL_VIEW_DESC类型指针，可填&dsvDesc（见上注释代码）
		nullptr,// 由于在创建深度模板资源时已经定义深度模板数据属性，所以这里可以指定为空指针
		DepthStencilView());// DSV句柄

	// 把资源从初始状态转换到深度缓冲状态
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
}

// 标记深度/模板资源的状态
//void D3DApp::resourceBarrierBuild() {
//	cmdList->ResourceBarrier(1,	// Barrier屏障个数
//		&CD3DX12_RESOURCE_BARRIER::Transition(depthStencilBuffer.Get(),// 视图的COM接口(?)。指定之前的深度/模板缓冲区的接口。
//			D3D12_RESOURCE_STATE_COMMON,	// 转换前状态（创建时的状态，即CreateCommittedResource函数中定义的状态）
//			D3D12_RESOURCE_STATE_DEPTH_WRITE));	// 转换后状态为可写入的深度图，还有一个D3D12_RESOURCE_STATE_DEPTH_READ是只可读的深度图
//	// 等所有命令都进入cmdList后，将命令从命令列表传入命令队列，也就是从CPU传入GPU的过程。
//	// 注意：在传入命令队列前必须关闭命令列表。
//	ThrowIfFailed(cmdList->Close());	// 命令添加完后将其关闭
//	ID3D12CommandList* cmdLists[] = { cmdList.Get() };	// 声明并定义命令列表数组
//	cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);	// 将命令从命令列表传至命令队列
//}

// 实现围栏
void D3DApp::FlushCommandQueue() {
	mCurrentFence++;	// CPU传完命令并关闭后，将当前围栏值+1
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));	// 当GPU处理完CPU传入的命令后，将fence接口中的围栏值+1，即fence->GetCompletedValue()+1
	if (mFence->GetCompletedValue() < mCurrentFence)	// 如果小于，说明GPU没有处理完所有命令
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, L"FenceSetDone");	// 创建事件
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));// 当围栏达到mCurrentFence值（即执行到Signal（）指令修改了围栏值）时触发的eventHandle事件
		WaitForSingleObject(eventHandle, INFINITE);// 等待GPU命中围栏，激发事件（阻塞当前线程直到事件触发，注意此Enent需先设置再等待，
							   // 如果没有Set就Wait，就死锁了，Set永远不会调用，所以也就没线程可以唤醒这个线程）
		CloseHandle(eventHandle);
	}
}

// 设置视口和裁剪矩形
void D3DApp::CreateViewPortAndScissorRect() {
	// 视口设置
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;
	// 裁剪矩形设置（矩形外的像素都将被剔除）
	// 前两个为左上点坐标，后两个为右下点坐标
	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}

// 计算帧率和每帧多少毫秒
void D3DApp::CalculateFrameState() {
	using namespace _GameTimer;
	static int frameCnt = 0;// 总帧数
	static float timeElapsed = 0.0f;//总时间
	frameCnt++;
	if (gt.TotalTime() - timeElapsed >= 1.0f) {
		float fps = frameCnt;// 每秒多少帧
		float mspf = 1000.0f / fps;// 每帧多少毫秒

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);
		// 将帧数显示在窗口上
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

// 窗口尺寸改变事件
void D3DApp::OnResize() {
	assert(mD3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// 改变资源前先同步
	FlushCommandQueue();
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// 释放之前的资源，为我们重新创建做好准备
	for (int i = 0; i < SwapChainBufferCount; i++) 
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// 重新调整后台缓冲区资源的大小
	ThrowIfFailed(mSwapChain->ResizeBuffers(SwapChainBufferCount,
		mClientWidth,
		mClientHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	// 后台缓冲区索引置零
	mCurrBackBuffer = 0;

	CreateRTV();
	CreateDSV();
	// 执行Resize命令
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 等待Resize命令完成
	FlushCommandQueue();

	// 更新视口/投影矩阵适应窗口变换
	CreateViewPortAndScissorRect();
}
