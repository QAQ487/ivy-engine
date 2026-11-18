/**===========================================================================

 File   :  [debug_text.cpp]

 Author : Terry Yeung
 Date   : 2025/11/17
 -----------------------------------------------------------------------------

 ===========================================================================**/
#include "debug_text.h"
#include <algorithm>
#include "engine/ui/ui_font.h"
#include "engine/rendering/sprite/texture.h"
#include "engine/core/frame_capture.h"
using namespace DirectX;
#include <D3Dcompiler.h>
using namespace Microsoft::WRL;

#pragma comment(lib, "d3dcompiler.lib")


namespace hal
{
	ComPtr<ID3D11BlendState> DebugText::m_pBlendState;
	ComPtr<ID3D11DepthStencilState> DebugText::m_pDepthStencilState;
	ComPtr<ID3D11RasterizerState> DebugText::m_pRasterizerState;

	ComPtr<ID3D11VertexShader> DebugText::m_pVertexShader;
	ComPtr<ID3D11InputLayout> DebugText::m_pInputLayout;
	ComPtr<ID3D11Buffer> DebugText::m_pVSConstantBuffer;
	ComPtr<ID3D11PixelShader> DebugText::m_pPixelShader;
	ComPtr<ID3D11SamplerState> DebugText::m_pSamplerState;

	DebugText::DebugText(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, UINT screenWidth, UINT screenHeight, float offsetX, float offsetY, ULONG maxLine, ULONG maxCharactersPerLine, float lineSpacing, float characterSpacing)
		: m_pDevice(pDevice), m_pContext(pContext), m_OffsetX(offsetX), m_OffsetY(offsetY), m_MaxLine(maxLine), m_MaxCharactersPerLine(maxCharactersPerLine), m_LineSpacing(lineSpacing), m_CharacterSpacing(characterSpacing)
	{
		m_pFont = UIFont_GetBuiltinMono();

		if (!m_LineSpacing) {
			m_LineSpacing = 16.0f;
		}

		if (!m_CharacterSpacing) {
			m_CharacterSpacing = 9.0f;
		}

		if (m_pFont) {
			// Fit the font into the cell: never taller than a line, never wider than a column.
			m_CellAdvance = m_pFont->glyphs['0' - ' '].xadvance;
			const float byHeight = m_pFont->lineHeight > 0.0f ? m_LineSpacing / m_pFont->lineHeight : 1.0f;
			const float byWidth = m_CellAdvance > 0.0f ? m_CharacterSpacing / m_CellAdvance : 1.0f;
			m_GlyphScale = (std::min)(byHeight, byWidth);
		}

		m_TextLines.emplace_back();

		if (!m_pBlendState) {
			// Create blend state
			D3D11_BLEND_DESC blend_desc{};
			blend_desc.RenderTarget[0].BlendEnable = TRUE;
			blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			m_pDevice->CreateBlendState(&blend_desc, m_pBlendState.GetAddressOf());
		}

		if (!m_pDepthStencilState) {
			// Create depth-stencil state
			D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
			depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
			depth_stencil_desc.StencilEnable = FALSE;
			depth_stencil_desc.DepthEnable = FALSE; // Disable depth test
			depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

			m_pDevice->CreateDepthStencilState(&depth_stencil_desc, m_pDepthStencilState.GetAddressOf());
		}

		if (!m_pRasterizerState) {
			// Create rasterizer state
			D3D11_RASTERIZER_DESC rasterizer_desc{};
			rasterizer_desc.FillMode = D3D11_FILL_SOLID;
			rasterizer_desc.CullMode = D3D11_CULL_BACK;
			rasterizer_desc.DepthClipEnable = TRUE; // Enable depth clipping

			m_pDevice->CreateRasterizerState(&rasterizer_desc, m_pRasterizerState.GetAddressOf());
		}

		if (!m_pVertexShader) {

			// Create vertex shader
			static const char* vs_text = R"(
				float4x4 mtx;

				struct VS_IN
				{
					float4 posL  : POSITION0;
					float4 color : COLOR0;
					float2 uv    : TEXCOORD0;
				};

				struct VS_OUT
				{
					float4 posH  : SV_POSITION;
					float4 color : COLOR0;
					float2 uv    : TEXCOORD0;
				};

				VS_OUT main(VS_IN vsin)
				{
					VS_OUT vsout;
    
					vsout.posH = mul(vsin.posL, mtx);
					vsout.color = vsin.color;
					vsout.uv = vsin.uv;
    
					return vsout;
				}
			)";

			ComPtr<ID3DBlob> pVSBlob;
			D3DCompile(vs_text, strlen(vs_text), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, pVSBlob.GetAddressOf(), nullptr);
			m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, m_pVertexShader.GetAddressOf());

