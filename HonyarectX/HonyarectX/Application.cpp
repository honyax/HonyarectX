#if 0

#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"

/// <summary>ウィンドウ定数</summary>
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

/// <summary>面倒だけど書かなあかんやつ</summary>
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		// ウィンドウが破棄されたら呼ばれる
		// OSに対して「もうこのアプリは終わる」と伝える
		PostQuitMessage(0);
		return 0;
	}
	// 規定の処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	HINSTANCE hInst = GetModuleHandle(nullptr);

	// ウィンドウクラス生成＆登録
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;		// コールバック関数の指定
	windowClass.lpszClassName = _T("HonyarectX");			// アプリケーションクラス名（適当でよい）
	windowClass.hInstance = GetModuleHandle(0);				// ハンドルの取得
	RegisterClassEx(&_windowClass);							// アプリケーションクラス（ウィンドウクラスの指定をOSに伝える）

	RECT wrc = { 0, 0, window_width, window_height };		// ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);		// 関数を使ってウィンドウのサイズを補正する

	// ウィンドウオブジェクトの生成
	hwnd = CreateWindow(
		windowClass.lpszClassName,		// クラス名指定
		_T("HonyarectX"),				// タイトルバーの文字
		WS_OVERLAPPEDWINDOW,			// タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT,					// 表示x座標はOSにお任せ
		CW_USEDEFAULT,					// 表示y座標はOSにお任せ
		wrc.right - wrc.left,			// ウィンドウ幅
		wrc.bottom - wrc.top,			// ウィンドウ高
		nullptr,						// 親ウィンドウハンドル
		nullptr,						// メニューハンドル
		windowClass.hInstance,			// 呼び出しアプリケーションハンドル
		nullptr);						// 追加パラメーター
}

SIZE Application::GetWindowSize() const
{
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW);			// ウィンドウ表示
	float angle = 0.0f;
	MSG msg = {};
	UINT frame = 0;
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {
			break;
		}

		// 全体の描画準備
		_dx12->BeginDraw();

		// PMD用の描画パイプラインに合わせる
		_dx12->CommandList()->SetPipelineState(_pmdRenderer->GetPipelineState());
		// ルートシグネチャもPMD用に合わせる
		_dx12->CommandList()->SetGraphicsRootSignature(_pmdRenderer->GetRootSignature());

		_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_dx12->SetScene();

		_pmdActor->Update();
		_pmdActor->Draw();

		_dx12->EndDraw();

		// フリップ
		_dx12->Swapchain()->Present(1, 0);
	}
}

bool Application::Init()
{
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	CreateGameWindow(_hwnd, _windowClass);

	// DirectX12ラッパー生成＆初期化
	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));
	_pmdActor.reset(new PMDActor("Model/初音ミク.pmd", *_pmdRenderer));

	return true;
}

