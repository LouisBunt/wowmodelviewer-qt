#ifndef BLENDERADDONINSTALLER_H
#define BLENDERADDONINSTALLER_H

#include <QString>

// Copies the bundled Blender add-on into every Blender the user has.
//
// Modelled on wow.export's installer, which has proven the approach for years: no
// Blender process, no preferences editing -- just the add-on folder copied into each
// version's scripts/addons directory, where Blender picks it up on its next start.
// Activation stays a one-time manual step in Blender's own Add-ons dialog; automating
// it would mean rewriting Blender's preferences file behind its back.
namespace BlenderAddonInstaller
{
  struct Result
  {
    int installedVersions = 0;   // how many Blender version folders received the add-on
    QString error;               // empty on success; user-facing (German) otherwise
  };

  // Where the add-on ships from: "<exe dir>/blender_addon/io_import_wmv_fbx" in a
  // packaged install, with a fallback to the development tree so an uninstalled build
  // can exercise the same path. Empty when neither exists.
  QString addonSourceDir();

  Result install();
}

#endif
