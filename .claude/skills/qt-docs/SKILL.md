---
name: qt-docs
description: Use before writing or changing Hindsight's Qt code (settings window, Tools menu, dialogs, Qt helpers like base64 or JSON, threads, networking) to check it against the Qt 6 version OBS ships and the OBS frontend API.
---

# Qt 6 docs

Hindsight's UI (the settings window under Tools) uses Qt 6, linked through
`ENABLE_QT` and `ENABLE_FRONTEND_API` in `CMakeLists.txt`. We do not ship our
own Qt: the plugin runs inside OBS and uses the Qt that OBS loaded, so code
must work with that exact version and module set.

## Which library to reach for

Use libobs first, then Qt, then our own code. libobs works in every part of
the plugin, including the audio filter; Qt is fine for UI and helpers that
already run on Qt's side. Only write it ourselves when neither has it.

## Which version

- Windows and macOS builds use the prebuilt Qt from obs-deps. Its version is
  `dependencies.qt6.version` in `buildspec.json` (an obs-deps release date,
  `2025-07-11` when this skill was written). That release builds **Qt 6.8.3**,
  set in `deps.qt/qt6.ps1` and `deps.qt/qt6.zsh`:
  https://github.com/obsproject/obs-deps/blob/2025-07-11/deps.qt/qt6.ps1
  When `buildspec.json` changes, check that file at the new tag for the new
  Qt version.
- Linux builds use the distro's Qt 6, so the version can differ. Stay within
  APIs that exist in Qt 6.8.
- Docs for the 6.8 series: https://doc.qt.io/qt-6.8/ (it shows the newest 6.8
  patch release, which has the same API as 6.8.3). Look for "This function was
  introduced in Qt 6.x" notes before using anything newer than 6.8.

## What OBS's Qt includes

From the obs-deps build scripts at that tag:

- Modules: qtbase (Core, Gui, Widgets, Network, Concurrent and the rest of
  qtbase), qtimageformats, qtshadertools, qtmultimedia, qtsvg and qttools.
- Turned off: SQL, print support, test lib, D-Bus, ICU and PDF. Do not plan
  on them.
- TLS: OpenSSL is off. Windows uses Schannel (the built-in Windows TLS), so
  HTTPS through Qt Network works there without shipping OpenSSL.
- We link only `Qt6::Core` and `Qt6::Widgets` today. Before adding another
  module (for example `Qt6::Network`), check that OBS's own Windows install
  ships its DLL. Never ship our own copy of a Qt DLL: it would clash with the
  one OBS has already loaded. If OBS does not ship it, use libobs or another
  library instead.

## Sources

- Qt Core (strings, `QByteArray::toBase64`, JSON, files, timers):
  https://doc.qt.io/qt-6.8/qtcore-index.html
- Qt Widgets (dialogs, forms, layouts):
  https://doc.qt.io/qt-6.8/qtwidgets-index.html
- Qt Network (`QNetworkAccessManager` for HTTPS):
  https://doc.qt.io/qt-6.8/qtnetwork-index.html
- Threads and QObjects: https://doc.qt.io/qt-6.8/threads-qobject.html
  and reentrancy rules: https://doc.qt.io/qt-6.8/threads-reentrancy.html
- `QMetaObject::invokeMethod` for handing work to the UI thread:
  https://doc.qt.io/qt-6.8/qmetaobject.html
- OBS frontend API, which connects our Qt code to the OBS window:
  https://docs.obsproject.com/reference-frontend-api
  (`obs_frontend_add_tools_menu_qaction`, `obs_frontend_get_main_window`,
  `obs_frontend_add_dock_by_id`, `obs_frontend_add_event_callback`). The
  header at our OBS tag wins over that page, as in `obs-plugin-docs`.

## Rules for Hindsight

- **Widgets only on the UI thread.** Create and touch widgets only from the
  thread OBS runs its UI on. The audio filter, hotkey callbacks and our worker
  threads must not call Qt widget code; post back with
  `QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection)`.
- **QObjects stay on their thread.** A `QNetworkAccessManager` "can only be
  used from the thread it belongs to" (Qt docs), and it needs an event loop on
  that thread. Do not create one on a plain `std::thread`.
- **Parent the windows.** Parent dialogs to
  `static_cast<QWidget *>(obs_frontend_get_main_window())` so they follow the
  OBS window and its theme.
- **Clean up on unload.** Anything we add to OBS's UI must be removed or
  deleted in `obs_module_unload`, or OBS can crash on exit.
- **Text goes through `obs_module_text`.** Keep labels in
  `data/locale/en-US.ini` and convert with `QString::fromUtf8`, rather than
  hardcoding English or using Qt's translation system.
- **Secrets:** follow the Secrets section of `obs-plugin-docs`. A password
  field in a Qt dialog is still saved wherever we choose to store it.

## When you change Qt code

- Link the Qt class page you followed in the PR description, and say which
  thread the code runs on.
- If a Qt API is newer than 6.8, or needs a module we do not link, say so and
  choose another way.
