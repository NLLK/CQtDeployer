/*
 * Copyright (C) 2018-2021 QuasarApp.
 * Distributed under the lgplv3 software license, see the accompanying
 * Everyone is permitted to copy and distribute verbatim copies
 * of this license document, but changing it is not allowed.
 */

#include "deploycore.h"
#include "metafilemanager.h"

#include <quasarapp.h>
#include <QDir>
#include <configparser.h>
#include "filemanager.h"

#include <assert.h>

bool MetaFileManager::createRunScriptWindows(const QString &target) {

    auto cnf = DeployCore::_config;
    auto targetinfo = cnf->targets().value(target);
    if (!targetinfo.isValid()) {
        return false;
    }
    auto distro = cnf->getDistro(target);

    QFileInfo targetInfo(target);

    QString content;
    auto runScript = targetinfo.getRunScript();

    QFile script(runScript);
    if (!script.open(QIODevice::ReadOnly)) {
        return false;
    }
    content = script.readAll();
    script.close();
    replace(toReplace(target, distro), content);

    QString fname = DeployCore::_config->getTargetDir(target) + QDir::separator() + targetInfo.baseName()+ ".bat";

    QFile F(fname);
    if (!F.open(QIODevice::WriteOnly)) {
        return false;
    }

    F.write(content.toUtf8());
    F.flush();
    F.close();

    _fileManager->addToDeployed(fname);

    return F.setPermissions(QFileDevice::ExeOther | QFileDevice::WriteOther |
                            QFileDevice::ReadOther | QFileDevice::ExeUser |
                            QFileDevice::WriteUser | QFileDevice::ReadUser |
                            QFileDevice::ExeOwner | QFileDevice::WriteOwner |
                            QFileDevice::ReadOwner);
}

bool MetaFileManager::createRunScriptLinux(const QString &target) {
    auto cnf = DeployCore::_config;
    auto targetinfo = cnf->targets().value(target);

    if (!cnf->targets().contains(target)) {
        return false;
    }
    auto distro = cnf->getDistro(target);

    QFileInfo targetInfo(target);

    QString content;
    auto runScript = targetinfo.getRunScript();
    QFile script(runScript);
    if (!script.open(QIODevice::ReadOnly)) {
        return false;
    }
    content = script.readAll();
    script.close();
    replace(toReplace(target, distro), content);


    QString fname = DeployCore::_config->getTargetDir(target) + QDir::separator() + targetInfo.baseName()+ ".sh";

    QFile F(fname);
    if (!F.open(QIODevice::WriteOnly)) {
        return false;
    }

    F.write(content.toUtf8());
    F.flush();
    F.close();

    _fileManager->addToDeployed(fname);

    return F.setPermissions(QFileDevice::ExeOther | QFileDevice::WriteOther |
                            QFileDevice::ReadOther | QFileDevice::ExeUser |
                            QFileDevice::WriteUser | QFileDevice::ReadUser |
                            QFileDevice::ExeOwner | QFileDevice::WriteOwner |
                            QFileDevice::ReadOwner);
}

QString MetaFileManager::generateCustoScriptBlok(bool bat) const {

    QString res = "";

    QString commentMarker = "# ";
    if (bat) {
        commentMarker = ":: ";
    }

    auto cstSh = QuasarAppUtils::Params::getArg("customScript", "");
    if (cstSh.size()) {

        res = "\n" +
                commentMarker + "Begin Custom Script (generated by customScript flag)\n"
              "%0\n" +
                commentMarker + "End Custom Script\n"
              "\n";

        res = res.arg(cstSh);
    }

    return res;
}

MetaFileManager::MetaFileManager(FileManager *manager):
    _fileManager(manager)
{
    assert(_fileManager);
}

bool MetaFileManager::createRunScript(const QString &target) {

    QFileInfo info(target);
    auto sufix = info.completeSuffix();

    if (sufix.contains("exe", Qt::CaseSensitive)) {
        return createRunScriptWindows(target);
    }

    if (sufix.isEmpty()) {
        return createRunScriptLinux(target);
    }

    return true;

}

