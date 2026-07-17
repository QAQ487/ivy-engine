cbuffer CB_ViewProj : register(b0) {
	float4x4 g_view;
	float4x4 g_proj;
};

struct VS_IN {
	float3 position : POSITION;
	float2 texcoord : TEXCOORD0;
	float3 instancePosition : INSTANCEPOS0;
	float instanceSize : INSTANCESIZE0;
	float4 instanceColor : INSTANCECOLOR0;
	float4 instanceUv : INSTANCEUV0;
	float instanceRotation : INSTANCEROT0;
};

struct VS_OUT {
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
	float4 color : COLOR0;
};

VS_OUT main(VS_IN input) {
	VS_OUT output;

	float cosR = cos(input.instanceRotation);
	float sinR = sin(input.instanceRotation);
	float2 localPos = float2(
		cosR * input.position.x - sinR * input.position.y,
		sinR * input.position.x + cosR * input.position.y
	);

	float3 right = float3(g_view[0][0], g_view[1][0], g_view[2][0]);
	float3 up = float3(g_view[0][1], g_view[1][1], g_view[2][1]);
	float3 worldPos = input.instancePosition
		+ right * localPos.x * input.instanceSize
		+ up * localPos.y * input.instanceSize;

	float4 viewPos = mul(float4(worldPos, 1.0f), g_view);
	output.position = mul(viewPos, g_proj);
	output.texcoord = input.instanceUv.xy + input.texcoord * input.instanceUv.zw;
	output.color = input.instanceColor;
	return output;
}
