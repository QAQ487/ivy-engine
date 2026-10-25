#include "engine/rendering/sprite/texture.h"
#include "engine/debug_draw/debug_ofstream.h"
#include "DirectXTex.h"
#include "engine/ui/ui_font.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdio>
namespace hal { debug_ostream dout; }

int main() {
    assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
    ID3D11Device* device = nullptr; ID3D11DeviceContext* context = nullptr;
    assert(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context)));
    Texture_Initialize(device, context);
    auto path = std::filesystem::temp_directory_path() / (L"ivy_texture_reload_" + std::to_wstring(GetCurrentProcessId()) + L".png");
    auto write = [&](size_t width, unsigned char color) {
        DirectX::ScratchImage image;
        assert(SUCCEEDED(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, 4, 1, 1)));
        memset(image.GetPixels(), color, image.GetPixelsSize());
        assert(SUCCEEDED(DirectX::SaveToWICFile(*image.GetImage(0,0,0), DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str())));
    };
    write(4, 255);
    const int id = TextureLoadQuiet(path.c_str(), false); assert(id >= 0);
    const auto alias = path.parent_path() / L"." / path.filename();
    assert(TextureLoadQuiet(alias.c_str(), false) == id);
    auto* initial = TextureGetSRV(id); initial->AddRef();
    write(8, 127);
    assert(TextureReload(path.c_str()) == TextureReloadResult::Reloaded);
    assert(TextureLoadQuiet(path.c_str(), false) == id);
    assert(TextureGetSize(id).x == 8 && TextureGetSRV(id) != initial);
    initial->Release();
    auto* good = TextureGetSRV(id);
    { std::ofstream(path, std::ios::binary) << "incomplete PNG"; }
    assert(TextureReload(path.c_str()) == TextureReloadResult::Failed);
    assert(TextureGetSRV(id) == good && TextureGetSize(id).x == 8);
    for (int i=0;i<20;++i) { write(4+i, static_cast<unsigned char>(i)); assert(TextureReload(path.c_str()) == TextureReloadResult::Reloaded); assert(TextureLoadQuiet(path.c_str(), false) == id); }
    std::filesystem::remove(path);
    assert(TextureReload(path.c_str()) == TextureReloadResult::Failed);
    assert(TextureGetSize(id).x == 23);
    write(4, 255);
    TextureAllRelease();
    assert(TextureReload(path.c_str()) == TextureReloadResult::NotLoaded);
    std::filesystem::remove(path);
    const auto fontPath = std::filesystem::temp_directory_path() / (L"ivy_font_reload_" + std::to_wstring(GetCurrentProcessId()) + L".ttf");
    std::filesystem::copy_file(L"C:/Windows/Fonts/arial.ttf", fontPath, std::filesystem::copy_options::overwrite_existing);
    const UIFont* font = UIFont_Get(fontPath.c_str()); assert(font);
    const int atlas = font->textureId;
    std::filesystem::copy_file(L"C:/Windows/Fonts/times.ttf", fontPath, std::filesystem::copy_options::overwrite_existing);
    assert(UIFont_Reload(fontPath.c_str()));
    assert(UIFont_Get(fontPath.c_str()) == font && font->textureId == atlas);
    const auto advance = font->glyphs['m'-32].xadvance;
    { std::ofstream(fontPath, std::ios::binary) << "broken"; }
    assert(!UIFont_Reload(fontPath.c_str()) && font->textureId == atlas && font->glyphs['m'-32].xadvance == advance);
    std::filesystem::remove(fontPath);
    Texture_Finalize(); context->ClearState(); context->Release(); device->Release(); CoUninitialize();
    std::puts("PASS: WARP texture ID, SRV replacement, dimensions, corrupt/deleted rollback, 20 replacements");
}