void Application::Terminate()
{
	// もうクラスは使わないので登録解除する
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

Application& Application::Instance()
{
	static Application instance;
	return instance;
}

Application::Application()
{
}

Application::~Application()
{
}

#else

#include "Application.h"

#pragma comment(lib, "DirectXTex.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
using namespace DirectX;
using namespace Microsoft::WRL;

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// ウィンドウが破棄されたら呼ばれる
	if (msg == WM_DESTROY)
	{
		// OSに対して「もうこのアプリは終わる」と伝える
		PostQuitMessage(0);
		return 0;
	}
	// 規定の処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

/// <summary>
/// モデルのパスとテクスチャのパスから合成パスを得る
/// </summary>
/// <param name="modelPath">アプリケーションから見たpmdモデルのパス</param>
/// <param name="texPath">PMDモデルから見たテクスチャのパス</param>
/// <returns>アプリケーションから見たテクスチャのパス</returns>
string GetTexturePathFromModelAndTexPath(const string& modelPath, const char* texPath)
{
	auto folderPath = modelPath.substr(0, modelPath.rfind('/') + 1);
	return folderPath + texPath;
}

/// <summary>
/// ファイル名から拡張子を取得する
/// </summary>
/// <param name="path">対象のパス文字列</param>
/// <returns>拡張子</returns>
string GetExtension(const string& path)
{
	auto idx = path.rfind('.');
	return path.substr(idx + 1, path.length() - idx - 1);
}

/// <summary>
/// テクスチャのパスをセパレータ文字で分離する
/// </summary>
/// <param name="path">対象のパス文字列</param>
/// <param name="splitter">区切り文字</param>
/// <returns>分離後の文字列ペア</returns>
pair<string, string> SplitFileName(const string& path, const char splitter = '*')
{
	auto idx = path.find(splitter);
	pair<string, string> ret;
	ret.first = path.substr(0, idx);
	ret.second = path.substr(idx + 1, path.length() - idx - 1);
	return ret;
}

wstring GetWideStringFromString(const string& str)
{
	// 呼び出し1回目（文字列数を得る）
	auto num1 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);

	wstring wstr;						// stringのwchar_t版
	wstr.resize(num1);					// 得られた文字列数でリサイズ

	// 呼び出し2回目（確保済みのwstrに変換文字列をコピー
	auto num2 = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, str.c_str(), -1, &wstr[0], num1);

	assert(num1 == num2);				// 一応チェック
	return wstr;
}

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));

	debugLayer->EnableDebugLayer();		// デバッグレイヤを有効化
	debugLayer->Release();				// 有効化したらインターフェースを解放
}

ID3D12Resource* Application::CreateDummyTextureBuff(UINT64 width, UINT height)
{
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
	ID3D12Resource* texBuff = nullptr;
	auto result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);
	if (FAILED(result)) {
		return nullptr;
	}

	return texBuff;
}

ID3D12Resource* Application::CreateWhiteTexture()
{
	auto texBuff = CreateDummyTextureBuff(4, 4);
	vector<UCHAR> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0xff);		// 全部 0xFF で埋める

	// データ転送
	texBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, static_cast<UINT>(data.size()));

	return texBuff;
}


ID3D12Resource* Application::CreateBlackTexture()
{
	auto texBuff = CreateDummyTextureBuff(4, 4);
	vector<UCHAR> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0x00);		// 全部 0x00 で埋める

	// データ転送
	texBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, static_cast<UINT>(data.size()));

	return texBuff;
}


ID3D12Resource* Application::CreateGrayGradiationTexture()
{
	auto texBuff = CreateDummyTextureBuff(4, 256);	// 高さは256

	// 上が白くて下が黒いテクスチャデータを作成
	vector<UINT> data(4 * 256);
	auto it = data.begin();
	UINT c = 0xff;
	for (; it != data.end(); it += 4) {
		auto col = (0xff << 24) | (c << 16) | (c << 8) | c;
		fill(it, it + 4, col);
		c--;
	}

	texBuff->WriteToSubresource(0, nullptr, data.data(), 4 * sizeof(UINT), static_cast<UINT>(sizeof(UINT) * data.size()));

	return texBuff;
}

HRESULT Application::LoadTextureFile(string& texPath, TexMetadata* meta, ScratchImage& img)
{
	auto ext = GetExtension(texPath);
	auto path = GetWideStringFromString(texPath);

	if (ext == "sph" || ext == "spa" || ext == "bmp" || ext == "png" || ext == "jpg") {
		return LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	}
	else if (ext == "tga") {
		return LoadFromTGAFile(path.c_str(), TGA_FLAGS_NONE, meta, img);
	}
	else if (ext == "dds") {
		return LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, meta, img);
	}
	else {
		return static_cast<HRESULT>(-1);
	}
}

