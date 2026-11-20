// editor_inspector_panel.cpp
// Properties (inspector) panel. Every section is built from the width-safe
// insp:: widgets (inspector_widgets.h): icon rows with short words, colored XYZ
// fields, icon toggles, and a collapsible header per component with an enable
// toggle and a Reset / Copy JSON / Remove menu. Nothing is wider than the panel.

#include "editor_gui_common_includes.h"
#include "engine/editor/inspector_widgets.h"
#include "engine/editor/terrain_editor.h"
namespace fs = std::filesystem;
using json = nlohmann::json;

const std::string PREFAB_DIR = "rom/prefabs/";

bool ScrollValue(float* value, float baseSpeed) {
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            *value += wheel * baseSpeed * g_scrollSensitivity;
            return true;
        }
    }
    return false;
}
std::string LowerAssetExtension(const char* path) {
    if (!path) return "";
    std::string ext = PathToUtf8(PathFromUtf8(path).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    return ext;
}
bool AssetPathHasExtension(const char* path, const char* const* extensions, int count) {
    const std::string ext = LowerAssetExtension(path);
    for (int i = 0; i < count; ++i) {
        if (ext == extensions[i])
            return true;
    }
    return false;
}
bool AcceptAssetPathDrop(const char* const* extensions, int count, std::string& outPath) {
    bool accepted = false;
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            if (path && AssetPathHasExtension(path, extensions, count)) {
                outPath = path;
                accepted = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return accepted;
}
bool DrawScriptPropertyField(GameObject* editNode,
                                    const std::string& key,
                                    nlohmann::json& val) {
    insp::PropRow row(nullptr, key.c_str());
    if (val.is_number_float()) {
        float f = val.get<float>();
        if (ImGui::DragFloat("##v", &f, 0.1f)) { val = f; return true; }
        if (ImGui::IsItemHovered()) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) { f += wheel * 0.1f * g_scrollSensitivity; val = f; return true; }
        }
    }
    else if (val.is_number_integer() || val.is_number_unsigned()) {
        int iv = val.get<int>();
        if (ImGui::InputInt("##v", &iv)) { val = iv; return true; }
    }
    else if (val.is_boolean()) {
        bool bv = val.get<bool>();
        if (ImGui::Checkbox("##v", &bv)) { val = bv; return true; }
    }
    else if (val.is_string()) {
        static GameObject* s_editNode = nullptr;
        static std::string s_editKey;
        static char s_strBuf[256] = {};
        if (s_editNode != editNode || s_editKey != key) {
            s_editNode = editNode; s_editKey = key;
            strncpy_s(s_strBuf, val.get<std::string>().c_str(), sizeof(s_strBuf) - 1);
        }
        if (ImGui::InputText("##v", s_strBuf, sizeof(s_strBuf),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            val = std::string(s_strBuf);
            return true;
        }
        if (!ImGui::IsItemActive())
            strncpy_s(s_strBuf, val.get<std::string>().c_str(), sizeof(s_strBuf) - 1);
    }
    else {
        ImGui::TextDisabled("-");
    }
    return false;
}
void DrawPropertiesEmptyState() {
    ImGui::Spacing();
    insp::ReadOnlyRow(ICON_FA_ARROW_POINTER, "Select", "Pick an object in the Hierarchy or viewport",
        "Nothing selected");
}

namespace {

const char* const kTextureExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
const char* const kModelExts[] = { ".fbx", ".obj", ".gltf", ".glb" };

std::string Vec3Text(const DirectX::XMFLOAT3& v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f  %.2f  %.2f", v.x, v.y, v.z);
    return buf;
}

// One component section: undo bracket + header, then Finish() handles the
// header menu actions (Reset / Remove) after the undo bracket closes.
class Section {
public:
    Section(GameObject* node, const char* type, const char* icon, const char* name,
            insp::Accent accent, Component* comp, bool* enabled, bool& dirty,
            bool canRemove = true, bool canReset = true,
            void (*extraMenu)(void*) = nullptr)
        : m_node(node), m_type(type), m_comp(comp), m_dirty(dirty)
    {
        EditorUndo_BeginComponentEdit(node, type);
        m_header = insp::ComponentHeader(type, icon, name, accent, enabled, comp,
            canRemove, canReset && comp != nullptr, extraMenu, nullptr);
        if (m_header.changed)
            m_dirty = true;
        ImGui::PushID(type);
    }
    bool Open() const { return m_header.open; }
    void EndEdit() {
        if (m_ended)
            return;
        m_ended = true;
        ImGui::PopID();
        EditorUndo_EndComponentEdit(m_node, m_type);
    }
    // Returns true when the component was removed (do not touch it afterwards).
    template <class Remove>
    bool Finish(Remove remove) {
        EndEdit();
        if (m_header.action == insp::HeaderAction::Reset && m_comp) {
            const std::string before = m_comp->Serialize();
            if (std::unique_ptr<Component> fresh = ComponentFactory::Instance().Create(m_type)) {
                m_comp->Deserialize(fresh->Serialize());
                std::string after = m_comp->Serialize();
                if (after != before) {
                    EditorUndo_PushComponentEdit(m_node, m_type, before, std::move(after));
                    m_dirty = true;
                }
            }
        }
        else if (m_header.action == insp::HeaderAction::Remove) {
            EditorUndo_PushRemoveComponent(m_node, m_type);
            remove();
            m_dirty = true;
            return true;
        }
        return false;
    }
private:
    GameObject* m_node;
    const char* m_type;
    Component* m_comp;
    bool& m_dirty;
    insp::HeaderResult m_header;
    bool m_ended = false;
};

void Mark(bool edited, bool& dirty) {
    if (edited)
        dirty = true;
}

// ---------------------------------------------------------------- Node header

void DrawNodeHeader(GameObject* n, char* nameBuf, size_t nameBufSize, bool& dirty) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const float frameH = ImGui::GetFrameHeight();
    const std::string& prefab = n->GetPrefabAssetPath();

    // Name (+ prefab link icon).
    ImGui::SetNextItemWidth(prefab.empty() ? -FLT_MIN
        : (std::max)(1.0f, ImGui::GetContentRegionAvail().x - frameH - st.ItemInnerSpacing.x));
    if (ImGui::InputTextWithHint("##NodeName", "Name", nameBuf, nameBufSize,
        ImGuiInputTextFlags_EnterReturnsTrue)) {
        const std::string oldName = n->GetName();
        const std::string newName(nameBuf);
        n->SetName(newName);
        EditorUndo_PushRename(n, oldName, newName);
        EngineGUI_MarkSceneDirty();
    }
    if (!ImGui::IsItemActive())
        strncpy_s(nameBuf, nameBufSize, n->GetName().c_str(), nameBufSize - 1);
    if (!prefab.empty()) {
        ImGui::SameLine(0.0f, st.ItemInnerSpacing.x);
        insp::HintIcon(ICON_FA_LINK, insp::InfoColor(), prefab.c_str());
    }

    // Tag.
    TagRegistry& tags = TagRegistry::Instance();
    int currentTag = n->GetTagId();
    if (!tags.IsValid(currentTag))
        currentTag = static_cast<int>(Tag::TAG_NONE);
    {
        insp::PropRow row(ICON_FA_TAG, "Tag");
        if (ImGui::BeginCombo("##v", tags.GetName(currentTag), ImGuiComboFlags_HeightLarge)) {
            const float removeW = ImGui::GetFrameHeight();
            const float spacing = st.ItemInnerSpacing.x;
            const int noneId = static_cast<int>(Tag::TAG_NONE);
            for (int i = 0; i < tags.GetCount(); ++i) {
                ImGui::PushID(i);
                const bool selected = (i == currentTag);
                const bool showRemove = (i != noneId);
                const bool canRemove = tags.CanRemove(i);
                const char* name = tags.GetName(i);
                const float labelW = showRemove
                    ? ImGui::GetContentRegionAvail().x - removeW - spacing : 0.0f;
                if (ImGui::Selectable(name, selected, 0,
                        ImVec2(showRemove ? (std::max)(labelW, 1.0f) : 0.0f, 0.0f))) {
                    const int oldTag = n->GetTagId();
                    n->SetTag(i);
                    EditorUndo_PushTag(n, oldTag, i);
                    dirty = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
                if (showRemove) {
                    ImGui::SameLine(0.0f, spacing);
                    if (!canRemove)
                        ImGui::BeginDisabled();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 0.55f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.2f, 0.2f, 0.75f));
                    if (ImGui::Button(ICON_FA_XMARK "##RemoveTag", ImVec2(removeW, removeW))) {
                        g_pendingDeleteTagId = i;
                        g_showDeleteTagPopup = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopStyleColor(3);
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled |
                            ImGuiHoveredFlags_DelayShort))
                        ImGui::SetTooltip(canRemove ? "Delete tag" : "Built-in");
                    if (!canRemove)
                        ImGui::EndDisabled();
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            if (ImGui::Selectable(ICON_FA_PLUS "  New")) {
                g_newTagName[0] = '\0';
                g_showAddTagPopup = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndCombo();
        }
    }

    // Draw order.
    insp::RowLabel(ICON_FA_SORT, "Order", "Later = drawn on top");
    static const char* const kOrderIcons[] = { ICON_FA_ANGLES_DOWN, ICON_FA_ARROW_DOWN,
                                               ICON_FA_ARROW_UP, ICON_FA_ANGLES_UP };
    static const char* const kOrderTips[] = { "Back", "Down", "Up", "Top" };
    switch (insp::IconButtonRow("order", kOrderIcons, kOrderTips, 4)) {
    case 0: Mark(UndoableReorderNode(n, MoveNodeToBack), dirty); break;
    case 1: Mark(UndoableReorderNode(n, [](GameObject* g) { return MoveNodeOrder(g, -1); }), dirty); break;
    case 2: Mark(UndoableReorderNode(n, [](GameObject* g) { return MoveNodeOrder(g, 1); }), dirty); break;
    case 3: Mark(UndoableReorderNode(n, MoveNodeToFront), dirty); break;
    default: break;
    }
}

// ------------------------------------------------------------------ Transform

void TransformMenu(void*) {
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
    ImGui::SliderFloat(ICON_FA_COMPUTER_MOUSE "  Wheel", &g_scrollSensitivity, 0.1f, 5.0f, "%.1f");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Mouse wheel step");
}

void DrawTransform(GameObject* n, engine::Transform* t, bool& dirty) {
    Section s(n, "SceneTransform", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Transform",
        insp::Accent::Transform, t, nullptr, dirty, false, true, &TransformMenu);
    if (s.Open()) {
        const float before[9] = {
            t->localPosition.x, t->localPosition.y, t->localPosition.z,
            t->localRotation.x, t->localRotation.y, t->localRotation.z,
            t->localScale.x, t->localScale.y, t->localScale.z };
        bool edited = false;
        edited |= insp::DragVecN(ICON_FA_LOCATION_DOT, "Pos", "Position (local)",
            &t->localPosition.x, 3, 0.1f, 0.0f, 0.0f, 0.0f, 0.1f * g_scrollSensitivity);
        edited |= insp::DragVecN(ICON_FA_ROTATE, "Rot", "Rotation (deg, local)",
            &t->localRotation.x, 3, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f * g_scrollSensitivity, "%.1f");
        edited |= insp::DragVecN(ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, "Scale", "Scale (local)",
            &t->localScale.x, 3, 0.1f, 0.001f, 1000.0f, 1.0f, 0.1f * g_scrollSensitivity);
        const float after[9] = {
            t->localPosition.x, t->localPosition.y, t->localPosition.z,
            t->localRotation.x, t->localRotation.y, t->localRotation.z,
            t->localScale.x, t->localScale.y, t->localScale.z };
        if (edited && JsonFloat_TransformChanged(before, after))
            dirty = true;
        else if (edited) {
            // A change that does not survive 3dp — keep the original bits.
            t->localPosition = { before[0], before[1], before[2] };
            t->localRotation = { before[3], before[4], before[5] };
            t->localScale    = { before[6], before[7], before[8] };
        }
        n->SyncLegacyTransformFromComponents();
    }
    s.Finish([] {});
    n->SyncLegacyTransformFromComponents();
}

// --------------------------------------------------------------------- Visual

void DrawVisual(GameObject* n, VisualComponent* vc, bool& dirty) {
    Section s(n, "visual", ICON_FA_CUBE, "Visual", insp::Accent::Render, vc, nullptr, dirty);
    if (s.Open()) {
        // Mesh
        const auto& defs = ModelAssetCache::Instance().GetObjectDefinitions();
        int currentIdx = -1;
        for (int i = 0; i < (int)defs.size(); ++i)
            if (defs[i].assetPath == vc->modelPath) { currentIdx = i; break; }
        std::string preview = vc->modelPath.empty() ? std::string("None")
            : (currentIdx >= 0 ? defs[currentIdx].displayName
                               : std::string(ICON_FA_TRIANGLE_EXCLAMATION " ") + vc->modelPath);
        {
            insp::PropRow row(ICON_FA_CUBE, "Mesh");
            if (ImGui::BeginCombo("##v", preview.c_str())) {
                for (int i = 0; i < (int)defs.size(); ++i) {
                    const bool selected = (i == currentIdx);
                    if (!selected && ModelAssetCache::IsHiddenPrimitiveAlias(defs[i].assetPath))
                        continue;
                    if (ImGui::Selectable(defs[i].displayName.c_str(), selected) &&
                        vc->modelPath != defs[i].assetPath) {
                        vc->modelPath = defs[i].assetPath;
                        dirty = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (!vc->modelPath.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip(currentIdx < 0 ? "Missing / custom\n%s" : "%s", vc->modelPath.c_str());
            std::string dropped;
            if (AcceptAssetPathDrop(kModelExts, IM_ARRAYSIZE(kModelExts), dropped) &&
                vc->modelPath != dropped) {
                vc->modelPath = dropped;
                dirty = true;
            }
        }

        const bool volumeMode = RenderSettings_GetConst().shadowMode == ShadowMode::Volume;
        insp::Toggle toggles[] = {
            { ICON_FA_PERSON_WALKING, "Animate", &vc->playAnimation },
            { ICON_FA_CLOUD, "Shadow Vol", &vc->shadowVolume,
              vc->shadowVolume && !volumeMode ? "Needs Shadow Mode: Volume" : nullptr },
        };
        Mark(insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles)), dirty);

        // Shader
        {
            const bool useDefault = vc->shaderOverride.empty();
            const Shader3DVariant sceneDefault = RenderSettings_GetDefaultShader();
            const Shader3DVariant current = useDefault ? sceneDefault : Shader3DVariantFromId(vc->shaderOverride);
            const std::string label = useDefault
                ? std::string("Default: ") + Shader3DVariantLabel(sceneDefault)
                : ShaderVariantPreviewLabel(current);
            insp::PropRow row(ICON_FA_PALETTE, "Shader");
            if (ImGui::BeginCombo("##v", label.c_str())) {
                if (ImGui::Selectable("Default", useDefault)) {
                    vc->shaderOverride.clear();
                    dirty = true;
                }
                for (int i = 0; i < Shader3DVariantCount(); ++i) {
                    const Shader3DVariant v = Shader3DVariantByIndex(i);
                    if (!Shader3DVariantIsUserVisible(v))
                        continue;
                    const bool selected = !useDefault && Shader3DVariantId(v) == vc->shaderOverride;
                    if (ImGui::Selectable(Shader3DVariantLabel(v), selected)) {
                        vc->shaderOverride = Shader3DVariantId(v);
                        if (v == Shader3DVariant::SurfaceFX)
                            vc->hasSurfaceFx = true;
                        dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Scene default: %s", Shader3DVariantLabel(sceneDefault));

            if (current == Shader3DVariant::SurfaceFX) {
                vc->hasSurfaceFx = true;
                insp::SubHeading(ICON_FA_WAND_MAGIC_SPARKLES, "Surface FX",
                    "Detail texture scrolls (not the normal map)", false);
                std::string tex = vc->surfaceFxTexturePath;
                if (insp::AssetField(ICON_FA_IMAGE, "Detail", nullptr, tex, kTextureExts, IM_ARRAYSIZE(kTextureExts))) {
                    vc->surfaceFxTexturePath = tex;
                    dirty = true;
                }
                auto& fx = vc->surfaceFx;
                { insp::PropRow r(ICON_FA_PALETTE, "Color 1"); Mark(ImGui::ColorEdit4("##v", &fx.Color1.x), dirty); }
                { insp::PropRow r(ICON_FA_PALETTE, "Color 2"); Mark(ImGui::ColorEdit4("##v", &fx.Color2.x), dirty); }
                Mark(insp::DragVecN(ICON_FA_WIND, "Scroll 1", "Scroll speed 1", &fx.ScrollSpeed1.x, 2, 0.001f), dirty);
                Mark(insp::DragVecN(ICON_FA_WIND, "Scroll 2", "Scroll speed 2", &fx.ScrollSpeed2.x, 2, 0.001f), dirty);
                { insp::PropRow r(ICON_FA_EXPAND, "UV"); Mark(ImGui::DragFloat("##v", &fx.UvScale, 0.001f, 0.001f, 10.0f), dirty); }
                { insp::PropRow r(ICON_FA_CIRCLE_HALF_STROKE, "Fresnel"); Mark(ImGui::DragFloat("##v", &fx.FresnelPower, 0.05f, 0.1f, 16.0f), dirty); }
                { insp::PropRow r(ICON_FA_SUN, "Emission"); Mark(ImGui::DragFloat("##v", &fx.EmissionStrength, 0.01f, 0.0f, 5.0f), dirty); }
                { insp::PropRow r(nullptr, "Grad In", "Gradient start"); Mark(ImGui::DragFloat("##v", &fx.GradientStart, 0.01f, 0.0f, 1.0f), dirty); }
                { insp::PropRow r(nullptr, "Grad Out", "Gradient end"); Mark(ImGui::DragFloat("##v", &fx.GradientEnd, 0.01f, 0.0f, 1.0f), dirty); }
                { insp::PropRow r(nullptr, "Edge In", "Edge fade start"); Mark(ImGui::DragFloat("##v", &fx.EdgeFadeStart, 0.01f, 0.0f, 1.0f), dirty); }
                { insp::PropRow r(nullptr, "Edge Out", "Edge fade end"); Mark(ImGui::DragFloat("##v", &fx.EdgeFadeEnd, 0.01f, 0.0f, 1.0f), dirty); }
            }
        }

        // LOD
        {
            const char* lodHint = nullptr;
            if (vc->playAnimation && !vc->lods.empty())
                lodHint = "Ignored while Animate is on";
            else if (!vc->lods.empty() || vc->cullDistance > 0.0f)
                lodHint = "Play / runtime only (Rendering > Mesh LOD)";
            insp::SubHeading(ICON_FA_LAYER_GROUP, "LOD", lodHint, vc->playAnimation && !vc->lods.empty());

            auto sortLods = [vc]() {
                if (vc->lodScreenSize)
                    std::stable_sort(vc->lods.begin(), vc->lods.end(),
                        [](const VisualComponent::LodLevel& a, const VisualComponent::LodLevel& b) {
                            return a.screenHeight > b.screenHeight; });
                else
                    std::stable_sort(vc->lods.begin(), vc->lods.end(),
                        [](const VisualComponent::LodLevel& a, const VisualComponent::LodLevel& b) {
                            return a.maxDistance < b.maxDistance; });
            };
            insp::RowLabel(ICON_FA_RULER, "Mode", "LOD switch rule");
            static const char* const kModeIcons[] = { ICON_FA_RULER, ICON_FA_EXPAND };
            static const char* const kModeTips[] = { "Distance", "Screen" };
            int mode = vc->lodScreenSize ? 1 : 0;
            if (insp::ChoiceBar("lodmode", kModeIcons, kModeTips, 2, &mode)) {
                vc->lodScreenSize = (mode == 1);
                if (vc->lodScreenSize) {
                    // Seed unset rows so switching modes never leaves a level that cannot switch.
                    float h = 0.25f;
                    for (VisualComponent::LodLevel& l : vc->lods) {
                        if (l.screenHeight <= 0.0f) l.screenHeight = h;
                        h *= 0.5f;
                    }
                }
                sortLods();
                vc->lodLastLevel = -1;
                dirty = true;
            }

            const ImGuiStyle& st = ImGui::GetStyle();
            const float frameH = ImGui::GetFrameHeight();
            int removeIdx = -1;
            for (int i = 0; i < static_cast<int>(vc->lods.size()); ++i) {
                VisualComponent::LodLevel& lod = vc->lods[static_cast<size_t>(i)];
                ImGui::PushID(i);
                char word[16];
                std::snprintf(word, sizeof(word), "LOD %d", i + 1);
                const float fieldW = insp::RowLabel(nullptr, word,
                    vc->lodScreenSize ? "Switch below screen height" : "Switch at distance");
                ImGui::SetNextItemWidth((std::max)(1.0f, fieldW - frameH - st.ItemInnerSpacing.x));
                if (vc->lodScreenSize) {
                    float percent = lod.screenHeight * 100.0f;
                    if (ImGui::DragFloat("##LodScreen", &percent, 0.1f, 0.0f, 100.0f, "%.1f %%")) {
                        lod.screenHeight = std::min(std::max(percent, 0.0f), 100.0f) * 0.01f;
                        dirty = true;
                    }
                } else if (ImGui::DragFloat("##LodDist", &lod.maxDistance, 0.5f, 0.0f, 100000.0f, "%.1f m")) {
                    if (lod.maxDistance < 0.0f) lod.maxDistance = 0.0f;
                    dirty = true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                    sortLods();
                ImGui::SameLine(0.0f, st.ItemInnerSpacing.x);
                if (ImGui::Button(ICON_FA_XMARK "##rm", ImVec2(frameH, frameH)))
                    removeIdx = i;
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Remove");
                std::string path = lod.modelPath;
                if (insp::AssetField(ICON_FA_CUBE, "Mesh", nullptr, path, kModelExts, IM_ARRAYSIZE(kModelExts))) {
                    lod.modelPath = path;
                    dirty = true;
                }
                ImGui::PopID();
            }
            if (removeIdx >= 0) {
                vc->lods.erase(vc->lods.begin() + removeIdx);
                dirty = true;
            }
            if (static_cast<int>(vc->lods.size()) < kLodMaxLevels - 1 && insp::WideButton(ICON_FA_PLUS, "LOD")) {
                VisualComponent::LodLevel lod;
                lod.maxDistance = vc->lods.empty() ? 20.0f : vc->lods.back().maxDistance + 30.0f;
                lod.screenHeight = vc->lods.empty() ? 0.25f
                    : std::max(vc->lods.back().screenHeight * 0.5f, 0.005f);
                vc->lods.push_back(lod);
                dirty = true;
            }
            {
                insp::PropRow row(ICON_FA_EYE_SLASH, "Cull", "Hide beyond distance (0 = never)");
                if (ImGui::DragFloat("##v", &vc->cullDistance, 1.0f, 0.0f, 100000.0f,
                        vc->cullDistance > 0.0f ? "%.1f m" : "Never")) {
                    if (vc->cullDistance < 0.0f) vc->cullDistance = 0.0f;
                    dirty = true;
                }
            }
        }
    }
    s.Finish([n] { n->RemoveComponent<VisualComponent>(); });
}

// --------------------------------------------------------------------- Lights

void LightColorRows(DirectX::XMFLOAT3& color, float& intensity, DirectX::XMFLOAT3& spec,
                    float& specPower, bool& dirty) {
    { insp::PropRow r(ICON_FA_PALETTE, "Color"); Mark(ImGui::ColorEdit3("##v", &color.x), dirty); }
    { insp::PropRow r(ICON_FA_SUN, "Intensity"); Mark(ImGui::DragFloat("##v", &intensity, 0.05f, 0.0f, 10.0f), dirty); }
    { insp::PropRow r(ICON_FA_GEM, "Specular"); Mark(ImGui::ColorEdit3("##v", &spec.x), dirty); }
    { insp::PropRow r(ICON_FA_BULLSEYE, "Spec Pow", "Specular power"); Mark(ImGui::DragFloat("##v", &specPower, 1.0f, 1.0f, 256.0f), dirty); }
}

void DrawDirectionalLight(GameObject* n, DirectionalLightComponent* dl, bool& dirty) {
    Section s(n, "DirectionalLight", ICON_FA_SUN, "Directional", insp::Accent::Light, dl, &dl->enabled, dirty);
    if (s.Open()) {
        LightColorRows(dl->color, dl->intensity, dl->specColor, dl->specPower, dirty);
        insp::Toggle toggles[] = { { ICON_FA_CLOUD, "Shadows", &dl->castShadows } };
        Mark(insp::ToggleRow(toggles, 1), dirty);
        if (const engine::Transform* t = n->GetComponent<engine::Transform>())
            insp::ReadOnlyRow(ICON_FA_LOCATION_ARROW, "Dir", "Rotate Transform to aim (+Z)",
                Vec3Text(DirectionalLightComponent::ComputeDirectionWS(t)).c_str());
    }
    s.Finish([n] { n->RemoveComponent<DirectionalLightComponent>(); });
}

void DrawPointLight(GameObject* n, PointLightComponent* pl, bool& dirty) {
    Section s(n, "PointLight", ICON_FA_LIGHTBULB, "Point Light", insp::Accent::Light, pl, &pl->enabled, dirty);
    if (s.Open()) {
        LightColorRows(pl->color, pl->intensity, pl->specColor, pl->specPower, dirty);
        { insp::PropRow r(ICON_FA_RULER, "Range"); Mark(ImGui::DragFloat("##v", &pl->range, 0.1f, 0.1f, 100.0f), dirty); }
        { insp::PropRow r(ICON_FA_CHART_LINE, "Atten", "Attenuation kc, kl, kq"); Mark(ImGui::DragFloat3("##v", &pl->attenuation.x, 0.01f, 0.0f, 10.0f), dirty); }
    }
    s.Finish([n] { n->RemoveComponent<PointLightComponent>(); });
}

void DrawSpotLight(GameObject* n, SpotLightComponent* sl, bool& dirty) {
    Section s(n, "SpotLight", ICON_FA_LIGHTBULB, "Spot Light", insp::Accent::Light, sl, &sl->enabled, dirty);
    if (s.Open()) {
        LightColorRows(sl->color, sl->intensity, sl->specColor, sl->specPower, dirty);
        { insp::PropRow r(ICON_FA_RULER, "Range"); Mark(ImGui::DragFloat("##v", &sl->range, 0.1f, 0.1f, 100.0f), dirty); }
        { insp::PropRow r(ICON_FA_CIRCLE_DOT, "Inner", "Inner angle (deg)"); Mark(ImGui::DragFloat("##v", &sl->innerAngleDeg, 0.5f, 0.0f, 89.0f), dirty); }
        { insp::PropRow r(ICON_FA_CIRCLE, "Outer", "Outer angle (deg)"); Mark(ImGui::DragFloat("##v", &sl->outerAngleDeg, 0.5f, 0.0f, 89.0f), dirty); }
        { insp::PropRow r(ICON_FA_CHART_LINE, "Atten", "Attenuation kc, kl, kq"); Mark(ImGui::DragFloat3("##v", &sl->attenuation.x, 0.01f, 0.0f, 10.0f), dirty); }
        insp::Toggle toggles[] = { { ICON_FA_CLOUD, "Shadows", &sl->castShadows } };
        Mark(insp::ToggleRow(toggles, 1), dirty);
        if (const engine::Transform* t = n->GetComponent<engine::Transform>())
            insp::ReadOnlyRow(ICON_FA_LOCATION_ARROW, "Dir", "Rotate Transform to aim (+Z)",
                Vec3Text(LightGizmo::GetShineDirectionWS(t)).c_str());
    }
    s.Finish([n] { n->RemoveComponent<SpotLightComponent>(); });
}

// --------------------------------------------------------------------- Camera

void DrawCamera(GameObject* n, CameraComponent* cam, bool& dirty) {
    Section s(n, "Camera", ICON_FA_VIDEO, "Camera", insp::Accent::Camera, cam, &cam->enabled, dirty);
    if (s.Open()) {
        insp::RowLabel(ICON_FA_EYE, "View", "Projection");
        static const char* const kProjIcons[] = { ICON_FA_VIDEO, ICON_FA_BORDER_ALL };
        static const char* const kProjTips[] = { "Persp", "Ortho" };
        int proj = cam->orthographic ? 1 : 0;
        if (insp::ChoiceBar("proj", kProjIcons, kProjTips, 2, &proj)) {
            cam->orthographic = (proj == 1);
            dirty = true;
        }
        if (cam->orthographic) {
            insp::PropRow r(ICON_FA_ARROWS_UP_DOWN, "Height", "View height");
            Mark(ImGui::DragFloat("##v", &cam->orthographicHeight, 0.1f, 0.01f, 100000.0f), dirty);
        } else {
            insp::PropRow r(ICON_FA_CIRCLE_HALF_STROKE, "FOV", "Vertical FOV (deg)");
            Mark(ImGui::DragFloat("##v", &cam->fov, 0.5f, 1.0f, 179.0f), dirty);
        }
        { insp::PropRow r(ICON_FA_ANGLES_DOWN, "Near", "Near clip"); Mark(ImGui::DragFloat("##v", &cam->nearClip, 0.01f, 0.01f, 100.0f), dirty); }
        { insp::PropRow r(ICON_FA_ANGLES_UP, "Far", "Far clip"); Mark(ImGui::DragFloat("##v", &cam->farClip, 1.0f, 1.0f, 100000.0f), dirty); }
        insp::Toggle toggles[] = { { ICON_FA_GAMEPAD, "Fly", &cam->debugFly } };
        Mark(insp::ToggleRow(toggles, 1), dirty);
        if (const engine::Transform* t = n->GetComponent<engine::Transform>()) {
            const std::string up = "Up " + Vec3Text(CameraComponent::ComputeUpWS(t)) +
                "\nRight " + Vec3Text(CameraComponent::ComputeRightWS(t)) +
                "\nAim with Transform (+Z)";
            insp::ReadOnlyRow(ICON_FA_LOCATION_ARROW, "Fwd", up.c_str(),
                Vec3Text(CameraComponent::ComputeForwardWS(t)).c_str());
        }
    }
    s.Finish([n] { n->RemoveComponent<CameraComponent>(); });
}

// ------------------------------------------------------------------ Collider

void DrawCollider(GameObject* n, ColliderComponent* cc, bool& dirty) {
    static const char* const kNames[] = { "Collider", "Box Collider", "Sphere Collider",
        "Capsule Collider", "Mesh Collider", "Cylinder Collider" };
    static const char* const kIcons[] = { ICON_FA_SHAPES, ICON_FA_VECTOR_SQUARE, ICON_FA_CIRCLE,
        ICON_FA_CAPSULES, ICON_FA_DRAW_POLYGON, ICON_FA_DATABASE };
    const int idx = static_cast<int>(cc->colliderType);
    const bool known = idx >= 0 && idx < IM_ARRAYSIZE(kNames);
    const char* type = cc->GetTypeName();
    Section s(n, type, known ? kIcons[idx] : kIcons[0], known ? kNames[idx] : kNames[0],
        insp::Accent::Physics, cc, nullptr, dirty);
    if (s.Open()) {
        if (cc->colliderType != ColliderType::NONE)
            Mark(insp::DragVecN(ICON_FA_CROSSHAIRS, "Center", nullptr, &cc->colliderCenter.x, 3, 0.1f,
                0.0f, 0.0f, 0.0f, 0.1f * g_scrollSensitivity), dirty);
        switch (cc->colliderType) {
        case ColliderType::BOX:
            Mark(insp::DragVecN(ICON_FA_EXPAND, "Half", "Half extents", &cc->halfExtents.x, 3, 0.1f), dirty);
            break;
        case ColliderType::SPHERE:
            { insp::PropRow r(ICON_FA_CIRCLE, "Radius"); Mark(ImGui::DragFloat("##v", &cc->radius, 0.1f), dirty); }
            break;
        case ColliderType::CAPSULE:
        case ColliderType::CYLINDER:
            { insp::PropRow r(ICON_FA_CIRCLE, "Radius"); Mark(ImGui::DragFloat("##v", &cc->radius, 0.1f), dirty); }
            { insp::PropRow r(ICON_FA_ARROWS_UP_DOWN, "Height"); Mark(ImGui::DragFloat("##v", &cc->height, 0.1f), dirty); }
            break;
        case ColliderType::MESH:
            { insp::PropRow r(ICON_FA_EXPAND, "Padding", "Mesh size padding"); Mark(ImGui::DragFloat("##v", &cc->meshSizePadding, 0.01f, 0.0f, 10.0f), dirty); }
            break;
        default:
            break;
        }

        insp::Toggle toggles[] = {
            { ICON_FA_EYE, "Debug", &cc->showDebugCollider },
            { ICON_FA_GHOST, "Trigger", &cc->isTrigger },
        };
        if (insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles))) {
            n->m_debugCollider = cc->showDebugCollider;
            dirty = true;
        }

        if (cc->colliderType == ColliderType::BOX || cc->colliderType == ColliderType::SPHERE ||
            cc->colliderType == ColliderType::CAPSULE || cc->colliderType == ColliderType::CYLINDER) {
            static GameObject* s_autoFitFailNode = nullptr;
            const bool failed = (s_autoFitFailNode == n);
            const float frameH = ImGui::GetFrameHeight();
            const float sp = ImGui::GetStyle().ItemInnerSpacing.x;
            const float w = failed ? (std::max)(1.0f, ImGui::GetContentRegionAvail().x - frameH - sp) : -FLT_MIN;
            if (ImGui::Button(ICON_FA_WAND_MAGIC_SPARKLES "  Fit", ImVec2(w, 0.0f))) {
                if (AutoFitColliderToMesh(n, cc)) {
                    s_autoFitFailNode = nullptr;
                    dirty = true;
                    ImGui::MarkItemEdited(ImGui::GetItemID());
                } else {
                    s_autoFitFailNode = n;
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Auto-fit to mesh");
            if (failed) {
                ImGui::SameLine(0.0f, sp);
                insp::HintIcon(ICON_FA_TRIANGLE_EXCLAMATION, insp::WarningColor(), "No usable mesh vertices");
            }
        }

        static const char* const kPhysMatExts[] = { ".physmat" };
        std::string mat = cc->physicsMaterial;
        if (insp::AssetField(ICON_FA_DROPLET, "PhysMat", "Physics material (.physmat)", mat,
                kPhysMatExts, IM_ARRAYSIZE(kPhysMatExts))) {
            cc->physicsMaterial = mat;
            dirty = true;
        }
        if (Collider* col = n->GetCollider()) {
            const ContactSurface& surface = col->GetContactSurface();
            if (surface.assigned) {
                char buf[96];
                std::snprintf(buf, sizeof(buf), "%.2f / %.2f / %.2f",
                    surface.staticFriction, surface.dynamicFriction, surface.restitution);
                insp::ReadOnlyRow(nullptr, "Surface", "Static / dynamic friction / restitution", buf);
            } else if (!cc->physicsMaterial.empty()) {
                const ImVec4 warn = insp::WarningColor();
                insp::ReadOnlyRow(nullptr, "Surface", "Using rigidbody friction / restitution",
                    ICON_FA_TRIANGLE_EXCLAMATION " Not loaded", &warn);
            }
        }
    }
    s.Finish([n] { RemoveColliderComponent(n); });
}

// ----------------------------------------------------------------- Rigidbody

void DrawRigidbody(GameObject* n, RigidbodyComponent* rbc, bool& dirty) {
    Section s(n, "Rigidbody", ICON_FA_WEIGHT_HANGING, "Rigidbody", insp::Accent::Physics, rbc, nullptr, dirty);
    if (s.Open()) {
        Rigidbody& rb = rbc->Body();
        bool useGravity = rb.UseGravity();
        bool isKinematic = rb.IsKinematic();
        insp::Toggle toggles[] = {
            { ICON_FA_ARROW_DOWN, "Gravity", &useGravity },
            { ICON_FA_HAND, "Kinematic", &isKinematic },
        };
        if (insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles))) {
            rb.SetUseGravity(useGravity);
            rb.SetIsKinematic(isKinematic);
            dirty = true;
        }
        auto row = [&](const char* icon, const char* word, const char* tip, float value, float speed,
                       float lo, float hi, void (Rigidbody::*set)(float)) {
            insp::PropRow r(icon, word, tip);
            if (ImGui::DragFloat("##v", &value, speed, lo, hi)) {
                (rb.*set)(value);
                dirty = true;
            }
        };
        row(ICON_FA_WEIGHT_HANGING, "Mass", nullptr, rb.GetMass(), 0.05f, 0.001f, 1000.0f, &Rigidbody::SetMass);
        row(ICON_FA_WIND, "Drag", nullptr, rb.GetDrag(), 0.01f, 0.0f, 50.0f, &Rigidbody::SetDrag);
        row(ICON_FA_ROTATE, "Ang Drag", "Angular drag", rb.GetAngularDrag(), 0.01f, 0.0f, 50.0f, &Rigidbody::SetAngularDrag);
        row(ICON_FA_ARROWS_UP_DOWN, "Bounce", "Restitution", rb.GetRestitution(), 0.01f, 0.0f, 2.0f, &Rigidbody::SetRestitution);
        row(ICON_FA_HAND, "Friction", nullptr, rb.GetFriction(), 0.01f, 0.0f, 10.0f, &Rigidbody::SetFriction);
    }
    s.Finish([n] { n->RemoveComponent<RigidbodyComponent>(); });
}

// ------------------------------------------------------------------ Material

void DrawMaterial(GameObject* n, MaterialComponent* mc, bool& dirty) {
    Section s(n, "Material", ICON_FA_FILL_DRIP, "Material", insp::Accent::Render, mc, nullptr, dirty);
    if (s.Open()) {
        { insp::PropRow r(ICON_FA_PALETTE, "Color"); Mark(ImGui::ColorEdit3("##v", &mc->color.x), dirty); }
        { insp::PropRow r(ICON_FA_CIRCLE_HALF_STROKE, "Rough", "Roughness"); Mark(ImGui::DragFloat("##v", &mc->roughness, 0.01f, 0.0f, 1.0f), dirty); }
        { insp::PropRow r(ICON_FA_GEM, "Metal", "Metallic"); Mark(ImGui::DragFloat("##v", &mc->metallic, 0.01f, 0.0f, 1.0f), dirty); }
        std::string normal = mc->normalMapPath;
        if (insp::AssetField(ICON_FA_IMAGE, "Normal", "Normal map", normal, kTextureExts, IM_ARRAYSIZE(kTextureExts))) {
            mc->normalMapPath = normal;
            dirty = true;
        }
        { insp::PropRow r(ICON_FA_GAUGE, "Strength", "Normal strength"); Mark(ImGui::DragFloat("##v", &mc->normalStrength, 0.05f, 0.0f, 4.0f), dirty); }

        VisualComponent* vc = n->GetComponent<VisualComponent>();
        const bool isToon = vc && (vc->shaderOverride == "toon" ||
            (vc->shaderOverride.empty() && RenderSettings_GetDefaultShader() == Shader3DVariant::Toon));
        bool showToon = isToon;
        if (isToon)
            insp::SubHeading(ICON_FA_PAINTBRUSH, "Toon");
        else
            showToon = ImGui::TreeNodeEx(ICON_FA_PAINTBRUSH "  Toon", ImGuiTreeNodeFlags_NoTreePushOnOpen);
        if (showToon) {
            { insp::PropRow r(ICON_FA_LAYER_GROUP, "Steps", "Shade steps"); Mark(ImGui::DragFloat("##v", &mc->shadeSteps, 0.1f, 1.0f, 8.0f), dirty); }
            { insp::PropRow r(ICON_FA_WAVE_SQUARE, "Smooth", "Shade smoothness"); Mark(ImGui::DragFloat("##v", &mc->shadeSmoothness, 0.005f, 0.0f, 0.5f), dirty); }
            { insp::PropRow r(ICON_FA_GEM, "Spec", "Specular threshold"); Mark(ImGui::DragFloat("##v", &mc->specularThreshold, 0.01f, 0.0f, 1.0f), dirty); }
            { insp::PropRow r(ICON_FA_CIRCLE_HALF_STROKE, "Rim", "Rim strength"); Mark(ImGui::DragFloat("##v", &mc->rimStrength, 0.01f, 0.0f, 2.0f), dirty); }
            { insp::PropRow r(ICON_FA_CLOUD, "Shadow", "Shadow tint"); Mark(ImGui::ColorEdit3("##v", &mc->shadowTint.x), dirty); }
            { insp::PropRow r(ICON_FA_PEN, "Outline", "Outline width"); Mark(ImGui::DragFloat("##v", &mc->outlineWidth, 0.0005f, 0.0f, 0.05f, "%.4f"), dirty); }
            { insp::PropRow r(ICON_FA_PALETTE, "Line Col", "Outline color"); Mark(ImGui::ColorEdit4("##v", &mc->outlineColor.x), dirty); }
            std::string ramp = mc->rampTexturePath;
            if (insp::AssetField(ICON_FA_IMAGE, "Ramp", "Ramp texture (optional)", ramp, kTextureExts, IM_ARRAYSIZE(kTextureExts))) {
                mc->rampTexturePath = ramp;
                dirty = true;
            }
        }
    }
    s.Finish([n] { n->RemoveComponent<MaterialComponent>(); });
}

// -------------------------------------------------------------------- Effect

void DrawEffect(GameObject* n, EffectComponent* ec, bool& dirty) {
    Section s(n, "Effect", ICON_FA_WAND_MAGIC_SPARKLES, "Effect", insp::Accent::Effect, ec, nullptr, dirty);
    if (s.Open()) {
        static const char* const kEffectExts[] = { ".efk", ".efkefc" };
        std::string path = ec->effectPath;
        if (insp::AssetField(ICON_FA_WAND_MAGIC_SPARKLES, "File", "Effekseer file", path, kEffectExts, IM_ARRAYSIZE(kEffectExts))) {
            ec->SetEffectPath(path.c_str());
            dirty = true;
        }
        { insp::PropRow r(ICON_FA_GAUGE_HIGH, "Speed"); Mark(ImGui::SliderFloat("##v", &ec->speed, 0.0f, 5.0f), dirty); }
        insp::Toggle toggles[] = {
            { ICON_FA_PLAY, "Auto Play", &ec->autoPlay },
            { ICON_FA_TRASH_CAN, "Auto Destroy", &ec->autoDestroy },
        };
        Mark(insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles)), dirty);
        static const char* const kIcons[] = { ICON_FA_PLAY, ICON_FA_STOP };
        static const char* const kTips[] = { "Play", "Stop" };
        switch (insp::IconButtonRow("fxplay", kIcons, kTips, 2)) {
        case 0: ec->Play(); break;
        case 1: ec->Stop(); break;
        default: break;
        }
    }
    s.Finish([n] { n->RemoveComponent<EffectComponent>(); });
}

// --------------------------------------------------------------------- Audio

void DrawAudio(GameObject* n, AudioComponent* ac, bool& dirty) {
    Section s(n, "Audio", ICON_FA_VOLUME_HIGH, "Audio", insp::Accent::Audio, ac, nullptr, dirty);
    if (s.Open()) {
        static const char* const kWavExts[] = { ".wav" };
        std::string clip = ac->clipPath;
        if (insp::AssetField(ICON_FA_FILE_AUDIO, "Clip", "WAV file", clip, kWavExts, IM_ARRAYSIZE(kWavExts))) {
            ac->SetClipPath(clip);
            dirty = true;
        }
        insp::RowLabel(ICON_FA_SLIDERS, "Bus", nullptr);
        static const char* const kBusIcons[] = { ICON_FA_MUSIC, ICON_FA_BOLT, ICON_FA_MICROPHONE };
        static const char* const kBusTips[] = { "BGM", "SFX", "Voice" };
        int bus = ac->bus == AudioType::BGM ? 0 : (ac->bus == AudioType::VOICE ? 2 : 1);
        if (insp::ChoiceBar("bus", kBusIcons, kBusTips, 3, &bus)) {
            ac->bus = bus == 0 ? AudioType::BGM : (bus == 2 ? AudioType::VOICE : AudioType::SFX);
            dirty = true;
        }
        {
            insp::PropRow r(ICON_FA_VOLUME_HIGH, "Volume");
            if (ImGui::SliderFloat("##v", &ac->volume, 0.0f, 1.0f, "%.2f")) {
                ac->ApplyLiveSettings();
                dirty = true;
            }
        }
        insp::Toggle toggles[] = {
            { ICON_FA_REPEAT, "Loop", &ac->loop },
            { ICON_FA_PLAY, "Awake", &ac->autoPlay },
            { ICON_FA_CUBE, "3D", &ac->spatial },
        };
        Mark(insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles)), dirty);
        if (ac->spatial) {
            insp::PropRow r(ICON_FA_RULER, "Range", "Distance scale");
            if (ImGui::DragFloat("##v", &ac->distanceScale, 0.5f, 0.1f, 500.0f, "%.1f")) {
                ac->ApplyLiveSettings();
                dirty = true;
            }
        }
        static const char* const kIcons[] = { ICON_FA_PLAY, ICON_FA_STOP };
        static const char* const kTips[] = { "Play", "Stop" };
        switch (insp::IconButtonRow("audioplay", kIcons, kTips, 2)) {
        case 0: ac->Play(); break;
        case 1: ac->Stop(); break;
        default: break;
        }
    }
    s.Finish([n] { n->RemoveComponent<AudioComponent>(); });
}

void DrawAudioListener(GameObject* n, AudioListenerComponent* alc, bool& dirty) {
    Section s(n, "AudioListener", ICON_FA_EAR_LISTEN, "Listener", insp::Accent::Audio, alc, nullptr,
        dirty, true, false);
    s.Finish([n] { n->RemoveComponent<AudioListenerComponent>(); });
}

// -------------------------------------------------------------------- Script

void DrawScript(GameObject* n, ScriptComponent* sc, bool& dirty) {
    const std::string cls = sc->GetScriptClass();
    Section s(n, "Script", ICON_FA_CODE, cls.empty() ? "Script" : cls.c_str(), insp::Accent::Script, sc,
        nullptr, dirty, true, false);
    if (s.Open()) {
        if (Script* script = sc->GetScript()) {
            (void)script;
            nlohmann::json props = sc->GetEditableProperties();
            if (props.is_object() && !props.empty()) {
                bool changed = false;
                for (auto& [key, val] : props.items())
                    changed |= DrawScriptPropertyField(n, key, val);
                if (changed) {
                    sc->ApplyProperties(props);
                    dirty = true;
                }
            }
        } else {
            const ImVec4 warn = insp::WarningColor();
            insp::ReadOnlyRow(ICON_FA_TRIANGLE_EXCLAMATION, "State", "Script class not in GameScripts",
                "Not loaded", &warn);
        }
    }
    s.Finish([n] { n->RemoveComponent<ScriptComponent>(); });
}

// ------------------------------------------------------------------------ 2D

void DrawRigidbody2D(GameObject* n, Rigidbody2DComponent* body, bool& dirty) {
    const bool playing = EngineGUI_IsPlaying();
    Section s(n, "Rigidbody2D", ICON_FA_WEIGHT_HANGING, "Rigidbody 2D", insp::Accent::Physics, body,
        nullptr, dirty, !playing, !playing);
    if (s.Open()) {
        ImGui::BeginDisabled(playing);
        insp::RowLabel(ICON_FA_SHAPES, "Type", "Dynamic / kinematic: root, uniform +XY scale");
        static const char* const kIcons[] = { ICON_FA_LOCK, ICON_FA_FEATHER, ICON_FA_HAND };
        static const char* const kTips[] = { "Static", "Dynamic", "Kinematic" };
        int type = static_cast<int>(body->bodyType);
        if (insp::ChoiceBar("type", kIcons, kTips, 3, &type)) {
            body->bodyType = static_cast<BodyType2D>(type);
            dirty = true;
        }
        { insp::PropRow r(ICON_FA_WEIGHT_HANGING, "Mass"); Mark(ImGui::DragFloat("##v", &body->mass, 0.01f, 0.001f, 100000.0f), dirty); }
        { insp::PropRow r(ICON_FA_ARROW_DOWN, "Gravity", "Gravity scale"); Mark(ImGui::DragFloat("##v", &body->gravityScale, 0.01f, -100.0f, 100.0f), dirty); }
        { insp::PropRow r(ICON_FA_WIND, "Damping", "Linear damping"); Mark(ImGui::DragFloat("##v", &body->linearDamping, 0.01f, 0.0f, 100.0f), dirty); }
        Mark(insp::DragVecN(ICON_FA_ARROW_TREND_UP, "Velocity", "Initial velocity", &body->initialVelocity.x, 2, 0.01f), dirty);
        insp::Toggle toggles[] = {
            { ICON_FA_LOCK, "Fix Rot", &body->fixedRotation },
            { ICON_FA_BOLT, "CCD", &body->bullet },
        };
        Mark(insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles)), dirty);
        ImGui::EndDisabled();
    }
    s.Finish([n] { n->RemoveComponent<Rigidbody2DComponent>(); });
}

