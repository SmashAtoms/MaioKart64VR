#include "xr_app.hpp"
#include <android/log.h>

namespace aether {void Log(const std::string& message){__android_log_write(ANDROID_LOG_INFO,"Aether64",message.c_str());}}
extern "C" void android_main(android_app* app){
    app_dummy();
    aether::XrApp engine(app);app->userData=&engine;
    app->onAppCmd=[](android_app* state,int32_t command){static_cast<aether::XrApp*>(state->userData)->Command(command);};
    engine.Run();app->userData=nullptr;
}
