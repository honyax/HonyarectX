#pragma once

#if 0

#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#include <wrl.h>
#include <memory>

class Dx12Wrapper;
class PMDRenderer;
class PMDActor;

/// <summary>
/// シングルトン
/// </summary>
class Application
{
private:
	/// <summary>ウィンドウ周り</summary>
	WNDCLASSEX _windowClass;
	HWND _hwnd;
	std::shared_ptr<Dx12Wrapper> _dx12;
	std::shared_ptr<PMDRenderer> _pmdRenderer;
	std::shared_ptr<PMDActor> _pmdActor;
	
	/// <summary>ゲーム用ウィンドウの生成</summary>
	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);

	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

public:
	/// <summary>Applicationのシングルトンインスタンスを得る</summary>
	static Application& Instance();

	/// <summary>初期化</summary>
	bool Init();

	/// <summary>ループ起動</summary>
	void Run();

	/// <summary>後処理</summary>
	void Terminate();
	SIZE GetWindowSize() const;
	~Application();

};

#else

#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>

#ifdef _DEBUG
#include <iostream>
#endif

class Application
{
private:
	WNDCLASSEX _windowClass = {};

	Microsoft::WRL::ComPtr<ID3D12Device> _dev = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> _swapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;

	DirectX::XMMATRIX _worldMat;
	DirectX::XMMATRIX _viewMat;
	DirectX::XMMATRIX _projMat;

	// シェーダー側に投げられるマテリアルデータ
	struct MaterialForHlsl {
		DirectX::XMFLOAT3 diffuse;			// ディフューズ色
		float alpha;						// ディフューズα
		DirectX::XMFLOAT3 specular;			// スペキュラ色
		float specularity;					// スペキュラの強さ（乗算値）
		DirectX::XMFLOAT3 ambient;			// アンビエント色
	};
	// それ以外のマテリアルデータ
	struct AdditionalMaterial {
		std::string texPath;				// テクスチャファイルパス
		int toonIdx;						// トゥーン番号
		bool edgeFlg;						// マテリアルごとの輪郭線フラグ
	};
	// 全体をまとめるデータ
	struct Material {
		unsigned int indicesNum;			// インデックス数
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};
	std::vector<Material> _materials;

	// シェーダー側に渡すための基本的な行列データ
	struct SceneMatrix {
		DirectX::XMMATRIX world;			// モデル本体を回転させたり移動させたりする行列
		DirectX::XMMATRIX view;				// ビュー行列
		DirectX::XMMATRIX proj;				// プロジェクション行列
		DirectX::XMFLOAT3 eye;				// 視点座標
	};
	SceneMatrix* _mapMatrix;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _basicDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _materialDescHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	std::map<std::string, ID3D12Resource*> _resourceTable;

	// パイプライン、ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipeline = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	// バックバッファ
	std::vector<ID3D12Resource*> _backBuffers;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;

	// ビューポート、シザー矩形
	D3D12_VIEWPORT _viewport = {};
	D3D12_RECT _scissorRect = {};

	/// <summary>ダミーのテクスチャのバッファ（4x4）を作成</summary>
	ID3D12Resource* CreateDummyTextureBuff(UINT64 width, UINT height);
	/// <summary>ダミーの白いテクスチャ（4x4）を作成</summary>
	ID3D12Resource* CreateWhiteTexture();
	/// <summary>ダミーの黒いテクスチャ（4x4）を作成</summary>
	ID3D12Resource* CreateBlackTexture();
	/// <summary>ダミーのトゥーン用グラデーションテクスチャを作成</summary>
	ID3D12Resource* CreateGrayGradiationTexture();

	HRESULT LoadTextureFile(std::string& texPath, DirectX::TexMetadata* meta, DirectX::ScratchImage& img);
	ID3D12Resource* LoadTextureFromFile(std::string& texPath);

	/// <summary>
	/// シングルトンのためにコンストラクタをprivateにし、
	/// さらにコピーと代入を禁止
	/// </summary>
	Application();
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;

public:
	/// <summary>Applicationのシングルトンインスタンスを得る</summary>
	static Application& Instance();

	///初期化
	bool Init();

	///ループ起動
	void Run();

	///後処理
	void Terminate();

	SIZE GetWindowSize() const;

	~Application();

};
#endif
