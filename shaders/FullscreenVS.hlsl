struct VS_OUT
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD0;
};

VS_OUT main(uint id : SV_VertexID)
{
	VS_OUT o;
	// Fullscreen triangle: id 0,1,2 cover NDC [-1,1]^2
	float2 uv = float2((id << 1) & 2, id & 2);
	o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
	o.uv = uv;
	return o;
}
