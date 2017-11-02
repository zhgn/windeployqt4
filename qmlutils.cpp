﻿/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmlutils.h"
#include "utils.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QCoreApplication>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>

QT_BEGIN_NAMESPACE

bool operator==(const QmlImportScanResult::Module &m1, const QmlImportScanResult::Module &m2)
{
    return m1.className.isEmpty() ? m1.name == m2.name : m1.className == m2.className;
}

// Return install path (cp -r semantics)
QString QmlImportScanResult::Module::installPath(const QString &root) const
{
    QString result = root;
    const int lastSlashPos = relativePath.lastIndexOf(QLatin1Char('/'));
    if (lastSlashPos != -1) {
        result += QLatin1Char('/');
        result += relativePath.left(lastSlashPos);
    }
    return result;
}

static QString qmlDirectoryRecursion(Platform platform, const QString &path)
{
    QDir dir(path);
    if (!dir.entryList(QStringList(QString::fromLatin1("*.qml")), QDir::Files, QDir::NoSort).isEmpty())
        return dir.path();
    const QFileInfoList &subDirs = dir.entryInfoList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
    foreach (const QFileInfo &subDirFi , subDirs) {
        if (!isBuildDirectory(platform, subDirFi.fileName())) {
            const QString subPath = qmlDirectoryRecursion(platform, subDirFi.absoluteFilePath());
            if (!subPath.isEmpty())
                return subPath;
        }
    }
    return QString();
}

// Find a directory containing QML files in the project
QString findQmlDirectory(int platform, const QString &startDirectoryName)
{
    QDir startDirectory(startDirectoryName);
    if (isBuildDirectory(Platform(platform), startDirectory.dirName()))
        startDirectory.cdUp();
    return qmlDirectoryRecursion(Platform(platform), startDirectory.path());
}

static void findFileRecursion(const QDir &directory, Platform platform,
                              DebugMatchMode debugMatchMode, QStringList *matches)
{
    const QStringList &dlls = findSharedLibraries(directory, platform, debugMatchMode);
    foreach (const QString &dll , dlls)
        matches->append(directory.filePath(dll));
    const QFileInfoList &subDirs = directory.entryInfoList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    foreach (const QFileInfo &subDirFi , subDirs) {
        QDir subDirectory(subDirFi.absoluteFilePath());
        if (subDirectory.isReadable())
            findFileRecursion(subDirectory, platform, debugMatchMode, matches);
    }
}

QmlImportScanResult runQmlImportScanner(const QString &directory, const QString &qmlImportPath,
                                        bool usesWidgets, int platform, DebugMatchMode debugMatchMode,
                                        QString *errorMessage)
{
    bool quickControlsHandled = false;
    QmlImportScanResult result;
    QStringList arguments;
    arguments << QString::fromLatin1("-importPath") << qmlImportPath << QString::fromLatin1("-rootPath") << directory;
    unsigned long exitCode;
    QByteArray stdOut;
    QByteArray stdErr;
    const QString binary = QString::fromLatin1("qmlimportscanner");
    if (!runProcess(binary, arguments, QDir::currentPath(), &exitCode, &stdOut, &stdErr, errorMessage))
        return result;
    if (exitCode) {
        *errorMessage = binary + QString::fromLatin1(" returned ") + QString::number(exitCode)
                        + QString::fromLatin1(": ") + QString::fromLocal8Bit(stdErr);
        return result;
    }
    QJsonParseError jsonParseError;
    const QJsonDocument data = QJsonDocument::fromJson(stdOut, &jsonParseError);
    if (data.isNull() ) {
        *errorMessage = binary + QString::fromLatin1(" returned invalid JSON output: ")
                        + jsonParseError.errorString() + QString::fromLatin1(" :\"")
                        + QString::fromLocal8Bit(stdOut) + QLatin1Char('"');
        return result;
    }
    const QJsonArray array = data.array();
    const int childCount = array.count();
    for (int c = 0; c < childCount; ++c) {
        const QJsonObject object = array.at(c).toObject();
        if (object.value(QString::fromLatin1("type")).toString() == QLatin1String("module")) {
            const QString path = object.value(QString::fromLatin1("path")).toString();
            if (!path.isEmpty()) {
                QmlImportScanResult::Module module;
                module.name = object.value(QString::fromLatin1("name")).toString();
                module.className = object.value(QString::fromLatin1("classname")).toString();
                module.sourcePath = path;
                module.relativePath = object.value(QString::fromLatin1("relativePath")).toString();
                result.modules.append(module);
                findFileRecursion(QDir(path), Platform(platform), debugMatchMode, &result.plugins);
                // QTBUG-48424, QTBUG-45977: In release mode, qmlimportscanner does not report
                // the dependency of QtQuick.Controls on QtQuick.PrivateWidgets due to missing files.
                // Recreate the run-time logic here as best as we can - deploy it if
                //      1) QtWidgets is used
                //      2) QtQuick.Controls is used
                if (!quickControlsHandled && usesWidgets && module.name == QLatin1String("QtQuick.Controls")) {
                    quickControlsHandled = true;
                    QmlImportScanResult::Module privateWidgetsModule;
                    privateWidgetsModule.name = QString::fromLatin1("QtQuick.PrivateWidgets");
                    privateWidgetsModule.className = QString::fromLatin1("QtQuick2PrivateWidgetsPlugin");
                    privateWidgetsModule.sourcePath = QFileInfo(path).absolutePath() + QString::fromLatin1("/PrivateWidgets");
                    privateWidgetsModule.relativePath = QString::fromLatin1("QtQuick/PrivateWidgets");
                    result.modules.append(privateWidgetsModule);
                    findFileRecursion(QDir(privateWidgetsModule.sourcePath), Platform(platform), debugMatchMode, &result.plugins);
                }
            }
        }
    }
    result.ok = true;
    return result;
}

void QmlImportScanResult::append(const QmlImportScanResult &other)
{
    foreach (const QmlImportScanResult::Module &module , other.modules) {
        if (std::find(modules.constBegin(), modules.constEnd(), module) == modules.constEnd())
            modules.append(module);
    }
    foreach (const QString &plugin , other.plugins) {
        if (!plugins.contains(plugin))
            plugins.append(plugin);
    }
}

QT_END_NAMESPACE