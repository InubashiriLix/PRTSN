#include "src/app/inc/PrtnApp.h"

// #include "src/app/examples/inc/EspNowEchoApp.h"
// #include "src/app/examples/inc/EmptyTemplateApp.h"
// #include "src/app/examples/inc/PwmTestApp.h"
#include "src/app/examples/inc/INMP441Example.h"

// namespace SelectedApp = EspNowEchoApp;
// namespace SelectedApp = EmptyTemplateApp;
// namespace SelectedApp = PwmTestApp;
namespace SelectedApp = INMP441Example;

void PrtnApp::setup() {
    SelectedApp::setup();
}

void PrtnApp::idle() {
    SelectedApp::idle();
}