ID3D12Resource* Application::LoadTextureFromFile(string& texPath)
{
	auto it = _resourceTable.find(texPath);
	if (it != _resourceTable.end()) {
		return _resourceTable[texPath];
	}

	// WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	auto result = LoadTextureFile(texPath, &metadata, scratchImg);
	if (FAILED(result)) {
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0);	// 生データ抽出

	// WriteToSubresourceで転送する用のヒープ設定
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(metadata.format, metadata.width,
		static_cast<UINT>(metadata.height), static_cast<UINT16>(metadata.arraySize), static_cast<UINT16>(metadata.mipLevels));

	// バッファー作成
	ID3D12Resource* texBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,						// 特に指定なし
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);
	if (FAILED(result)) {
		return nullptr;
	}

	result = texBuff->WriteToSubresource(
		0,
		nullptr,							// 全領域へコピー
		img->pixels,						// 元データアドレス
		static_cast<UINT>(img->rowPitch),	// 1ラインサイズ
		static_cast<UINT>(img->slicePitch)	// 全サイズ
	);

	if (FAILED(result)) {
		return nullptr;
	}

	_resourceTable[texPath] = texBuff;
	return texBuff;
}

Application& Application::Instance()
{
	static Application instance;
	return instance;
}

bool Application::Init()
{
	DebugOutputFormatString("Show window test.");

	_windowClass.cbSize = sizeof(WNDCLASSEX);
	// コールバック関数の指定
	_windowClass.lpfnWndProc = (WNDPROC)WindowProcedure;
	// アプリケーションクラス名（適当でよい）
	_windowClass.lpszClassName = _T("DX12Sample");
	// ハンドルの取得
	_windowClass.hInstance = GetModuleHandle(nullptr);

	// アプリケーションクラス（ウィンドウクラスの指定をOSに伝える）
	RegisterClassEx(&_windowClass);

	// ウィンドウサイズを決める
	RECT wrc = { 0, 0, window_width, window_height };
	// 関数を使ってウィンドウのサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(
		_windowClass.lpszClassName,		// クラス名指定
		_T("DX12テスト"),		// タイトルバーの文字
		WS_OVERLAPPEDWINDOW,	// タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT,			// 表示x座標はOSにお任せ
		CW_USEDEFAULT,			// 表示y座標はOSにお任せ
		wrc.right - wrc.left,	// ウィンドウ幅
		wrc.bottom - wrc.top,	// ウィンドウ高
		nullptr,				// 親ウィンドウハンドル
		nullptr,				// メニューハンドル
		_windowClass.hInstance,			// 呼び出しアプリケーションハンドル
		nullptr);				// 追加パラメーター

#ifdef _DEBUG
	// デバッグレイヤーをONに
	// デバイス生成時前にやっておかないと、デバイス生成後にやるとデバイスがロストしてしまう
	EnableDebugLayer();
#endif // _DEBUG

#ifdef _DEBUG
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf())))) {
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf())))) {
			return false;
		}
	}
#else
	if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&_dxgiFactory)))) {
		return false;
	}
