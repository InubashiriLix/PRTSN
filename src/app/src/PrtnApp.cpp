#include "src/app/inc/PrtnApp.h"

#include "src/app/examples/inc/EspNowEchoApp.h"

namespace SelectedApp = EspNowEchoApp;

void PrtnApp::setup() {
    SelectedApp::setup();
}

void PrtnApp::idle() {
    SelectedApp::idle();
}
