// editor_inspector_panel.cpp
// Extracted from engine/core/engine_gui.cpp (phase-1 editor UI split; behaviour-preserving).

#include "editor_gui_common_includes.h"
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
    std::string wid = "##SC_" + key;
    if (val.is_number_float()) {
        float f = val.get<float>();
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        if (ImGui::DragFloat(wid.c_str(), &f, 0.1f)) { val = f; return true; }
        if (ImGui::IsItemHovered()) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) { f += wheel * 0.1f * g_scrollSensitivity; val = f; return true; }
        }
    }
    else if (val.is_number_integer() || val.is_number_unsigned()) {
        int iv = val.get<int>();
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt(wid.c_str(), &iv)) { val = iv; return true; }
    }
    else if (val.is_boolean()) {
        bool bv = val.get<bool>();
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        if (ImGui::Checkbox(wid.c_str(), &bv)) { val = bv; return true; }
    }
    else if (val.is_string()) {
        static GameObject* s_editNode = nullptr;
        static std::string s_editKey;
        static char s_strBuf[256] = {};
        ImGui::Text("%s:", key.c_str()); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        if (s_editNode != editNode || s_editKey != key) {
            s_editNode = editNode; s_editKey = key;
            strncpy_s(s_strBuf, val.get<std::string>().c_str(), sizeof(s_strBuf) - 1);
        }
        if (ImGui::InputText(wid.c_str(), s_strBuf, sizeof(s_strBuf),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            val = std::string(s_strBuf);
            return true;
        }
        if (!ImGui::IsItemActive())
            strncpy_s(s_strBuf, val.get<std::string>().c_str(), sizeof(s_strBuf) - 1);
    }
    return false;
}
void DrawPropertiesEmptyState() {
    ImGui::TextDisabled("No object selected");
    ImGui::Spacing();
    ImGui::TextWrapped("Use GameObject menu to create objects or UI elements.");
    ImGui::Spacing();
    ImGui::TextWrapped("Use the Asset Browser below to browse rom/ assets.");
    ImGui::Spacing();
    ImGui::TextWrapped("Use Edit > Project Settings > Rendering for skybox, shadows, and render defaults.");
}