#endif // _DEBUG

	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(tmpAdapter);
	}
	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		// アダプターの説明オブジェクト取得
		adpt->GetDesc(&adesc);

		std::wstring strDesc = adesc.Description;

		// 探したいアダプターの名前を確認
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel;
	for (auto lvl : levels) {
		if (D3D12CreateDevice(tmpAdapter, lvl, IID_PPV_ARGS(_dev.ReleaseAndGetAddressOf())) == S_OK) {
			featureLevel = lvl;
			break;
		}
	}

	HRESULT result = S_OK;
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf()));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));

	// コマンドキューの作成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;				// タイムアウトなし
	cmdQueueDesc.NodeMask = 0;										// アダプターを1つしか使わないときは0で良い
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;	// プライオリティは特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;				// コマンドリストと合わせる
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf()));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = window_width;
	swapChainDesc.Height = window_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;				// ピクセルフォーマット
	swapChainDesc.Stereo = false;									// ステレオ表示フラグ（3Dディスプレイのステレオモード）
	swapChainDesc.SampleDesc.Count = 1;								// マルチサンプルの指定（Count = 1, Quality = 0でよい）
	swapChainDesc.SampleDesc.Quality = 0;							// 
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;				// 
	swapChainDesc.BufferCount = 2;									// ダブルバッファーなので2
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;					// バックバッファーは伸び縮み可能
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;		// フリップ後は速やかに破棄
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;			// 特に指定なし
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	// ウィンドウ <=> フルスクリーン切り替え可能

	result = _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue.Get(),
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)_swapChain.ReleaseAndGetAddressOf());

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		// レンダーターゲットビュー
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;						// 表裏の2つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	// シェーダ側から参照する必要が無い
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf()));

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapChain->GetDesc(&swcDesc);
	_backBuffers.resize(swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;	// ガンマ補正あり（sRGB）
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	for (UINT idx = 0; idx < swcDesc.BufferCount; ++idx) {
		// スワップチェーン上のバックバッファを取得
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));

		// レンダーターゲットビューを生成
		_dev->CreateRenderTargetView(
			_backBuffers[idx],
			&rtvDesc,
			handle);

		// ポインタをずらす
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// 深度バッファ作成
	// 深度バッファの仕様
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	// 2次元のテクスチャデータ
	depthResDesc.Width = window_width;
	depthResDesc.Height = window_height;
	depthResDesc.DepthOrArraySize = 1;								// テクスチャ配列でも、3Dテクスチャでもない
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;					// 深度値書き込み用フォーマット
	depthResDesc.SampleDesc.Count = 1;								// サンプルは1ピクセルあたり1つ
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	// デプスステンシルとして使用

	// 深度値用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// このクリアバリューが重要な意味を持つ
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;						// 深さ1（最大値）でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;					// 32bit深度値としてクリア

	ID3D12Resource* depthBuffer = nullptr;
	result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,							// デプス書き込み用に使用
		&depthClearValue,
		IID_PPV_ARGS(&depthBuffer));

	// 深度のためのディスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};					// 深度に使うよという事がわかればいい
	dsvHeapDesc.NumDescriptors = 1;									// 深度ビュー1つのみ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;				// デプスステンシルビューとして使う
	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));

	// 深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;							// デプス値に32bit使用
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;			// 2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;							// フラグは特になし
	_dev->CreateDepthStencilView(depthBuffer, &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// フェンス
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf()));

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	// PMDヘッダ構造体
	struct PMDHeader {
		float version;			// 例：00 00 80 3F == 1.00
		char model_name[20];	// モデル名
		char comment[256];		// モデルコメント
	};
	char signature[3];
	PMDHeader pmdHeader = {};
	FILE* fp;
	string strModelPath = "Model/初音ミク.pmd";
	//string strModelPath = "Model/初音ミクmetal.pmd";
	//string strModelPath = "Model/巡音ルカ.pmd";
	auto err = fopen_s(&fp, strModelPath.c_str(), "rb");
	if (fp == nullptr) {
		char strerr[256];
		strerror_s(strerr, 256, err);
		wchar_t strerr2[256];
		size_t strCount;
		mbstowcs_s(&strCount, strerr2, strerr, sizeof(strerr));
		MessageBox(hwnd, strerr2, L"Open Error", MB_ICONERROR);
		return false;
	}
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdHeader, sizeof(pmdHeader), 1, fp);

	unsigned int vertNum;					// 頂点数
	fread(&vertNum, sizeof(vertNum), 1, fp);

	constexpr size_t pmdvertex_size = 38;						// 頂点1つあたりのサイズ
	vector<unsigned char> vertices(vertNum * pmdvertex_size);	// バッファの確保
	fread(vertices.data(), vertices.size(), 1, fp);				// 読み込み

	unsigned int indicesNum;				// インデックス数
	fread(&indicesNum, sizeof(indicesNum), 1, fp);
	vector<unsigned short> indices(indicesNum);					// インデックス用バッファ
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

#pragma pack(1)	// ここから1バイトパッキング（アライメントは発生しない）
	// PMDマテリアル構造体
	struct PMDMaterial {
		XMFLOAT3 diffuse;					// ディフューズ食
		float alpha;						// ディフューズα
		float specularity;					// スペキュラの強さ（乗算値）
		XMFLOAT3 specular;					// スペキュラ色
		XMFLOAT3 ambient;					// アンビエント色
		unsigned char toonIdx;				// トゥーン番号
		unsigned char edgeFlg;				// マテリアル毎の輪郭線フラグ
		// 1バイトパッキングにしないとここで2バイトのパディングが発生する
		unsigned int indicesNum;			// このマテリアルが割当たるインデックス数
		char texFilePath[20];				// テクスチャファイル名
	};	// 70バイトになる（パディングなしの場合）