bool MetaFileManager::createQConf(const QString &target) {
    auto cnf = DeployCore::_config;

    if (!cnf->targets().contains(target)) {
        return false;
    }
    auto distro = cnf->getDistro(target);

    QString content =
            "[Paths]\n"
            "Prefix= ." + distro.getRootDir(distro.getBinOutDir()) + "\n"
            "Libraries= ." + distro.getLibOutDir() + "\n"
            "Plugins= ." + distro.getPluginsOutDir() + "\n"
            "Imports= ." + distro.getQmlOutDir() + "\n"
            "Translations= ." + distro.getTrOutDir() + "\n"
            "Qml2Imports= ." + distro.getQmlOutDir() + "\n";


    content.replace("//", "/");
    content = QDir::fromNativeSeparators(content);

    QString fname = DeployCore::_config->getTargetDir(target) + distro.getBinOutDir() + "qt.conf";

    QFile F(fname);
    if (!F.open(QIODevice::WriteOnly)) {
        return false;
    }

    F.write(content.toUtf8());
    F.flush();
    F.close();

    _fileManager->addToDeployed(fname);

    return F.setPermissions(QFileDevice::ExeOther | QFileDevice::WriteOther |
                            QFileDevice::ReadOther | QFileDevice::ExeUser |
                            QFileDevice::WriteUser | QFileDevice::ReadUser |
                            QFileDevice::ExeOwner | QFileDevice::WriteOwner |
                            QFileDevice::ReadOwner);
}

QHash<QString, QString> MetaFileManager::toReplace(const QString& target,
                                                   const DistroModule& distro) const {
    QFileInfo targetInfo(target);

    QHash<QString, QString> result = {
        {"CQT_BIN_PATH", QDir::toNativeSeparators(distro.getBinOutDir())},
        {"CQT_LIB_PATH", QDir::toNativeSeparators(distro.getLibOutDir())},
        {"CQT_QML_PATH", QDir::toNativeSeparators(distro.getQmlOutDir())},
        {"CQT_PLUGIN_PATH", QDir::toNativeSeparators(distro.getPluginsOutDir())},
        {"CQT_SYSTEM_LIB_PATH", QDir::toNativeSeparators(distro.getLibOutDir() + DeployCore::systemLibsFolderName())},
        {"CQT_BASE_NAME", QDir::toNativeSeparators(targetInfo.baseName())}
    };

    bool fGui = DeployCore::isGui(_mudulesMap.value(target));


    if (targetInfo.completeSuffix().compare("exe", Qt::CaseInsensitive) == 0) {
        result.insert("CQT_CUSTOM_SCRIPT_BLOCK", generateCustoScriptBlok(true));

        // Run application as invoke of the console for consle applications
        // And run gui applciation in the detached mode.
        QString runCmd;
        if (fGui) {
            runCmd = "start \"" + targetInfo.baseName() + "\" %0 " +
                    "\"%BASE_DIR%" + distro.getBinOutDir() + targetInfo.fileName() + "\" %*";
            runCmd = QDir::toNativeSeparators(runCmd).arg("/B");

        } else {
            runCmd = "call \"%BASE_DIR%" + distro.getBinOutDir() + targetInfo.fileName() + "\" %*";
            runCmd = QDir::toNativeSeparators(runCmd);
        }

        result.insert("CQT_RUN_COMMAND", runCmd);

    } else {
        result.insert("CQT_CUSTOM_SCRIPT_BLOCK", generateCustoScriptBlok(false));

        QString runCmd = "\"$BASE_DIR" + distro.getBinOutDir() + targetInfo.fileName() + "\" \"$@\" ";

        result.insert("CQT_RUN_COMMAND", QDir::toNativeSeparators(runCmd));
    }

    return result;
}

void MetaFileManager::replace(const QHash<QString, QString> &map, QString &content) {
    for (auto it = map.begin(); it != map.end(); ++it) {
        content = content.replace(it.key(), it.value());
    }

}

void MetaFileManager::createRunMetaFiles(const QHash<QString, DeployCore::QtModule>& modulesMap) {

    _mudulesMap = modulesMap;
    for (auto i = DeployCore::_config->targets().cbegin(); i != DeployCore::_config->targets().cend(); ++i) {

        if (i.value().fEnableRunScript() && !createRunScript(i.key())) {
            QuasarAppUtils::Params::log("Failed to create a run script: " + i.key(),
                                        QuasarAppUtils::Error);
        }

        if (!createQConf(i.key())) {
            QuasarAppUtils::Params::log("Failed to create the qt.conf file", QuasarAppUtils::Warning);
        }
    }
}
