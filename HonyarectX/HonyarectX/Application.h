#pragma once

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
	Microsoft::WRL::ComPtr<ID3D12Device> _dev = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> _swapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;

	std::map<std::string, ID3D12Resource*> _resourceTable;

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

	~Application();

};
