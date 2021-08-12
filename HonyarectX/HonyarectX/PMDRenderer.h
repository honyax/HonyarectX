#pragma once

#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include <memory>

class Dx12Wrapper;
class PMDActor;

class PMDRenderer
{
	friend PMDActor;

private:
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// <summary>PMD用パイプライン</summary>
	ComPtr<ID3D12PipelineState> _pipeline = nullptr;
	/// <summary>PMD用ルートシグネチャ</summary>
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

	/// <summary>
	/// PMD用共通テクスチャ（白、黒、グレイスケールグラデーション）
	/// </summary>
	ComPtr<ID3D12Resource> _whiteTex = nullptr;
	ComPtr<ID3D12Resource> _blackTex = nullptr;
	ComPtr<ID3D12Resource> _gradTex = nullptr;

	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	/// <summary>白テクスチャの生成</summary>
	ID3D12Resource* CreateWhiteTexture();
	/// <summary>黒テクスチャの生成</summary>
	ID3D12Resource* CreateBlackTexture();
	/// <summary>グレーテクスチャの生成</summary>
	ID3D12Resource* CreateGrayGradationTexture();

	/// <summary>パイプライン初期化</summary>
	HRESULT CreateGraphicsPipelineForPMD();
	/// <summary>ルートシグネチャ初期化</summary>
	HRESULT CreateRootSignature();
	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);

public:
	PMDRenderer(Dx12Wrapper& dx12);
	~PMDRenderer();
	void Update();
	void Draw();
	ID3D12PipelineState* GetPipelineState();
	ID3D12RootSignature* GetRootSignature();
};
