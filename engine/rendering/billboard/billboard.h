/**===========================================================================

 File   :  [billboard.h]

 Author : Terry
 Date   : 2025/12/4
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef BILLBOARD_H
#define BILLBOARD_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>

using namespace DirectX;

// Billboard data structure
struct BillboardData {
	XMFLOAT3 position;
	XMFLOAT2 size;
	XMFLOAT4 color;
	ID3D11ShaderResourceView* ptr_texture;
	float    rotation;  // camera: in-plane rad; ground: yaw around Y (rad)
	bool     groundAligned = false;
	XMFLOAT2 uvOffset = { 0.0f, 0.0f };
	XMFLOAT2 uvScale = { 1.0f, 1.0f };
};

class Billboard {
public:
	struct VertexBillboard {
		XMFLOAT3 position;
		XMFLOAT2 texcoord;
	};

private:
	// Shader related
	static ID3D11VertexShader* s_ptr_vs;
	static ID3D11PixelShader* s_ptr_ps;
	static ID3D11InputLayout* s_ptr_il;
	static ID3D11Buffer* s_ptr_cb_vp;
	static ID3D11Buffer* s_ptr_cb_billboard;
	static ID3D11SamplerState* s_ptr_sampler;
	static ID3D11RasterizerState* s_ptr_raster_no_cull;
	static ID3D11DepthStencilState* s_ptr_depth_read_only;

	// Vertex buffer (unit quad)
	static ID3D11Buffer* s_ptr_vertex_buffer;

	// Device context storage
	static ID3D11Device* s_ptr_device;
	static ID3D11DeviceContext* s_ptr_context;

	// View/Proj matrix cache
	static XMMATRIX s_view_matrix;
	static XMMATRIX s_proj_matrix;

	// Draw list
	static std::vector<BillboardData> s_draw_list;

	// Constant buffer structures
	struct CB_ViewProj {
		XMFLOAT4X4 view;
		XMFLOAT4X4 proj;
	};

	struct CB_Billboard {
		XMFLOAT3 position;
		float    padding1;
		XMFLOAT2 size;
		float    rotation;
		float    groundAligned;
		XMFLOAT4 color;
		XMFLOAT2 uvOffset;
		XMFLOAT2 uvScale;
	};

public:
	Billboard() = delete;
	~Billboard() = delete;

	// Static functions (main interface)
	static bool Initialize();
	static void Finalize();
	static void Update(double elapsed_time);
	static void Draw();

	// View/Proj settings
	static void SetViewMatrix(const XMMATRIX& view) { s_view_matrix = view; }
	static void SetProjectionMatrix(const XMMATRIX& proj) { s_proj_matrix = proj; }

	// Billboard registration (add to draw list)
	static void Add(const XMFLOAT3& position, const XMFLOAT2& size, const XMFLOAT4& color, ID3D11ShaderResourceView* ptr_texture, float rotation = 0.0f, bool groundAligned = false);
	static void Add(const XMFLOAT3& position, const XMFLOAT2& size, const XMFLOAT4& color, ID3D11ShaderResourceView* ptr_texture, float rotation, bool groundAligned, const XMFLOAT2& uvOffset, const XMFLOAT2& uvScale);
	static void Add(const BillboardData& data);

	// Clear draw list
	static void ClearDrawList() { s_draw_list.clear(); }
};

#endif // !BILLBOARD_H