#pragma pack()	// 1バイトパッキング解除


	unsigned int materialNum;				// マテリアル数
	fread(&materialNum, sizeof(materialNum), 1, fp);

	vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

	_materials.resize(pmdMaterials.size());

	// コピー
	for (UINT i = 0; i < pmdMaterials.size(); i++) {
		_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		_materials[i].material.alpha = pmdMaterials[i].alpha;
		_materials[i].material.specular = pmdMaterials[i].specular;
		_materials[i].material.specularity = pmdMaterials[i].specularity;
		_materials[i].material.ambient = pmdMaterials[i].ambient;
	}

	vector<ID3D12Resource*> textureResources(materialNum);
	vector<ID3D12Resource*> sphResources(materialNum);
	vector<ID3D12Resource*> spaResources(materialNum);
	vector<ID3D12Resource*> toonResources(materialNum);
	for (UINT i = 0; i < pmdMaterials.size(); i++) {
		// トゥーンリソースの読み込み
		string toonFilePath = "toon/";
		char toonFileName[16];
		sprintf_s(toonFileName, 16, "toon%02d.bmp", pmdMaterials[i].toonIdx + 1);
		toonFilePath += toonFileName;
		toonResources[i] = LoadTextureFromFile(toonFilePath);

		textureResources[i] = nullptr;
		sphResources[i] = nullptr;
		spaResources[i] = nullptr;
		if (strlen(pmdMaterials[i].texFilePath) == 0) {
			continue;
		}

		string texFileName = pmdMaterials[i].texFilePath;
		string sphFileName = "";
		string spaFileName = "";

		if (count(texFileName.begin(), texFileName.end(), '*') > 0) {
			// スプリッタがある
			auto namepair = SplitFileName(texFileName);
			auto firstExt = GetExtension(namepair.first);
			if (firstExt == "sph") {
				texFileName = namepair.second;
				sphFileName = namepair.first;
			}
			else if (firstExt == "spa") {
				texFileName = namepair.second;
				spaFileName = namepair.first;
			}
			else {
				texFileName = namepair.first;
				auto secondExt = GetExtension(namepair.second);
				if (secondExt == "sph") {
					sphFileName = namepair.second;
				}
				else if (secondExt == "spa") {
					spaFileName = namepair.second;
				}
			}
		}
		else {
			auto ext = GetExtension(texFileName);
			if (ext == "sph") {
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else if (ext == "spa") {
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
		}

		if (texFileName != "") {
			auto texFilePath = GetTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
			textureResources[i] = LoadTextureFromFile(texFilePath);
		}
		if (sphFileName != "") {
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			sphResources[i] = LoadTextureFromFile(sphFilePath);
		}
		if (spaFileName != "") {
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			spaResources[i] = LoadTextureFromFile(spaFilePath);
		}
	}

	fclose(fp);

	// ヒープ設定
	// CPUからアクセスする（マップで設定する）のでUPLOAD
	CD3DX12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD, 0, 0);
	// リソース設定
	CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size());
	// 頂点バッファ作成
	ID3D12Resource* vertBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	// 頂点情報のコピー（マップ）
	unsigned char* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	_vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();	// バッファの仮想アドレス
	_vbView.SizeInBytes = static_cast<UINT>(vertices.size());	// 全バイト数
	_vbView.StrideInBytes = pmdvertex_size;						// 1頂点あたりのバイト数

	// インデックス情報
	ID3D12Resource* idxBuff = nullptr;
	// 設定は、バッファのサイズ以外頂点バッファの設定を使いまわしてOKっぽい
	resDesc.Width = indices.size() * sizeof(indices[0]);
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);
	// 作ったバッファにインデックスデータをコピー
	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);
	// インデックスバッファビューを作成
	_ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = static_cast<UINT>(indices.size() * sizeof(indices[0]));

	// マテリアルバッファーを作成
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xFF;
	ID3D12Resource* materialBuff = nullptr;
	heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * materialNum);
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&materialBuff)
	);
	// マップマテリアルにコピー（実データとアライメントサイズの違いがあるため強引にキャストしてコピーしている）
	char* mapMaterial = nullptr;
	result = materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	for (auto& m : _materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material;			// データコピー
		mapMaterial += materialBuffSize;
	}
	materialBuff->Unmap(0, nullptr);
	// ディスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = materialNum * 5;		// マテリアル数 x5（定数、基本テクスチャ、sph、spa、toon）
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(_materialDescHeap.ReleaseAndGetAddressOf()));
	// マテリアルビューの作成
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress();				// バッファーアドレス
	matCBVDesc.SizeInBytes = static_cast<UINT>(materialBuffSize);					// マテリアルの256アライメントサイズ
	// 通常テクスチャビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;									// デフォルト
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;		// 後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;							// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;												// ミップマップは使用しないので1
	auto matDescHeapH = _materialDescHeap->GetCPUDescriptorHandleForHeapStart();	// 先頭を記録
	auto inc = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	auto whiteTex = CreateWhiteTexture();
	auto blackTex = CreateBlackTexture();
	auto grayTex = CreateGrayGradiationTexture();
	for (UINT i = 0; i < materialNum; i++) {
		// マテリアル用定数バッファービュー
		_dev->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += inc;
		matCBVDesc.BufferLocation += materialBuffSize;

		// シェーダーリソースビュー
		// テクスチャが空の場合は白テクスチャを使う
		auto texResource = textureResources[i] == nullptr ? whiteTex : textureResources[i];
		srvDesc.Format = texResource->GetDesc().Format;
		_dev->CreateShaderResourceView(texResource, &srvDesc, matDescHeapH);
		matDescHeapH.ptr += inc;

		// スフィア
		auto sphResource = sphResources[i] == nullptr ? whiteTex : sphResources[i];
		srvDesc.Format = sphResource->GetDesc().Format;
		_dev->CreateShaderResourceView(sphResource, &srvDesc, matDescHeapH);
		matDescHeapH.ptr += inc;

		auto spaResource = spaResources[i] == nullptr ? blackTex : spaResources[i];
		srvDesc.Format = spaResource->GetDesc().Format;
		_dev->CreateShaderResourceView(spaResource, &srvDesc, matDescHeapH);
		matDescHeapH.ptr += inc;

		auto toonResource = toonResources[i] == nullptr ? grayTex : toonResources[i];
		srvDesc.Format = toonResource->GetDesc().Format;
		_dev->CreateShaderResourceView(toonResource, &srvDesc, matDescHeapH);
		matDescHeapH.ptr += inc;
	}

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	result = D3DCompileFromFile(L"BasicVertexShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &vsBlob, &errorBlob);
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			OutputDebugStringA("BasicVertexShader.hlslファイルが見当たりません");
		}
		else {
			std::string errStr;
			errStr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errStr.begin());
			errStr += "\n";
			OutputDebugStringA(errStr.c_str());
		}
		return false;
	}
	result = D3DCompileFromFile(L"BasicPixelShader.hlsl",
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &psBlob, &errorBlob);
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			OutputDebugStringA("BasicPixelShader.hlslファイルが見当たりません");
		}
		else {
			std::string errStr;
			errStr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errStr.begin());
			errStr += "\n";
			OutputDebugStringA(errStr.c_str());
		}
		return false;
	}

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONE_NO",  0, DXGI_FORMAT_R16G16_UINT,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHT",   0, DXGI_FORMAT_R8_UINT,         0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "EDGE_FLG", 0, DXGI_FORMAT_R8_UINT,         0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	// グラフィックスパイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr;								// あとで設定する
	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob);
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob);
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;				// 0xffffffff

	// レンダーターゲットブレンド設定（ひとまず加算や乗算、αブレンディングは使用しない）
	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;		// カリングしない

	gpipeline.DepthStencilState.DepthEnable = true;					// 深度バッファーを使う
	gpipeline.DepthStencilState.StencilEnable = false;				// あとで
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;			// 小さい方を採用
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;	// 書き込む

	gpipeline.InputLayout.pInputElementDescs = inputLayout;			// レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout);		// レイアウト配列数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;	// ストリップ時のカットなし
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;	// 三角形で構成

	gpipeline.NumRenderTargets = 1;									// 今は1つのみ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;			// 0～1に正規化されたRGBA

	gpipeline.SampleDesc.Count = 1;									// サンプリングは1ピクセルにつき1
	gpipeline.SampleDesc.Quality = 0;								// クオリティは最低

	// ルートシグネチャの作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	CD3DX12_DESCRIPTOR_RANGE descTblRange[3] = {};				// 定数2つとテクスチャ
	descTblRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);	// 定数[b0]（座標変換用）
	descTblRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);	// 定数[b1]（マテリアル用）
	descTblRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);	// テクスチャ4つ

	// ルートパラメーター
	CD3DX12_ROOT_PARAMETER rootParam[2] = {};
	rootParam[0].InitAsDescriptorTable(1, &descTblRange[0]);	// 座標変換
	rootParam[1].InitAsDescriptorTable(2, &descTblRange[1]);	// マテリアル周り

	rootSignatureDesc.pParameters = rootParam;						// ルートパラメーターの先頭アドレス
	rootSignatureDesc.NumParameters = 2;							// ルートパラメーター数

	CD3DX12_STATIC_SAMPLER_DESC samplerDesc[2] = {};
	samplerDesc[0].Init(0);
	samplerDesc[1].Init(1, D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	rootSignatureDesc.pStaticSamplers = samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 2;

	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	result = _dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf()));
	rootSigBlob->Release();

	gpipeline.pRootSignature = _rootSignature.Get();
	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_pipeline.ReleaseAndGetAddressOf()));

	// ビューポート
	_viewport.Width = window_width;		// 出力先の幅（ピクセル数）
	_viewport.Height = window_height;	// 出力先の高さ（ピクセル数）
	_viewport.TopLeftX = 0;				// 出力先の左上座標X
	_viewport.TopLeftY = 0;				// 出力先の左上座標Y
	_viewport.MaxDepth = 1.0f;			// 深度最大値
	_viewport.MinDepth = 0.0f;			// 深度最小値

	// シザー矩形
	_scissorRect.top = 0;									// 切り抜き上座標
	_scissorRect.left = 0;									// 切り抜き左座標
	_scissorRect.right = _scissorRect.left + window_width;	// 切り抜き右座標
	_scissorRect.bottom = _scissorRect.top + window_height;	// 切り抜き下座標

	// 定数バッファ作成
	_worldMat = XMMatrixRotationY(0);
	//XMFLOAT3 eye(0, 10, -15);
	XMFLOAT3 eye(0, 15, -7.5f);
	//XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 target(0, 15, 0);
	XMFLOAT3 up(0, 1, 0);
	_viewMat = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	_projMat = XMMatrixPerspectiveFovLH(
		XM_PIDIV2,								// 画角90°
		static_cast<float>(window_width) / static_cast<float>(window_height),	// アスペクト比
		1.0f,									// 近い方
		100.0f									// 遠い方
	);
	ID3D12Resource* constBuff = nullptr;
	CD3DX12_HEAP_PROPERTIES constHeapProp(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC constResDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneMatrix) + 0xff) & ~0xFF);
	result = _dev->CreateCommittedResource(
		&constHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&constResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	result = constBuff->Map(0, nullptr, (void**)&_mapMatrix);			// マップ
	// 行列の内容をコピー
	_mapMatrix->world = _worldMat;
	_mapMatrix->view = _viewMat;
	_mapMatrix->proj = _projMat;
	_mapMatrix->eye = eye;

	// ディスクリプタヒープを作る
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;		// シェーダから見えるように
	descHeapDesc.NodeMask = 0;											// マスクは0
	descHeapDesc.NumDescriptors = 2;									// SRVとCBVの2つ
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;			// シェーダリソースビュー（および定数、UAVも）
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_basicDescHeap.ReleaseAndGetAddressOf()));		// 生成

	// ディスクリプタの先頭ハンドルを取得しておく
	auto basicHeapHandle = _basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(constBuff->GetDesc().Width);
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);			// 定数バッファビューの作成

	return true;
}

