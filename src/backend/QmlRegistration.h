#pragma once

namespace usbcan {

class AppController;

// Registers the backend types under `import UsbCan.Backend` and exposes the
// controller as the `App` singleton. Call before loading any QML.
void registerQmlTypes(AppController *controller);

} // namespace usbcan
