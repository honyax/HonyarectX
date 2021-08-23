#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <wrl.h>

class Dx12Wrapper;
class PMDRenderer;

class PMDActor
{
	friend PMDRenderer;

private:
	UINT _duration = 0;
	PMDRenderer& _renderer;
	Dx12Wrapper& _dx12;
	DirectX::XMMATRIX _localMat;
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
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
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
		uint32_t boneIdx;					// ボーンインデックス
		uint32_t boneType;					// ボーン種別
		uint32_t parentBone;				// 親ボーン
		uint32_t ikParentBone;				// IK親ボーン
		DirectX::XMFLOAT3 startPos;			// ボーン基準点（回転中心）
		std::vector<BoneNode*> children;	// 子ノード
	};
	std::map<std::string, BoneNode> _boneNodeTable;
	std::vector<std::string> _boneNameArray;		// インデックスから名前を変作詞安いようにしておく
	std::vector<BoneNode*> _boneNodeAddressArray;	// インデックスからノードを検索しやすいようにしておく

	struct PMDIK {
		uint16_t boneIdx;					// IK対象のボーンを示す
		uint16_t targetIdx;					// ターゲットに近づけるためのボーンのインデックス
		uint16_t iterations;				// 試行回数
		float limit;						// 一回あたりの回転制限
		std::vector<uint16_t> nodeIdxes;	// 間のノード番号
	};
	std::vector<PMDIK> _ikData;

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

	void RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat, bool flg = false);

	/// <summary>テスト用Y軸回転</summary>
	float _angle;

	struct KeyFrame {
		UINT frameNo;
		DirectX::XMVECTOR quaternion;
		DirectX::XMFLOAT3 offset;
		DirectX::XMFLOAT2 p1;
		DirectX::XMFLOAT2 p2;
		KeyFrame(UINT fno, DirectX::XMVECTOR& q, DirectX::XMFLOAT3& ofst, const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2) :
			frameNo(fno), quaternion(q), offset(ofst), p1(ip1), p2(ip2)
		{
		}
	};
	std::unordered_map<std::string, std::vector<KeyFrame>> _motiondata;

	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n = 12);

	std::vector<uint32_t> _kneeIdxes;

	/// <summary>アニメーション開始時点のミリ秒時刻</summary>
	UINT64 _startTime;

	void MotionUpdate();

	/// <summary>CCD-IKによりボーン方向を解決</summary>
	void SolveCCDIK(const PMDIK& ik);

	/// <summary>余弦定理IKによりボーン方向を解決</summary>
	void SolveCosineIK(const PMDIK& ik);

	/// <summary>LookAt行列によりボーン方向を解決</summary>
	void SolveLookAt(const PMDIK& ik);

	void IKSolve(int frameNo);

	//IKオンオフデータ
	struct VMDIKEnable {
		uint32_t frameNo;
		std::unordered_map<std::string, bool> ikEnableTable;
	};
	std::vector<VMDIKEnable> _ikEnableData;

public:
	PMDActor(const char* filepath, PMDRenderer& renderer);
	~PMDActor();
	/// <summary>クローンは頂点及びマテリアルは共通のバッファを見るようにする</summary>
	PMDActor* Clone();
	void LoadVMDFile(const char* filepath, const char* name);
	void Update();
	void Draw();
	void PlayAnimation();

	void LookAt(float x, float y, float z);
};
