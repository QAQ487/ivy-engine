#pragma once
#include "imgui/imgui.h"
#include <DirectXMath.h>
#include <cmath>

// Normalized pivots use +Y up: (0,1) is top-left for world sprites and UI alike.
inline bool DrawPivotPreset(const char* label,DirectX::XMFLOAT2& pivot,const void* owner) {
    static const char* names[]={"Center","Top Left","Top Center","Top Right",
        "Middle Left","Middle Right","Bottom Left","Bottom Center","Bottom Right","Custom"};
    static const DirectX::XMFLOAT2 values[]={{.5f,.5f},{0,1},{.5f,1},{1,1},
        {0,.5f},{1,.5f},{0,0},{.5f,0},{1,0}};
    static const void* lastOwner=nullptr;
    static bool custom=false;
    if(lastOwner!=owner) {lastOwner=owner;custom=false;}
    int current=9;
    for(int i=0;i<9;++i) if(std::fabs(pivot.x-values[i].x)<.00001f&&std::fabs(pivot.y-values[i].y)<.00001f) {current=i;break;}
    if(custom) current=9;
    bool changed=false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1);
    if(ImGui::Combo("##Preset",&current,names,IM_ARRAYSIZE(names))) {
        custom=current==9;
        if(!custom) {pivot=values[current];changed=true;}
    }
    if(current==9) {
        ImGui::TextUnformatted("Custom Pivot");
        ImGui::SetNextItemWidth(-1);
        changed|=ImGui::DragFloat2("##Custom",&pivot.x,.01f,0,1);
    }
    ImGui::PopID();
    return changed;
}
