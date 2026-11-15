#pragma once
#include <QString>
#include <functional>

class QObject;

// Finding and starting the Python helper scripts (speech recognition and text to speech).
namespace PythonScripts {

// The Python interpreter to run: the "speech/python" setting, or "python" from PATH.
QString pythonProgram();

// Full path to a script from the scripts/ folder next to the executable (or one or two
// levels up, for a build tree). Empty if it is not found.
QString findScript(const QString& fileName);

// Checks without blocking the UI that Python starts. `done(ok, error)` is called once,
// on the event loop, with a readable error when it fails (not installed, not on PATH,
// or the Windows Store stub).
void checkPython(QObject* context, std::function<void(bool ok, const QString& error)> done);

} // namespace PythonScripts
