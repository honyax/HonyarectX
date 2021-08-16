#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include <d3dx12.h>
using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

namespace
{
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
	/// モデルのパスとテクスチャのパスから合成パスを得る
	/// </summary>
	/// <param name="modelPath">アプリケーションから見たpmdモデルのパス</param>
	/// <param name="texPath">PMDモデルから見たテクスチャのパス</param>
	/// <returns>アプリケーションから見たテクスチャのパス</returns>
	string GetTexturePathFromModelAndTexPath(const string& modelPath, const char* texPath)
	{
		// ファイルのフォルダ区切りは\と/の二種類が使用される可能性があり
		// ともかく末尾の\か/を得られればいいので、双方のrfindをとり比較する
		// int型に代入しているのは見つからなかった場合はrfindがepos(-1→0xffffffff)を返すため
		int pathIndex1 = static_cast<int>(modelPath.rfind('/'));
		int pathIndex2 = static_cast<int>(modelPath.rfind('\\'));
		auto pathIndex = max(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}
}

void* PMDActor::Transform::operator new(size_t size)
{
	return _aligned_malloc(size, 16);
}

PMDActor::PMDActor(const char* filepath, PMDRenderer& renderer) :
	_renderer(renderer),
	_dx12(renderer._dx12),
	_angle(0.0f)
{
	_transform.world = XMMatrixIdentity();
	LoadPMDFile(filepath);
	CreateTransformView();
	CreateMaterialData();
	CreateMaterialAndTextureView();
}

PMDActor::~PMDActor()
{
}

void PMDActor::LoadVMDFile(const char* filepath, const char* name)
{
	FILE* fp;
	fopen_s(&fp, filepath, "rb");
	if (fp == nullptr) {
		assert(0);
		return;
	}

	fseek(fp, 50, SEEK_SET);	// 最初の50バイトは飛ばしてOK
	UINT keyframeNum = 0;
	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

	struct VMDKeyFrame {
		char boneName[15];		// ボーン名
		UINT frameNo;			// フレーム番号
		XMFLOAT3 location;		// 位置
		XMFLOAT4 quaternion;	// クォータニオン（回転）
		UINT8 bezier[64];		// [4][4][4] ベジェ補間パラメータ
	};
	vector<VMDKeyFrame> keyframes(keyframeNum);
	for (auto& keyframe : keyframes) {
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp);	// ボーン名
		
		fread(&keyframe.frameNo,
			sizeof(keyframe.frameNo)				// フレーム番号
			+ sizeof(keyframe.location)			// 位置（IKの時に使用予定）
			+ sizeof(keyframe.quaternion)			// クォータニオン
			+ sizeof(keyframe.bezier),			// 補間ベジェデータ
			1, fp);
	}

	// VMDのキーフレームデータから、実際に使用するキーフレームテーブルへ変換
	for (auto& f : keyframes) {
		auto q = XMLoadFloat4(&f.quaternion);
		XMFLOAT2 ip1((float)f.bezier[3] / 127.0f, (float)f.bezier[7] / 127.0f);
		XMFLOAT2 ip2((float)f.bezier[11] / 127.0f, (float)f.bezier[15] / 127.0f);
		_motiondata[f.boneName].emplace_back(KeyFrame(f.frameNo, q, ip1, ip2));
	}

	for (auto& bonemotion : _motiondata) {
		auto node = _boneNodeTable[bonemotion.first];
		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *
			XMMatrixRotationQuaternion(bonemotion.second[0].quaternion) *
			XMMatrixTranslation(pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat;
	}
	auto ident = XMMatrixIdentity();
	RecursiveMatrixMultiply(&_boneNodeTable["センター"], ident);
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

HRESULT PMDActor::LoadPMDFile(const char* path)
{
	// PMDヘッダ構造体
	struct PMDHeader {
		float version;			// 例：00 00 80 3F == 1.00
		char model_name[20];	// モデル名
		char comment[256];		// モデルコメント
	};
	char signature[3];
	PMDHeader pmdheader = {};

	string strModelPath = path;

	FILE* fp;
	fopen_s(&fp, strModelPath.c_str(), "rb");
	if (fp == nullptr) {
		// エラー処理
		assert(0);
		return ERROR_FILE_NOT_FOUND;
	}
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum;		// 頂点数
	fread(&vertNum, sizeof(vertNum), 1, fp);

#pragma pack(1)	// ここから1バイトパッキング（アライメントは発生しない）
	// PMDマテリアル構造体
	struct PMDMaterial {
		XMFLOAT3 diffuse;					// ディフューズ色
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

	constexpr size_t pmdvertex_size = 38;						// 頂点1つあたりのサイズ
	vector<unsigned char> vertices(vertNum * pmdvertex_size);	// バッファの確保
	fread(vertices.data(), vertices.size(), 1, fp);				// 読み込み

	unsigned int indicesNum;									// インデックス数
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(vertices[0]));

	// UPLOAD（確保は可能）
	auto result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vb.ReleaseAndGetAddressOf())
	);

