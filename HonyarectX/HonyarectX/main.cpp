#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>

#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "DirectXTex.lib")
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
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;	// ガンマ補正あり（sRGB）
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	for (UINT idx = 0; idx < swcDesc.BufferCount; ++idx) {
		// スワップチェーン上のバックバッファを取得
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&backBuffers[idx]));

		// レンダーターゲットビューを生成
		_dev->CreateRenderTargetView(
			backBuffers[idx],
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
	ID3D12DescriptorHeap* dsvHeap = nullptr;
	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

	// 深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;							// デプス値に32bit使用
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;			// 2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;							// フラグは特になし
	_dev->CreateDepthStencilView(depthBuffer, &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// フェンス
	ID3D12Fence* fence = nullptr;
	UINT64 fenceVal = 0;
	result = _dev->CreateFence(fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

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
	auto err = fopen_s(&fp, "Model/初音ミク.pmd", "rb");
	if (fp == nullptr) {
		char strerr[256];
		strerror_s(strerr, 256, err);
		wchar_t strerr2[256];
		size_t strCount;
		mbstowcs_s(&strCount, strerr2, strerr, sizeof(strerr));
		MessageBox(hwnd, strerr2, L"Open Error", MB_ICONERROR);
		return -1;
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

	// シェーダー側に投げられるマテリアルデータ
	struct MaterialForHlsl {
		XMFLOAT3 diffuse;					// ディフューズ色
		float alpha;						// ディフューズα
		XMFLOAT3 specular;					// スペキュラ色
		float specularity;					// スペキュラの強さ（乗算値）
		XMFLOAT3 ambient;					// アンビエント色
	};

	// それ以外のマテリアルデータ
	struct AdditionalMaterial {
		string texPath;						// テクスチャファイルパス
		int toonIdx;						// トゥーン番号
		bool edgeFlg;						// マテリアルごとの輪郭線フラグ
	};

	// 全体をまとめるデータ
	struct Material {
		unsigned int indicesNum;			// インデックス数
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	unsigned int materialNum;				// マテリアル数
	fread(&materialNum, sizeof(materialNum), 1, fp);

	vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

	vector<Material> materials(pmdMaterials.size());

	// コピー
	for (UINT i = 0; i < pmdMaterials.size(); i++) {
		materials[i].indicesNum = pmdMaterials[i].indicesNum;
		materials[i].material.diffuse = pmdMaterials[i].diffuse;
		materials[i].material.alpha = pmdMaterials[i].alpha;
		materials[i].material.specular = pmdMaterials[i].specular;
		materials[i].material.specularity = pmdMaterials[i].specularity;
		materials[i].material.ambient = pmdMaterials[i].ambient;
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

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();	// バッファの仮想アドレス
	vbView.SizeInBytes = static_cast<UINT>(vertices.size());	// 全バイト数
	vbView.StrideInBytes = pmdvertex_size;						// 1頂点あたりのバイト数

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
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = static_cast<UINT>(indices.size() * sizeof(indices[0]));

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
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONE_NO",  0, DXGI_FORMAT_R16G16_UINT,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHT",   0, DXGI_FORMAT_R8_UINT,         0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "EDGE_FLG", 0, DXGI_FORMAT_R8_UINT,         0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
	ID3D12RootSignature* rootSignature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descTblRange[1] = {};					// 定数の1つ
	descTblRange[0].NumDescriptors = 1;								// 定数ひとつ
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;	// 種別は定数
	descTblRange[0].BaseShaderRegister = 0;							// 0番スロットから
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.DescriptorTable.pDescriptorRanges = &descTblRange[0];	//デスクリプタレンジのアドレス
	rootParam.DescriptorTable.NumDescriptorRanges = 1;				//デスクリプタレンジ数
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;		//全てのシェーダから見える

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

	// シェーダー側に渡すための基本的な行列データ
	struct MatricesData {
		XMMATRIX world;							// モデル本体を回転させたり移動させたりする行列
		XMMATRIX viewproj;						// ビューとプロジェクション合成行列
	};

	// 定数バッファ作成
	float angle = 0;
	auto worldMat = XMMatrixRotationY(angle);
	XMFLOAT3 eye(0, 10, -15);
	XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 up(0, 1, 0);
	auto viewMat = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	auto projMat = XMMatrixPerspectiveFovLH(
		XM_PIDIV2,								// 画角90°
		static_cast<float>(window_width) / static_cast<float>(window_height),	// アスペクト比
		1.0f,									// 近い方
		100.0f									// 遠い方
	);
	ID3D12Resource* constBuff = nullptr;
	CD3DX12_HEAP_PROPERTIES constHeapProp(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC constResDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(MatricesData) + 0xff) & ~0xFF);
	result = _dev->CreateCommittedResource(
		&constHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&constResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	MatricesData* mapMatrix;											// マップ先を示すポインタ
	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);			// マップ
	// 行列の内容をコピー
	mapMatrix->world = worldMat;
	mapMatrix->viewproj = viewMat * projMat;

	// ディスクリプタヒープを作る
	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;		// シェーダから見えるように
	descHeapDesc.NodeMask = 0;											// マスクは0
	descHeapDesc.NumDescriptors = 2;									// SRVとCBVの2つ
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;			// シェーダリソースビュー（および定数、UAVも）
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));		// 生成

	// ディスクリプタの先頭ハンドルを取得しておく
	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(constBuff->GetDesc().Width);
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);			// 定数バッファビューの作成

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
		worldMat = XMMatrixRotationY(angle);
		mapMatrix->world = worldMat;
		mapMatrix->viewproj = viewMat * projMat;

		// DirectX処理
		// バックバッファのインデックスを取得
		auto bbIdx = _swapChain->GetCurrentBackBufferIndex();

		// リソースバリア設定
		// PRESENT状態からレンダーターゲット状態へ
		CD3DX12_RESOURCE_BARRIER barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffers[bbIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		_cmdList->ResourceBarrier(1, &barrierDesc);

		_cmdList->SetPipelineState(pipelineState);

		// レンダーターゲットを指定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		_cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);

		// 画面クリア命令
		frame++;
		float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorRect);
		_cmdList->SetGraphicsRootSignature(rootSignature);

		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);		// 三角形の集合として描画
		//_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);		// 点の集合として描画
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);

		_cmdList->SetGraphicsRootSignature(rootSignature);
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap);
		_cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());

		//_cmdList->DrawInstanced(vertNum, 1, 0, 0);
		_cmdList->DrawIndexedInstanced(indicesNum, 1, 0, 0, 0);

		// 再び、リソースバリアによってレンダーターゲット→PRESENT状態に移行する
		barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffers[bbIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
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
