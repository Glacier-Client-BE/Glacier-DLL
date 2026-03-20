#include "NotificationSystem.h"
#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <chrono> // Added for std::chrono
#include <cstdio> // Added for snprintf

// Helper function to resolve the C3861 error
static ImU32 ImAlphaBlendColors(ImU32 col, float alpha) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    c.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

NotificationSystem::NotificationSystem()
    : ModuleBase("Notifications","On-screen toast notifications","notificationsystem",ModuleCategory::Utility,0.f,0.f)
{
    m_settings.defineFloat("duration","Duration (s)",  3.f,1.f,10.f);
    m_settings.defineFloat("width",   "Toast Width", 240.f,120.f,400.f);
    m_settings.defineFloat("posX",    "Position X",   -1.f,-1.f,9999.f); // -1 = auto right
    m_settings.defineFloat("posY",    "Position Y",   60.f, 0.f,2000.f);
    m_settings.defineBool ("animate", "Slide Anim",   true);
}

void NotificationSystem::push(const std::string& title,const std::string& msg,ImU32 accent){
    s_queue.push_back({title,msg,accent,std::chrono::high_resolution_clock::now()});
    if(s_queue.size()>5) s_queue.pop_front();
}

void NotificationSystem::onRenderImGui(){
    float dur=m_settings.getFloat("duration");
    float tw =m_settings.getFloat("width");
    float py =m_settings.getFloat("posY");
    float px =m_settings.getFloat("posX");
    float screenW=ImGui::GetIO().DisplaySize.x;
    
    if(px<0) px=screenW-tw-16.f;
    
    auto now=std::chrono::high_resolution_clock::now();
    
    // purge expired
    while(!s_queue.empty()&&std::chrono::duration<float>(now-s_queue.front().born).count()>dur)
        s_queue.pop_front();
        
    float oy=py;
    for(auto& n: s_queue){
        float age=std::chrono::duration<float>(now-n.born).count();
        float fade=1.f; 
        if(age > dur - 0.4f) fade = (dur - age) / 0.4f;
        
        float slideX=0.f;
        if(m_settings.getBool("animate")&&age<0.2f) slideX=(1.f-(age/.2f))*(tw+20.f);
        
        ImGui::SetNextWindowPos({px+slideX,oy},ImGuiCond_Always);
        ImGui::SetNextWindowSize({tw,58.f}); 
        ImGui::SetNextWindowBgAlpha(0.88f*fade);
        
        ImGuiWindowFlags f=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoSavedSettings;
            
        ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(0.172f,0.184f,0.2f,0.92f*fade));
        ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.447f,.537f,.855f,.3f*fade));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,8); 
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
        
        char wid[32]; 
        snprintf(wid,sizeof(wid),"##notif%p",&n);
        
        if(ImGui::Begin(wid,nullptr,f)){
            ImDrawList* dl=ImGui::GetWindowDrawList(); 
            ImVec2 bp=ImGui::GetWindowPos();
            
            // Fixed line 44: simplified call to use the helper function correctly
            dl->AddRectFilled(bp,{bp.x+4,bp.y+58}, ImAlphaBlendColors(n.accent, fade), 2);
            
            dl->AddText(nullptr,13.f,{bp.x+12,bp.y+8},IM_COL32(255,255,255,(int)(230*fade)),n.title.c_str());
            dl->AddText(nullptr,11.f,{bp.x+12,bp.y+26},IM_COL32(153,170,181,(int)(190*fade)),n.msg.c_str());
            
            // progress bar
            float prog=1.f-age/dur;
            dl->AddRectFilled({bp.x,bp.y+54},{bp.x+tw,bp.y+58},IM_COL32(35,39,42,200),0,ImDrawFlags_RoundCornersBottom);
            dl->AddRectFilled({bp.x,bp.y+54},{bp.x+tw*prog,bp.y+58}, ImAlphaBlendColors(n.accent, fade), 0, ImDrawFlags_RoundCornersBottom);
        }
        ImGui::End(); 
        ImGui::PopStyleVar(2); 
        ImGui::PopStyleColor(2);
        oy+=66.f;
    }
}