	unsigned char* vertMap = nullptr;
	result = _vb->Map(0, nullptr, (void**)&vertMap);
	copy(vertices.begin(), vertices.end(), vertMap);
	_vb->Unmap(0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress();		// バッファの仮想アドレス
	_vbView.SizeInBytes = static_cast<UINT>(vertices.size());	// 全バイト数
	_vbView.StrideInBytes = pmdvertex_size;						// 1頂点あたりのバイト数

	vector<unsigned short> indices(indicesNum);					// インデックス用バッファ
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	auto resDescBuf = CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0]));

	// 設定は、バッファのサイズ以外頂点バッファの設定を使いまわしてOKだと思われる
	result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDescBuf,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_ib.ReleaseAndGetAddressOf())
	);

	// 作ったバッファにインデックスデータをコピー
	unsigned short* mappedIdx = nullptr;
	_ib->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(indices.begin(), indices.end(), mappedIdx);
	_ib->Unmap(0, nullptr);

	// インデックスバッファビューを作成
	_ibView.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = static_cast<UINT>(indices.size() * sizeof(indices[0]));

	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);
	_materials.resize(materialNum);
	_textureResources.resize(materialNum);
	_sphResources.resize(materialNum);
	_spaResources.resize(materialNum);
	_toonResources.resize(materialNum);

	vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

	// コピー
	for (UINT i = 0; i < pmdMaterials.size(); i++) {
		_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		_materials[i].material.alpha = pmdMaterials[i].alpha;
		_materials[i].material.specular = pmdMaterials[i].specular;
		_materials[i].material.specularity = pmdMaterials[i].specularity;
		_materials[i].material.ambient = pmdMaterials[i].ambient;
	}

	for (UINT i = 0; i < pmdMaterials.size(); i++) {
		// トゥーンリソースの読み込み
		string toonFilePath = "toon/";
		char toonFileName[16];
		sprintf_s(toonFileName, 16, "toon%02d.bmp", pmdMaterials[i].toonIdx + 1);
		toonFilePath += toonFileName;
		_toonResources[i] = _dx12.GetTextureByPath(toonFilePath.c_str());

		_textureResources[i] = nullptr;
		_sphResources[i] = nullptr;
		_spaResources[i] = nullptr;
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
			else {
				texFileName = pmdMaterials[i].texFilePath;
			}
		}

		if (texFileName != "") {
			auto texFilePath = GetTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
			_textureResources[i] = _dx12.GetTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "") {
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			_sphResources[i] = _dx12.GetTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "") {
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			_spaResources[i] = _dx12.GetTextureByPath(spaFilePath.c_str());
		}
	}

	UINT16 boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);
#pragma pack(1)
	// 読み込み用ボーン構造体
	struct PMDBone {
		char boneName[20];					// ボーン名
		UINT16 parentNo;					// 親ボーン番号
		UINT16 nextNo;						// 先端のボーン番号
		UINT8 type;							// ボーン種別
		UINT16 ikBoneNo;					// IKボーン番号
		XMFLOAT3 pos;						// ボーンの基準点座標
	};
#pragma pack()	// 1バイトパッキング解除
	vector<PMDBone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);

	// インデックスと名前の対応関係構築のために後で使う
	vector<string> boneNames(pmdBones.size());
	// ボーンノードマップを作る
	for (int idx = 0; idx < pmdBones.size(); ++idx) {
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}
	// 親子関係を構築する
	for (auto& pb : pmdBones) {
		if (pb.parentNo >= pmdBones.size()) {
			continue;
		}
		auto parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}
	_boneMatrices.resize(pmdBones.size());

	// ボーンをすべて初期化
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	fclose(fp);

	return S_OK;
}

