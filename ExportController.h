#ifndef EXPORTCONTROLLER_H
#define EXPORTCONTROLLER_H

#include <vector>

#include <QObject>
#include <QString>

class ExporterPlugin;
class WoWModel;
class QWidget;

// Drives the exporter plugins.
//
// The plugin API in Source/core is already Qt (QPluginLoader, Q_DECLARE_INTERFACE)
// and knows nothing about the toolkit above it, so this is mostly a matter of
// loading the directory and calling exportModel().
//
// Only in-process export is handled here. The wx front-end runs FBX in a child
// process because the FBX SDK is not thread-safe and mutates process globals -- see
// ExportJobManager. That machinery is not ported yet; FBX therefore runs in-process
// too, which is fine for a single export but loses the crash isolation.
class ExportController : public QObject
{
  Q_OBJECT

public:
  explicit ExportController(QObject* parent = nullptr);

  // Loads ./plugins. Returns how many exporters were found.
  int loadPlugins();

  struct Format { QString label; QString filter; ExporterPlugin* plugin; };
  const std::vector<Format>& formats() const { return formats_; }

  // Asks for a path and exports. Returns an empty string on success, otherwise the
  // reason.
  QString exportModel(WoWModel* model, int formatIndex, QWidget* parent);

  // Same thing without the file dialog, so an export can be proven from a headless
  // run instead of only being assumed to work.
  QString exportTo(WoWModel* model, int formatIndex, const QString& path);

private:
  std::vector<Format> formats_;
};

#endif