void DrawCollider2D(GameObject* n, Collider2DComponent* collider, bool& dirty) {
    const bool playing = EngineGUI_IsPlaying();
    Section s(n, "Collider2D", collider->shape == Shape2D::Box ? ICON_FA_SQUARE : ICON_FA_CIRCLE,
        "Collider 2D", insp::Accent::Physics, collider, playing ? nullptr : &collider->enabled, dirty,
        !playing, !playing);
    if (s.Open()) {
        if (playing) {
            insp::Toggle toggles[] = { { ICON_FA_EYE, "Debug", &collider->showDebugCollider } };
            Mark(insp::ToggleRow(toggles, 1), dirty);
        } else {
            insp::Toggle toggles[] = {
                { ICON_FA_EYE, "Debug", &collider->showDebugCollider },
                { ICON_FA_GHOST, "Trigger", &collider->isTrigger },
            };
            Mark(insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles)), dirty);
        }
        ImGui::BeginDisabled(playing);
        insp::RowLabel(ICON_FA_SHAPES, "Shape", "Local units; uniform +XY scale");
        static const char* const kIcons[] = { ICON_FA_SQUARE, ICON_FA_CIRCLE };
        static const char* const kTips[] = { "Box", "Circle" };
        int shape = static_cast<int>(collider->shape);
        if (insp::ChoiceBar("shape", kIcons, kTips, 2, &shape)) {
            collider->shape = static_cast<Shape2D>(shape);
            dirty = true;
        }
        if (collider->shape == Shape2D::Box)
            Mark(insp::DragVecN(ICON_FA_EXPAND, "Size", nullptr, &collider->size.x, 2, 0.01f, 0.001f, 100000.0f, 1.0f), dirty);
        else {
            insp::PropRow r(ICON_FA_CIRCLE, "Radius");
            Mark(ImGui::DragFloat("##v", &collider->radius, 0.01f, 0.001f, 100000.0f), dirty);
        }
        Mark(insp::DragVecN(ICON_FA_CROSSHAIRS, "Offset", nullptr, &collider->offset.x, 2, 0.01f), dirty);
        { insp::PropRow r(ICON_FA_HAND, "Friction"); Mark(ImGui::DragFloat("##v", &collider->friction, 0.01f, 0.0f, 1.0f), dirty); }
        { insp::PropRow r(ICON_FA_ARROWS_UP_DOWN, "Bounce", "Restitution"); Mark(ImGui::DragFloat("##v", &collider->restitution, 0.01f, 0.0f, 1.0f), dirty); }
        { insp::PropRow r(ICON_FA_HASHTAG, "Category", "Category bits (hex)");
          Mark(ImGui::InputScalar("##v", ImGuiDataType_U32, &collider->categoryBits, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal), dirty); }
        { insp::PropRow r(ICON_FA_FILTER, "Mask", "Mask bits (hex)");
          Mark(ImGui::InputScalar("##v", ImGuiDataType_U32, &collider->maskBits, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal), dirty); }
        ImGui::EndDisabled();
    }
    s.Finish([n] { n->RemoveComponent<Collider2DComponent>(); });
}

