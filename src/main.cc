#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#include <iostream>
#include <string>
#include <fstream>
#include "Core/Application.h"
#include "Core/log.h"
#include "Ui/MainLayer.h"
#include "Ui/LogLayer.h"
#include <Ui/TarkovMapTool.h>

#define VLD 0

#if VLD
#include <vld.h>
#endif

int main(int argc, char **argv)
{
    initLogger(INFO);
    Application *app = new Application("TarkovMapTrackerCC", 1920, 1080);
    app->PushLayer<MainLayer>();
    app->PushLayer<TarkovMapTool>();
    app->Run();
    return 0;
}