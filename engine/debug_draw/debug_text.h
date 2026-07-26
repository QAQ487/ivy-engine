/**===========================================================================

 File   :  [debug_text.h]

 Author : Hayato Otsuki
 Date   : 2025/11/17
 -----------------------------------------------------------------------------

 ===========================================================================**/
#ifndef DEBUG_TEXT_H
#define DEBUG_TEXT_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <unordered_map>
#include <deque>
#include <tuple>
#include <string>
#include <wrl/client.h> // For Microsoft::WRL::ComPtr

namespace hal {
	class DebugText {
	public:
		struct StringBlock {
			std::string characters;
			DirectX::XMFLOAT4 color;
			StringBlock(DirectX::XMFLOAT4 c) : color(c) {}
		};
		struct Line {
			std::deque<StringBlock> strings;
			UINT characterCount = 0;
			UINT spaceCount = 0;
		};

		DebugText(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
			const wchar_t* pFontTextureFileName,
			UINT screenWidth, UINT screenHeight,
			float offsetX, float offsetY,
			ULONG maxLine, ULONG maxCharactersPerLine,
			float lineSpacing, float characterSpacing);
		~DebugText();

		void SetOffset(float x, float y) { m_OffsetX = x; m_OffsetY = y; }
		void SetText(const char* pText, DirectX::XMFLOAT4 color);
		void Draw();
		void Clear();

		// 計測用 API
		float GetLineSpacing() const { return m_LineSpacing; }
		float GetCharacterSpacing() const { return m_CharacterSpacing; }

		// 与えられた文字列の幅・高さ（行数×行間 / 最大列数×文字間）を概算取得
		void MeasureText(const char* text, float& outWidth, float& outHeight) const;

	private:
		struct Vertex {
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 color;
			DirectX::XMFLOAT2 texcoord;
		};

		ID3D11Device* m_pDevice = nullptr;
		ID3D11DeviceContext* m_pContext = nullptr;

		std::wstring m_FileName;
		float  m_OffsetX = 0.0f;
		float  m_OffsetY = 0.0f;

		ULONG  m_MaxLine = 0;
		ULONG  m_MaxCharactersPerLine = 0;

		float  m_LineSpacing = 0.0f;
		float  m_CharacterSpacing = 0.0f;

		UINT   m_TextureWidth = 0;
		UINT   m_TextureHeight = 0;

		std::deque<Line> m_TextLines;
		ULONG  m_CharacterCount = 0;
		ULONG  m_BufferSourceCharacterCount = 0;

		// 静的共有リソース
		static std::unordered_map<std::wstring, std::tuple<ID3D11Resource*, ID3D11ShaderResourceView*>> m_TextureMap;
		static Microsoft::WRL::ComPtr<ID3D11BlendState>       m_pBlendState;
		static Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pDepthStencilState;
		static Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_pRasterizerState;

		static Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVertexShader;
		static Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_pInputLayout;
		static Microsoft::WRL::ComPtr<ID3D11Buffer>       m_pVSConstantBuffer;
		static Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_pPixelShader;
		static Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pSamplerState;

		ID3D11Resource* m_pTexture = nullptr;
		ID3D11ShaderResourceView* m_pTextureView = nullptr;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pIndexBuffer;

		void createBuffer(ULONG characterCount);
	};
} // namespace hal

#endif // !DEBUG_TEXT_H
