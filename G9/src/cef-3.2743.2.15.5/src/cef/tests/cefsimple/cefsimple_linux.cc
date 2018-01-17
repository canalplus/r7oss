// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefsimple/simple_app.h"

#ifdef USE_X11

#include <X11/Xlib.h>

#include "include/base/cef_logging.h"

namespace {

int XErrorHandlerImpl(Display *display, XErrorEvent *event) {
  LOG(WARNING)
        << "X error received: "
        << "type " << event->type << ", "
        << "serial " << event->serial << ", "
        << "error_code " << static_cast<int>(event->error_code) << ", "
        << "request_code " << static_cast<int>(event->request_code) << ", "
        << "minor_code " << static_cast<int>(event->minor_code);
  return 0;
}

int XIOErrorHandlerImpl(Display *display) {
  return 0;
}

}  // namespace

#endif  // USE_X11

#define SET_STRING(target, value) CefString(&target).FromASCII(value)
#define RESOURCES_DIR_PATH "/usr/share/cef/"
#define LOCALES_PATH "/usr/share/cef/locales"
#define REMOTE_DEBUGGING_PORT 9222
#define CACHE_PATH "/usr/share/cef/cache"

// Entry point function for all processes.
int main(int argc, char* argv[]) {
  // Provide CEF with command-line arguments.
  CefMainArgs main_args(argc, argv);

  // SimpleApp implements application-level callbacks. It will create the first
  // browser instance in OnContextInitialized() after CEF has initialized.
  CefRefPtr<SimpleApp> app(new SimpleApp);

  // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
  // that share the same executable. This function checks the command-line and,
  // if this is a sub-process, executes the appropriate logic.
  int exit_code = CefExecuteProcess(main_args, NULL, NULL);
  if (exit_code >= 0) {
    // The sub-process has completed so return here.
    return exit_code;
  }

#ifdef USE_X11
  // Install xlib error handlers so that the application won't be terminated
  // on non-fatal errors.
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif  // USE_X11

  // Specify CEF global settings here.
  CefSettings settings;

  // Location where cache data will be stored on disk
  SET_STRING(settings.cache_path, CACHE_PATH);
  // The fully qualified path for the locales directory
  SET_STRING(settings.locales_dir_path, LOCALES_PATH);
  // The fully qualified path for the resources directory.
  SET_STRING(settings.resources_dir_path, RESOURCES_DIR_PATH);
  // Set to a value between 1024 and 65535 to enable remote debugging on the specified port
  settings.remote_debugging_port = REMOTE_DEBUGGING_PORT;

  // Initialize CEF for the browser process.
  CefInitialize(main_args, settings, app.get(), NULL);

  // Run the CEF message loop. This will block until CefQuitMessageLoop() is
  // called.
  CefRunMessageLoop();

  // Shut down CEF.
  CefShutdown();

  return 0;
}