void DrawEditorInspectorPanel(bool playVisuals, bool* open)
{
    if (open && !*open)
        return;

    // --- Properties (dockable) ---
    if (!ImGui::Begin(kEditorPropertiesWindowTitle, open)) {
        ImGui::End();
        return;
    }
    {
        if (playVisuals)
            ImGui::BeginDisabled(true);

        if (g_selNode) {
            bool runtimeDirty = false;
            static GameObject* s_lastSelNode = nullptr;
            static char nodeNameBuf[128] = {};
            if (g_selNode != s_lastSelNode) {
                EditorUndo_DiscardComponentEdit();
                s_lastSelNode = g_selNode;
                strncpy_s(nodeNameBuf, g_selNode->GetName().c_str(), sizeof(nodeNameBuf) - 1);
            }

            // Do not auto-add Visual on select/inspect — that marked scenes dirty
            // without a user edit. Add via the component combo instead.
            VisualComponent* vc = g_selNode->GetComponent<VisualComponent>();

            ImGui::Text("Node: %s", g_selNode->GetName().c_str());
            ImGui::Separator();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##NodeName", nodeNameBuf, sizeof(nodeNameBuf),
                ImGuiInputTextFlags_EnterReturnsTrue)) {
                const std::string oldName = g_selNode->GetName();
                const std::string newName(nodeNameBuf);
                g_selNode->SetName(newName);
                EditorUndo_PushRename(g_selNode, oldName, newName);
                EngineGUI_MarkSceneDirty();
            }
            if (!ImGui::IsItemActive())
                strncpy_s(nodeNameBuf, g_selNode->GetName().c_str(), sizeof(nodeNameBuf) - 1);
            ImGui::Spacing();

            ImGui::Text("Hierarchy Order");
            if (ImGui::Button("Back", ImVec2(Ui(70.0f), 0)))
            {
                if (UndoableReorderNode(g_selNode, MoveNodeToBack))
                    runtimeDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Down", ImVec2(Ui(70.0f), 0)))
            {
                if (UndoableReorderNode(g_selNode, [](GameObject* n) {
                    return MoveNodeOrder(n, -1);
                }))
                    runtimeDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Up", ImVec2(Ui(70.0f), 0)))
            {
                if (UndoableReorderNode(g_selNode, [](GameObject* n) {
                    return MoveNodeOrder(n, 1);
                }))
                    runtimeDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Top", ImVec2(Ui(70.0f), 0)))
            {
                if (UndoableReorderNode(g_selNode, MoveNodeToFront))
                    runtimeDirty = true;
            }
            ImGui::TextDisabled("Later items draw later; UI later items appear on top.");
            ImGui::Spacing();

            // Transform
            if (engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
                ImGui::Text("Transform");
                ImGui::Separator();
                EditorUndo_BeginComponentEdit(g_selNode, "SceneTransform");
                const float xformBefore[9] = {
                    t->localPosition.x, t->localPosition.y, t->localPosition.z,
                    t->localRotation.x, t->localRotation.y, t->localRotation.z,
                    t->localScale.x, t->localScale.y, t->localScale.z
                };
                bool xformEdited = false;
                ImGui::Text("Position");
                ImGui::Text("X:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NPX", &t->localPosition.x, 0.1f);
                xformEdited |= ScrollValue(&t->localPosition.x);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NPY", &t->localPosition.y, 0.1f);
                xformEdited |= ScrollValue(&t->localPosition.y);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NPZ", &t->localPosition.z, 0.1f);
                xformEdited |= ScrollValue(&t->localPosition.z);
                ImGui::Spacing();
                ImGui::Text("Rotation");
                ImGui::Text("X:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NRX", &t->localRotation.x, 1.0f);
                xformEdited |= ScrollValue(&t->localRotation.x, 1.0f);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NRY", &t->localRotation.y, 1.0f);
                xformEdited |= ScrollValue(&t->localRotation.y, 1.0f);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NRZ", &t->localRotation.z, 1.0f);
                xformEdited |= ScrollValue(&t->localRotation.z, 1.0f);
                ImGui::Spacing();
                ImGui::Text("Scale");
                ImGui::Text("X:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NSX", &t->localScale.x, 0.1f, 0.001f, 1000.0f);
                xformEdited |= ScrollValue(&t->localScale.x);
                ImGui::Text("Y:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NSY", &t->localScale.y, 0.1f, 0.001f, 1000.0f);
                xformEdited |= ScrollValue(&t->localScale.y);
                ImGui::Text("Z:"); ImGui::SameLine(40);
                xformEdited |= ImGui::DragFloat("##NSZ", &t->localScale.z, 0.1f, 0.001f, 1000.0f);
                xformEdited |= ScrollValue(&t->localScale.z);
                ImGui::Spacing();
                const float xformAfter[9] = {
                    t->localPosition.x, t->localPosition.y, t->localPosition.z,
                    t->localRotation.x, t->localRotation.y, t->localRotation.z,
                    t->localScale.x, t->localScale.y, t->localScale.z
                };
                if (xformEdited && JsonFloat_TransformChanged(xformBefore, xformAfter))
                    runtimeDirty = true;
                else if (xformEdited && !JsonFloat_TransformChanged(xformBefore, xformAfter)) {
                    // DragFloat/Scroll reported a change that does not survive 3dp — keep bits.
                    t->localPosition = { xformBefore[0], xformBefore[1], xformBefore[2] };
                    t->localRotation = { xformBefore[3], xformBefore[4], xformBefore[5] };
                    t->localScale    = { xformBefore[6], xformBefore[7], xformBefore[8] };
                }
                g_selNode->SyncLegacyTransformFromComponents();
                EditorUndo_EndComponentEdit(g_selNode, "SceneTransform");
            }

            ImGui::Text("Scroll Sensitivity");
            ImGui::SliderFloat("##Sens", &g_scrollSensitivity, 0.1f, 5.0f, "%.1f");
            ImGui::Separator();

            ImGui::Text("Tag");
            {
                TagRegistry& tags = TagRegistry::Instance();
                int currentTag = g_selNode->GetTagId();
                if (!tags.IsValid(currentTag))
                    currentTag = static_cast<int>(Tag::TAG_NONE);

                ImGui::SameLine(80);
                ImGui::SetNextItemWidth(-1);
                const char* preview = tags.GetName(currentTag);
                if (ImGui::BeginCombo("##Tag", preview, ImGuiComboFlags_HeightLarge)) {
                    const float removeW = ImGui::GetFrameHeight();
                    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
                    const int noneId = static_cast<int>(Tag::TAG_NONE);

                    for (int i = 0; i < tags.GetCount(); ++i) {
                        ImGui::PushID(i);
                        const bool selected = (i == currentTag);
                        const bool showRemove = (i != noneId);
                        const bool canRemove = tags.CanRemove(i);
                        const char* name = tags.GetName(i);

                        if (showRemove) {
                            const float labelW =
                                ImGui::GetContentRegionAvail().x - removeW - spacing;
                            if (ImGui::Selectable(name, selected, 0,
                                    ImVec2((labelW > 1.0f) ? labelW : 1.0f, 0.0f))) {
                                const int oldTag = g_selNode->GetTagId();
                                g_selNode->SetTag(i);
                                EditorUndo_PushTag(g_selNode, oldTag, i);
                                runtimeDirty = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();

                            ImGui::SameLine(0.0f, spacing);
                            if (!canRemove)
                                ImGui::BeginDisabled();
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                ImVec4(0.85f, 0.25f, 0.25f, 0.55f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                ImVec4(0.9f, 0.2f, 0.2f, 0.75f));
                            if (ImGui::Button(ICON_FA_XMARK "##RemoveTag",
                                    ImVec2(removeW, removeW))) {
                                g_pendingDeleteTagId = i;
                                g_showDeleteTagPopup = true;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::PopStyleColor(3);
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled |
                                    ImGuiHoveredFlags_DelayShort)) {
                                ImGui::SetTooltip(canRemove
                                    ? "Delete tag"
                                    : "Built-in tags cannot be deleted");
                            }
                            if (!canRemove)
                                ImGui::EndDisabled();
                        }
                        else {
                            if (ImGui::Selectable(name, selected)) {
                                const int oldTag = g_selNode->GetTagId();
                                g_selNode->SetTag(i);
                                EditorUndo_PushTag(g_selNode, oldTag, i);
                                runtimeDirty = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::PopID();
                    }

                    ImGui::Separator();
                    if (ImGui::Selectable(ICON_FA_PLUS "  Add New")) {
                        g_newTagName[0] = '\0';
                        g_showAddTagPopup = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::Spacing();

            if (vc) {
            ImGui::Separator();
            ImGui::Text("Visual");
            EditorUndo_BeginComponentEdit(g_selNode, "visual");

            ImGui::Text("Mesh");
            {
                const auto& defs = ModelAssetCache::Instance().GetObjectDefinitions();
                int currentIdx = -1;
                for (int i = 0; i < (int)defs.size(); ++i) {
                    if (defs[i].assetPath == vc->modelPath) {
                        currentIdx = i;
                        break;
                    }
                }
                const char* preview = vc->modelPath.empty()
                    ? "(None)"
                    : (currentIdx >= 0 ? defs[currentIdx].displayName.c_str() : vc->modelPath.c_str());
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##Mesh", preview)) {
                    for (int i = 0; i < (int)defs.size(); ++i) {
                        const bool selected = (i == currentIdx);
                        if (!selected &&
                            ModelAssetCache::IsHiddenPrimitiveAlias(defs[i].assetPath))
                            continue;
                        if (ImGui::Selectable(defs[i].displayName.c_str(), selected)) {
                            if (vc->modelPath != defs[i].assetPath) {
                                vc->modelPath = defs[i].assetPath;
                                runtimeDirty = true;
                            }
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                {
                    const char* modelExts[] = { ".fbx", ".obj", ".gltf", ".glb" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(modelExts, IM_ARRAYSIZE(modelExts), droppedPath)) {
                        if (vc->modelPath != droppedPath) {
                            vc->modelPath = droppedPath;
                            runtimeDirty = true;
                        }
                    }
                }
                if (currentIdx < 0 && !vc->modelPath.empty())
                    ImGui::TextDisabled("Custom / missing asset: %s", vc->modelPath.c_str());
            }

            bool animate = vc->playAnimation;
            if (ImGui::Checkbox("Animate", &animate)) {
                vc->playAnimation = animate;
                runtimeDirty = true;
            }

            ImGui::Text("LOD");
            {
                // Level 0 is the Mesh above; each row switches TO that level at its
                // distance. Applies in Play / runtime only.
                const char* lodExts[] = { ".fbx", ".obj", ".gltf", ".glb" };
                int removeIdx = -1;
                auto sortLods = [vc]() {
                    if (vc->lodScreenSize) {
                        std::stable_sort(vc->lods.begin(), vc->lods.end(),
                            [](const VisualComponent::LodLevel& a, const VisualComponent::LodLevel& b) {
                                return a.screenHeight > b.screenHeight; });
                    } else {
                        std::stable_sort(vc->lods.begin(), vc->lods.end(),
                            [](const VisualComponent::LodLevel& a, const VisualComponent::LodLevel& b) {
                                return a.maxDistance < b.maxDistance; });
                    }
                };
                if (ImGui::Checkbox("Screen Size Mode", &vc->lodScreenSize)) {
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
                    runtimeDirty = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled(vc->lodScreenSize
                    ? "(switch by projected height)" : "(switch by distance)");
                for (int i = 0; i < static_cast<int>(vc->lods.size()); ++i) {
                    VisualComponent::LodLevel& lod = vc->lods[static_cast<size_t>(i)];
                    ImGui::PushID(i);
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("LOD %d", i + 1);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(90.0f);
                    if (vc->lodScreenSize) {
                        float percent = lod.screenHeight * 100.0f;
                        if (ImGui::DragFloat("##LodScreen", &percent, 0.1f, 0.0f, 100.0f, "%.1f %%")) {
                            lod.screenHeight = std::min(std::max(percent, 0.0f), 100.0f) * 0.01f;
                            runtimeDirty = true;
                        }
                    } else if (ImGui::DragFloat("##LodDist", &lod.maxDistance, 0.5f, 0.0f, 100000.0f, "%.1f m")) {
                        if (lod.maxDistance < 0.0f) lod.maxDistance = 0.0f;
                        runtimeDirty = true;
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        sortLods();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X"))
                        removeIdx = i;
                    char pathBuf[512];
                    std::snprintf(pathBuf, sizeof(pathBuf), "%s", lod.modelPath.c_str());
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##LodPath", pathBuf, sizeof(pathBuf))) {
                        lod.modelPath = pathBuf;
                        runtimeDirty = true;
                    }
                    std::string droppedLod;
                    if (AcceptAssetPathDrop(lodExts, IM_ARRAYSIZE(lodExts), droppedLod)) {
                        lod.modelPath = droppedLod;
                        runtimeDirty = true;
                    }
                    ImGui::PopID();
                }
                if (removeIdx >= 0) {
                    vc->lods.erase(vc->lods.begin() + removeIdx);
                    runtimeDirty = true;
                }
                if (static_cast<int>(vc->lods.size()) < kLodMaxLevels - 1) {
                    if (ImGui::SmallButton("Add LOD")) {
                        VisualComponent::LodLevel lod;
                        lod.maxDistance = vc->lods.empty() ? 20.0f : vc->lods.back().maxDistance + 30.0f;
                        lod.screenHeight = vc->lods.empty() ? 0.25f
                            : std::max(vc->lods.back().screenHeight * 0.5f, 0.005f);
                        vc->lods.push_back(lod);
                        runtimeDirty = true;
                    }
                }
                if (ImGui::DragFloat("Cull Distance", &vc->cullDistance, 1.0f, 0.0f, 100000.0f,
                        vc->cullDistance > 0.0f ? "%.1f m" : "Never")) {
                    if (vc->cullDistance < 0.0f) vc->cullDistance = 0.0f;
                    runtimeDirty = true;
                }
                if (vc->playAnimation && !vc->lods.empty())
                    ImGui::TextDisabled("LOD levels ignored while Animate is on.");
                else if (!vc->lods.empty() || vc->cullDistance > 0.0f)
                    ImGui::TextDisabled("Play / runtime only; needs Mesh LOD on in Rendering.");
            }

            if (ImGui::Checkbox("Shadow Volume", &vc->shadowVolume))
                runtimeDirty = true;
            if (vc->shadowVolume && RenderSettings_GetConst().shadowMode != ShadowMode::Volume)
                ImGui::TextDisabled("Casts only when Rendering > Shadows > Mode is Shadow Volume.");
            else if (vc->shadowVolume)
                ImGui::TextDisabled("Needs a closed (watertight) mesh; open meshes cast nothing.");

            ImGui::Text("Shader");
            {
                const int variantCount = Shader3DVariantCount();
                const bool useSceneDefault = vc->shaderOverride.empty();
                const Shader3DVariant currentVariant = useSceneDefault
                    ? RenderSettings_GetDefaultShader()
                    : Shader3DVariantFromId(vc->shaderOverride);
                const std::string preview = useSceneDefault
                    ? "Use Scene Default"
                    : ShaderVariantPreviewLabel(currentVariant);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##ObjectShader", preview.c_str())) {
                    if (ImGui::Selectable("Use Scene Default", useSceneDefault)) {
                        vc->shaderOverride.clear();
                        runtimeDirty = true;
                    }
                    for (int i = 0; i < variantCount; ++i) {
                        const Shader3DVariant v = Shader3DVariantByIndex(i);
                        if (!Shader3DVariantIsUserVisible(v))
                            continue;
                        const bool selected = !useSceneDefault
                            && (Shader3DVariantId(v) == vc->shaderOverride);
                        if (ImGui::Selectable(Shader3DVariantLabel(v), selected)) {
                            vc->shaderOverride = Shader3DVariantId(v);
                            if (v == Shader3DVariant::SurfaceFX)
                                vc->hasSurfaceFx = true;
                            runtimeDirty = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (!useSceneDefault) {
                    ImGui::TextDisabled("Scene default: %s",
                        Shader3DVariantLabel(RenderSettings_GetDefaultShader()));
                }

                if (currentVariant == Shader3DVariant::SurfaceFX) {
                    vc->hasSurfaceFx = true;
                    ImGui::Separator();
                    ImGui::Text("Surface FX");

                    char texBuf[256];
                    strncpy_s(texBuf, vc->surfaceFxTexturePath.c_str(), sizeof(texBuf) - 1);
                    texBuf[sizeof(texBuf) - 1] = '\0';
                    ImGui::Text("Detail Texture");
                    if (ImGui::InputText("##SfxTex", texBuf, sizeof(texBuf))) {
                        vc->surfaceFxTexturePath = texBuf;
                        runtimeDirty = true;
                    }
                    {
                        const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                        std::string droppedPath;
                        if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                            vc->surfaceFxTexturePath = droppedPath;
                            runtimeDirty = true;
                        }
                    }

                    if (ImGui::ColorEdit4("Color 1", &vc->surfaceFx.Color1.x))
                        runtimeDirty = true;
                    if (ImGui::ColorEdit4("Color 2", &vc->surfaceFx.Color2.x))
                        runtimeDirty = true;
                    if (ImGui::DragFloat2("Scroll Speed 1", &vc->surfaceFx.ScrollSpeed1.x, 0.001f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat2("Scroll Speed 2", &vc->surfaceFx.ScrollSpeed2.x, 0.001f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("UV Scale", &vc->surfaceFx.UvScale, 0.001f, 0.001f, 10.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Fresnel Power", &vc->surfaceFx.FresnelPower, 0.05f, 0.1f, 16.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Emission Strength", &vc->surfaceFx.EmissionStrength, 0.01f, 0.0f, 5.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Gradient Start", &vc->surfaceFx.GradientStart, 0.01f, 0.0f, 1.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Gradient End", &vc->surfaceFx.GradientEnd, 0.01f, 0.0f, 1.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Edge Fade Start", &vc->surfaceFx.EdgeFadeStart, 0.01f, 0.0f, 1.0f))
                        runtimeDirty = true;
                    if (ImGui::DragFloat("Edge Fade End", &vc->surfaceFx.EdgeFadeEnd, 0.01f, 0.0f, 1.0f))
                        runtimeDirty = true;
                    ImGui::TextDisabled("Detail texture scrolls; not Material normal map.");
                }
            }
            EditorUndo_EndComponentEdit(g_selNode, "visual");
            }

            ImGui::Spacing();

            if (DirectionalLightComponent* dl = g_selNode->GetComponent<DirectionalLightComponent>()) {
                ImGui::Separator();
                ImGui::Text("Directional Light");
                EditorUndo_BeginComponentEdit(g_selNode, "DirectionalLight");
                if (ImGui::Checkbox("Enabled", &dl->enabled))
                    runtimeDirty = true;
                if (ImGui::ColorEdit3("Color", &dl->color.x))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Intensity", &dl->intensity, 0.05f, 0.0f, 10.0f))
                    runtimeDirty = true;
                if (ImGui::ColorEdit3("Specular", &dl->specColor.x))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Spec Power", &dl->specPower, 1.0f, 1.0f, 256.0f))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Cast Shadows", &dl->castShadows))
                    runtimeDirty = true;

                if (const engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
                    const DirectX::XMFLOAT3 dir = DirectionalLightComponent::ComputeDirectionWS(t);
                    ImGui::Text("Direction: (%.2f, %.2f, %.2f)", dir.x, dir.y, dir.z);
                    ImGui::TextDisabled("Rotate Transform to aim light (+Z forward).");
                }
                ImGui::TextDisabled("Shader: Directional (Pixel) on Visual component.");
                EditorUndo_EndComponentEdit(g_selNode, "DirectionalLight");
                ImGui::Spacing();
            }

            if (CameraComponent* cam = g_selNode->GetComponent<CameraComponent>()) {
                ImGui::Separator();
                ImGui::Text("Camera");
                EditorUndo_BeginComponentEdit(g_selNode, "Camera");
                if (ImGui::Checkbox("Enabled", &cam->enabled))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Editor Camera Controls on Play", &cam->debugFly)) runtimeDirty = true;
                if (ImGui::Checkbox("Orthographic", &cam->orthographic))
                    runtimeDirty = true;
                if (cam->orthographic) {
                    if (ImGui::DragFloat("View Height", &cam->orthographicHeight, 0.1f, 0.01f, 100000.0f))
                        runtimeDirty = true;
                }
                else if (ImGui::DragFloat("FOV", &cam->fov, 0.5f, 1.0f, 179.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Near Clip", &cam->nearClip, 0.01f, 0.01f, 100.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Far Clip", &cam->farClip, 1.0f, 1.0f, 100000.0f))
                    runtimeDirty = true;
                if (const engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
                    const DirectX::XMFLOAT3 fwd = CameraComponent::ComputeForwardWS(t);
                    const DirectX::XMFLOAT3 up = CameraComponent::ComputeUpWS(t);
                    const DirectX::XMFLOAT3 right = CameraComponent::ComputeRightWS(t);
                    ImGui::Text("Forward: (%.2f, %.2f, %.2f)", fwd.x, fwd.y, fwd.z);
                    ImGui::Text("Up:      (%.2f, %.2f, %.2f)", up.x, up.y, up.z);
                    ImGui::Text("Right:   (%.2f, %.2f, %.2f)", right.x, right.y, right.z);
                    ImGui::TextDisabled("Wire: blue=fwd green=up red=right. Aim with Transform (+Z).");
                    ImGui::TextDisabled("View uses world-up bank (no upside-down past 90 pitch).");
                }
                EditorUndo_EndComponentEdit(g_selNode, "Camera");
                ImGui::Spacing();
            }

            if (PointLightComponent* pl = g_selNode->GetComponent<PointLightComponent>()) {
                ImGui::Separator();
                ImGui::Text("Point Light");
                EditorUndo_BeginComponentEdit(g_selNode, "PointLight");
                if (ImGui::Checkbox("Enabled", &pl->enabled)) runtimeDirty = true;
                if (ImGui::ColorEdit3("Color", &pl->color.x)) runtimeDirty = true;
                if (ImGui::DragFloat("Intensity", &pl->intensity, 0.05f, 0.0f, 10.0f)) runtimeDirty = true;
                if (ImGui::ColorEdit3("Specular", &pl->specColor.x)) runtimeDirty = true;
                if (ImGui::DragFloat("Spec Power", &pl->specPower, 1.0f, 1.0f, 256.0f)) runtimeDirty = true;
                if (ImGui::DragFloat("Range", &pl->range, 0.1f, 0.1f, 100.0f)) runtimeDirty = true;
                ImGui::Text("Attenuation (kc, kl, kq)");
                if (ImGui::DragFloat3("##PtAtten", &pl->attenuation.x, 0.01f, 0.0f, 10.0f)) runtimeDirty = true;
                ImGui::TextDisabled("Shader: Point (Pixel) on Visual component.");
                EditorUndo_EndComponentEdit(g_selNode, "PointLight");
                ImGui::Spacing();
            }

            if (SpotLightComponent* sl = g_selNode->GetComponent<SpotLightComponent>()) {
                ImGui::Separator();
                ImGui::Text("Spot Light");
                EditorUndo_BeginComponentEdit(g_selNode, "SpotLight");
                if (ImGui::Checkbox("Enabled", &sl->enabled)) runtimeDirty = true;
                if (ImGui::ColorEdit3("Color", &sl->color.x)) runtimeDirty = true;
                if (ImGui::DragFloat("Intensity", &sl->intensity, 0.05f, 0.0f, 10.0f)) runtimeDirty = true;
                if (ImGui::ColorEdit3("Specular", &sl->specColor.x)) runtimeDirty = true;
                if (ImGui::DragFloat("Spec Power", &sl->specPower, 1.0f, 1.0f, 256.0f)) runtimeDirty = true;
                if (ImGui::DragFloat("Range", &sl->range, 0.1f, 0.1f, 100.0f)) runtimeDirty = true;
                if (ImGui::DragFloat("Inner Angle", &sl->innerAngleDeg, 0.5f, 0.0f, 89.0f)) runtimeDirty = true;
                if (ImGui::DragFloat("Outer Angle", &sl->outerAngleDeg, 0.5f, 0.0f, 89.0f)) runtimeDirty = true;
                if (ImGui::Checkbox("Cast Shadows", &sl->castShadows)) runtimeDirty = true;
                ImGui::Text("Attenuation (kc, kl, kq)");
                if (ImGui::DragFloat3("##SpAtten", &sl->attenuation.x, 0.01f, 0.0f, 10.0f)) runtimeDirty = true;
                if (const engine::Transform* t = g_selNode->GetComponent<engine::Transform>()) {
                    const DirectX::XMFLOAT3 shine = LightGizmo::GetShineDirectionWS(t);
                    ImGui::Text("Direction: (%.2f, %.2f, %.2f)", shine.x, shine.y, shine.z);
                    ImGui::TextDisabled("Rotate Transform to aim spot (+Z forward).");
                }
                ImGui::TextDisabled("Shader: Spot (Pixel) on Visual component.");
                EditorUndo_EndComponentEdit(g_selNode, "SpotLight");
                ImGui::Spacing();
            }

            if (ColliderComponent* cc = FindColliderComponent(g_selNode)) {
                ImGui::Separator();
                const char* colliderTypeName=cc->GetTypeName();
                const char* colliderLabels[]={"Collider (Legacy)","Box Collider","Sphere Collider","Capsule Collider","Mesh Collider","Cylinder Collider"};
                const int colliderIndex=static_cast<int>(cc->colliderType);
                ImGui::TextUnformatted(colliderIndex>=0&&colliderIndex<IM_ARRAYSIZE(colliderLabels)?colliderLabels[colliderIndex]:"Collider (Legacy)");
                EditorUndo_BeginComponentEdit(g_selNode, colliderTypeName);
                auto noteColliderEdit = [&](bool edited) {
                    if (edited)
                        runtimeDirty = true;
                };
                if (cc->colliderType != ColliderType::NONE) {
                    ImGui::Text("Center");
                    ImGui::Text("X:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##CCX", &cc->colliderCenter.x, 0.1f));
                    noteColliderEdit(ScrollValue(&cc->colliderCenter.x));
                    ImGui::Text("Y:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##CCY", &cc->colliderCenter.y, 0.1f));
                    noteColliderEdit(ScrollValue(&cc->colliderCenter.y));
                    ImGui::Text("Z:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##CCZ", &cc->colliderCenter.z, 0.1f));
                    noteColliderEdit(ScrollValue(&cc->colliderCenter.z));
                }
                if (cc->colliderType == ColliderType::BOX) {
                    ImGui::Text("Half Extents");
                    ImGui::Text("X:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##HEX", &cc->halfExtents.x, 0.1f));
                    ImGui::Text("Y:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##HEY", &cc->halfExtents.y, 0.1f));
                    ImGui::Text("Z:"); ImGui::SameLine(40);
                    noteColliderEdit(ImGui::DragFloat("##HEZ", &cc->halfExtents.z, 0.1f));
                }
                else if (cc->colliderType == ColliderType::SPHERE) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##Rad", &cc->radius, 0.1f));
                }
                else if (cc->colliderType == ColliderType::CAPSULE) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##CRad", &cc->radius, 0.1f));
                    ImGui::Text("Height:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##CHei", &cc->height, 0.1f));
                }
                else if (cc->colliderType == ColliderType::CYLINDER) {
                    ImGui::Text("Radius:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##CyRad", &cc->radius, 0.1f));
                    ImGui::Text("Height:"); ImGui::SameLine(80);
                    noteColliderEdit(ImGui::DragFloat("##CyHei", &cc->height, 0.1f));
                }
                else if (cc->colliderType == ColliderType::MESH) {
                    ImGui::Text("Size Padding:"); ImGui::SameLine(100);
                    noteColliderEdit(ImGui::DragFloat("##MSP", &cc->meshSizePadding, 0.01f, 0.0f, 10.0f));
                }
                if (cc->colliderType == ColliderType::BOX
                    || cc->colliderType == ColliderType::SPHERE
                    || cc->colliderType == ColliderType::CAPSULE
                    || cc->colliderType == ColliderType::CYLINDER) {
                    static GameObject* s_autoFitFailNode = nullptr;
                    if (ImGui::Button("Auto-Fit")) {
                        if (AutoFitColliderToMesh(g_selNode, cc)) {
                            s_autoFitFailNode = nullptr;
                            runtimeDirty = true;
                            ImGui::MarkItemEdited(ImGui::GetItemID());
                        } else {
                            s_autoFitFailNode = g_selNode;
                        }
                    }
                    if (s_autoFitFailNode == g_selNode)
                        ImGui::TextDisabled("Auto-Fit: no usable mesh vertices.");
                }
                if (ImGui::Checkbox("Show Debug Collider", &cc->showDebugCollider)) {
                    g_selNode->m_debugCollider = cc->showDebugCollider;
                    runtimeDirty = true;
                }
                if (ImGui::Checkbox("Is Trigger", &cc->isTrigger)) {
                    runtimeDirty = true;
                }
                {
                    ImGui::Text("Physics Material");
                    char matBuf[256];
                    strncpy_s(matBuf, cc->physicsMaterial.c_str(), sizeof(matBuf) - 1);
                    matBuf[sizeof(matBuf) - 1] = '\0';
                    ImGui::SameLine(140);
                    if (ImGui::InputText("##PhysMat", matBuf, sizeof(matBuf))) {
                        cc->physicsMaterial = matBuf;
                        runtimeDirty = true;
                    }
                    const char* physMatExts[] = { ".physmat" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(physMatExts, IM_ARRAYSIZE(physMatExts), droppedPath)) {
                        cc->physicsMaterial = droppedPath;
                        runtimeDirty = true;
                    }
                    if (!cc->physicsMaterial.empty()) {
                        ImGui::SameLine();
                        if (ImGui::Button("Clear##PhysMat")) {
                            cc->physicsMaterial.clear();
                            runtimeDirty = true;
                        }
                    }
                    if (Collider* col = g_selNode->GetCollider()) {
                        const ContactSurface& surface = col->GetContactSurface();
                        if (surface.assigned) {
                            ImGui::TextDisabled(
                                "Static %.3f  Dynamic %.3f  Restitution %.3f",
                                surface.staticFriction, surface.dynamicFriction, surface.restitution);
                        } else if (!cc->physicsMaterial.empty()) {
                            ImGui::TextDisabled("Not loaded; using rigidbody friction/restitution.");
                        }
                    }
                }
                EditorUndo_EndComponentEdit(g_selNode, colliderTypeName);
                if (ImGui::Button("Remove Collider")) {
                    EditorUndo_PushRemoveComponent(g_selNode, colliderTypeName);
                    RemoveColliderComponent(g_selNode);
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            if (RigidbodyComponent* rbc = g_selNode->GetComponent<RigidbodyComponent>()) {
                ImGui::Separator();
                ImGui::Text("Rigidbody");
                EditorUndo_BeginComponentEdit(g_selNode, "Rigidbody");
                Rigidbody& rb = rbc->Body();

                bool useGravity = rb.UseGravity();
                if (ImGui::Checkbox("Use Gravity", &useGravity)) {
                    rb.SetUseGravity(useGravity);
                    runtimeDirty = true;
                }

                bool isKinematic = rb.IsKinematic();
                if (ImGui::Checkbox("Is Kinematic", &isKinematic)) {
                    rb.SetIsKinematic(isKinematic);
                    runtimeDirty = true;
                }

                float mass = rb.GetMass();
                if (ImGui::DragFloat("Mass", &mass, 0.05f, 0.001f, 1000.0f)) {
                    rb.SetMass(mass);
                    runtimeDirty = true;
                }

                float drag = rb.GetDrag();
                if (ImGui::DragFloat("Drag", &drag, 0.01f, 0.0f, 50.0f)) {
                    rb.SetDrag(drag);
                    runtimeDirty = true;
                }

                float angularDrag = rb.GetAngularDrag();
                if (ImGui::DragFloat("Angular Drag", &angularDrag, 0.01f, 0.0f, 50.0f)) {
                    rb.SetAngularDrag(angularDrag);
                    runtimeDirty = true;
                }

                float restitution = rb.GetRestitution();
                if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 2.0f)) {
                    rb.SetRestitution(restitution);
                    runtimeDirty = true;
                }

                float friction = rb.GetFriction();
                if (ImGui::DragFloat("Friction", &friction, 0.01f, 0.0f, 10.0f)) {
                    rb.SetFriction(friction);
                    runtimeDirty = true;
                }

                EditorUndo_EndComponentEdit(g_selNode, "Rigidbody");
                if (ImGui::Button("Remove Rigidbody")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Rigidbody");
                    g_selNode->RemoveComponent<RigidbodyComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            // Material property editor
            if (MaterialComponent* mc = g_selNode->GetComponent<MaterialComponent>()) {
                ImGui::Separator();
                ImGui::Text("Material");
                EditorUndo_BeginComponentEdit(g_selNode, "Material");

                if (ImGui::ColorEdit3("Color", &mc->color.x))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Roughness", &mc->roughness, 0.01f, 0.0f, 1.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Metallic", &mc->metallic, 0.01f, 0.0f, 1.0f))
                    runtimeDirty = true;

                char nbuf[256];
                strncpy_s(nbuf, mc->normalMapPath.c_str(), sizeof(nbuf) - 1);
                nbuf[sizeof(nbuf) - 1] = '\0';
                ImGui::Text("Normal Map");
                if (ImGui::InputText("##MatNormal", nbuf, sizeof(nbuf))) {
                    mc->normalMapPath = nbuf;
                    runtimeDirty = true;
                }
                {
                    const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                        mc->normalMapPath = droppedPath;
                        runtimeDirty = true;
                    }
                }
                if (ImGui::DragFloat("Normal Strength", &mc->normalStrength, 0.05f, 0.0f, 4.0f))
                    runtimeDirty = true;

                // Toon fields (useful when shader is "toon"; harmless otherwise)
                {
                    VisualComponent* vc = g_selNode->GetComponent<VisualComponent>();
                    const bool isToon = vc && (
                        vc->shaderOverride == "toon"
                        || (vc->shaderOverride.empty()
                            && RenderSettings_GetDefaultShader() == Shader3DVariant::Toon));
                    if (isToon || ImGui::TreeNode("Toon / Outline")) {
                        if (isToon)
                            ImGui::TextDisabled("Toon / Outline");
                        if (ImGui::DragFloat("Shade Steps", &mc->shadeSteps, 0.1f, 1.0f, 8.0f))
                            runtimeDirty = true;
                        if (ImGui::DragFloat("Shade Smoothness", &mc->shadeSmoothness, 0.005f, 0.0f, 0.5f))
                            runtimeDirty = true;
                        if (ImGui::DragFloat("Spec Threshold", &mc->specularThreshold, 0.01f, 0.0f, 1.0f))
                            runtimeDirty = true;
                        if (ImGui::DragFloat("Rim Strength", &mc->rimStrength, 0.01f, 0.0f, 2.0f))
                            runtimeDirty = true;
                        if (ImGui::ColorEdit3("Shadow Tint", &mc->shadowTint.x))
                            runtimeDirty = true;
                        if (ImGui::DragFloat("Outline Width", &mc->outlineWidth, 0.0005f, 0.0f, 0.05f, "%.4f"))
                            runtimeDirty = true;
                        if (ImGui::ColorEdit4("Outline Color", &mc->outlineColor.x))
                            runtimeDirty = true;

                        char rbuf[256];
                        strncpy_s(rbuf, mc->rampTexturePath.c_str(), sizeof(rbuf) - 1);
                        rbuf[sizeof(rbuf) - 1] = '\0';
                        ImGui::Text("Ramp Texture (optional)");
                        if (ImGui::InputText("##MatRamp", rbuf, sizeof(rbuf))) {
                            mc->rampTexturePath = rbuf;
                            runtimeDirty = true;
                        }
                        {
                            const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                            std::string droppedPath;
                            if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                                mc->rampTexturePath = droppedPath;
                                runtimeDirty = true;
                            }
                        }
                        if (!isToon)
                            ImGui::TreePop();
                    }
                }

                EditorUndo_EndComponentEdit(g_selNode, "Material");
                if (ImGui::Button("Remove Material")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Material");
                    g_selNode->RemoveComponent<MaterialComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            // Effect property editor
            if (EffectComponent* ec = g_selNode->GetComponent<EffectComponent>()) {
                ImGui::Separator();
                ImGui::Text("Effect");
                EditorUndo_BeginComponentEdit(g_selNode, "Effect");

                std::string path = ec->effectPath;
                char buf[256];
                strncpy_s(buf, path.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                ImGui::Text("Path"); ImGui::SameLine(60);
                if (ImGui::InputText("##EffPath", buf, sizeof(buf))) {
                    ec->SetEffectPath(buf);
                    runtimeDirty = true;
                }
                {
                    const char* effectExts[] = { ".efk", ".efkefc" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(effectExts, IM_ARRAYSIZE(effectExts), droppedPath)) {
                        ec->SetEffectPath(droppedPath.c_str());
                        runtimeDirty = true;
                    }
                }

                if (ImGui::Checkbox("Auto Play", &ec->autoPlay)) {
                    runtimeDirty = true;
                }

                if (ImGui::Checkbox("Auto Destroy", &ec->autoDestroy)) {
                    runtimeDirty = true;
                }

                ImGui::Text("Speed"); ImGui::SameLine(60);
                if (ImGui::SliderFloat("##EffSpeed", &ec->speed, 0.0f, 5.0f)) {
                    runtimeDirty = true;
                }

                if (ImGui::Button("Play")) {
                    ec->Play();
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    ec->Stop();
                }

                EditorUndo_EndComponentEdit(g_selNode, "Effect");
                if (ImGui::Button("Remove Effect")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Effect");
                    g_selNode->RemoveComponent<EffectComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            if (AudioComponent* ac = g_selNode->GetComponent<AudioComponent>()) {
                ImGui::Separator();
                ImGui::Text("Audio");
                EditorUndo_BeginComponentEdit(g_selNode, "Audio");

                std::string path = ac->clipPath;
                char buf[256];
                strncpy_s(buf, path.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                ImGui::Text("Clip"); ImGui::SameLine(80);
                if (ImGui::InputText("##AudioClip", buf, sizeof(buf))) {
                    ac->SetClipPath(buf);
                    runtimeDirty = true;
                }
                {
                    const char* wavExts[] = { ".wav" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(wavExts, IM_ARRAYSIZE(wavExts), droppedPath)) {
                        ac->SetClipPath(droppedPath);
                        runtimeDirty = true;
                    }
                }

                const char* busPreview = "SFX";
                if (ac->bus == AudioType::BGM) busPreview = "BGM";
                else if (ac->bus == AudioType::VOICE) busPreview = "Voice";
                ImGui::Text("Bus"); ImGui::SameLine(80);
                if (ImGui::BeginCombo("##AudioBus", busPreview)) {
                    if (ImGui::Selectable("BGM", ac->bus == AudioType::BGM)) {
                        ac->bus = AudioType::BGM;
                        runtimeDirty = true;
                    }
                    if (ImGui::Selectable("SFX", ac->bus == AudioType::SFX)) {
                        ac->bus = AudioType::SFX;
                        runtimeDirty = true;
                    }
                    if (ImGui::Selectable("Voice", ac->bus == AudioType::VOICE)) {
                        ac->bus = AudioType::VOICE;
                        runtimeDirty = true;
                    }
                    ImGui::EndCombo();
                }

                ImGui::Text("Volume"); ImGui::SameLine(80);
                if (ImGui::SliderFloat("##AudioVol", &ac->volume, 0.0f, 1.0f, "%.2f")) {
                    ac->ApplyLiveSettings();
                    runtimeDirty = true;
                }

                if (ImGui::Checkbox("Loop", &ac->loop))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Play On Awake", &ac->autoPlay))
                    runtimeDirty = true;
                if (ImGui::Checkbox("3D", &ac->spatial))
                    runtimeDirty = true;
                if (ac->spatial) {
                    ImGui::Text("Range"); ImGui::SameLine(80);
                    if (ImGui::DragFloat("##AudioRange", &ac->distanceScale, 0.5f, 0.1f, 500.0f, "%.1f")) {
                        ac->ApplyLiveSettings();
                        runtimeDirty = true;
                    }
                }

                if (ImGui::Button("Play##Audio"))
                    ac->Play();
                ImGui::SameLine();
                if (ImGui::Button("Stop##Audio"))
                    ac->Stop();

                EditorUndo_EndComponentEdit(g_selNode, "Audio");
                if (ImGui::Button("Remove Audio")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Audio");
                    g_selNode->RemoveComponent<AudioComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            if (AudioListenerComponent* alc = g_selNode->GetComponent<AudioListenerComponent>()) {
                (void)alc;
                ImGui::Separator();
                ImGui::Text("AudioListener");
                ImGui::TextDisabled("3D ear. Active camera's listener is used.");
                if (ImGui::Button("Remove AudioListener")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "AudioListener");
                    g_selNode->RemoveComponent<AudioListenerComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            // Script property editor
            if (ScriptComponent* sc = g_selNode->GetComponent<ScriptComponent>()) {
                Script* script = sc->GetScript();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f),
                    "Script: %s", sc->GetScriptClass().c_str());
                if (script) {
                    EditorUndo_BeginComponentEdit(g_selNode, "Script");
                    nlohmann::json props = sc->GetEditableProperties();
                    if (props.is_object() && !props.empty()) {
                        bool changed = false;
                        for (auto& [key, val] : props.items()) {
                            if (DrawScriptPropertyField(g_selNode, key, val))
                                changed = true;
                        }
                        if (changed) {
                            sc->ApplyProperties(props);
                            runtimeDirty = true;
                        }
                    }
                    else {
                        ImGui::TextDisabled("(no editable properties)");
                    }
                    EditorUndo_EndComponentEdit(g_selNode, "Script");
                }
                else {
                    ImGui::TextDisabled("(script not loaded)");
                }
                if (ImGui::Button("Remove Script")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Script");
                    g_selNode->RemoveComponent<ScriptComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            // 2D physics settings are authored before Play; runtime owns the bodies.
            if (auto* body = g_selNode->GetComponent<Rigidbody2DComponent>()) {
                ImGui::Separator();
                ImGui::TextUnformatted("Rigidbody 2D");
                ImGui::PushID("Rigidbody2D");
                ImGui::BeginDisabled(EngineGUI_IsPlaying());
                EditorUndo_BeginComponentEdit(g_selNode, "Rigidbody2D");
                ImGui::PushItemWidth(-1);
                ImGui::TextUnformatted("Body Type");
                int type = static_cast<int>(body->bodyType);
                if (ImGui::Combo("##Type", &type, "Static\0Dynamic\0Kinematic\0")) {
                    body->bodyType = static_cast<BodyType2D>(type);
                    runtimeDirty = true;
                }
                auto number = [&](const char* label, float* value, float minimum, float maximum) {
                    ImGui::TextUnformatted(label);
                    std::string id = std::string("##") + label;
                    if (ImGui::DragFloat(id.c_str(), value, 0.01f, minimum, maximum)) runtimeDirty = true;
                };
                number("Mass", &body->mass, 0.001f, 100000.0f);
                number("Gravity Scale", &body->gravityScale, -100.0f, 100.0f);
                number("Linear Damping", &body->linearDamping, 0.0f, 100.0f);
                ImGui::TextUnformatted("Initial Velocity (XY)");
                if (ImGui::DragFloat2("##Velocity", &body->initialVelocity.x, 0.01f)) runtimeDirty = true;
                if (ImGui::Checkbox("Fixed Rotation", &body->fixedRotation)) runtimeDirty = true;
                if (ImGui::Checkbox("Continuous Collision", &body->bullet)) runtimeDirty = true;
                ImGui::PopItemWidth();
                ImGui::TextWrapped("Dynamic and kinematic bodies must be root objects with uniform positive XY scale. Edit physics settings before Play.");
                EditorUndo_EndComponentEdit(g_selNode, "Rigidbody2D");
                if (ImGui::Button("Remove Rigidbody 2D")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Rigidbody2D");
                    g_selNode->RemoveComponent<Rigidbody2DComponent>();
                    runtimeDirty = true;
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            if (auto* collider = g_selNode->GetComponent<Collider2DComponent>()) {
                ImGui::Separator();
                ImGui::TextUnformatted("Collider 2D");
                ImGui::PushID("Collider2D");
                EditorUndo_BeginComponentEdit(g_selNode, "Collider2D");
                if (ImGui::Checkbox("Show Debug Collider", &collider->showDebugCollider)) runtimeDirty = true;
                EditorUndo_EndComponentEdit(g_selNode, "Collider2D");
                ImGui::BeginDisabled(EngineGUI_IsPlaying());
                EditorUndo_BeginComponentEdit(g_selNode, "Collider2D");
                ImGui::PushItemWidth(-1);
                if (ImGui::Checkbox("Enabled", &collider->enabled)) runtimeDirty = true;
                ImGui::TextUnformatted("Shape");
                int shape = static_cast<int>(collider->shape);
                if (ImGui::Combo("##Shape", &shape, "Box\0Circle\0")) {
                    collider->shape = static_cast<Shape2D>(shape);
                    runtimeDirty = true;
                }
                if (collider->shape == Shape2D::Box) {
                    ImGui::TextUnformatted("Size (XY)");
                    if (ImGui::DragFloat2("##Size", &collider->size.x, 0.01f, 0.001f, 100000.0f)) runtimeDirty = true;
                } else {
                    ImGui::TextUnformatted("Radius");
                    if (ImGui::DragFloat("##Radius", &collider->radius, 0.01f, 0.001f, 100000.0f)) runtimeDirty = true;
                }
                ImGui::TextUnformatted("Offset (XY)");
                if (ImGui::DragFloat2("##Offset", &collider->offset.x, 0.01f)) runtimeDirty = true;
                if (ImGui::Checkbox("Trigger", &collider->isTrigger)) runtimeDirty = true;
                ImGui::TextUnformatted("Friction");
                if (ImGui::DragFloat("##Friction", &collider->friction, 0.01f, 0.0f, 1.0f)) runtimeDirty = true;
                ImGui::TextUnformatted("Restitution");
                if (ImGui::DragFloat("##Restitution", &collider->restitution, 0.01f, 0.0f, 1.0f)) runtimeDirty = true;
                ImGui::TextUnformatted("Category Bits (hex)");
                if (ImGui::InputScalar("##Category", ImGuiDataType_U32, &collider->categoryBits, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal)) runtimeDirty = true;
                ImGui::TextUnformatted("Mask Bits (hex)");
                if (ImGui::InputScalar("##Mask", ImGuiDataType_U32, &collider->maskBits, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal)) runtimeDirty = true;
                ImGui::PopItemWidth();
                ImGui::TextWrapped("Box size and circle radius use local world units. Use uniform positive XY scale; settings are locked during Play.");
                EditorUndo_EndComponentEdit(g_selNode, "Collider2D");
                if (ImGui::Button("Remove Collider 2D")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Collider2D");
                    g_selNode->RemoveComponent<Collider2DComponent>();
                    runtimeDirty = true;
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }

            if (SpriteRendererComponent* sprite = g_selNode->GetComponent<SpriteRendererComponent>()) {
                ImGui::Separator();
                ImGui::Text("Sprite Renderer");
                ImGui::PushID("SpriteRenderer");
                EditorUndo_BeginComponentEdit(g_selNode, "SpriteRenderer");
                char textureBuf[1024];
                strncpy_s(textureBuf, sprite->texturePath.c_str(), _TRUNCATE);
                if (ImGui::InputText("Texture", textureBuf, sizeof(textureBuf))) {
                    sprite->texturePath = textureBuf;
                    runtimeDirty = true;
                }
                const char* extensions[] = { ".png", ".jpg", ".jpeg", ".bmp" };
                std::string droppedPath;
                if (AcceptAssetPathDrop(extensions, IM_ARRAYSIZE(extensions), droppedPath)) {
                    sprite->texturePath = droppedPath;
                    runtimeDirty = true;
                }
                if (ImGui::DragFloat("Pixels Per Unit", &sprite->pixelsPerUnit, 1.0f, 0.01f, 100000.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat4("Source Rect", &sprite->sourceRect.x, 1.0f, 0.0f, 100000.0f))
                    runtimeDirty = true;
                ImGui::TextDisabled("Source rect: x, y, width, height in pixels. Zero size uses full texture.");
                if(DrawPivotPreset("Pivot",sprite->pivot,sprite)) runtimeDirty=true;
                if (ImGui::ColorEdit4("Tint", &sprite->tint.x)) runtimeDirty = true;
                if (ImGui::Checkbox("Flip X", &sprite->flipX)) runtimeDirty = true;
                if (ImGui::Checkbox("Flip Y", &sprite->flipY)) runtimeDirty = true;
                if (ImGui::InputInt("Sorting Layer", &sprite->sortingLayer)) runtimeDirty = true;
                if (ImGui::InputInt("Order In Layer", &sprite->orderInLayer)) runtimeDirty = true;
                EditorUndo_EndComponentEdit(g_selNode, "SpriteRenderer");
                if (ImGui::Button("Remove Sprite Renderer")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "SpriteRenderer");
                    g_selNode->RemoveComponent<SpriteRendererComponent>();
                    runtimeDirty = true;
                }
                ImGui::PopID();
                ImGui::Spacing();
            }

            if (auto* animator = g_selNode->GetComponent<SpriteAnimatorComponent>()) {
                ImGui::Separator();
                ImGui::Text("Sprite Animator");
                ImGui::PushID("SpriteAnimator");
                EditorUndo_BeginComponentEdit(g_selNode, "SpriteAnimator");
                bool changed = false;
                char defaultName[256];
                strncpy_s(defaultName, animator->defaultClip.c_str(), _TRUNCATE);
                if (ImGui::InputText("Default Clip", defaultName, sizeof(defaultName))) {
                    animator->defaultClip = defaultName;
                    changed = true;
                }
                changed |= ImGui::Checkbox("Auto Play", &animator->autoPlay);
                static char newClipName[256] = "idle";
                ImGui::InputText("New Clip Name", newClipName, sizeof(newClipName));
                if (ImGui::Button("Add Clip") && newClipName[0]) {
                    changed |= animator->clips.emplace(newClipName, SpriteAnimationClip{}).second;
                }
                static char metadataPath[1024] = "";
                static char importName[256] = "idle";
                static bool importFailed = false;
                ImGui::InputText("Sheet Metadata JSON", metadataPath, sizeof(metadataPath));
                const char* jsonExtensions[] = { ".json" };
                std::string metadataDrop;
                if (AcceptAssetPathDrop(jsonExtensions, IM_ARRAYSIZE(jsonExtensions), metadataDrop))
                    strncpy_s(metadataPath, metadataDrop.c_str(), _TRUNCATE);
                ImGui::InputText("Import Clip Name", importName, sizeof(importName));
                if (ImGui::Button("Import Sheet")) {
                    const bool imported = animator->ImportSheet(metadataPath, importName);
                    importFailed = !imported;
                    changed |= imported;
                }
                if (importFailed) ImGui::TextDisabled("Import failed. Check metadata path and frame rectangles.");
                for (auto it = animator->clips.begin(); it != animator->clips.end();) {
                    ImGui::PushID(it->first.c_str());
                    bool removeClip = false;
                    if (ImGui::TreeNode("Clip", "%s", it->first.c_str())) {
                        auto& clip = it->second;
                        char texture[1024];
                        strncpy_s(texture, clip.texture.c_str(), _TRUNCATE);
                        if (ImGui::InputText("Texture", texture, sizeof(texture))) {
                            clip.texture = texture;
                            changed = true;
                        }
                        const char* extensions[] = { ".png", ".jpg", ".jpeg", ".bmp" };
                        std::string droppedPath;
                        if (AcceptAssetPathDrop(extensions, IM_ARRAYSIZE(extensions), droppedPath)) {
                            clip.texture = droppedPath;
                            changed = true;
                        }
                        float fps = static_cast<float>(clip.fps);
                        if (ImGui::DragFloat("FPS", &fps, 0.1f, 0.01f, 1000.0f)) {
                            clip.fps = (std::max)(0.01f, fps);
                            changed = true;
                        }
                        changed |= ImGui::Checkbox("Loop", &clip.loop);
                        for (size_t i = 0; i < clip.frames.size();) {
                            ImGui::PushID(static_cast<int>(i));
                            auto& frame = clip.frames[i];
                            float rect[] = {frame.x, frame.y, frame.w, frame.h};
                            if (ImGui::DragFloat4("Frame XYWH", rect, 1.0f, 0.0f, 100000.0f)) {
                                frame = {rect[0], rect[1], (std::max)(1.0f, rect[2]), (std::max)(1.0f, rect[3])};
                                changed = true;
                            }
                            const bool removeFrame = ImGui::Button("Remove Frame");
                            ImGui::PopID();
                            if (removeFrame) {
                                clip.frames.erase(clip.frames.begin() + i);
                                changed = true;
                            } else ++i;
                        }
                        if (ImGui::Button("Add Frame")) {
                            clip.frames.push_back(SpriteAnimationFrame{});
                            changed = true;
                        }
                        removeClip = ImGui::Button("Remove Clip");
                        ImGui::TreePop();
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
                    runtimeDirty = true;
                }
                EditorUndo_EndComponentEdit(g_selNode, "SpriteAnimator");
                if (ImGui::Button("Remove Sprite Animator")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "SpriteAnimator");
                    g_selNode->RemoveComponent<SpriteAnimatorComponent>();
                    runtimeDirty = true;
                }
                ImGui::PopID();
                ImGui::Spacing();
            }

            if (BillboardComponent* bbc = g_selNode->GetComponent<BillboardComponent>()) {
                ImGui::Separator();
                ImGui::Text("Billboard");
                EditorUndo_BeginComponentEdit(g_selNode, "Billboard");

                char texBuf[256];
                strncpy_s(texBuf, bbc->texturePath.c_str(), sizeof(texBuf) - 1);
                texBuf[sizeof(texBuf) - 1] = '\0';
                ImGui::Text("Texture");
                if (ImGui::InputText("##BillboardTex", texBuf, sizeof(texBuf))) {
                    bbc->texturePath = texBuf;
                    bbc->InvalidateTextureCache();
                    runtimeDirty = true;
                }
                {
                    const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                        bbc->texturePath = droppedPath;
                        bbc->InvalidateTextureCache();
                        runtimeDirty = true;
                    }
                }
                if (ImGui::DragFloat("Size X", &bbc->sizeX, 0.05f, 0.01f, 1000.0f))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Size Y", &bbc->sizeY, 0.05f, 0.01f, 1000.0f))
                    runtimeDirty = true;
                if (ImGui::ColorEdit4("Color##Billboard", &bbc->color.x))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Rotation##Billboard", &bbc->rotation, 0.5f))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Ground Aligned", &bbc->groundAligned))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Enabled##Billboard", &bbc->enabled))
                    runtimeDirty = true;

                EditorUndo_EndComponentEdit(g_selNode, "Billboard");
                if (ImGui::Button("Remove Billboard")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Billboard");
                    g_selNode->RemoveComponent<BillboardComponent>();
                    runtimeDirty = true;
                }
                ImGui::Spacing();
            }

            if (TerrainComponent* tc = g_selNode->GetComponent<TerrainComponent>()) {
                ImGui::Separator();
                ImGui::Text("Terrain");
                ImGui::PushID("TerrainSection");
                EditorUndo_BeginComponentEdit(g_selNode, "Terrain");
                terrain::Settings& ts = tc->settings;

                char hmBuf[256];
                strncpy_s(hmBuf, ts.heightmap.c_str(), sizeof(hmBuf) - 1);
                hmBuf[sizeof(hmBuf) - 1] = '\0';
                ImGui::Text("Heightmap (.r16, empty = flat)");
                if (ImGui::InputText("##TerrainHeightmap", hmBuf, sizeof(hmBuf))) {
                    ts.heightmap = hmBuf;
                    tc->InvalidateHeightmap();
                    runtimeDirty = true;
                }
                {
                    const char* hmExts[] = { ".r16" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(hmExts, IM_ARRAYSIZE(hmExts), droppedPath)) {
                        ts.heightmap = droppedPath;
                        tc->InvalidateHeightmap();
                        runtimeDirty = true;
                    }
                }
                if (ImGui::Button("Reload Heightmap"))
                    tc->InvalidateHeightmap();
                tc->EnsureLoaded();
                if (tc->IsLoaded()) {
                    const terrain::ChunkGrid& grid = tc->GetChunkGrid();
                    ImGui::TextDisabled("%d x %d samples, %d x %d chunks of %d cells",
                        tc->GetHeightmap().resolution, tc->GetHeightmap().resolution,
                        grid.chunksPerSide, grid.chunksPerSide, grid.chunkQuads);
                } else if (!tc->GetLoadError().empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", tc->GetLoadError().c_str());
                }

                if (ImGui::DragFloat("Size X", &ts.layout.sizeX, 1.0f, terrain::kMinSize, terrain::kMaxSize))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Size Z", &ts.layout.sizeZ, 1.0f, terrain::kMinSize, terrain::kMaxSize))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Height Scale", &ts.layout.heightScale, 0.5f, 0.0f, terrain::kMaxHeightScale))
                    runtimeDirty = true;
                if (ImGui::DragFloat("Height Offset", &ts.layout.heightOffset, 0.5f,
                        -terrain::kMaxHeightOffset, terrain::kMaxHeightOffset))
                    runtimeDirty = true;
                ImGui::TextDisabled("Position moves the terrain; rotation and scale are ignored.");

                {
                    static const int kChunkSizes[] = { 8, 16, 32, 64, 128, 256 };
                    char preview[16];
                    snprintf(preview, sizeof(preview), "%d", ts.chunkQuads);
                    if (ImGui::BeginCombo("Chunk Cells", preview)) {
                        for (int q : kChunkSizes) {
                            char label[16];
                            snprintf(label, sizeof(label), "%d", q);
                            if (ImGui::Selectable(label, ts.chunkQuads == q)) {
                                ts.chunkQuads = q;
                                runtimeDirty = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (ImGui::DragFloat("LOD Bias##Terrain", &ts.lodBias, 0.05f, terrain::kMinLodBias, terrain::kMaxLodBias))
                    runtimeDirty = true;

                char texBuf[256];
                strncpy_s(texBuf, ts.texture.c_str(), sizeof(texBuf) - 1);
                texBuf[sizeof(texBuf) - 1] = '\0';
                ImGui::Text("Texture");
                if (ImGui::InputText("##TerrainTexture", texBuf, sizeof(texBuf))) {
                    ts.texture = texBuf;
                    tc->InvalidateTexture();
                    runtimeDirty = true;
                }
                {
                    const char* texExts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" };
                    std::string droppedPath;
                    if (AcceptAssetPathDrop(texExts, IM_ARRAYSIZE(texExts), droppedPath)) {
                        ts.texture = droppedPath;
                        tc->InvalidateTexture();
                        runtimeDirty = true;
                    }
                }
                if (ImGui::DragFloat("Texture Tile Size", &ts.textureTileSize, 0.1f,
                        terrain::kMinTextureTileSize, terrain::kMaxTextureTileSize))
                    runtimeDirty = true;
                {
                    float color[3] = { ts.colorR, ts.colorG, ts.colorB };
                    if (ImGui::ColorEdit3("Color##Terrain", color)) {
                        ts.colorR = color[0];
                        ts.colorG = color[1];
                        ts.colorB = color[2];
                        runtimeDirty = true;
                    }
                }
                if (ImGui::SliderFloat("Roughness##Terrain", &ts.roughness, 0.0f, 1.0f))
                    runtimeDirty = true;
                if (ImGui::SliderFloat("Metallic##Terrain", &ts.metallic, 0.0f, 1.0f))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Cast Shadows##Terrain", &ts.castShadows))
                    runtimeDirty = true;
                if (ImGui::Checkbox("Enabled##Terrain", &ts.enabled))
                    runtimeDirty = true;
                ts = terrain::SanitizeSettings(ts);

                EditorUndo_EndComponentEdit(g_selNode, "Terrain");
                if (ImGui::Button("Remove Terrain")) {
                    EditorUndo_PushRemoveComponent(g_selNode, "Terrain");
                    g_selNode->RemoveComponent<TerrainComponent>();
                    runtimeDirty = true;
                }
                ImGui::PopID();
                ImGui::Spacing();
            }

            ImGui::Separator();
            ImGui::Text("Components:");
            for (const auto& kv : g_selNode->GetComponents())
                ImGui::BulletText("%s", kv.second->GetTypeName());
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##AddComponent", "Add Component...", ImGuiComboFlags_HeightLarge)) {
                static ImGuiTextFilter componentFilter;
                const bool opening=ImGui::IsWindowAppearing();
                if(opening) {componentFilter.Clear();ImGui::SetKeyboardFocusHere();}
                componentFilter.Draw("##ComponentSearch", -1);
                ImGui::Separator();
                int availableCount=0;
                auto option=[&](const char* label,const char* typeName,bool available,auto create) {
                    if(!available||!componentFilter.PassFilter(label))return;
                    ++availableCount;
                    if(ImGui::Selectable(label)) {
                        create();
                        EditorUndo_PushAddComponent(g_selNode,typeName);
                        runtimeDirty=true;
                        ImGui::CloseCurrentPopup();
                    }
                };
                option("Transform","SceneTransform",!g_selNode->GetComponent<engine::Transform>(),[&] {g_selNode->AddComponent<engine::Transform>();});
                option("Visual","visual",!g_selNode->GetComponent<VisualComponent>(),[&] {g_selNode->AddComponent<VisualComponent>();});
                option("Sprite Renderer","SpriteRenderer",!g_selNode->GetComponent<SpriteRendererComponent>(),[&] {g_selNode->AddComponent<SpriteRendererComponent>();});
                option("Sprite Animator","SpriteAnimator",!g_selNode->GetComponent<SpriteAnimatorComponent>(),[&] {g_selNode->AddComponent<SpriteAnimatorComponent>();});
                option("Billboard","Billboard",!g_selNode->GetComponent<BillboardComponent>(),[&] {g_selNode->AddComponent<BillboardComponent>();});
                option("Terrain","Terrain",!g_selNode->GetComponent<TerrainComponent>(),[&] {g_selNode->AddComponent<TerrainComponent>();});
                option("Material","Material",!g_selNode->GetComponent<MaterialComponent>(),[&] {g_selNode->AddComponent<MaterialComponent>();});
                option("Camera","Camera",!g_selNode->GetComponent<CameraComponent>(),[&] {g_selNode->AddComponent<CameraComponent>();});
                option("Directional Light","DirectionalLight",!g_selNode->GetComponent<DirectionalLightComponent>(),[&] {g_selNode->AddComponent<DirectionalLightComponent>();});
                option("Point Light","PointLight",!g_selNode->GetComponent<PointLightComponent>(),[&] {g_selNode->AddComponent<PointLightComponent>();});
                option("Spot Light","SpotLight",!g_selNode->GetComponent<SpotLightComponent>(),[&] {g_selNode->AddComponent<SpotLightComponent>();});
                option("Rigidbody 3D","Rigidbody",!g_selNode->GetComponent<RigidbodyComponent>(),[&] {g_selNode->AddComponent<RigidbodyComponent>();});
                option("Rigidbody 2D","Rigidbody2D",!g_selNode->GetComponent<Rigidbody2DComponent>(),[&] {g_selNode->AddComponent<Rigidbody2DComponent>();});
                option("Script","Script",!g_selNode->GetComponent<ScriptComponent>(),[&] {g_selNode->AddComponent<ScriptComponent>();});
                option("Effect","Effect",!g_selNode->GetComponent<EffectComponent>(),[&] {g_selNode->AddComponent<EffectComponent>();});
                option("Audio","Audio",!g_selNode->GetComponent<AudioComponent>(),[&] {g_selNode->AddComponent<AudioComponent>();});
                option("Audio Listener","AudioListener",!g_selNode->GetComponent<AudioListenerComponent>(),[&] {g_selNode->AddComponent<AudioListenerComponent>();});
                const auto* currentCollider=FindColliderComponent(g_selNode);
                const bool canAdd3D=!currentCollider||currentCollider->colliderType==ColliderType::NONE;
                auto removeEmptyCollider=[&] {if(currentCollider)RemoveColliderComponent(g_selNode);};
                option("Box Collider 3D","BoxCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<BoxColliderComponent>();});
                option("Sphere Collider 3D","SphereCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<SphereColliderComponent>();});
                option("Capsule Collider 3D","CapsuleCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<CapsuleColliderComponent>();});
                option("Cylinder Collider 3D","CylinderCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<CylinderColliderComponent>();});
                option("Mesh Collider 3D","MeshCollider",canAdd3D,[&] {removeEmptyCollider();g_selNode->AddComponent<MeshColliderComponent>();});
                option("Box Collider 2D","Collider2D",!g_selNode->GetComponent<Collider2DComponent>(),[&] {g_selNode->AddComponent<Collider2DComponent>()->shape=Shape2D::Box;});
                option("Circle Collider 2D","Collider2D",!g_selNode->GetComponent<Collider2DComponent>(),[&] {g_selNode->AddComponent<Collider2DComponent>()->shape=Shape2D::Circle;});
                if(!availableCount)ImGui::TextDisabled("No matching components available.");
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            if (runtimeDirty) {
                EngineGUI_MarkSceneDirty();
                SyncSceneRuntime();
            }

            // Save as prefab
            ImGui::Separator();
            if (ImGui::Button("Save as Prefab", ImVec2(-1, 0))) {
                if (!fs::exists(PREFAB_DIR)) fs::create_directories(PREFAB_DIR);
                std::string prefabName = g_selNode->GetName();
                for (char& c : prefabName)
                    if (c == ' ' || c == '/' || c == '\\') c = '_';
                PrefabSerializer::SaveToFile(*g_selNode, PREFAB_DIR + prefabName + ".json");
            }

            ImGui::Separator();
            if (ImGui::Button("Delete Node", ImVec2(-1, 30))) {
                EditorUndo_DiscardComponentEdit();
                s_lastSelNode = nullptr;
                DeleteGameObject(g_selNode);
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
    ImGui::End();
}
