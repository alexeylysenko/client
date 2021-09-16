/*
 * Copyright (C) by Dominik Schmidt <dschmidt@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "vfs.h"
#include "plugin.h"
#include "version.h"
#include "syncjournaldb.h"

#include "common/filesystembase.h"

#include <QDir>
#include <QPluginLoader>
#include <QLoggingCategory>

using namespace OCC;

Vfs::Vfs(QObject* parent)
    : QObject(parent)
{
}

Vfs::~Vfs() = default;

QString Vfs::modeToString(Mode mode)
{
    // Note: Strings are used for config and must be stable
    switch (mode) {
    case Off:
        return QStringLiteral("off");
    case WithSuffix:
        return QStringLiteral("suffix");
    case WindowsCfApi:
        return QStringLiteral("wincfapi");
    }
    return QStringLiteral("off");
}

Optional<Vfs::Mode> Vfs::modeFromString(const QString &str)
{
    // Note: Strings are used for config and must be stable
    if (str == QLatin1String("off")) {
        return Off;
    } else if (str == QLatin1String("suffix")) {
        return WithSuffix;
    } else if (str == QLatin1String("wincfapi")) {
        return WindowsCfApi;
    }
    return {};
}

Result<void, QString> Vfs::checkAvailability(const QString &path)
{
#ifdef Q_OS_WIN
    const auto mode = bestAvailableVfsMode();
    if (mode == Mode::WindowsCfApi) {
        const auto info = QFileInfo(path);
        if (QDir(info.canonicalFilePath()).isRoot()) {
            return tr("The Virtual filesystem feature does not support a drive as sync root");
        }
        const auto fs = FileSystem::fileSystemForPath(info.absoluteFilePath());
        if (fs != QLatin1String("NTFS")) {
            return tr("The Virtual filesystem feature requires a NTFS file system, %1 is using %2").arg(path, fs);
        }
        const auto type = GetDriveTypeW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(info.absoluteFilePath().mid(0, 3)).utf16()));
        if (type == DRIVE_REMOTE) {
            return tr("The Virtual filesystem feature is not supported on network drives");
        }
    }
#else
    Q_UNUSED(path);
#endif
    return {};
}

void Vfs::start(const VfsSetupParams &params)
{
    _setupParams = params;
    startImpl(params);
}

bool Vfs::setPinStateInDb(const QString &folderPath, PinState state)
{
    auto path = folderPath.toUtf8();
    _setupParams.journal->internalPinStates().wipeForPathAndBelow(path);
    if (state != PinState::Inherited)
        _setupParams.journal->internalPinStates().setForPath(path, state);
    return true;
}

Optional<PinState> Vfs::pinStateInDb(const QString &folderPath)
{
    auto pin = _setupParams.journal->internalPinStates().effectiveForPath(folderPath.toUtf8());
    return pin;
}

Vfs::AvailabilityResult Vfs::availabilityInDb(const QString &folderPath)
{
    auto path = folderPath.toUtf8();
    auto pin = _setupParams.journal->internalPinStates().effectiveForPathRecursive(path);
    // not being able to retrieve the pin state isn't too bad
    auto hydrationStatus = _setupParams.journal->hasHydratedOrDehydratedFiles(path);
    if (!hydrationStatus)
        return AvailabilityError::DbError;

    if (hydrationStatus->hasDehydrated) {
        if (hydrationStatus->hasHydrated)
            return VfsItemAvailability::Mixed;
        if (pin && *pin == PinState::OnlineOnly)
            return VfsItemAvailability::OnlineOnly;
        else
            return VfsItemAvailability::AllDehydrated;
    } else if (hydrationStatus->hasHydrated) {
        if (pin && *pin == PinState::AlwaysLocal)
            return VfsItemAvailability::AlwaysLocal;
        else
            return VfsItemAvailability::AllHydrated;
    }
    return AvailabilityError::NoSuchItem;
}

VfsOff::VfsOff(QObject *parent)
    : Vfs(parent)
{
}

VfsOff::~VfsOff() = default;

static QString modeToPluginName(Vfs::Mode mode)
{
    if (mode == Vfs::WithSuffix)
        return QStringLiteral("suffix");
    if (mode == Vfs::WindowsCfApi)
        return QStringLiteral("win");
    return QString();
}

Q_LOGGING_CATEGORY(lcPlugin, "plugins", QtInfoMsg)

bool OCC::isVfsPluginAvailable(Vfs::Mode mode)
{
    // TODO: cache plugins available?
    if (mode == Vfs::Off)
        return true;
    auto name = modeToPluginName(mode);
    if (name.isEmpty())
        return false;
    auto pluginPath = pluginFileName(QStringLiteral("vfs"), name);
    QPluginLoader loader(pluginPath);

    auto basemeta = loader.metaData();
    if (basemeta.isEmpty() || !basemeta.contains(QStringLiteral("IID"))) {
        qCDebug(lcPlugin) << "Plugin doesn't exist" << loader.fileName();
        return false;
    }
    if (basemeta[QStringLiteral("IID")].toString() != QLatin1String("org.owncloud.PluginFactory")) {
        qCWarning(lcPlugin) << "Plugin has wrong IID" << loader.fileName() << basemeta[QStringLiteral("IID")];
        return false;
    }

    auto metadata = basemeta[QStringLiteral("MetaData")].toObject();
    if (metadata[QStringLiteral("type")].toString() != QLatin1String("vfs")) {
        qCWarning(lcPlugin) << "Plugin has wrong type" << loader.fileName() << metadata[QStringLiteral("type")];
        return false;
    }
    if (metadata[QStringLiteral("version")].toString() != QStringLiteral(MIRALL_VERSION_STRING)) {
        qCWarning(lcPlugin) << "Plugin has wrong version" << loader.fileName() << metadata[QStringLiteral("version")];
        return false;
    }

    // Attempting to load the plugin is essential as it could have dependencies that
    // can't be resolved and thus not be available after all.
    if (!loader.load()) {
        qCWarning(lcPlugin) << "Plugin failed to load:" << loader.errorString();
        return false;
    }

    return true;
}

Vfs::Mode OCC::bestAvailableVfsMode()
{
    if (isVfsPluginAvailable(Vfs::WindowsCfApi)) {
        return Vfs::WindowsCfApi;
    } else if (isVfsPluginAvailable(Vfs::WithSuffix)) {
        return Vfs::WithSuffix;
    }
    return Vfs::Off;
}

std::unique_ptr<Vfs> OCC::createVfsFromPlugin(Vfs::Mode mode)
{
    if (mode == Vfs::Off)
        return std::make_unique<VfsOff>();

    auto name = modeToPluginName(mode);
    if (name.isEmpty())
        return nullptr;
    auto pluginPath = pluginFileName(QStringLiteral("vfs"), name);

    if (!isVfsPluginAvailable(mode)) {
        qCCritical(lcPlugin) << "Could not load plugin: not existant or bad metadata" << pluginPath;
        return nullptr;
    }

    QPluginLoader loader(pluginPath);
    auto plugin = loader.instance();
    if (!plugin) {
        qCCritical(lcPlugin) << "Could not load plugin" << pluginPath << loader.errorString();
        return nullptr;
    }

    auto factory = qobject_cast<PluginFactory *>(plugin);
    if (!factory) {
        qCCritical(lcPlugin) << "Plugin" << loader.fileName() << "does not implement PluginFactory";
        return nullptr;
    }

    auto vfs = std::unique_ptr<Vfs>(qobject_cast<Vfs *>(factory->create(nullptr)));
    if (!vfs) {
        qCCritical(lcPlugin) << "Plugin" << loader.fileName() << "does not create a Vfs instance";
        return nullptr;
    }

    qCInfo(lcPlugin) << "Created VFS instance from plugin" << pluginPath;
    return vfs;
}
