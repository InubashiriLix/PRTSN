#include "src/app/inc/PrtnApp.h"

// #include "src/app/examples/inc/EspNowEchoApp.h"
// #include "src/app/examples/inc/EmptyTemplateApp.h"
// #include "src/app/examples/inc/PwmTestApp.h"
// #include "src/app/examples/inc/INMP441Example.h"
// #include "src/app/examples/inc/MPU6050Example.h"
#include "src/app/examples/inc/NkroKeyboardExample.h"
// #include "src/app/examples/inc/SDD1306AnimationApp.h"
// #include "src/app/examples/inc/ST7735AnimationExample.h"
// #include "src/app/examples/inc/ST7735FpsTextExample.h"
// #include "src/app/examples/inc/KeyBoard4x5Example.h"
// #include "src/app/examples/inc/WS2812Example.h"

// namespace SelectedApp = EspNowEchoApp;
// namespace SelectedApp = EmptyTemplateApp;
// namespace SelectedApp = PwmTestApp;
// namespace SelectedApp = INMP441Example;
// namespace SelectedApp = MPU6050Example;
namespace SelectedApp = NkroKeyboardExample;
// namespace SelectedApp = SSD1306AnimationApp;
// namespace SelectedApp = ST7735AnimationExample;
// namespace SelectedApp = ST7735FpsTextExample;
// namespace SelectedApp = KeyBoard4x5Example;
// namespace SelectedApp = WS2812Example;

void PrtnApp::setup() {
    SelectedApp::setup();
}

void PrtnApp::idle() {
    SelectedApp::idle();
}
