#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <d3dcompiler.h>

#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
using namespace DirectX;

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

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapChain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));

	debugLayer->EnableDebugLayer();		// デバッグレイヤを有効化
	debugLayer->Release();				// 有効化したらインターフェースを解放
}

#ifdef _DEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif
{
	DebugOutputFormatString("Show window test.");

	// ウィンドウクラスの生成＆登録
	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	// コールバック関数の指定
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	// アプリケーションクラス名（適当でよい）
	w.lpszClassName = _T("DX12Sample");
	// ハンドルの取得
	w.hInstance = GetModuleHandle(nullptr);

	// アプリケーションクラス（ウィンドウクラスの指定をOSに伝える）
	RegisterClassEx(&w);

	// ウィンドウサイズを決める
	RECT wrc = { 0, 0, window_width, window_height };
	// 関数を使ってウィンドウのサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(
		w.lpszClassName,		// クラス名指定
		_T("DX12テスト"),		// タイトルバーの文字
		WS_OVERLAPPEDWINDOW,	// タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT,			// 表示x座標はOSにお任せ
		CW_USEDEFAULT,			// 表示y座標はOSにお任せ
		wrc.right - wrc.left,	// ウィンドウ幅
		wrc.bottom - wrc.top,	// ウィンドウ高
		nullptr,				// 親ウィンドウハンドル
		nullptr,				// メニューハンドル
		w.hInstance,			// 呼び出しアプリケーションハンドル
		nullptr);				// 追加パラメーター

#ifdef _DEBUG
	// デバッグレイヤーをONに
	// デバイス生成時前にやっておかないと、デバイス生成後にやるとデバイスがロストしてしまう
	EnableDebugLayer();
#endif // _DEBUG

#ifdef _DEBUG
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory)))) {
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&_dxgiFactory)))) {
			return -1;
		}
	}
