#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <string>
#include <wrl.h>

class Dx12Wrapper;
class PMDRenderer;

class PMDActor
{
	friend PMDRenderer;

private:
	PMDRenderer& _renderer;
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	/// <summary>頂点関連</summary>
	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	/// <summary>座標変換行列（今はワールドのみ）</summary>
	ComPtr<ID3D12Resource> _transformMat = nullptr;
	/// <summary>座標変換ヒープ</summary>
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;

	/// <summary>
	/// シェーダー側に投げられるマテリアルデータ
	/// </summary>
	struct MaterialForHlsl {
		DirectX::XMFLOAT3 diffuse;			// ディフューズ色
		float alpha;						// ディフューズα
		DirectX::XMFLOAT3 specular;			// スペキュラ色
		float specularity;					// スペキュラの強さ（乗算値）
		DirectX::XMFLOAT3 ambient;			// アンビエント色
	};
	/// <summary>
	/// それ以外のマテリアルデータ
	/// </summary>
	struct AdditionalMaterial {
		std::string texPath;				// テクスチャファイルパス
		int toonIdx;						// トゥーン番号
		bool edgeFlg;						// マテリアルごとの輪郭線フラグ
	};
	/// <summary>
	/// 全体をまとめるデータ
	/// </summary>
	struct Material {
		unsigned int indicesNum;			// インデックス数
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform {
		// 内部に持ってるXMMATRIXメンバが16バイトアライメントであるため
		// Transformをnewする際には16バイト境界に確保する
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	/// <summary>マテリアル関連</summary>
	std::vector<Material> _materials;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	/// <summary>ボーン関連</summary>
	std::vector<DirectX::XMMATRIX> _boneMatrices;

	struct BoneNode {
		int boneIdx;						// ボーンインデックス
		DirectX::XMFLOAT3 startPos;			// ボーン基準点（回転中心）
		std::vector<BoneNode*> children;	// 子ノード
	};
	std::map<std::string, BoneNode> _boneNodeTable;

	/// <summary>
	/// 読み込んだマテリアルをもとにマテリアルバッファを作成
	/// </summary>
	HRESULT CreateMaterialData();

	/// <summary>マテリアルヒープ（5個分）</summary>
	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr;
	/// <summary>マテリアル＆テクスチャのビューを作成</summary>
	HRESULT CreateMaterialAndTextureView();

	/// <summary>座標変換用ビューの作成</summary>
	HRESULT CreateTransformView();

	/// <summary>PMDファイルのロード</summary>
	HRESULT LoadPMDFile(const char* path);

	/// <summary>テスト用Y軸回転</summary>
	float _angle;

public:
	PMDActor(const char* filepath, PMDRenderer& renderer);
	~PMDActor();
	/// <summary>クローンは頂点及びマテリアルは共通のバッファを見るようにする</summary>
	PMDActor* Clone();
	void Update();
	void Draw();
};
