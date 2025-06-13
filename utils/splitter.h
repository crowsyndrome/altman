#pragma once
#include <imgui.h>

inline void Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1 = 20.0f, float min_size2 = 20.0f)
{
    using namespace ImGui;
    ImVec2 backup_pos = GetCursorPos();
    if (split_vertically)
        SetCursorPosX(backup_pos.x + *size1);
    else
        SetCursorPosY(backup_pos.y + *size1);

    PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
    PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
    Button("##Splitter", split_vertically ? ImVec2(thickness, -1.0f) : ImVec2(-1.0f, thickness));
    PopStyleColor(3);
    SetItemAllowOverlap();

    if (IsItemActive())
    {
        float delta = split_vertically ? GetIO().MouseDelta.x : GetIO().MouseDelta.y;
        if (delta < min_size1 - *size1)
            delta = min_size1 - *size1;
        if (delta > *size2 - min_size2)
            delta = *size2 - min_size2;
        *size1 += delta;
        *size2 -= delta;
    }
    SetCursorPos(backup_pos);
}
