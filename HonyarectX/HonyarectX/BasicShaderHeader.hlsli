// 頂点シェーダ→ピクセルシェーダへのやり取りに使用する構造体
struct BasicType {
	float4 svpos:SV_POSITION;	// システム用頂点座標
	float2 uv:TEXCOORD;			// UV座標
};

Texture2D<float4> tex : register(t0);	// 0番スロットに設定されたテクスチャ
SamplerState smp : register(s0);		// 0番スロットに設定されたサンプラー

// 定数バッファー
cbuffer cbuff0 : register(b0)
{
	matrix mat;							// 変換行列
}