bool PivotRow(DirectX::XMFLOAT2& pivot, const void* owner) {
    static const char* names[] = { "Center", "Top Left", "Top", "Top Right", "Left", "Right",
        "Bottom Left", "Bottom", "Bottom Right", "Custom" };
    static const DirectX::XMFLOAT2 values[] = { {.5f, .5f}, {0, 1}, {.5f, 1}, {1, 1},
        {0, .5f}, {1, .5f}, {0, 0}, {.5f, 0}, {1, 0} };
    static const void* lastOwner = nullptr;
    static bool custom = false;
    if (lastOwner != owner) { lastOwner = owner; custom = false; }
    int current = 9;
    for (int i = 0; i < 9; ++i)
        if (std::fabs(pivot.x - values[i].x) < .00001f && std::fabs(pivot.y - values[i].y) < .00001f) { current = i; break; }
    if (custom) current = 9;
    bool changed = false;
    {
        insp::PropRow r(ICON_FA_CROSSHAIRS, "Pivot");
        if (ImGui::Combo("##v", &current, names, IM_ARRAYSIZE(names))) {
            custom = current == 9;
            if (!custom) { pivot = values[current]; changed = true; }
        }
    }
    if (current == 9)
        changed |= insp::DragVecN(nullptr, "Custom", "Pivot (0-1, bottom-left)", &pivot.x, 2, 0.01f, 0.0f, 1.0f, 0.5f);
    return changed;
}

