/** @file AppImagePaths.h
 * @brief Helpers for paths that are meaningless to the user when the application runs from an AppImage.
 *
 * An AppImage is mounted by its runtime under a random directory (e.g. `/tmp/.mount_OOpenCIKgwKG`) and the bundled
 * toolchain lives inside it (`.../compiler/gcc`, `.../compiler/include`, ...). Those directories exist, so a naive
 * "does the path exist?" check accepts them as a starting point of file dialogs. They disappear when the application
 * exits and the user never wants to browse them, so file dialogs must start somewhere else. */
#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace viz::appimage
{

/** @brief Root of the mounted AppImage (the `APPDIR` set by the AppImage runtime), or an empty string when the
 *         application does not run from an AppImage. */
inline QString mountedRoot()
{
    QString root = QString::fromUtf8(qgetenv("APPDIR"));

    // Our AppRun is started by the runtime, which exports APPDIR; if it is missing but APPIMAGE is set, the
    // executable lives in <root>/usr/bin, so derive the root from its location.
    if (root.isEmpty() && ! qgetenv("APPIMAGE").isEmpty())
    {
        root = QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).absolutePath();
    }

    if (root.isEmpty())
        return {};

    const QString canonical = QFileInfo(root).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(root) : canonical;
}

/** @brief Whether @p path points inside the mounted AppImage (and so is of no use as a user-facing location). */
inline bool isInsideMountedImage(const QString& path)
{
    const QString root = mountedRoot();
    if (root.isEmpty() || path.isEmpty())
        return false;

    QString canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty())
        canonical = QDir::cleanPath(QFileInfo(path).absoluteFilePath());

    return canonical == root || canonical.startsWith(root + QLatin1Char('/'));
}

/** @brief Directory file dialogs should start in when nothing better is known.
 *
 * Prefers the directory the application was started from (`OWD` is the original working directory exported by the
 * AppImage runtime), then the current directory, then the directory holding the AppImage file itself, then the
 * home directory. Directories inside the mounted image are never returned. */
inline QString defaultStartDirectory()
{
    const QString candidates[] = {
        QString::fromUtf8(qgetenv("OWD")),
        QDir::currentPath(),
        QFileInfo(QString::fromUtf8(qgetenv("APPIMAGE"))).absolutePath(),
    };

    for (const QString& candidate : candidates)
    {
        if (! candidate.isEmpty() && QDir(candidate).exists() && ! isInsideMountedImage(candidate))
            return QDir(candidate).absolutePath();
    }

    return QDir::homePath();
}

} // namespace viz::appimage