			// Vertex shader input layout desc
			D3D11_INPUT_ELEMENT_DESC layout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			UINT numElements = ARRAYSIZE(layout);

			// Create input layout
			m_pDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), m_pInputLayout.GetAddressOf());
		}

		if (!m_pVSConstantBuffer) {
			// Create constant buffer
			D3D11_BUFFER_DESC constant_buffer_desc = {};
			constant_buffer_desc.ByteWidth = sizeof(XMFLOAT4X4);
			constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

			m_pDevice->CreateBuffer(&constant_buffer_desc, nullptr, m_pVSConstantBuffer.GetAddressOf());

			// Upload orthographic matrix
			XMFLOAT4X4 mtx;
			XMStoreFloat4x4(&mtx, XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 0.0f, 1.0f)));
			m_pContext->UpdateSubresource(m_pVSConstantBuffer.Get(), 0, nullptr, &mtx, 0, 0);
		}

		if (!m_pPixelShader) {
			// Create pixel shader
			static const char* ps_text = R"(
				struct PS_INPUT
				{
					float4 posH  : SV_POSITION;
					float4 color : COLOR0;
					float2 uv    : TEXCOORD0;
				};
				Texture2D fontTexture : register(t0);
				SamplerState fontSampler : register(s0);
				float4 main(PS_INPUT psin) : SV_TARGET
				{
					float4 color = fontTexture.Sample(fontSampler, psin.uv);
					return color * psin.color; // Multiply texture color by vertex color
				}
			)";
			ComPtr<ID3DBlob> pPSBlob;
			D3DCompile(ps_text, strlen(ps_text), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, pPSBlob.GetAddressOf(), nullptr);
			m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, m_pPixelShader.GetAddressOf());
		}

		if( !m_pSamplerState) {
			// Create sampler state
			D3D11_SAMPLER_DESC sampler_desc = {};
			sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // TTF glyphs are scaled, so smooth them
			sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
			sampler_desc.MinLOD = 0.0f;
			sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
			m_pDevice->CreateSamplerState(&sampler_desc, m_pSamplerState.GetAddressOf());
		}
	}

	DebugText::~DebugText() = default;

	void DebugText::SetText(const char* pText, XMFLOAT4 color)
	{		
		m_TextLines.back().strings.emplace_back(color); // Start a new colored run

		for(; *pText; pText++) {
	
			if (*pText == '\n') {
				m_TextLines.emplace_back(); // New line
				m_TextLines.back().strings.emplace_back(color); // Start a new colored run
			}
			else if (*pText == '\r') {
				// Ignore CR ('\r')
			}
			else if (*pText == '\t') {
				while (m_TextLines.back().characterCount % 4 != 0) {
					if (m_MaxCharactersPerLine && m_TextLines.back().characterCount >= m_MaxCharactersPerLine) {
						m_TextLines.emplace_back(); // New line
						// m_TextLines.back().strings.emplace_back(color); // Start a new colored run
						break;
					}
					m_TextLines.back().strings.back().characters += ' '; // Expand tab to spaces
					m_TextLines.back().characterCount++;
					m_TextLines.back().spaceCount++;
				}
			}
			else {
				if (m_MaxCharactersPerLine && m_TextLines.back().characterCount >= m_MaxCharactersPerLine) {
					m_TextLines.emplace_back(); // New line
					m_TextLines.back().strings.emplace_back(color); // Start a new colored run
				}

				if (*pText >= ' ' && *pText <= '~') {
					m_TextLines.back().strings.back().characters += *pText;
					*pText != ' ' ? m_CharacterCount++ : m_TextLines.back().spaceCount++;
					m_TextLines.back().characterCount++;
				}
				else {
					m_TextLines.back().strings.back().characters += '?'; // Replace unsupported chars with '?'
					m_CharacterCount++;
					m_TextLines.back().characterCount++;
				}
			}
		}

		int last_line_count = m_TextLines.back().characterCount ? 0 : -1;

		while(m_MaxLine && m_TextLines.size() + last_line_count > m_MaxLine) {
			ULONG remove_character_count = m_TextLines.front().characterCount - m_TextLines.front().spaceCount;
			m_CharacterCount -= remove_character_count; // Drop chars from oldest line
			m_TextLines.pop_front(); // Drop oldest line
		}
	}

	void DebugText::Draw()
	{
		if( !m_CharacterCount){
			return; // Nothing to draw
		}

		ID3D11ShaderResourceView* atlasView = m_pFont ? TextureGetSRV(m_pFont->textureId) : nullptr;
		if (!atlasView) {
			return; // Built-in font missing: draw nothing rather than garbage
		}

		if (!m_pVertexBuffer || m_CharacterCount > m_BufferSourceCharacterCount) {
			createBuffer(m_CharacterCount);
		}

		// Lock vertex and index buffers
		D3D11_MAPPED_SUBRESOURCE msr;
		m_pContext->Map(m_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

		// Vertex buffer write pointer
		Vertex* v = (Vertex*)msr.pData;

		m_pContext->Map(m_pIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

		// Index buffer write pointer
		WORD* indices = (WORD*)msr.pData;

		// Fill vertex/index data
		UINT lineCount = 0;
		WORD characterCount = 0;
		const float atlasW = static_cast<float>(m_pFont->atlasWidth);
		const float atlasH = static_cast<float>(m_pFont->atlasHeight);
		const float cellPadX = (m_CharacterSpacing - m_CellAdvance * m_GlyphScale) * 0.5f;
		const float cellPadY = (m_LineSpacing - m_pFont->lineHeight * m_GlyphScale) * 0.5f;
		const float baseline = cellPadY + m_pFont->ascent * m_GlyphScale;

		for (const auto& strings : m_TextLines) {

			UINT columnCount = 0;

			for (const auto& string : strings.strings) {

				for (const auto& code : string.characters) {

					const int index = code - ' ';
					const UIGlyph* glyph = (index > 0 && index < 95) ? &m_pFont->glyphs[index] : nullptr;

					if (glyph && glyph->srcW > 0 && glyph->srcH > 0) {
						const float u0 = glyph->srcX / atlasW;
						const float v0 = glyph->srcY / atlasH;
						const float u1 = (glyph->srcX + glyph->srcW) / atlasW;
						const float v1 = (glyph->srcY + glyph->srcH) / atlasH;
						const float x = m_OffsetX + columnCount * m_CharacterSpacing + cellPadX + glyph->xoff * m_GlyphScale;
						const float y = m_OffsetY + lineCount * m_LineSpacing + baseline + glyph->yoff * m_GlyphScale;
						const float w = glyph->srcW * m_GlyphScale;
						const float h = glyph->srcH * m_GlyphScale;

						v[0].position = { x, y, 1.0f };
						v[0].color = string.color;
						v[0].texcoord = { u0, v0 };

						v[1].position = { x + w, y, 1.0f };
						v[1].color = string.color;
						v[1].texcoord = { u1, v0 };

						v[2].position = { x, y + h, 1.0f };
						v[2].color = string.color;
						v[2].texcoord = { u0, v1 };

						v[3].position = { x + w, y + h, 1.0f };
						v[3].color = string.color;
						v[3].texcoord = { u1, v1 };

						v += 4;

						indices[0] = characterCount * 4 + 0;
						indices[1] = characterCount * 4 + 1;
						indices[2] = characterCount * 4 + 2;
						indices[3] = characterCount * 4 + 2;
						indices[4] = characterCount * 4 + 1;
						indices[5] = characterCount * 4 + 3;
						indices += 6;

						characterCount++;
					}

					columnCount++;
				}
			}

			lineCount++;
		}

		// Unlock vertex and index buffers
		m_pContext->Unmap(m_pVertexBuffer.Get(), 0);
		m_pContext->Unmap(m_pIndexBuffer.Get(), 0);

		if (characterCount == 0) {
			return; // Only blanks
		}

		// Bind vertex buffer to the pipeline
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		m_pContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &stride, &offset);

		// Bind index buffer to the pipeline
		m_pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
		
		// Bind vertex shader to the pipeline
		m_pContext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);

		// Bind constant buffer to the pipeline
		m_pContext->VSSetConstantBuffers(0, 1, m_pVSConstantBuffer.GetAddressOf());

		// Bind input layout to the pipeline
		m_pContext->IASetInputLayout(m_pInputLayout.Get());

		// Bind pixel shader, texture, and sampler to the pipeline
		m_pContext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);
		m_pContext->PSSetShaderResources(0, 1, &atlasView);
		m_pContext->PSSetSamplers(0, 1, m_pSamplerState.GetAddressOf());

		ComPtr<ID3D11BlendState> pPreviousBlendState; // Previous blend state
		float previous_blend_factor[4];
		UINT previous_sample_mask;
		m_pContext->OMGetBlendState(pPreviousBlendState.GetAddressOf(), previous_blend_factor, &previous_sample_mask);

		// Set blend state
		float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_pContext->OMSetBlendState(m_pBlendState.Get(), blend_factor, 0xffffffff);

		ComPtr<ID3D11DepthStencilState> pPreviousDepthStencilState; // Previous depth-stencil state
		UINT previous_stencil_ref = 0;
		m_pContext->OMGetDepthStencilState(pPreviousDepthStencilState.GetAddressOf(), &previous_stencil_ref);

		// Set depth-stencil state (depth off)
		m_pContext->OMSetDepthStencilState(m_pDepthStencilState.Get(), 0);

		// Set rasterizer state
		ComPtr<ID3D11RasterizerState> pPreviousRasterizerState; // Previous rasterizer state
		m_pContext->RSGetState(pPreviousRasterizerState.GetAddressOf());
		m_pContext->RSSetState(m_pRasterizerState.Get());

		// Set primitive topology
		m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Draw polygons
		FrameCapture_RecordTextDraw();
        m_pContext->DrawIndexed(characterCount * 6, 0, 0);

		// Restore blend, depth-stencil, and rasterizer states
		m_pContext->OMSetBlendState(pPreviousBlendState.Get(), previous_blend_factor, 0xffffffff);
		m_pContext->OMSetDepthStencilState(pPreviousDepthStencilState.Get(), previous_stencil_ref);
		m_pContext->RSSetState(pPreviousRasterizerState.Get());
	}

	void DebugText::Clear()
	{
		m_TextLines.clear();
		m_TextLines.emplace_back(); // Keep one empty line
		m_CharacterCount = 0;
	}

	void DebugText::MeasureText(const char* text, float& outWidth, float& outHeight) const {
		outWidth = 0.0f;
		outHeight = 0.0f;
		if (!text || !*text) {
			return;
		}

		UINT currentCols = 0;
		UINT maxCols = 0;
		UINT lines = 1;

		auto flushLine = [&] () {
			if (currentCols > maxCols) maxCols = currentCols;
			currentCols = 0;
		};

		while (*text) {
			char c = *text++;
			switch (c) {
				case '\r':
					// Ignore
					break;
				case '\n':
					flushLine();
					lines++;
					break;
				case '\t':
					// Expand tab to 4 columns
					do {
						currentCols++;
					} while (currentCols % 4 != 0);
					break;
				default:
					currentCols++;
					break;
			}
		}
		// Flush last line
		flushLine();

		outWidth = static_cast<float>(maxCols) * m_CharacterSpacing;
		outHeight = static_cast<float>(lines) * m_LineSpacing;
	}

	void DebugText::createBuffer(ULONG characterCount)
	{
		// Create vertex buffer
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Vertex) * characterCount * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		m_pDevice->CreateBuffer(&bd, NULL, m_pVertexBuffer.ReleaseAndGetAddressOf());

		// Create index buffer
		bd.ByteWidth = sizeof(WORD) * characterCount * 6;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

		m_pDevice->CreateBuffer(&bd, NULL, m_pIndexBuffer.ReleaseAndGetAddressOf());

		m_BufferSourceCharacterCount = characterCount;
	}

}