void DrawSpriteRenderer(GameObject* n, SpriteRendererComponent* sprite, bool& dirty) {
    Section s(n, "SpriteRenderer", ICON_FA_IMAGE, "Sprite", insp::Accent::Render, sprite, nullptr, dirty);
    if (s.Open()) {
        static const char* const kExts[] = { ".png", ".jpg", ".jpeg", ".bmp" };
        std::string tex = sprite->texturePath;
        if (insp::AssetField(ICON_FA_IMAGE, "Texture", nullptr, tex, kExts, IM_ARRAYSIZE(kExts))) {
            sprite->texturePath = tex;
            dirty = true;
        }
        { insp::PropRow r(ICON_FA_RULER_COMBINED, "PPU", "Pixels per unit"); Mark(ImGui::DragFloat("##v", &sprite->pixelsPerUnit, 1.0f, 0.01f, 100000.0f), dirty); }
        { insp::PropRow r(ICON_FA_BORDER_ALL, "Rect", "Source x, y, w, h (px); 0 size = full");
          Mark(ImGui::DragFloat4("##v", &sprite->sourceRect.x, 1.0f, 0.0f, 100000.0f), dirty); }
        Mark(PivotRow(sprite->pivot, sprite), dirty);
        { insp::PropRow r(ICON_FA_PALETTE, "Tint"); Mark(ImGui::ColorEdit4("##v", &sprite->tint.x), dirty); }
        insp::Toggle toggles[] = {
            { ICON_FA_ARROWS_LEFT_RIGHT, "Flip X", &sprite->flipX },
            { ICON_FA_ARROWS_UP_DOWN, "Flip Y", &sprite->flipY },
        };
        Mark(insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles)), dirty);
        { insp::PropRow r(ICON_FA_LAYER_GROUP, "Layer", "Sorting layer"); Mark(ImGui::InputInt("##v", &sprite->sortingLayer), dirty); }
        { insp::PropRow r(ICON_FA_SORT, "Order", "Order in layer"); Mark(ImGui::InputInt("##v", &sprite->orderInLayer), dirty); }
    }
    s.Finish([n] { n->RemoveComponent<SpriteRendererComponent>(); });
}

