#include "aether/launcher.hpp"
#include <algorithm>

namespace aether {
void Launcher::Open(Page page){page_=page;selected_=0;}
void Launcher::SetGameState(bool data,bool available,std::string reason){gameData_=data;gameAvailable_=available;unavailable_=std::move(reason);}
void Launcher::Navigate(int delta){int n=static_cast<int>(GetPanel().rows.size());if(n)selected_=(selected_+delta+n)%n;}
Panel Launcher::GetPanel()const {
    Panel p;p.selected=selected_;p.message=message;
    const auto camera=settings.camera==CameraMode::Driver?"Driver":"Chase";
    switch(page_){
    case Page::Home:p.title="AETHER64 XR";p.rows={"Games","Controls test","Camera test","Settings","Diagnostics"};break;
    case Page::Games:p.title="MARIO KART 64";p.rows={gameData_?"ROM ready":"ROM or ZIP needed","Import ZIP or ROM file","Back"};p.message=gameAvailable_?"V1 is ready to start. Game data stays on this headset.":unavailable_;break;
    case Page::Controls:p.title="CONTROLS TEST";p.rows={"Recenter","Back"};p.message=inputStatus;break;
    case Page::CameraTest:p.title="CAMERA TEST";p.rows={"Recenter","Back"};p.message="Look left, right, behind, and lean. White marks span one meter. Cubes surround you.";break;
    case Page::Settings:p.title="SETTINGS";p.rows={std::string("Default camera: ")+camera,std::string("Diagnostics: ")+(settings.diagnostics?"On":"Off"),"Recenter","Back"};p.message="Driver view is the default. Steering uses the left thumbstick.";break;
    case Page::Diagnostics:p.title="DIAGNOSTICS";p.rows={"Export report","Back"};p.message=diagnostics;break;
    case Page::Pause:p.title="PAUSED";p.rows={"Resume",std::string("Camera: ")+camera,"Recenter","Return to launcher"};break;
    }
    return p;
}
UiCommand Launcher::Activate(){
    switch(page_){
    case Page::Home:
        switch(selected_){case 0:Open(Page::Games);break;case 1:Open(Page::Controls);return UiCommand::StartTest;case 2:Open(Page::CameraTest);return UiCommand::StartTest;case 3:Open(Page::Settings);break;case 4:Open(Page::Diagnostics);break;}break;
    case Page::Games:
        if(selected_==0){if(!gameData_)return UiCommand::Import;if(gameAvailable_)return UiCommand::Launch;message=unavailable_;}
        else if(selected_==1)return UiCommand::Import;else Open(Page::Home);break;
    case Page::Controls:case Page::CameraTest:if(selected_==0)return UiCommand::Recenter;Open(Page::Home);return UiCommand::ExitGame;
    case Page::Settings:
        if(selected_==0){settings.camera=settings.camera==CameraMode::Driver?CameraMode::Chase:CameraMode::Driver;return UiCommand::SaveSettings;}
        if(selected_==1){settings.diagnostics=!settings.diagnostics;return UiCommand::SaveSettings;}
        if(selected_==2)return UiCommand::Recenter;Open(Page::Home);break;
    case Page::Diagnostics:if(selected_==0)return UiCommand::ExportDiagnostics;Open(Page::Home);break;
    case Page::Pause:
        if(selected_==0)return UiCommand::Resume;
        if(selected_==1){settings.camera=settings.camera==CameraMode::Driver?CameraMode::Chase:CameraMode::Driver;return UiCommand::SaveSettings;}
        if(selected_==2)return UiCommand::Recenter;Open(Page::Home);return UiCommand::ExitGame;
    }
    return UiCommand::None;
}
UiCommand Launcher::Back(){if(page_==Page::Controls||page_==Page::CameraTest){Open(Page::Home);return UiCommand::ExitGame;}if(page_==Page::Pause)return UiCommand::None;Open(Page::Home);return UiCommand::None;}
}
