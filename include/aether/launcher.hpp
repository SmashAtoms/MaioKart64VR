#pragma once
#include "aether/adapter.hpp"
#include <string>
#include <vector>

namespace aether {
enum class Page { Home, Games, Controls, CameraTest, Settings, Diagnostics, Pause };
enum class UiCommand { None, Import, Launch, StartTest, Resume, ExitGame, Recenter, SaveSettings, ExportDiagnostics };
struct Settings { CameraMode camera=CameraMode::Driver; float renderScale=1; bool diagnostics=false; };
struct Panel { std::string title; std::vector<std::string> rows; int selected=0; std::string message; };
class Launcher {
    Page page_=Page::Home;
    int selected_=0;
    bool gameData_=false, gameAvailable_=false;
    std::string unavailable_="MK64 integration is not included in this diagnostic build.";
public:
    Settings settings;
    std::string message="Choose Import ZIP or ROM file. Your game file stays on this headset.";
    std::string inputStatus, diagnostics;
    Page CurrentPage() const {return page_;}
    void Open(Page page);
    void SetGameState(bool data,bool available,std::string reason={});
    void Navigate(int delta);
    UiCommand Activate();
    UiCommand Back();
    Panel GetPanel() const;
};
}
