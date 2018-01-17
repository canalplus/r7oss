#ifndef CEF_LIBCEF_COMMON_DISPLAY_H_
#define CEF_LIBCEF_COMMON_DISPLAY_H_
#pragma once

#include "base/callback_forward.h"

// Display information.
struct CefDisplayInfo {
  // Display width.
  unsigned int width;

  // Display height.
  unsigned int height;
};

// Get display information.
typedef base::Callback<CefDisplayInfo()> CefGetDisplayInfoCB;

#endif // CEF_LIBCEF_COMMON_DISPLAY_H_
