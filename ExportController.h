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

  // What goes into the file. The exporter enforces its own dependencies on top of this
  // (skinning implies mesh + skeleton, animation implies skeleton), so an inconsistent
  // combination is corrected rather than silently dropped -- see FBXExporter::exportModel.
  //
  // `clips` are model animation indices; empty means no animation is written even when
  // `animation` is set, which is exactly how the FBX exporter reads it.
  struct Options {
    bool mesh = true;
    bool skeleton = true;
    bool skinning = true;
    bool animation = false;
    std::vector<int> clips;
  };

  void setOptions(const Options& o) { options_ = o; }
  const Options& options() const { return options_; }

  // Asks for a path and exports. Returns an empty string on success, otherwise the
  // reason.
  QString exportModel(WoWModel* model, int formatIndex, QWidget* parent);

  // Same thing without the file dialog, so an export can be proven from a headless
  // run instead of only being assumed to work.
  QString exportTo(WoWModel* model, int formatIndex, const QString& path);

private:
  std::vector<Format> formats_;
  Options options_;
};

#endif
