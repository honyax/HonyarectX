// 頂点シェーダ→ピクセルシェーダへのやり取りに使用する構造体
struct BasicType {
	float4 svpos:SV_POSITION;	// システム用頂点座標
	float2 uv:TEXCOORD;			// UV座標
};