HRESULT PMDActor::CreateTransformView()
{
	// GPUバッファ作成
	auto buffSize = sizeof(XMMATRIX) * (1 + _boneMatrices.size());
	buffSize = (buffSize + 0xff) & ~0xFF;
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(buffSize);

	auto result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	// マップとコピー
	result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	_mappedMatrices[0] = _transform.world;
	auto armNode = _boneNodeTable["左腕"];
	auto& armPos = armNode.startPos;
	auto armMat = XMMatrixTranslation(-armPos.x, -armPos.y, -armPos.z)
		* XMMatrixRotationZ(XM_PIDIV2)
		* XMMatrixTranslation(armPos.x, armPos.y, armPos.z);
	auto elbowNode = _boneNodeTable["左ひじ"];
	auto& elbowPos = elbowNode.startPos;
	auto elbowMat = XMMatrixTranslation(-elbowPos.x, -elbowPos.y, -elbowPos.z)
		* XMMatrixRotationZ(-XM_PIDIV2)
		* XMMatrixTranslation(elbowPos.x, elbowPos.y, elbowPos.z);
	_boneMatrices[armNode.boneIdx] = armMat;
	_boneMatrices[elbowNode.boneIdx] = elbowMat;
	auto ident = XMMatrixIdentity();
	RecursiveMatrixMultiply(&_boneNodeTable["センター"], ident);
	std::copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

	// ビューの作成
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.NumDescriptors = 1;		// とりあえずワールドひとつ
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	// デスクリプタヒープ種別
	// 生成
	result = _dx12.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(buffSize);
	_dx12.Device()->CreateConstantBufferView(&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

void PMDActor::RecursiveMatrixMultiply(BoneNode* node, DirectX::XMMATRIX& mat)
{
	if (node == nullptr)
		return;

	_boneMatrices[node->boneIdx] = mat;
	for (auto& cnode : node->children) {
		auto cmat = _boneMatrices[cnode->boneIdx] * mat;
		RecursiveMatrixMultiply(cnode, cmat);
	}
}

HRESULT PMDActor::CreateMaterialData()
{
	// マテリアルバッファを作成
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xFF;

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * _materials.size());

	auto result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	// マップマテリアルにコピー
	char* mapMaterial = nullptr;
	result = _materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	for (auto& m : _materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material;			// データコピー
		mapMaterial += materialBuffSize;						// 次のアライメント位置まで進める
	}
	_materialBuff->Unmap(0, nullptr);

	return S_OK;
}

HRESULT PMDActor::CreateMaterialAndTextureView()
{
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.NumDescriptors = static_cast<UINT>(_materials.size() * 5);		// マテリアル数 x5（定数、基本テクスチャ、sph、spa、toon）
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;	// デスクリプタヒープ種別
	auto result = _dx12.Device()->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	// マテリアルビューの作成
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xFF;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();				// バッファーアドレス
	matCBVDesc.SizeInBytes = static_cast<UINT>(materialBuffSize);					// マテリアルの256アライメントサイズ

	// 通常テクスチャビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;		// 後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;							// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;												// ミップマップは使用しないので1
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;									// デフォルト
	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(_materialHeap->GetCPUDescriptorHandleForHeapStart());	// 先頭を記録
	auto incSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (UINT i = 0; i < _materials.size(); i++) {
		// マテリアル用定数バッファービュー
		_dx12.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		// シェーダーリソースビュー
		// テクスチャが空の場合は白テクスチャを使う
		auto texResource = _textureResources[i] == nullptr ? _renderer._whiteTex : _textureResources[i];
		srvDesc.Format = texResource->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(texResource.Get(), &srvDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;

		// スフィア
		auto sphResource = _sphResources[i] == nullptr ? _renderer._whiteTex : _sphResources[i];
		srvDesc.Format = sphResource->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(sphResource.Get(), &srvDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;

		auto spaResource = _spaResources[i] == nullptr ? _renderer._blackTex : _spaResources[i];
		srvDesc.Format = spaResource->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(spaResource.Get(), &srvDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;

		auto toonResource = _toonResources[i] == nullptr ? _renderer._gradTex : _toonResources[i];
		srvDesc.Format = toonResource->GetDesc().Format;
		_dx12.Device()->CreateShaderResourceView(toonResource.Get(), &srvDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
	}

	return S_OK;
}

void PMDActor::Update()
{
	_angle += 0.03f;
	_mappedMatrices[0] = XMMatrixRotationY(_angle);
}

void PMDActor::Draw()
{
	_dx12.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	_dx12.CommandList()->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* transheaps[] = { _transformHeap.Get() };
	_dx12.CommandList()->SetDescriptorHeaps(1, transheaps);
	_dx12.CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };
	_dx12.CommandList()->SetDescriptorHeaps(1, mdh);

	auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart();
	auto cbvSrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
	UINT idxOffset = 0;
	for (auto& m : _materials) {
		_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
		_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvSrvIncSize;
		idxOffset += m.indicesNum;
	}
}