// Text field + icon button on one row. Returns true when the button was pressed.
bool TextWithButton(const char* icon, const char* word, const char* tip, char* buf, size_t size,
                    const char* buttonIcon, const char* buttonTip) {
    ImGui::PushID(word);
    const float frameH = ImGui::GetFrameHeight();
    const float sp = ImGui::GetStyle().ItemInnerSpacing.x;
    const float fieldW = insp::RowLabel(icon, word, tip);
    ImGui::SetNextItemWidth((std::max)(1.0f, fieldW - frameH - sp));
    ImGui::InputText("##v", buf, size);
    ImGui::SameLine(0.0f, sp);
    char id[64];
    std::snprintf(id, sizeof(id), "%s##btn", buttonIcon);
    const bool pressed = ImGui::Button(id, ImVec2(frameH, frameH));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", buttonTip);
    ImGui::PopID();
    return pressed;
}

void DrawSpriteAnimator(GameObject* n, SpriteAnimatorComponent* animator, bool& dirty) {
    Section s(n, "SpriteAnimator", ICON_FA_FILM, "Animator", insp::Accent::Render, animator, nullptr,
        dirty, true, false);
    if (s.Open()) {
        bool changed = false;
        {
            char defaultName[256];
            strncpy_s(defaultName, animator->defaultClip.c_str(), _TRUNCATE);
            insp::PropRow r(ICON_FA_STAR, "Default", "Default clip");
            if (ImGui::InputText("##v", defaultName, sizeof(defaultName))) {
                animator->defaultClip = defaultName;
                changed = true;
            }
        }
        insp::Toggle toggles[] = { { ICON_FA_PLAY, "Auto Play", &animator->autoPlay } };
        changed |= insp::ToggleRow(toggles, 1);

        static char newClipName[256] = "idle";
        if (TextWithButton(ICON_FA_PLUS, "New", "New clip name", newClipName, sizeof(newClipName),
                ICON_FA_PLUS, "Add clip") && newClipName[0])
            changed |= animator->clips.emplace(newClipName, SpriteAnimationClip{}).second;

        static std::string metadataPath;
        static char importName[256] = "idle";
        static bool importFailed = false;
        static const char* const kJsonExts[] = { ".json" };
        insp::AssetField(ICON_FA_FILE_CODE, "Sheet", "Sheet metadata JSON", metadataPath, kJsonExts, 1);
        if (TextWithButton(ICON_FA_FILE_IMPORT, "Import", "Import clip name", importName, sizeof(importName),
                ICON_FA_FILE_IMPORT, "Import sheet")) {
            const bool imported = animator->ImportSheet(metadataPath.c_str(), importName);
            importFailed = !imported;
            changed |= imported;
        }
        if (importFailed) {
            const ImVec4 err = insp::ErrorColor();
            insp::ReadOnlyRow(ICON_FA_TRIANGLE_EXCLAMATION, "Import", "Check metadata path and frame rects",
                "Failed", &err);
        }

        static const char* const kTexExts[] = { ".png", ".jpg", ".jpeg", ".bmp" };
        const float frameH = ImGui::GetFrameHeight();
        const float sp = ImGui::GetStyle().ItemInnerSpacing.x;
        for (auto it = animator->clips.begin(); it != animator->clips.end();) {
            ImGui::PushID(it->first.c_str());
            bool removeClip = false;
            const std::string shown = insp::Ellipsize(it->first,
                (std::max)(1.0f, ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 3.0f));
            if (ImGui::TreeNodeEx("clip", ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s  %s", ICON_FA_FILM, shown.c_str())) {
                auto& clip = it->second;
                std::string tex = clip.texture;
                if (insp::AssetField(ICON_FA_IMAGE, "Texture", nullptr, tex, kTexExts, IM_ARRAYSIZE(kTexExts))) {
                    clip.texture = tex;
                    changed = true;
                }
                float fps = static_cast<float>(clip.fps);
                {
                    insp::PropRow r(ICON_FA_STOPWATCH, "FPS");
                    if (ImGui::DragFloat("##v", &fps, 0.1f, 0.01f, 1000.0f)) {
                        clip.fps = (std::max)(0.01f, fps);
                        changed = true;
                    }
                }
                insp::Toggle loop[] = { { ICON_FA_REPEAT, "Loop", &clip.loop } };
                changed |= insp::ToggleRow(loop, 1);
                for (size_t i = 0; i < clip.frames.size();) {
                    ImGui::PushID(static_cast<int>(i));
                    auto& frame = clip.frames[i];
                    float rect[] = { frame.x, frame.y, frame.w, frame.h };
                    char word[16];
                    std::snprintf(word, sizeof(word), "#%zu", i + 1);
                    const float fieldW = insp::RowLabel(nullptr, word, "Frame x, y, w, h (px)");
                    ImGui::SetNextItemWidth((std::max)(1.0f, fieldW - frameH - sp));
                    if (ImGui::DragFloat4("##f", rect, 1.0f, 0.0f, 100000.0f)) {
                        frame = { rect[0], rect[1], (std::max)(1.0f, rect[2]), (std::max)(1.0f, rect[3]) };
                        changed = true;
                    }
                    ImGui::SameLine(0.0f, sp);
                    const bool removeFrame = ImGui::Button(ICON_FA_XMARK "##rm", ImVec2(frameH, frameH));
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                        ImGui::SetTooltip("Remove frame");
                    ImGui::PopID();
                    if (removeFrame) {
                        clip.frames.erase(clip.frames.begin() + i);
                        changed = true;
                    } else ++i;
                }
                static const char* const kIcons[] = { ICON_FA_PLUS, ICON_FA_TRASH_CAN };
                static const char* const kTips[] = { "Frame", "Delete clip" };
                switch (insp::IconButtonRow("clipops", kIcons, kTips, 2)) {
                case 0: clip.frames.push_back(SpriteAnimationFrame{}); changed = true; break;
                case 1: removeClip = true; break;
                default: break;
                }
            }
            ImGui::PopID();
            if (removeClip) {
                if (animator->defaultClip == it->first) animator->defaultClip.clear();
                it = animator->clips.erase(it);
                changed = true;
            } else ++it;
        }
        if (changed) {
            animator->OnReset();
            dirty = true;
        }
    }
    s.Finish([n] { n->RemoveComponent<SpriteAnimatorComponent>(); });
}

// ----------------------------------------------------------------- Billboard

void DrawBillboard(GameObject* n, BillboardComponent* bbc, bool& dirty) {
    Section s(n, "Billboard", ICON_FA_IMAGE, "Billboard", insp::Accent::Render, bbc, &bbc->enabled, dirty);
    if (s.Open()) {
        std::string tex = bbc->texturePath;
        if (insp::AssetField(ICON_FA_IMAGE, "Texture", nullptr, tex, kTextureExts, IM_ARRAYSIZE(kTextureExts))) {
            bbc->texturePath = tex;
            bbc->InvalidateTextureCache();
            dirty = true;
        }
        float size[2] = { bbc->sizeX, bbc->sizeY };
        if (insp::DragVecN(ICON_FA_EXPAND, "Size", nullptr, size, 2, 0.05f, 0.01f, 1000.0f, 1.0f)) {
            bbc->sizeX = size[0];
            bbc->sizeY = size[1];
            dirty = true;
        }
        { insp::PropRow r(ICON_FA_PALETTE, "Color"); Mark(ImGui::ColorEdit4("##v", &bbc->color.x), dirty); }
        { insp::PropRow r(ICON_FA_ROTATE, "Rot", "Rotation (deg)"); Mark(ImGui::DragFloat("##v", &bbc->rotation, 0.5f), dirty); }
        insp::Toggle toggles[] = { { ICON_FA_MAP, "Ground", &bbc->groundAligned } };
        Mark(insp::ToggleRow(toggles, 1), dirty);
    }
    s.Finish([n] { n->RemoveComponent<BillboardComponent>(); });
}

// ------------------------------------------------------------------- Terrain

void DrawTerrain(GameObject* n, TerrainComponent* tc, bool& dirty) {
    terrain::Settings& ts = tc->settings;
    Section s(n, "Terrain", ICON_FA_MOUNTAIN, "Terrain", insp::Accent::Render, tc, &ts.enabled, dirty,
        true, false);
    if (s.Open()) {
        static const char* const kHmExts[] = { ".r16" };
        std::string hm = ts.heightmap;
        if (insp::AssetField(ICON_FA_MOUNTAIN, "Height", ".r16 heightmap; empty = flat", hm, kHmExts, 1)) {
            ts.heightmap = hm;
            tc->InvalidateHeightmap();
            dirty = true;
        }
        tc->EnsureLoaded();
        {
            char info[96] = "";
            const ImVec4 err = insp::ErrorColor();
            const ImVec4* color = nullptr;
            const char* tip = "Reload";
            if (tc->IsLoaded()) {
                const terrain::ChunkGrid& grid = tc->GetChunkGrid();
                std::snprintf(info, sizeof(info), "%d\xC2\xB2 \xC2\xB7 %d\xC2\xB2 x %d",
                    tc->GetHeightmap().resolution, grid.chunksPerSide, grid.chunkQuads);
                tip = "Samples / chunks x cells";
            } else if (!tc->GetLoadError().empty()) {
                std::snprintf(info, sizeof(info), "%s", tc->GetLoadError().c_str());
                color = &err;
            }
            const float frameH = ImGui::GetFrameHeight();
            const float sp = ImGui::GetStyle().ItemInnerSpacing.x;
            const float fieldW = insp::RowLabel(ICON_FA_TABLE_CELLS, "Grid", tip);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float textW = (std::max)(1.0f, fieldW - frameH - sp);
            ImGui::Dummy(ImVec2(textW, frameH));
            if (info[0] && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", info);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->PushClipRect(p, ImVec2(p.x + textW, p.y + frameH), true);
            dl->AddText(ImVec2(p.x, p.y + ImGui::GetStyle().FramePadding.y),
                color ? ImGui::ColorConvertFloat4ToU32(*color) : ImGui::GetColorU32(ImGuiCol_TextDisabled), info);
            dl->PopClipRect();
            ImGui::SameLine(0.0f, sp);
            if (ImGui::Button(ICON_FA_ARROWS_ROTATE "##reload", ImVec2(frameH, frameH)))
                tc->InvalidateHeightmap();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Reload heightmap");
        }

        insp::SubHeading(ICON_FA_RULER_COMBINED, "Shape", "Position moves it; rotation / scale ignored", false);
        {
            float size[2] = { ts.layout.sizeX, ts.layout.sizeZ };
            if (insp::DragVecN(ICON_FA_EXPAND, "Size", "Size X / Z (m)", size, 2, 1.0f,
                    terrain::kMinSize, terrain::kMaxSize, 256.0f, 0.0f, "%.1f", "XZ")) {
                ts.layout.sizeX = size[0];
                ts.layout.sizeZ = size[1];
                dirty = true;
            }
        }
        { insp::PropRow r(ICON_FA_ARROWS_UP_DOWN, "Height", "Height scale (m)");
          Mark(ImGui::DragFloat("##v", &ts.layout.heightScale, 0.5f, 0.0f, terrain::kMaxHeightScale), dirty); }
        { insp::PropRow r(ICON_FA_ARROW_UP, "Offset", "Height offset (m)");
          Mark(ImGui::DragFloat("##v", &ts.layout.heightOffset, 0.5f, -terrain::kMaxHeightOffset, terrain::kMaxHeightOffset), dirty); }
        {
            static const int kChunkSizes[] = { 8, 16, 32, 64, 128, 256 };
            char preview[16];
            snprintf(preview, sizeof(preview), "%d", ts.chunkQuads);
            insp::PropRow r(ICON_FA_BORDER_ALL, "Chunk", "Cells per chunk");
            if (ImGui::BeginCombo("##v", preview)) {
                for (int q : kChunkSizes) {
                    char label[16];
                    snprintf(label, sizeof(label), "%d", q);
                    if (ImGui::Selectable(label, ts.chunkQuads == q)) {
                        ts.chunkQuads = q;
                        dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
        { insp::PropRow r(ICON_FA_LAYER_GROUP, "LOD Bias");
          Mark(ImGui::DragFloat("##v", &ts.lodBias, 0.05f, terrain::kMinLodBias, terrain::kMaxLodBias), dirty); }

        // Paint layers (ADR 0011 step 5). Layer 0 mirrors the legacy texture fields.
        insp::SubHeading(ICON_FA_PAINTBRUSH, "Layers", "Paint with the Paint tool", false);
        std::vector<terrain::LayerSettings> layers = terrain::EffectiveLayers(ts);
        bool layersChanged = false;
        for (int i = 0; i < static_cast<int>(layers.size()); ++i) {
            ImGui::PushID(i);
            char word[16];
            snprintf(word, sizeof(word), "L%d", i + 1);
            layersChanged |= insp::AssetField(ICON_FA_IMAGE, word, "Layer texture", layers[i].texture,
                kTextureExts, IM_ARRAYSIZE(kTextureExts));
            {
                insp::PropRow r(nullptr, "Tile", "Tile size (m)");
                layersChanged |= ImGui::DragFloat("##v", &layers[i].tileSize, 0.1f,
                    terrain::kMinTextureTileSize, terrain::kMaxTextureTileSize);
            }
            ImGui::PopID();
        }
        {
            const bool canAdd = layers.size() < static_cast<size_t>(terrain::kMaxLayers);
            const bool canRemove = layers.size() > 1;
            static const char* const kIcons[] = { ICON_FA_PLUS, ICON_FA_MINUS };
            static const char* const kTips[] = { "Layer", "Last" };
            ImGui::BeginDisabled(!canAdd && !canRemove);
            const int pressed = insp::IconButtonRow("layerops", kIcons, kTips, 2);
            ImGui::EndDisabled();
            if (pressed == 0 && canAdd) {
                layers.push_back(terrain::LayerSettings{});
                layersChanged = true;
            } else if (pressed == 1 && canRemove) {
                layers.pop_back();
                layersChanged = true;
            }
        }
        if (layersChanged) {
            ts.layers = layers;
            ts.texture = layers[0].texture;
            ts.textureTileSize = layers[0].tileSize;
            tc->InvalidateTexture();
            dirty = true;
        }

        insp::SubHeading(ICON_FA_PALETTE, "Surface");
        {
            float color[3] = { ts.colorR, ts.colorG, ts.colorB };
            insp::PropRow r(ICON_FA_PALETTE, "Color");
            if (ImGui::ColorEdit3("##v", color)) {
                ts.colorR = color[0];
                ts.colorG = color[1];
                ts.colorB = color[2];
                dirty = true;
            }
        }
        { insp::PropRow r(ICON_FA_CIRCLE_HALF_STROKE, "Rough", "Roughness"); Mark(ImGui::SliderFloat("##v", &ts.roughness, 0.0f, 1.0f), dirty); }
        { insp::PropRow r(ICON_FA_GEM, "Metal", "Metallic"); Mark(ImGui::SliderFloat("##v", &ts.metallic, 0.0f, 1.0f), dirty); }
        insp::Toggle toggles[] = {
            { ICON_FA_CLOUD, "Shadows", &ts.castShadows },
            { ICON_FA_SHIELD, "Collide", &ts.collision,
              ts.collision && FindColliderComponent(n) ? "Collider component used instead" : nullptr },
        };
        Mark(insp::ToggleRow(toggles, IM_ARRAYSIZE(toggles)), dirty);
    }
    ts = terrain::SanitizeSettings(ts);
    s.EndEdit();
    if (s.Open())
        TerrainEditor_DrawInspector(n, *tc);
    s.Finish([n] { n->RemoveComponent<TerrainComponent>(); });
}

// -------------------------------------------------------------- Add component

void DrawAddComponent(GameObject* n, bool& dirty) {
    ImGui::Spacing();
    if (insp::WideButton(ICON_FA_PLUS, "Add", "Add component"))
        ImGui::OpenPopup("##AddComponentPopup");
    ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetFontSize() * 12.0f, 0.0f),
        ImVec2(FLT_MAX, ImGui::GetFontSize() * 30.0f));
    if (!ImGui::BeginPopup("##AddComponentPopup"))
        return;
    static ImGuiTextFilter filter;
    if (ImGui::IsWindowAppearing()) {
        filter.Clear();
        ImGui::SetKeyboardFocusHere();
    }
    filter.Draw("##ComponentSearch", -FLT_MIN);
    ImGui::Separator();
    int shown = 0;
    const char* group = nullptr;
    auto option = [&](const char* groupName, const char* icon, const char* label, const char* typeName,
                      bool available, auto create) {
        if (!available || !filter.PassFilter(label))
            return;
        if (!group || std::strcmp(group, groupName) != 0) {
            group = groupName;
            ImGui::TextDisabled("%s", groupName);
        }
        ++shown;
        char text[96];
        std::snprintf(text, sizeof(text), "%s  %s", icon, label);
        if (ImGui::Selectable(text)) {
            create();
            EditorUndo_PushAddComponent(n, typeName);
            dirty = true;
            ImGui::CloseCurrentPopup();
        }
    };
    const auto* currentCollider = FindColliderComponent(n);
    const bool canAdd3D = !currentCollider || currentCollider->colliderType == ColliderType::NONE;
    auto removeEmptyCollider = [&] { if (currentCollider) RemoveColliderComponent(n); };
    const bool has2D = n->GetComponent<Collider2DComponent>() != nullptr;

    option("Core", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Transform", "SceneTransform", !n->GetComponent<engine::Transform>(), [&] { n->AddComponent<engine::Transform>(); });
    option("Core", ICON_FA_CODE, "Script", "Script", !n->GetComponent<ScriptComponent>(), [&] { n->AddComponent<ScriptComponent>(); });
    option("Render", ICON_FA_CUBE, "Visual", "visual", !n->GetComponent<VisualComponent>(), [&] { n->AddComponent<VisualComponent>(); });
    option("Render", ICON_FA_FILL_DRIP, "Material", "Material", !n->GetComponent<MaterialComponent>(), [&] { n->AddComponent<MaterialComponent>(); });
    option("Render", ICON_FA_IMAGE, "Sprite Renderer", "SpriteRenderer", !n->GetComponent<SpriteRendererComponent>(), [&] { n->AddComponent<SpriteRendererComponent>(); });
    option("Render", ICON_FA_FILM, "Sprite Animator", "SpriteAnimator", !n->GetComponent<SpriteAnimatorComponent>(), [&] { n->AddComponent<SpriteAnimatorComponent>(); });
    option("Render", ICON_FA_IMAGE, "Billboard", "Billboard", !n->GetComponent<BillboardComponent>(), [&] { n->AddComponent<BillboardComponent>(); });
    option("Render", ICON_FA_MOUNTAIN, "Terrain", "Terrain", !n->GetComponent<TerrainComponent>(), [&] { n->AddComponent<TerrainComponent>(); });
    option("Render", ICON_FA_WAND_MAGIC_SPARKLES, "Effect", "Effect", !n->GetComponent<EffectComponent>(), [&] { n->AddComponent<EffectComponent>(); });
    option("Light", ICON_FA_VIDEO, "Camera", "Camera", !n->GetComponent<CameraComponent>(), [&] { n->AddComponent<CameraComponent>(); });
    option("Light", ICON_FA_SUN, "Directional Light", "DirectionalLight", !n->GetComponent<DirectionalLightComponent>(), [&] { n->AddComponent<DirectionalLightComponent>(); });
    option("Light", ICON_FA_LIGHTBULB, "Point Light", "PointLight", !n->GetComponent<PointLightComponent>(), [&] { n->AddComponent<PointLightComponent>(); });
    option("Light", ICON_FA_LIGHTBULB, "Spot Light", "SpotLight", !n->GetComponent<SpotLightComponent>(), [&] { n->AddComponent<SpotLightComponent>(); });
    option("Physics", ICON_FA_WEIGHT_HANGING, "Rigidbody 3D", "Rigidbody", !n->GetComponent<RigidbodyComponent>(), [&] { n->AddComponent<RigidbodyComponent>(); });
    option("Physics", ICON_FA_VECTOR_SQUARE, "Box Collider 3D", "BoxCollider", canAdd3D, [&] { removeEmptyCollider(); n->AddComponent<BoxColliderComponent>(); });
    option("Physics", ICON_FA_CIRCLE, "Sphere Collider 3D", "SphereCollider", canAdd3D, [&] { removeEmptyCollider(); n->AddComponent<SphereColliderComponent>(); });
    option("Physics", ICON_FA_CAPSULES, "Capsule Collider 3D", "CapsuleCollider", canAdd3D, [&] { removeEmptyCollider(); n->AddComponent<CapsuleColliderComponent>(); });
    option("Physics", ICON_FA_DATABASE, "Cylinder Collider 3D", "CylinderCollider", canAdd3D, [&] { removeEmptyCollider(); n->AddComponent<CylinderColliderComponent>(); });
    option("Physics", ICON_FA_DRAW_POLYGON, "Mesh Collider 3D", "MeshCollider", canAdd3D, [&] { removeEmptyCollider(); n->AddComponent<MeshColliderComponent>(); });
    option("Physics", ICON_FA_WEIGHT_HANGING, "Rigidbody 2D", "Rigidbody2D", !n->GetComponent<Rigidbody2DComponent>(), [&] { n->AddComponent<Rigidbody2DComponent>(); });
    option("Physics", ICON_FA_SQUARE, "Box Collider 2D", "Collider2D", !has2D, [&] { n->AddComponent<Collider2DComponent>()->shape = Shape2D::Box; });
    option("Physics", ICON_FA_CIRCLE, "Circle Collider 2D", "Collider2D", !has2D, [&] { n->AddComponent<Collider2DComponent>()->shape = Shape2D::Circle; });
    option("Audio", ICON_FA_VOLUME_HIGH, "Audio", "Audio", !n->GetComponent<AudioComponent>(), [&] { n->AddComponent<AudioComponent>(); });
    option("Audio", ICON_FA_EAR_LISTEN, "Audio Listener", "AudioListener", !n->GetComponent<AudioListenerComponent>(), [&] { n->AddComponent<AudioListenerComponent>(); });
    if (!shown)
        ImGui::TextDisabled("None");
    ImGui::EndPopup();
}

} // namespace

void DrawEditorInspectorPanel(bool playVisuals, bool* open)
{
    if (open && !*open)
        return;

    // --- Properties (dockable) ---
    if (!ImGui::Begin(kEditorPropertiesWindowTitle, open)) {
        ImGui::End();
        return;
    }
    // Every widget fills the remaining row width; nothing reserves a right-hand
    // label column, so the panel never needs horizontal scrolling.
    ImGui::PushItemWidth(-FLT_MIN);
    {
        if (playVisuals)
            ImGui::BeginDisabled(true);

        if (g_selNode) {
            bool dirty = false;
            static GameObject* s_lastSelNode = nullptr;
            static char nodeNameBuf[128] = {};
            if (g_selNode != s_lastSelNode) {
                EditorUndo_DiscardComponentEdit();
                s_lastSelNode = g_selNode;
                strncpy_s(nodeNameBuf, g_selNode->GetName().c_str(), sizeof(nodeNameBuf) - 1);
            }
            GameObject* n = g_selNode;

            DrawNodeHeader(n, nodeNameBuf, sizeof(nodeNameBuf), dirty);

            // Do not auto-add Visual on select/inspect — that marked scenes dirty
            // without a user edit. Add via Add Component instead.
            if (auto* t = n->GetComponent<engine::Transform>()) DrawTransform(n, t, dirty);
            if (auto* vc = n->GetComponent<VisualComponent>()) DrawVisual(n, vc, dirty);
            if (auto* dl = n->GetComponent<DirectionalLightComponent>()) DrawDirectionalLight(n, dl, dirty);
            if (auto* cam = n->GetComponent<CameraComponent>()) DrawCamera(n, cam, dirty);
            if (auto* pl = n->GetComponent<PointLightComponent>()) DrawPointLight(n, pl, dirty);
            if (auto* sl = n->GetComponent<SpotLightComponent>()) DrawSpotLight(n, sl, dirty);
            if (auto* cc = FindColliderComponent(n)) DrawCollider(n, cc, dirty);
            if (auto* rb = n->GetComponent<RigidbodyComponent>()) DrawRigidbody(n, rb, dirty);
            if (auto* mc = n->GetComponent<MaterialComponent>()) DrawMaterial(n, mc, dirty);
            if (auto* ec = n->GetComponent<EffectComponent>()) DrawEffect(n, ec, dirty);
            if (auto* ac = n->GetComponent<AudioComponent>()) DrawAudio(n, ac, dirty);
            if (auto* al = n->GetComponent<AudioListenerComponent>()) DrawAudioListener(n, al, dirty);
            if (auto* sc = n->GetComponent<ScriptComponent>()) DrawScript(n, sc, dirty);
            if (auto* b2 = n->GetComponent<Rigidbody2DComponent>()) DrawRigidbody2D(n, b2, dirty);
            if (auto* c2 = n->GetComponent<Collider2DComponent>()) DrawCollider2D(n, c2, dirty);
            if (auto* sr = n->GetComponent<SpriteRendererComponent>()) DrawSpriteRenderer(n, sr, dirty);
            if (auto* sa = n->GetComponent<SpriteAnimatorComponent>()) DrawSpriteAnimator(n, sa, dirty);
            if (auto* bb = n->GetComponent<BillboardComponent>()) DrawBillboard(n, bb, dirty);
            if (auto* tc = n->GetComponent<TerrainComponent>()) DrawTerrain(n, tc, dirty);

            DrawAddComponent(n, dirty);

            if (dirty) {
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }

            ImGui::Spacing();
            ImGui::Separator();
            static const char* const kIcons[] = { ICON_FA_FLOPPY_DISK, ICON_FA_TRASH_CAN };
            static const char* const kTips[] = { "Prefab", "Delete" };
            switch (insp::IconButtonRow("nodeops", kIcons, kTips, 2)) {
            case 0: {
                if (!fs::exists(PREFAB_DIR)) fs::create_directories(PREFAB_DIR);
                std::string prefabName = g_selNode->GetName();
                for (char& c : prefabName)
                    if (c == ' ' || c == '/' || c == '\\') c = '_';
                PrefabSerializer::SaveToFile(*g_selNode, PREFAB_DIR + prefabName + ".json");
                break;
            }
            case 1:
                EditorUndo_DiscardComponentEdit();
                s_lastSelNode = nullptr;
                DeleteGameObject(g_selNode);
                break;
            default:
                break;
            }
        }
        else if (g_selUiElement) {
            EditorUndo_BeginUiEdit(g_selUiElement);
            EngineGUIUI::DrawCanvasElementProperties(g_selUiElement);
            if (g_selUiElement)
                EditorUndo_EndUiEdit(g_selUiElement);
        }
        else {
            DrawPropertiesEmptyState();
        }

        if (playVisuals)
            ImGui::EndDisabled();
    }
    ImGui::PopItemWidth();
    ImGui::End();
}