void Application::Run()
{
	float angle = 0.0f;
	MSG msg = {};
	unsigned int frame = 0;
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// アプリケーションが終わるときにmessageがWM_QUITになる
		if (msg.message == WM_QUIT) {
			break;
		}

		// 対象を回転
		angle += 0.005f;
		_worldMat = XMMatrixRotationY(angle);
		_mapMatrix->world = _worldMat;

		// DirectX処理
		// バックバッファのインデックスを取得
		auto bbIdx = _swapChain->GetCurrentBackBufferIndex();

		// リソースバリア設定
		// PRESENT状態からレンダーターゲット状態へ
		CD3DX12_RESOURCE_BARRIER barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
			_backBuffers[bbIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		_cmdList->ResourceBarrier(1, &barrierDesc);

		_cmdList->SetPipelineState(_pipeline.Get());

		// レンダーターゲットを指定
		auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		auto dsvH = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
		_cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);

		// 画面クリア命令
		frame++;
		float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		_cmdList->RSSetViewports(1, &_viewport);
		_cmdList->RSSetScissorRects(1, &_scissorRect);

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);		// 三角形の集合として描画
		//_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);		// 点の集合として描画
		_cmdList->IASetVertexBuffers(0, 1, &_vbView);
		_cmdList->IASetIndexBuffer(&_ibView);

		_cmdList->SetGraphicsRootSignature(_rootSignature.Get());

		// WVP変換行列
		ID3D12DescriptorHeap* bdh[] = { _basicDescHeap.Get() };
		_cmdList->SetDescriptorHeaps(1, bdh);
		_cmdList->SetGraphicsRootDescriptorTable(0, _basicDescHeap->GetGPUDescriptorHandleForHeapStart());

		// マテリアル
		ID3D12DescriptorHeap* mdh[] = { _materialDescHeap.Get() };
		_cmdList->SetDescriptorHeaps(1, mdh);
		auto materialH = _materialDescHeap->GetGPUDescriptorHandleForHeapStart();	// ヒープ先頭
		auto cbvSrvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
		UINT idxOffset = 0;															// 最初はオフセットなし
		for (auto& m : _materials) {
			_cmdList->SetGraphicsRootDescriptorTable(1, materialH);
			_cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
			materialH.ptr += cbvSrvIncSize;		// 次ビューのために進める
			idxOffset += m.indicesNum;
		}

		// 再び、リソースバリアによってレンダーターゲット→PRESENT状態に移行する
		barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
			_backBuffers[bbIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		_cmdList->ResourceBarrier(1, &barrierDesc);

		// 命令のクローズ
		_cmdList->Close();

		// コマンドリストの実行
		ID3D12CommandList* cmdLists[] = { _cmdList.Get() };
		_cmdQueue->ExecuteCommandLists(1, cmdLists);

		// フェンスを使ってGPUの処理完了を待つ
		_cmdQueue->Signal(_fence.Get(), ++_fenceVal);
		if (_fence->GetCompletedValue() != _fenceVal) {
			auto ev = CreateEvent(nullptr, false, false, nullptr);
			_fence->SetEventOnCompletion(_fenceVal, ev);
			WaitForSingleObject(ev, INFINITE);
			CloseHandle(ev);
		}

		// キューをクリア
		_cmdAllocator->Reset();
		// 再びコマンドリストをためる準備
		_cmdList->Reset(_cmdAllocator.Get(), nullptr);

		// フリップ
		_swapChain->Present(1, 0);
	}

	return;
}

void Application::Terminate()
{
	// もうクラスは使わないので登録解除する
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

SIZE Application::GetWindowSize() const
{
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

Application::Application()
{
}

Application::~Application()
{
}

#endif
