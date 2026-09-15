/**===========================================================================

 File   :  [grid.h]

 Author : Terry Yeung
 Date   : 2025/10/23
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "engine/core/game_object.h"
#include <d3d11.h>
#include <DirectXMath.h>

class Grid : public GameObject {
private:
	ID3D11Buffer* m_vertexBuffer = nullptr;
	ID3D11ShaderResourceView* m_texture = nullptr;

public:
	void Initialize() override;
	void Finalize() override;
	void LogicUpdate(double elapsed_time) override;
	void Draw() override;

	void SetVertexBuffer(ID3D11Buffer* vb) { m_vertexBuffer = vb; }
	void SetTexture(ID3D11ShaderResourceView* tex) { m_texture = tex; }
};

void Grid_Initialize();
void Grid_Finalize();
void Grid_Draw();