#else
	if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&_dxgiFactory)))) {
		return -1;
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
		if (D3D12CreateDevice(tmpAdapter, lvl, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = lvl;
			break;
		}
	}

	HRESULT result = S_OK;
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));

	// コマンドキューの作成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;				// タイムアウトなし
	cmdQueueDesc.NodeMask = 0;										// アダプターを1つしか使わないときは0で良い
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;	// プライオリティは特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;				// コマンドリストと合わせる
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));

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

	result = _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapChain);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		// レンダーターゲットビュー
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;						// 表裏の2つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	// シェーダ側から参照する必要が無い
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapChain->GetDesc(&swcDesc);
	std::vector<ID3D12Resource*> backBuffers(swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	for (int idx = 0; idx < swcDesc.BufferCount; ++idx) {
		// スワップチェーン上のバックバッファを取得
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&backBuffers[idx]));

		// レンダーターゲットビューを生成
		_dev->CreateRenderTargetView(
			backBuffers[idx],
			nullptr,
			handle);

		// ポインタをずらす
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// フェンス
	ID3D12Fence* fence = nullptr;
	UINT64 fenceVal = 0;
	result = _dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

	// ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	// 頂点データ構造体
	struct Vertex {
		XMFLOAT3 pos;	// XYZ座標
		XMFLOAT2 uv;	// UV座標
	};

	// 頂点の定義
	Vertex vertices[] = {
		{ {-0.4f, -0.7f, 0.0f}, {0.0f, 1.0f} },	// 左下
		{ {-0.4f,  0.7f, 0.0f}, {0.0f, 0.0f} },	// 左上
		{ { 0.4f, -0.7f, 0.0f}, {1.0f, 1.0f} },	// 右下
		{ { 0.4f,  0.7f, 0.0f}, {1.0f, 0.0f} },	// 右上
	};

	// ヒープ設定
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;		// CPUからアクセスする（マップで設定する）のでUPLOAD
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProp.CreationNodeMask = 0;
	heapProp.VisibleNodeMask = 0;
	// リソース設定
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof(vertices);
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
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
	Vertex* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();	// バッファの仮想アドレス
	vbView.SizeInBytes = sizeof(vertices);						// 全バイト数
	vbView.StrideInBytes = sizeof(vertices[0]);					// 1頂点あたりのバイト数

	// インデックス情報
	unsigned short indices[] = { 0, 1, 2,  2, 1, 3 };
	ID3D12Resource* idxBuff = nullptr;
	// 設定は、バッファのサイズ以外頂点バッファの設定を使いまわしてOKっぽい
	resDesc.Width = sizeof(indices);
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
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);

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
		return -1;
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
		return -1;
	}

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	// グラフィックスパイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr;								// あとで設定する
	gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();		// Vertex Shader
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();		// Pixel Shader
	gpipeline.PS.BytecodeLength = psBlob->GetBufferSize();
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;				// 0xffffffff
	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	// レンダーターゲットブレンド設定（ひとまず加算や乗算、αブレンディングは使用しない）
	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	renderTargetBlendDesc.LogicOpEnable = false;					// ひとまず論理演算は使用しない
	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	gpipeline.RasterizerState.MultisampleEnable = false;			// まだアンチエイリアスは使わない
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;		// カリングしない
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;		// 中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true;				// 深度方向のクリッピングは有効に
	// ラスタライズの残り
	gpipeline.RasterizerState.FrontCounterClockwise = false;
	gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	gpipeline.RasterizerState.AntialiasedLineEnable = false;
	gpipeline.RasterizerState.ForcedSampleCount = 0;
	gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	gpipeline.DepthStencilState.DepthEnable = false;
	gpipeline.DepthStencilState.StencilEnable = false;

	gpipeline.InputLayout.pInputElementDescs = inputLayout;			// レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout);		// レイアウト配列数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;	// ストリップ時のカットなし
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;	// 三角形で構成

	gpipeline.NumRenderTargets = 1;									// 今は1つのみ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;			// 0～1に正規化されたRGBA

	gpipeline.SampleDesc.Count = 1;									// サンプリングは1ピクセルにつき1
	gpipeline.SampleDesc.Quality = 0;								// クオリティは最低

	// ルートシグネチャの作成
	ID3D12RootSignature* rootSignature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descTblRange = {};
	descTblRange.NumDescriptors = 1;								// テクスチャ1つ
	descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;		// 種別はテクスチャ
	descTblRange.BaseShaderRegister = 0;							// 0番スロットから
	descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;		// ピクセルシェーダーから見える
	rootParam.DescriptorTable.pDescriptorRanges = &descTblRange;	// ディスクリプタレンジのアドレス
	rootParam.DescriptorTable.NumDescriptorRanges = 1;				// ディスクリプタレンジ数

	rootSignatureDesc.pParameters = &rootParam;						// ルートパラメーターの先頭アドレス
	rootSignatureDesc.NumParameters = 1;							// ルートパラメーター数

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;			// 繰り返し
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;			// 繰り返し
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;			// 繰り返し
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;	// ボーダーの時は黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;			// 補間しない(ニアレストネイバー)
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;							// ミップマップ最大値
	samplerDesc.MinLOD = 0.0f;										// ミップマップ最小値
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;		// オーバーサンプリングの際リサンプリングしない
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	// ピクセルシェーダからのみ可視

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	result = _dev->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	rootSigBlob->Release();

	gpipeline.pRootSignature = rootSignature;
	ID3D12PipelineState* pipelineState = nullptr;
	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineState));

	// ビューポート
	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;		// 出力先の幅（ピクセル数）
	viewport.Height = window_height;	// 出力先の高さ（ピクセル数）
	viewport.TopLeftX = 0;				// 出力先の左上座標X
	viewport.TopLeftY = 0;				// 出力先の左上座標Y
	viewport.MaxDepth = 1.0f;			// 深度最大値
	viewport.MinDepth = 0.0f;			// 深度最小値

	// シザー矩形
	D3D12_RECT scissorRect = {};
	scissorRect.top = 0;									// 切り抜き上座標
	scissorRect.left = 0;									// 切り抜き左座標
	scissorRect.right = scissorRect.left + window_width;	// 切り抜き右座標
	scissorRect.bottom = scissorRect.top + window_height;	// 切り抜き下座標

	// ノイズテクスチャの作成
	struct TexRGBA
	{
		UINT8 R, G, B, A;
	};
	std::vector<TexRGBA> textureData(256 * 256);
	for (auto& rgba : textureData) {
		rgba.R = rand() % 256;
		rgba.G = rand() % 256;
		rgba.B = rand() % 256;
		rgba.A = 255;
	}

	// WriteToSubresourceで転送するためのヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;							// 特殊な設定なのでdefaultでもuploadでもない
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;	// ライトバックで
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;			// 転送がL0つまりCPU側から直で
	texHeapProp.CreationNodeMask = 0;									// 単一アダプタのため0
	texHeapProp.VisibleNodeMask = 0;									// 単一アダプタのため0

	D3D12_RESOURCE_DESC texResDesc = {};
	texResDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;						// RGBAフォーマット
	texResDesc.Width = 256;												// 幅
	texResDesc.Height = 256;											// 高さ
	texResDesc.DepthOrArraySize = 1;									// 2Dで配列でもないので1
	texResDesc.SampleDesc.Count = 1;									// 通常テクスチャなのでアンチエイリアシングしない
	texResDesc.SampleDesc.Quality = 0;									// クオリティは最低
	texResDesc.MipLevels = 1;											// ミップマップしないのでミップ数は1つ
	texResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;			// 2Dテクスチャ用
	texResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;					// レイアウトは決定しない
	texResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;						// 特にフラグなし

	ID3D12Resource* texBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,											// 特に指定なし
		&texResDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,						// テクスチャ用指定
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);

	result = texBuff->WriteToSubresource(
		0,
		nullptr,								// 全領域へコピー
		textureData.data(),						// 元データアドレス
		sizeof(TexRGBA) * 256,					// 1ラインサイズ
		sizeof(TexRGBA) * textureData.size()	// 全サイズ
	);

	// ディスクリプタヒープを作る
	ID3D12DescriptorHeap* texDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;		// シェーダから見えるように
	descHeapDesc.NodeMask = 0;											// マスクは0
	descHeapDesc.NumDescriptors = 1;									// ビューは今のところ1つだけ
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;			// シェーダリソースビュー（および定数、UAVも）
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&texDescHeap));		// 生成

	// 通常テクスチャビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;						// RGBA（0.0f～1.0fに正規化）
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;				// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;									// ミップマップは使用しないので1
	_dev->CreateShaderResourceView(
		texBuff,														// ビューと関連付けるバッファ
		&srvDesc,														// 先ほど設定したテクスチャ設定情報
		texDescHeap->GetCPUDescriptorHandleForHeapStart()				// ヒープのどこに割り当てるか
	);

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

		// DirectX処理
		// バックバッファのインデックスを取得
		auto bbIdx = _swapChain->GetCurrentBackBufferIndex();

		// リソースバリア設定
		D3D12_RESOURCE_BARRIER barrierDesc = {};
		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;		// バリアの種別（状態遷移なのでTRANSITION）
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;			// 特別なことはしないのでひとまずNONE
		barrierDesc.Transition.pResource = backBuffers[bbIdx];			// リソースのアドレス
		barrierDesc.Transition.Subresource = 0;									// サブリソース番号
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;		// 元の状態（PRESENT状態）
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;	// 後の状態（これからレンダーターゲットとして使うことを指定）
		_cmdList->ResourceBarrier(1, &barrierDesc);

		_cmdList->SetPipelineState(pipelineState);

		// レンダーターゲットを指定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

		// 画面クリア命令
		frame++;
		float r, g, b;
		r = 1.0f;
		g = 1.0f;
		b = (float)(0xff & frame) / 255.0f;
		float clearColor[] = { r, g, b, 1.0f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorRect);
		_cmdList->SetGraphicsRootSignature(rootSignature);

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);

		_cmdList->SetGraphicsRootSignature(rootSignature);
		_cmdList->SetDescriptorHeaps(1, &texDescHeap);
		_cmdList->SetGraphicsRootDescriptorTable(0, texDescHeap->GetGPUDescriptorHandleForHeapStart());

		//_cmdList->DrawInstanced(4, 1, 0, 0);
		_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

		// 再び、リソースバリアによってレンダーターゲット→PRESENT状態に移行する
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		_cmdList->ResourceBarrier(1, &barrierDesc);

		// 命令のクローズ
		_cmdList->Close();

		// コマンドリストの実行
		ID3D12CommandList* cmdLists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdLists);

		// フェンスを使ってGPUの処理完了を待つ
		_cmdQueue->Signal(fence, ++fenceVal);
		if (fence->GetCompletedValue() != fenceVal) {
			auto ev = CreateEvent(nullptr, false, false, nullptr);
			fence->SetEventOnCompletion(fenceVal, ev);
			WaitForSingleObject(ev, INFINITE);
			CloseHandle(ev);
		}

		// キューをクリア
		_cmdAllocator->Reset();
		// 再びコマンドリストをためる準備
		_cmdList->Reset(_cmdAllocator, nullptr);

		// フリップ
		_swapChain->Present(1, 0);
	}

	// もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}
