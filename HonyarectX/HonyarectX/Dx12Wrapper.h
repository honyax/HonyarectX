#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <DirectXTex.h>
#include <wrl.h>
#include <string>
#include <functional>

class Dx12Wrapper
{
private:
	SIZE _winSize;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// DXGIまわり
	/// <summary>DXGIインターフェイス</summary>
	ComPtr<IDXGIFactory4> _dxgiFactory = nullptr;
	/// <summary>スワップチェイン</summary>
	ComPtr<IDXGISwapChain4> _swapchain = nullptr;

	// DirectX12まわり
	/// <summary>デバイス</summary>
	ComPtr<ID3D12Device> _dev = nullptr;
	/// <summary>コマンドアロケータ</summary>
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	/// <summary>コマンドリスト</summary>
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	/// <summary>コマンドキュー</summary>
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;

	// 表示に関わるバッファ周り
	/// <summary>深度バッファ</summary>
	ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	/// <summary>バックバッファ（2つ以上…スワップチェインが確保）</summary>
	std::vector<ID3D12Resource*> _backBuffers;
	/// <summary>レンダーターゲット用デスクリプタヒープ</summary>
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	/// <summary>深度バッファビュー用デスクリプタヒープ</summary>
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	/// <summary>ビューポート</summary>
	std::unique_ptr<D3D12_VIEWPORT> _viewport;
	/// <summary>シザー矩形</summary>
	std::unique_ptr<D3D12_RECT> _scissorrect;

	/// <summary>シーンを構成するバッファ周り</summary>
	ComPtr<ID3D12Resource> _sceneConstBuff = nullptr;

	struct SceneData {
		DirectX::XMMATRIX view;	// ビュー行列
		DirectX::XMMATRIX proj;	// プロジェクション行列
		DirectX::XMFLOAT3 eye;	// 視点座標
	};
	SceneData* _mappedSceneData;
	ComPtr<ID3D12DescriptorHeap> _sceneDescHeap = nullptr;

	/// <summary>フェンス</summary>
	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	/// <summary>最終的なレンダーターゲットの生成</summary>
	HRESULT CreateFinalRenderTargets();
	/// <summary>デプスステンシルビューの生成</summary>
	HRESULT CreateDepthStencilView();

	/// <summary>スワップチェインの生成</summary>
	HRESULT CreateSwapChain(const HWND& hwnd);

	/// <summary>DXGIまわり初期化</summary>
	HRESULT InitializeDXGIDevice();

	/// <summary>コマンドまわり初期化</summary>
	HRESULT InitializeCommand();

	/// <summary>ビュープロジェクション用ビューの生成</summary>
	HRESULT CreateSceneView();

	/// <summary>ロード用テーブル</summary>
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> _loadLambdaTable;
	/// <summary>テクスチャテーブル</summary>
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> _textureTable;
	/// <summary>テクスチャローダテーブルの作成</summary>
	void CreateTextureLoaderTable();
	/// <summary></summary>
	ID3D12Resource* CreateTextureFromFile(const char* texpath);

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void Update();
	void BeginDraw();
	void EndDraw();
	/// <summary>テクスチャパスから必要なテクスチャバッファへのポインタを返す</summary>
	/// <param name="texpath">テクスチャファイルパス</param>
	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);

	/// <summary>デバイス</summary>
	ComPtr<ID3D12Device> Device();
	/// <summary>コマンドリスト</summary>
	ComPtr<ID3D12GraphicsCommandList> CommandList();
	/// <summary>スワップチェイン</summary>
	ComPtr<IDXGISwapChain4> Swapchain();

	void SetScene();
};
