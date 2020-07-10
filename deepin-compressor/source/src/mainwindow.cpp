/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     dongsen <dongsen@deepin.com>
 *
 * Maintainer: dongsen <dongsen@deepin.com>
 *             AaronZhang <ya.zhang@archermind.com>
 *             chenglu <chenglu@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include "homepage.h"
#include "uncompresspage.h"
#include "compresspage.h"
#include "compresssetting.h"
#include "compressor_success.h"
#include "compressor_fail.h"
#include "archive_manager.h"
#include "archivemodel.h"
#include "encryptionpage.h"
#include "progressdialog.h"
#include "extractpausedialog.h"
#include "settingdialog.h"
#include "encodingpage.h"
#include "archivesortfiltermodel.h"
#include "batchextract.h"
#include "batchcompress.h"
#include "openloadingpage.h"
#include "pluginmanager.h"
#include "utils.h"
#include "compressorapplication.h"
#include "structs.h"
#include "openwithdialog/openwithdialog.h"
#include "jobs.h"
#include "kprocess.h"
#include "monitorInterface.h"
#include "filewatcher.h"
#include "monitorAdaptor.h"

#include <DApplication>
#include <DFileWatcher>
#include <DDesktopServices>
#include <DMessageManager>
#include <DStandardPaths>
#include <DFontSizeManager>
#include <DWidgetUtil>

#include <QDebug>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QMimeDatabase>
#include <QShortcut>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSvgWidget>
#include <QTimer>
#include <QStackedLayout>
#include <QStackedLayout>
#include <QScreen>
#include <QUuid>
#include <QMessageBox>
#include <QElapsedTimer>

#include "unistd.h"

DWIDGET_USE_NAMESPACE

#define DEFAUTL_PATH DStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "tempfiles"+ QDir::separator()

int MainWindow::m_windowcount = 1;

MainWindow::MainWindow(QWidget *parent) : DMainWindow(parent)
{
//    setAttribute(Qt::WA_DeleteOnClose);
    m_model = new ArchiveModel(this);
    m_filterModel = new ArchiveSortFilterModel(this);

    m_mainWidget = new DWidget(this);
    m_mainLayout = new QStackedLayout(m_mainWidget);
    m_homePage = new HomePage(this);
    m_mainLayout->addWidget(m_homePage);
    m_homePage->setAutoFillBackground(true);

    // init window flags.
    setWindowTitle(tr("Archive Manager"));
    // setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setCentralWidget(m_mainWidget);
    setAcceptDrops(true);

    initTitleBar();

    m_startTimer = startTimer(500);

    loadWindowState();
}

MainWindow::~MainWindow()
{
    if (m_windowcount == 0) {
        if (this->pMapGlobalWnd != nullptr) {
            this->pMapGlobalWnd->mMapGlobal.clear();
        }
        if (this->pCurAuxInfo != nullptr) {
            this->pCurAuxInfo->information.clear();
        }
    }
    saveWindowState();
}

void MainWindow::bindAdapter()
{
    m_adaptor = new MonitorAdaptor(this);
}

qint64 MainWindow::getMediaFreeSpace()
{
    QList< QStorageInfo > list = QStorageInfo::mountedVolumes();
    qDebug() << "Volume Num: " << list.size();
    for (QStorageInfo &si : list) {
        qDebug() << si.displayName();
        if (si.displayName().count() > 7 && si.displayName().left(6) == "/media") {
            qDebug() << "Bytes Avaliable: " << si.bytesAvailable() / 1024 / 1024 << "MB";
            return si.bytesAvailable() / 1024 / 1024;
        }
    }

    return 0;
}

bool MainWindow::applicationQuit()
{
    if (m_pJob) {
        m_pJob->kill();
        m_pJob = nullptr;
    }

    if (m_Progess->getType() == Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSDRAGADD) {
        return true;
    }

    if (PAGE_ZIPPROGRESS == m_mainLayout->currentIndex()) {
        if (1 != m_Progess->showConfirmDialog()) {
            return false;
        }
        deleteCompressFile();
        QString destDirName;
        deleteDecompressFile(destDirName);
    } else if (7 == m_mainLayout->currentIndex()) {
        deleteCompressFile();
    }

    return true;
}

QString MainWindow::getAddFile()
{
    return  m_addFile;
}

void MainWindow::saveWindowState()
{
    QSettings settings(objectName());
    settings.setValue("geometry", saveGeometry());
}

void MainWindow::loadWindowState()
{
    QSettings settings(objectName());
    const QByteArray geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    } else {
        resize(620, 465);
    }

    setMinimumSize(620, 465);
}

QString MainWindow::getLoadFile()
{
    return m_loadfile;
}

qint64 MainWindow::getDiskFreeSpace()
{
//    QStorageInfo storage = QStorageInfo::root();
    QStorageInfo storage(m_pathstore);
    storage.refresh();
//    qDebug() << storage.name() << storage.bytesTotal() / 1024 / 1024 << "MB";
    qDebug() << "availableSize:" << storage.bytesAvailable() / 1024 / 1024 << "MB";
    return storage.bytesAvailable() / 1024 / 1024;
}

int MainWindow::queryDialogForClose()
{
    DDialog *dialog = new DDialog(this);
    dialog->setFixedWidth(440);
    QIcon icon = QIcon::fromTheme("deepin-compressor");
    dialog->setIcon(icon /*, QSize(32, 32)*/);
    dialog->setMessage(tr("Do you want to close the window even it has working job?"));
    dialog->addButton(tr("Cancel"));
    dialog->addButton(QObject::tr("OK"));
//    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect();
//    effect->setOffset(0, 4);
//    effect->setColor(QColor(0, 145, 255, 76));
//    effect->setBlurRadius(4);
//    dialog->getButton(2)->setGraphicsEffect(effect);

    const int mode = dialog->exec();
    delete dialog;
    qDebug() << mode;
    return mode;
}

void MainWindow::closeClean(QCloseEvent *event)
{
    if (m_pJob) {
        if (m_pJob->mType == KJob::ENUM_JOBTYPE::EXTRACTJOB) {
            this->closeExtractJobSafe();
        } else if (m_pJob->mType == KJob::ENUM_JOBTYPE::DELETEJOB) {
            DeleteJob *pJob = dynamic_cast<DeleteJob *>(m_pJob);
            pJob->archiveInterface()->extractPsdStatus = ReadOnlyArchiveInterface::ExtractPsdStatus::Canceled;
        } else {
            m_pJob->deleteLater();
            m_pJob = nullptr;
        }
    }

    event->accept();

    if (m_windowcount == 1) {
        return;
    }
    Archive::Entry *pRootEntry = m_model->getRootEntry();
    if (pRootEntry) {
        pRootEntry->clean();
    }
    SAFE_DELETE_ELE(pRootEntry);
    SAFE_DELETE_ELE(m_fileManager);
    SAFE_DELETE_ELE(pEventloop);
    SAFE_DELETE_ELE(m_spinner);
    SAFE_DELETE_ELE(m_pWatcher);
//    SAFE_DELETE_ELE(m_model);
//    SAFE_DELETE_ELE(m_logo);
//    SAFE_DELETE_ELE(m_titleFrame);
//    SAFE_DELETE_ELE(m_titlelabel);
    SAFE_DELETE_ELE(m_UnCompressPage);
    SAFE_DELETE_ELE(m_CompressPage);
//    SAFE_DELETE_ELE(m_mainLayout);
    SAFE_DELETE_ELE(m_homePage);
    SAFE_DELETE_ELE(m_CompressSetting);
//    SAFE_DELETE_ELE(m_Progess);
    SAFE_DELETE_ELE(m_CompressSuccess);
    SAFE_DELETE_ELE(m_CompressFail);
    SAFE_DELETE_ELE(m_encryptionpage);
    SAFE_DELETE_ELE(m_progressdialog);
    SAFE_DELETE_ELE(m_settingsDialog);
    SAFE_DELETE_ELE(m_pOpenLoadingPage);
    SAFE_DELETE_ELE(m_encodingpage);
    SAFE_DELETE_ELE(m_settings);
    SAFE_DELETE_ELE(m_mainWidget);

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    char options = OpenInfo::CLOSE;
    if (this->pCurAuxInfo != nullptr) {
        MainWindow_AuxInfo *curAuxInfo = this->pCurAuxInfo;
        QMap<QString, OpenInfo *>::iterator it = curAuxInfo->information.begin();
        if (this->pMapGlobalWnd != nullptr) {
            while (it != curAuxInfo->information.end()) {
                OpenInfo *pInfo = it.value();
                it++;

                MainWindow *p = this->pMapGlobalWnd->getOne(pInfo->strWinId);
                if (p != nullptr) {
                    p->close();//close all children mainwindow
                } else {
                    continue;
                }

                if (p->option == OpenInfo::CLOSE) {
                    options |= p->option;
                } else if (p->option == OpenInfo::OPEN) {
                    options |= p->option;
                } else if (p->option == OpenInfo::QUERY_CLOSE_CANCEL) {
                    options |= p->option;
                }
            }
        }


        QMap<QString, OpenInfo *>::iterator iter;
        QString key;
        for (iter = curAuxInfo->information.begin(); iter != curAuxInfo->information.end();) {
            key = iter.key();
            iter++;             //指针移至下一个位置
            if (curAuxInfo->information[key]->option == OpenInfo::CLOSE) {
                OpenInfo *p = curAuxInfo->information.take(key);
                SAFE_DELETE_ELE(p);
            }
        }
    }

    qDebug() << "子窗口开始关闭";
    //判断m_pJob是否结束
    if (m_pJob == nullptr) {
        if (options == OpenInfo::QUERY_CLOSE_CANCEL) {//如果子面板取消关闭
            event->ignore();
            this->option = OpenInfo::QUERY_CLOSE_CANCEL;
            return;
        } else if (options == OpenInfo::CLOSE) {//如果子面板那正常关闭
            //event->accept();
            closeClean(event);
            this->option = OpenInfo::CLOSE;
            removeFromParentInfo(this);
            if (this->pMapGlobalWnd != nullptr) {
                this->pMapGlobalWnd->remove(QString::number(this->winId()));
            }

        }
    } else {
        if (options == OpenInfo::QUERY_CLOSE_CANCEL) {
            event->ignore();
            this->option = OpenInfo::QUERY_CLOSE_CANCEL;
            return;
        } else {//如果子面板正常关闭；并且当前面板job完成
            int mode = queryDialogForClose();
            if (mode == 0 || mode == -1) {
                event->ignore();
                this->option = OpenInfo::QUERY_CLOSE_CANCEL;
                return;
            } else if (mode == 1) {
//                if (m_pJob) {
//                    ExtractJob *pJob = dynamic_cast<ExtractJob *>(m_pJob);
//                    m_pJob->doKill();
//                    m_pJob = nullptr;
//                }

                closeClean(event);
                removeFromParentInfo(this);

                this->option = OpenInfo::CLOSE;
                if (this->pMapGlobalWnd != nullptr) {
                    this->pMapGlobalWnd->remove(QString::number(this->winId()));
                }
            }

        }

    }

    if (m_Progess->getType() == Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSDRAGADD) {
        if (m_pJob && m_pJob->mType == Job::ENUM_JOBTYPE::ADDJOB) {
            AddJob *pAddJob = dynamic_cast<AddJob *>(m_pJob);
            pAddJob->kill();
            pAddJob = nullptr;
        }

        deleteCompressFile();
        slotquitApp();
        return;
    }

    if (PAGE_ZIPPROGRESS == m_mainLayout->currentIndex()) {
        if (1 != m_Progess->showConfirmDialog()) {
            event->ignore();
            return;
        }

        deleteCompressFile();
        deleteDecompressFile();

        if (m_pJob) {
            if (m_pJob->mType == KJob::ENUM_JOBTYPE::EXTRACTJOB) {
                QString destDirName;
                ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
                destDirName = pExtractJob->archiveInterface()->destDirName;
                deleteDecompressFile(destDirName);
            }
        }

        emit sigquitApp();
    } else if (PAGE_ZIP_FAIL == m_mainLayout->currentIndex()) {
        deleteCompressFile(/*m_compressDirFiles, CheckAllFiles(m_pathstore)*/);
        slotquitApp();
    } else {
        slotquitApp();
    }
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    /*if (m_timerId == event->timerId()) {
        m_progressTransFlag = true;
        killTimer(m_timerId);
        m_timerId = 0;
    } else */

    if (m_startTimer == event->timerId()) {
        if (!m_initflag) {
            InitUI();
            InitConnection();
            m_initflag = true;
        }

        killTimer(m_startTimer);
        m_startTimer = 0;
    } else if (m_watchTimer == event->timerId()) {
        if (m_CompressPage == nullptr) {
            return;
        }

        QStringList filelist = m_CompressPage->getCompressFilelist();
        for (int i = 0; i < filelist.count(); i++) {
            QFileInfo filein(filelist.at(i));
            if (!filein.exists()) {
                QString displayName = Utils::toShortString(filein.fileName());
                QString strTips = tr("%1 was changed on the disk, please import it again.").arg(displayName);
                DDialog *dialog = new DDialog(this);
                QPixmap pixmap = Utils::renderSVG(":assets/icons/deepin/builtin/icons/compress_warning_32px.svg", QSize(32, 32));
                dialog->setIcon(pixmap);
                dialog->addSpacing(32);
                dialog->setMinimumSize(380, 140);
                dialog->addButton(tr("OK"), true, DDialog::ButtonNormal);
//                QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect();
//                effect->setOffset(0, 4);
//                effect->setColor(QColor(0, 145, 255, 76));
//                effect->setBlurRadius(4);
//                dialog->getButton(0)->setFixedWidth(340);
//                dialog->getButton(0)->setGraphicsEffect(effect);

                DLabel *pLblContent = new DLabel(strTips, dialog);
                pLblContent->setAlignment(Qt::AlignmentFlag::AlignHCenter);
                DPalette pa;
                pa = DApplicationHelper::instance()->palette(pLblContent);
                pa.setBrush(DPalette::Text, pa.color(DPalette::ButtonText));
                DFontSizeManager::instance()->bind(pLblContent, DFontSizeManager::T6, QFont::Medium);
                pLblContent->setMinimumWidth(this->width());
                pLblContent->move(dialog->width() / 2 - pLblContent->width() / 2, 48);
                dialog->exec();

                filelist.removeAt(i);
                if (m_pageid != PAGE_ZIP) {
                    m_pageid = PAGE_ZIP;
                    refreshPage();
                }
                m_CompressPage->onRefreshFilelist(filelist);
                if (filelist.isEmpty()) {
                    m_pageid = PAGE_HOME;
                    m_CompressPage->setRootPathIndex();
                    refreshPage();
                } else {
                    m_CompressPage->setRootPathIndex();
                    refreshPage();
                }
                SAFE_DELETE_ELE(dialog);
            }
        }
    }
}

void MainWindow::InitUI()
{
    m_UnCompressPage = new UnCompressPage(this);
    m_CompressPage = new CompressPage(this);
    m_CompressSetting = new CompressSetting(this);
    m_Progess = new Progress(this);
    m_CompressSuccess = new Compressor_Success(this);
    m_CompressFail = new Compressor_Fail(this);
    m_encryptionpage = new EncryptionPage(this);
    m_progressdialog = new ProgressDialog(this);
    m_settingsDialog = new SettingDialog(this);
    m_encodingpage = new EncodingPage(this);
    m_settings = new QSettings(QDir(Utils::getConfigPath()).filePath("config.conf"), QSettings::IniFormat, this);
    m_pOpenLoadingPage = new OpenLoadingPage(this);

    if (m_settings->value("dir").toString().isEmpty()) {
        m_settings->setValue("dir", "");
    }

    // add widget to main layout.
    m_mainLayout->addWidget(m_UnCompressPage);
    m_mainLayout->addWidget(m_CompressPage);
    m_mainLayout->addWidget(m_CompressSetting);
    m_mainLayout->addWidget(m_Progess);
    m_mainLayout->addWidget(m_CompressSuccess);
    m_mainLayout->addWidget(m_CompressFail);
    m_mainLayout->addWidget(m_encryptionpage);
    m_mainLayout->addWidget(m_encodingpage);
    m_mainLayout->addWidget(m_pOpenLoadingPage);
    m_UnCompressPage->setAutoFillBackground(true);
    m_CompressPage->setAutoFillBackground(true);
    m_CompressSetting->setAutoFillBackground(true);
    m_Progess->setAutoFillBackground(true);
    m_CompressSuccess->setAutoFillBackground(true);
    m_CompressFail->setAutoFillBackground(true);
    m_encryptionpage->setAutoFillBackground(true);
    m_encodingpage->setAutoFillBackground(true);
}

QJsonObject MainWindow::creatShorcutJson()
{
    QJsonObject shortcut1;
    shortcut1.insert("name", tr("Close"));
    shortcut1.insert("value", "Alt+F4");

    QJsonObject shortcut2;
    shortcut2.insert("name", tr("Help"));
    shortcut2.insert("value", "F1");

    QJsonObject shortcut3;
    shortcut3.insert("name", tr("Select the file"));
    shortcut3.insert("value", "Ctrl+O");

    QJsonObject shortcut4;
    shortcut4.insert("name", tr("Delete"));
    shortcut4.insert("value", "Delete");

    //    QJsonObject shortcut5;
    //    shortcut5.insert("name", tr("Rename"));
    //    shortcut5.insert("value", "F2");

    QJsonObject shortcut6;
    shortcut6.insert("name", tr("Display shortcuts"));
    shortcut6.insert("value", "Ctrl+Shift+?");

    QJsonArray shortcutArray;
    shortcutArray.append(shortcut1);
    shortcutArray.append(shortcut2);
    shortcutArray.append(shortcut3);
    shortcutArray.append(shortcut4);
    // shortcutArray.append(shortcut5);
    shortcutArray.append(shortcut6);

    QJsonObject shortcut_group;
    shortcut_group.insert("groupName", tr("Shortcuts"));
    shortcut_group.insert("groupItems", shortcutArray);

    QJsonArray shortcutArrayall;
    shortcutArrayall.append(shortcut_group);

    QJsonObject main_shortcut;
    main_shortcut.insert("shortcut", shortcutArrayall);

    return main_shortcut;
}

void MainWindow::InitConnection()
{
    // connect the signals to the slot function.
    connect(m_homePage, &HomePage::fileSelected, this, &MainWindow::onSelected);
    connect(m_CompressPage, &CompressPage::sigFilelistIsEmpty, this, &MainWindow::onCompressPageFilelistIsEmpty);
    connect(m_CompressPage, &CompressPage::sigselectedFiles, this, &MainWindow::onSelected);
    connect(m_CompressPage, &CompressPage::sigRefreshFileList, this, &MainWindow::slotCalDeleteRefreshTotalFileSize);
    connect(m_CompressPage, &CompressPage::sigNextPress, this, &MainWindow::onCompressNext);
    connect(this, &MainWindow::sigZipAddFile, m_CompressPage, &CompressPage::onAddfileSlot);
    connect(this, &MainWindow::sigCompressedAddFile, m_UnCompressPage, &UnCompressPage::slotCompressedAddFile);
    connect(m_CompressSetting, &CompressSetting::sigCompressPressed, this, &MainWindow::onCompressPressed);
    connect(m_CompressSetting, &CompressSetting::sigUncompressStateAutoCompress, this, &MainWindow::onUncompressStateAutoCompress);
    connect(m_CompressSetting, &CompressSetting::sigUncompressStateAutoCompressEntry, this, &MainWindow::onUncompressStateAutoCompressEntry);
    connect(m_CompressSetting, &CompressSetting::sigFileUnreadable, this, &MainWindow::slotFileUnreadable);
    connect(m_Progess, &Progress::sigCancelPressed, this, &MainWindow::onCancelCompressPressed);
    connect(m_CompressSuccess, &Compressor_Success::sigQuitApp, this, &MainWindow::slotquitApp);
    connect(m_CompressSuccess, &Compressor_Success::sigBackButtonClicked, this, &MainWindow::slotBackButtonClicked);
    connect(m_titlebutton, &DPushButton::clicked, this, &MainWindow::onTitleButtonPressed);
    connect(this, &MainWindow::sigZipSelectedFiles, m_CompressPage, &CompressPage::onSelectedFilesSlot);
    connect(m_model, &ArchiveModel::loadingFinished, this, &MainWindow::slotLoadingFinished);
    connect(m_UnCompressPage, &UnCompressPage::sigDecompressPress, this, &MainWindow::slotextractSelectedFilesTo);
//    connect(m_UnCompressPage, &UnCompressPage::sigRefreshFileList, this, &MainWindow::slotUncompressCalDeleteRefreshTotalFileSize);
    connect(m_UnCompressPage, &UnCompressPage::sigRefreshEntryVector, this, &MainWindow::slotUncompressCalDeleteRefreshTotoalSize);
    connect(m_UnCompressPage, &UnCompressPage::sigFilelistIsEmpty, this, &MainWindow::onCompressPageFilelistIsEmpty);
    connect(m_encryptionpage, &EncryptionPage::sigExtractPassword, this, &MainWindow::SlotExtractPassword);
    connect(m_UnCompressPage, &UnCompressPage::sigextractfiles, this, &MainWindow::slotExtractSimpleFiles);
//    connect(this, &MainWindow::sigTipsWindowPopUp, m_UnCompressPage, &UnCompressPage::subWindowTipsPopSig);
    connect(m_UnCompressPage, &UnCompressPage::sigAutoCompress, m_CompressSetting, &CompressSetting::autoCompress);
    connect(m_UnCompressPage, &UnCompressPage::sigAutoCompressEntry, m_CompressSetting, &CompressSetting::autoCompressEntry);
    connect(m_UnCompressPage, &UnCompressPage::sigOpenExtractFile, this, &MainWindow::slotExtractSimpleFilesOpen);
    connect(m_UnCompressPage, &UnCompressPage::sigDeleteArchiveFiles, this, &MainWindow::deleteFromArchive);
//    connect(m_UnCompressPage, &UnCompressPage::sigAddArchiveFiles, this, &MainWindow::addToArchive);
    connect(m_CompressSetting, &CompressSetting::sigMoveFilesToArchive, this, &MainWindow::moveToArchive);
    connect(this, &MainWindow::deleteJobComplete, m_UnCompressPage, &UnCompressPage::slotDeleteJobFinished);
    connect(this, &MainWindow::sigUpdateTableView, m_UnCompressPage, &UnCompressPage::sigUpdateUnCompreeTableView);
    connect(m_progressdialog, &ProgressDialog::stopExtract, this, &MainWindow::slotKillExtractJob);
    connect(m_progressdialog, &ProgressDialog::sigResetPercentAndTime, this, &MainWindow::slotResetPercentAndTime);
    connect(m_CompressFail, &Compressor_Fail::sigFailRetry, this, &MainWindow::slotFailRetry);
    connect(m_CompressFail, &Compressor_Fail::sigBackButtonClickedOnFail, this, &MainWindow::slotBackButtonClicked);
    connect(m_CompressPage, &CompressPage::sigiscanaddfile, this, &MainWindow::onCompressAddfileSlot);
    connect(m_progressdialog, &ProgressDialog::extractSuccess, this, [ = ](QString msg) {
        QIcon icon = Utils::renderSVG(":assets/icons/deepin/builtin/icons/compress_success_30px.svg", QSize(30, 30));
        this->sendMessage(icon, msg);
        if (m_settingsDialog->isAutoOpen()) {
            //DDesktopServices::showFileItem(QUrl(m_decompressfilepath, QUrl::TolerantMode));
            QString fullpath = m_decompressfilepath + "/" + m_extractSimpleFiles.at(0)->property("name").toString();
            qDebug() << fullpath;
            QFileInfo fileinfo(fullpath);
            if (fileinfo.exists()) {
                if (fileinfo.isDir()) {
                    DDesktopServices::showFolder(fullpath);
                } else if (fileinfo.isFile()) {
                    DDesktopServices::showFileItem(fullpath);
                }
            }
        }
    });

    auto openkey = new QShortcut(QKeySequence(Qt::Key_Slash + Qt::CTRL + Qt::SHIFT), this);
    openkey->setContext(Qt::ApplicationShortcut);
    connect(openkey, &QShortcut::activated, this, [this] {
        const QRect &rect = window()->geometry();
        QPoint pos(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2);
        QStringList shortcutString;
        QJsonObject json = creatShorcutJson();

        QString param1 = "-j=" + QString(QJsonDocument(json).toJson());
        QString param2 = "-p=" + QString::number(pos.x()) + "," + QString::number(pos.y());
        shortcutString << param1 << param2;

        QProcess *shortcutViewProcess = new QProcess(this);
        shortcutViewProcess->startDetached("deepin-shortcut-viewer", shortcutString);

        connect(shortcutViewProcess, SIGNAL(finished(int)), shortcutViewProcess, SLOT(deleteLater()));
    });
}

QMenu *MainWindow::createSettingsMenu()
{
    QMenu *menu = new QMenu();

    m_openAction = menu->addAction(tr("Open file"));
    connect(m_openAction, &QAction::triggered, this, [this] {
        DFileDialog dialog(this);
        dialog.setAcceptMode(DFileDialog::AcceptOpen);
        dialog.setFileMode(DFileDialog::ExistingFiles);
        dialog.setAllowMixedSelection(true);

        QString historyDir = m_settings->value("dir").toString();
        if (historyDir.isEmpty())
        {
            historyDir = QDir::homePath();
        }
        dialog.setDirectory(historyDir);

        const int mode = dialog.exec();

        // save the directory string to config file.
        m_settings->setValue("dir", dialog.directoryUrl().toLocalFile());

        // if click cancel button or close button.
        if (mode != QDialog::Accepted)
        {
            return;
        }

        onSelected(dialog.selectedFiles());
    });

    // menu->insertAction();

    QAction *settingsAction = menu->addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, [this] { m_settingsDialog->exec(); });

    menu->addSeparator();

    return menu;
}

void MainWindow::initTitleBar()
{
    titlebar()->setMenu(createSettingsMenu());
    titlebar()->setFixedHeight(50);
    titlebar()->setTitle("");

    QIcon icon = QIcon::fromTheme("deepin-compressor");
    titlebar()->setIcon(icon);

    m_titlebutton = new DIconButton(DStyle::SP_IncreaseElement, this);
    m_titlebutton->setFixedSize(38, 38);
    m_titlebutton->setVisible(false);
    QHBoxLayout *leftLayout = new QHBoxLayout;
    leftLayout->addSpacing(6);
    leftLayout->addWidget(m_titlebutton);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    QFrame *left_frame = new QFrame(this);
    left_frame->setFixedWidth(6 + 38);
    left_frame->setContentsMargins(0, 0, 0, 0);
    left_frame->setLayout(leftLayout);
    titlebar()->addWidget(left_frame, Qt::AlignLeft);

    titlebar()->setContentsMargins(0, 0, 0, 0);
}

void MainWindow::setQLabelText(QLabel *label, const QString &text)
{
    QFontMetrics cs(label->font());
    int textWidth = cs.width(text);
    if (textWidth > label->width()) {
        label->setToolTip(text);
    } else {
        label->setToolTip("");
    }

    QFontMetrics elideFont(label->font());
    label->setText(elideFont.elidedText(text, Qt::ElideMiddle, label->width()));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    const auto *mime = e->mimeData();

    // not has urls.
    if (!mime->hasUrls()) {
        return e->ignore();
    }

    // traverse.
    m_homePage->setIconPixmap(true);
    return e->accept();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *e)
{
    m_homePage->setIconPixmap(false);

    DMainWindow::dragLeaveEvent(e);
}

void MainWindow::dropEvent(QDropEvent *e)
{
    auto *const mime = e->mimeData();

    if (false == mime->hasUrls()) {
        return e->ignore();
    }

    e->accept();

    // find font files.
    QStringList fileList;
    for (const auto &url : mime->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }

        fileList << url.toLocalFile();
        // const QFileInfo info(localPath);
        qDebug() << fileList;
    }

    if (fileList.size() == 0) {
        return;
    }

    m_homePage->setIconPixmap(false);
    onSelected(fileList);
    //    onRightMenuSelected(fileList);
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->accept();
}

bool MainWindow::onSubWindowActionFinished(int /*mode*/, const qint64 &/*pid*/, const QStringList &/*urls*/)
{
//    qDebug() << "子界面拖拽完成！进程pid为：" << pid;
//    qDebug() << "当前进程pid为：" << getpid() << ";父类进程pid为：" << getppid();
//    qDebug() << "进程列表中进程有：" << m_tempProcessId.size() << "个！";
//    QWriteLocker locker(&m_lock);
//    if (m_tempProcessId.empty()) {
//        return false;
//    }
//    if (!urls.isEmpty() && pid) {
//        if (m_subWinDragFiles.contains(pid)) {
//            m_subWinDragFiles[pid] = urls;
//        } else {
//            m_subWinDragFiles.insert(pid, urls);
//        }
//        m_mode = mode;
//        return true;
//    }
    return false;
}

bool MainWindow::popUpChangedDialog(const qint64 &pid)
{
    qDebug() << "主界面接收到要弹出对话框消息，当前进程pid为：" << getpid();
    QWriteLocker locker(&m_lock);
    if (m_tempProcessId.empty()) {
        qDebug() << "主子进程为空" ;
        return false;
    }
    if (m_subWinDragFiles.empty()) {
        qDebug() << "要添加拖拽文件为空" ;
        return false;
    }

    if (!m_tempProcessId.contains(pid)) {
        qDebug() << "子进程不再进程列表中" ;
        qDebug() << "列表中的子进程为：" << m_tempProcessId[0] ;
        return false;
    }
    m_curOperChildPid = pid;
    //pop dialog
    emit sigTipsWindowPopUp(m_mode, m_subWinDragFiles.value(pid));
    //m_tempProcessId.removeAll(pid);
    m_subWinDragFiles.remove(pid);
    return true;
}

bool MainWindow::createSubWindow(const QStringList &urls)
{
    if (urls.length() == 0) {
        MainWindow *subWindow = new MainWindow();
        subWindow->pMapGlobalWnd = this->pMapGlobalWnd;//获取deepin-compressor进程中的全局窗口map
        ++m_windowcount;
        subWindow->show();

        return true;
    }
    QString filePath = urls[0];
    QFileInfo fileInfo(filePath);

    QStringList inUrls = std::move(const_cast<QStringList & >(urls));
    qDebug() << "=================urls:" << inUrls;

    QString winid = "";
    for (int i = 0; i < inUrls.length(); i++) {
        if (inUrls[i].contains(HEADBUS)) {
            winid = inUrls[i];
            inUrls.removeOne(winid);
            winid.remove(HEADBUS);
            break;
        }
    }
    MainWindow *pParentWnd = nullptr;
    if (this->pMapGlobalWnd != nullptr) {
        pParentWnd = this->pMapGlobalWnd->getOne(winid);
    }
    //create sub mainwindow
//    if (inUrls.length() == 0) {
//        return false;
//    }
    MainWindow *subWindow = new MainWindow();
    if (fileInfo.exists() == true && (!subWindow->checkSettings(filePath))) {//判断目标文件是否合法
        return  false;
    }
    subWindow->pMapGlobalWnd = this->pMapGlobalWnd;//获取deepin-compressor进程中的全局窗口map
    subWindow->strChildMndExtractPath = this->strChildMndExtractPath;//子面板的解压路径必须和父面板的解压路径统一
    subWindow->strParentArchivePath = this->strParentArchivePath;//子面板的解压?当前路径必须和父面板的解压路径统一
    if (this->pMapGlobalWnd == nullptr) {
        this->pMapGlobalWnd = new GlobalMainWindowMap();
    }
    pMapGlobalWnd->insert(QString::number(subWindow->winId()), subWindow);

    if (pParentWnd != nullptr) {
        subWindow->pCurAuxInfo = new MainWindow_AuxInfo();
        subWindow->pCurAuxInfo->parentAuxInfo = pParentWnd->pCurAuxInfo;

        QString strModelIndex = inUrls.takeAt(1);//第一个参数存储的有modelIndex字符串
        if (pParentWnd->pCurAuxInfo != nullptr &&
                pParentWnd->pCurAuxInfo->information.contains(strModelIndex) == true) {
            OpenInfo *pInfo = pParentWnd->pCurAuxInfo->information[strModelIndex];
//            pInfo->isHidden = false;
            pInfo->option = OpenInfo::OPEN;
            pInfo->strWinId = QString::number(subWindow->winId());
            int childCount = pParentWnd->pCurAuxInfo->information.size();
            subWindow->move(pParentWnd->x() + childCount * 130, pParentWnd->y() + childCount * 92);
            connect(subWindow, &MainWindow::sigTipsWindowPopUp, pParentWnd->m_UnCompressPage, &UnCompressPage::slotSubWindowTipsPopSig);

            subWindow->m_pageid = PAGE_ZIP;
            //        subWindow->onRightMenuSelected(inUrls);
            QMetaObject::invokeMethod(subWindow, "onRightMenuSelected", Qt::DirectConnection, Q_ARG(QStringList, inUrls));
            //        subWindow->onSelected(inUrls);
        }
    } else {
        if (inUrls.length() > 0) {
            QMetaObject::invokeMethod(subWindow, "onRightMenuSelected", Qt::DirectConnection, Q_ARG(QStringList, inUrls));
        }
    }

    ++m_windowcount;
    subWindow->show();

    return true;
}

void MainWindow::setEnable()
{
    setAcceptDrops(true);

    // enable titlebar buttons.
    titlebar()->setDisableFlags(Qt::Widget);
}

void MainWindow::setDisable()
{
    setAcceptDrops(false);

    // disable titlebar buttons.
    titlebar()->setDisableFlags(Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint
                                | Qt::WindowSystemMenuHint);
}

void MainWindow::refreshPage()
{
    m_encryptionpage->resetPage();
    qDebug() << "m_pageid: " << m_pageid;
    switch (m_pageid) {
    case PAGE_HOME:

        if (m_fileManager) {
            SAFE_DELETE_ELE(m_fileManager);
        }
        m_Progess->resetProgress();
        m_openAction->setEnabled(true);
        setAcceptDrops(true);
        m_titlebutton->setVisible(false);
        titlebar()->setTitle("");
        m_mainLayout->setCurrentIndex(0);
        break;
    case PAGE_UNZIP:
        m_Progess->resetProgress();
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setIcon(DStyle::StandardPixmap::SP_IncreaseElement);
        m_titlebutton->setVisible(true);
        titlebar()->setTitle(m_decompressfilename);
        m_mainLayout->setCurrentIndex(1);
        break;
    case PAGE_ZIP:
        m_Progess->resetProgress();
        titlebar()->setTitle(tr("Create New Archive"));
        m_titlebutton->setIcon(DStyle::StandardPixmap::SP_IncreaseElement);
        m_openAction->setEnabled(true);
        m_titlebutton->setVisible(true);
        setAcceptDrops(true);
        m_watchTimer = startTimer(1000);
        m_CompressPage->onPathIndexChanged();
        m_mainLayout->setCurrentIndex(2);
        break;
    case PAGE_ZIPSET:
        titlebar()->setTitle(tr("Create New Archive"));
        m_titlebutton->setIcon(DStyle::StandardPixmap::SP_ArrowLeave);
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(true);
        m_mainLayout->setCurrentIndex(3);
        break;
    case PAGE_ZIPPROGRESS:
        if (0 != m_watchTimer) {
            killTimer(m_watchTimer);
            m_watchTimer = 0;
        }
        if (this->m_encryptiontype == EncryptionType::Encryption_Load) {
            int limitCounts = 10;
            int left = 5, right = 5;
            QString displayName = "";
            displayName = m_decompressfilename.length() > limitCounts ? m_decompressfilename.left(left) + "..." + m_decompressfilename.right(right) : m_decompressfilename;
            QString strTitle = tr("adding files to %1").arg(m_decompressfilename);
            titlebar()->setTitle(strTitle);
        } else {
            titlebar()->setTitle(tr("Compressing"));
        }
        m_Progess->setSpeedAndTimeText(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSING);
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(false);

        m_Progess->setFilename(m_decompressfilename);
        m_mainLayout->setCurrentIndex(4);
        m_Progess->pInfo()->startTimer();
        break;
    case PAGE_UNZIPPROGRESS:
        m_Progess->setSpeedAndTimeText(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(false);
        if (m_Progess->getType()) {
            titlebar()->setTitle(tr("Opening"));
        } else {
            titlebar()->setTitle(tr("Extracting"));
        }
        m_Progess->setFilename(m_decompressfilename);
        m_mainLayout->setCurrentIndex(4);
        m_Progess->pInfo()->startTimer();
        break;
    case PAGE_DELETEPROGRESS:
        m_Progess->setSpeedAndTimeText(Progress::ENUM_PROGRESS_TYPE::OP_DELETEING);
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(false);
        titlebar()->setTitle(tr("Deleteing"));
        m_Progess->setFilename(m_decompressfilename);
        m_mainLayout->setCurrentIndex(4);
        m_Progess->pInfo()->startTimer();
        break;
    case PAGE_ZIP_SUCCESS:
        titlebar()->setTitle("");
        m_CompressSuccess->setstringinfo(tr("Compression successful"));
        m_titlebutton->setIcon(DStyle::StandardPixmap::SP_ArrowLeave);
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(false);
        m_mainLayout->setCurrentIndex(5);
        break;
    case PAGE_ZIP_FAIL:
        titlebar()->setTitle("");
        m_CompressFail->setFailStr(tr("Compression failed"));
        m_titlebutton->setIcon(DStyle::StandardPixmap::SP_ArrowLeave);
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(false);
        m_mainLayout->setCurrentIndex(6);
        break;
    case PAGE_UNZIP_SUCCESS:
        if (m_fileManager) {
            m_fileManager->stopWatcher();
            SAFE_DELETE_ELE(m_fileManager);
        }
        titlebar()->setTitle("");
        m_CompressSuccess->setCompressPath(m_decompressfilepath);
        //m_CompressSuccess->setstringinfo(tr("Extraction successful"));
        m_titlebutton->setIcon(DStyle::StandardPixmap::SP_ArrowLeave);
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(false);
//        if (m_isrightmenu) {
//            if ("" != m_settingsDialog->getCurExtractPath() && m_UnCompressPage->getExtractType() != EXTRACT_HEAR) {
//                m_CompressSuccess->showfiledirSlot(false);
//            }
//        } else {
//            if (m_settingsDialog->isAutoOpen() && m_encryptiontype != Encryption_NULL) {
//                m_CompressSuccess->showfiledirSlot();
//            }
//        }

        if (m_isrightmenu) {
            if (m_settingsDialog->isAutoOpen()) {
                m_CompressSuccess->showfiledirSlot(false);
            }
            slotquitApp();
            return;
        } else {
            if (m_settingsDialog->isAutoOpen() && m_encryptiontype != Encryption_NULL) {
                m_CompressSuccess->showfiledirSlot(false);
            }
        }

        m_mainLayout->setCurrentIndex(5);
        break;
    case PAGE_UNZIP_FAIL:
        m_titlebutton->setIcon(DStyle::StandardPixmap::SP_ArrowLeave);
        titlebar()->setTitle("");
        m_CompressFail->setFailStr(tr("Extraction failed"));
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(false);
        m_mainLayout->setCurrentIndex(6);
        break;
    case PAGE_ENCRYPTION:
        titlebar()->setTitle(m_decompressfilename);
        m_openAction->setEnabled(false);
        setAcceptDrops(false);
        m_titlebutton->setVisible(false);
        if (m_progressdialog->isshown()) {
            // m_progressdialog->reject();
            m_progressdialog->hide();
            m_progressdialog->m_extractdialog->reject();
        }
        //        m_progressdialog->reject();
        //        m_progressdialog->m_extractdialog->reject();
        m_mainLayout->setCurrentIndex(7);
        m_encryptionpage->setPassowrdFocus();
        break;
    case PAGE_LOADING:
        m_mainLayout->setCurrentIndex(9);
        m_pOpenLoadingPage->start();
        break;
    default:
        break;
    }
}

//add calculate size of selected files
void MainWindow::calSelectedTotalFileSize(const QStringList &files)
{
    foreach (QString file, files) {
        QFileInfo fi(file);

        if (fi.isFile()) {
            qint64 curFileSize = fi.size();

#ifdef __aarch64__
            if (maxFileSize_ < curFileSize) {
                maxFileSize_ = curFileSize;
            }
#endif
            m_Progess->pInfo()->getTotalSize() += curFileSize;
        } else if (fi.isDir()) {
            m_Progess->pInfo()->getTotalSize() += calFileSize(file);
        }
    }
    m_CompressSetting->getSelectedFileSize(m_Progess->pInfo()->getTotalSize());
}

void MainWindow::calSelectedTotalEntrySize(QVector<Archive::Entry *> &vectorDel)
{
    qint64 size = 0;
    foreach (Archive::Entry *entry, vectorDel) {
        entry->calAllSize(size);
    }
//    m_ProgressIns += size;
    m_Progess->pInfo()->getTotalSize() += size;
}

qint64 MainWindow::calFileSize(const QString &path)
{
    QDir dir(path);
    qint64 size = 0;
    if (dir.entryInfoList().length() == 0) {
        QFileInfo file(path);
        return file.size();
    }
    foreach (QFileInfo fileInfo, dir.entryInfoList(QDir::Files)) {
        qint64 curFileSize = fileInfo.size();

#ifdef __aarch64__
        if (maxFileSize_ < curFileSize) {
            maxFileSize_ = curFileSize;
        }
#endif

        size += curFileSize;
    }

    foreach (QString subDir, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        size += calFileSize(path + QDir::separator() + subDir);
    }

    return size;
}

void MainWindow::calSpeedAndTime(unsigned long compressPercent)
{
    m_Progess->refreshSpeedAndTime(compressPercent);
}

void MainWindow::onSelected(const QStringList &files)
{
    m_UnCompressPage->getMainWindowWidth(this->width());
    calSelectedTotalFileSize(files);

    if (files.count() == 1 && Utils::isCompressed_file(files.at(0))) {
        m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
        if (0 == m_CompressPage->getCompressFilelist().count()) {
            if (this->m_model != nullptr) {
                Archive::Entry *pRootEntry = m_model->getRootEntry();
                if (pRootEntry) {
                    pRootEntry->clean();
                }
                this->m_model->resetmparent();
            }

            QString filename;
            filename = files.at(0);

            //            if (filename.contains(".7z.")) {
            //                filename = filename.left(filename.length() - 3) + "001";
            //            }

            transSplitFileName(filename);

            QFileInfo fileinfo(filename);
            m_decompressfilename = fileinfo.fileName();
            m_UnCompressPage->SetDefaultFile(fileinfo);
            if (strChildMndExtractPath == nullptr) {
                strChildMndExtractPath = new QString(fileinfo.path());

            }

            if (strParentArchivePath == nullptr) {
                strParentArchivePath = new QString(fileinfo.path());
            }

            if ("" != m_settingsDialog->getCurExtractPath() && m_UnCompressPage->getExtractType() != EXTRACT_HEAR) {
                m_UnCompressPage->setdefaultpath(m_settingsDialog->getCurExtractPath());
            } else {
                m_UnCompressPage->setdefaultpath(*strChildMndExtractPath);
            }
            m_UnCompressPage->getFileViewer()->setRootPathIndex();//added by hsw 20200612 重置m_pathindex
            m_pageid = PAGE_LOADING;
            loadArchive(filename);
        } else {
            DDialog *dialog = new DDialog(this);
            dialog->setFixedWidth(440);
            QIcon icon = QIcon::fromTheme("deepin-compressor");
            dialog->setIcon(icon /*, QSize(32, 32)*/);
            dialog->setMessage(tr("Do you want to add the archive to the list or open it in new window?"));
            dialog->addButton(tr("Cancel"));
            dialog->addButton(tr("Add"));
            dialog->addButton(tr("Open in new window"), true, DDialog::ButtonRecommend);
            QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect();
            effect->setOffset(0, 4);
            effect->setColor(QColor(0, 145, 255, 76));
            effect->setBlurRadius(4);
            dialog->getButton(2)->setGraphicsEffect(effect);

            const int mode = dialog->exec();
            SAFE_DELETE_ELE(dialog);
            qDebug() << mode;
            if (1 == mode) {
                emit sigZipSelectedFiles(files);
            } else if (2 == mode) {

                QStringList arguments;
                arguments << files.at(0);
                qDebug() << arguments;
                startCmd("deepin-compressor", arguments);
            }
        }
    } else {
        m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSING);
        m_pageid = PAGE_ZIP;
        emit sigZipSelectedFiles(files);
        refreshPage();
    }
}

void MainWindow::onRightMenuSelected(const QStringList &files)
{
    if (!m_initflag) {
        InitUI();
        InitConnection();
        m_initflag = true;
    }

    if (strParentArchivePath == nullptr && files.count() > 0) {
        strParentArchivePath = new QString(QFileInfo(files[0]).path());
    }

    m_UnCompressPage->getMainWindowWidth(this->width());
    calSelectedTotalFileSize(files);
//    QString info = "";
//    for (int i = 0; i < files.length(); i++) {
//        info += files[i];
//    }
//    QMessageBox::information(nullptr, "Title", info,
//                             QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (files.last() == QStringLiteral("extract_here")) {//解压

        m_isrightmenu = true;
        QFileInfo fileinfo(files.at(0));
        m_decompressfilename = fileinfo.fileName();
        m_UnCompressPage->SetDefaultFile(fileinfo);
        m_UnCompressPage->setdefaultpath(fileinfo.path());
        loadArchive(files.at(0));
        m_pageid = PAGE_UNZIPPROGRESS;
        m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
        refreshPage();
    } else if (files.last() == QStringLiteral("extract_here_multi")) {
        QStringList pathlist = files;
        pathlist.removeLast();
        QFileInfo fileinfo(pathlist.at(0));
        m_decompressfilename = fileinfo.fileName();
        m_UnCompressPage->SetDefaultFile(fileinfo);
        m_UnCompressPage->setdefaultpath(fileinfo.path());
        m_decompressfilepath = fileinfo.path();
        m_pageid = PAGE_UNZIPPROGRESS;
        m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
        refreshPage();

        BatchExtract *batchJob = new BatchExtract();
        batchJob->setAutoSubfolder(true);
        batchJob->setDestinationFolder(fileinfo.path());
        batchJob->setPreservePaths(true);

        for (const QString &url : qAsConst(pathlist)) {
            batchJob->addInput(QUrl::fromLocalFile(url));
        }

        connect(batchJob, SIGNAL(batchProgress(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)));
        connect(batchJob, &KJob::result, this, &MainWindow::slotExtractionDone);
        connect(batchJob, &BatchExtract::sendCurFile, this, &MainWindow::slotBatchExtractFileChanged);
        connect(batchJob, &BatchExtract::sendFailFile, this, &MainWindow::slotBatchExtractError);
        //        connect(batchJob, &BatchExtract::sigExtractJobPassword,
        //                this, &MainWindow::SlotNeedPassword, Qt::QueuedConnection);
        //        connect(batchJob, &BatchExtract::sigExtractJobPassword,
        //                m_encryptionpage, &EncryptionPage::wrongPassWordSlot);
        connect(batchJob,
                SIGNAL(batchFilenameProgress(KJob *, const QString &)),
                this,
                SLOT(SlotProgressFile(KJob *, const QString &)));
        //        connect(batchJob, &BatchExtract::sigCancelled,
        //                this, &MainWindow::sigquitApp);

        qDebug() << "Starting job";
        batchJob->start();
    } else if (files.last() == QStringLiteral("extract")) {
        QFileInfo fileinfo(files.at(0));
        m_decompressfilename = fileinfo.fileName();
        m_UnCompressPage->SetDefaultFile(fileinfo);
        if ("" != m_settingsDialog->getCurExtractPath() && m_UnCompressPage->getExtractType() != EXTRACT_HEAR) {
            m_UnCompressPage->setdefaultpath(m_settingsDialog->getCurExtractPath());
        } else {
            m_UnCompressPage->setdefaultpath(fileinfo.path());
        }

        loadArchive(files.at(0));
    } else if (files.last() == QStringLiteral("extract_multi")) {
        QString defaultpath;
        QFileInfo fileinfo(files.at(0));
        if ("" != m_settingsDialog->getCurExtractPath() && m_UnCompressPage->getExtractType() != EXTRACT_HEAR) {
            defaultpath = m_settingsDialog->getCurExtractPath();
        } else {
            defaultpath = fileinfo.path();
        }

        DFileDialog dialog;
        dialog.setAcceptMode(DFileDialog::AcceptOpen);
        dialog.setFileMode(DFileDialog::Directory);
        dialog.setWindowTitle(tr("Find directory"));
        dialog.setDirectory(defaultpath);

        const int mode = dialog.exec();

        if (mode != QDialog::Accepted) {
            QTimer::singleShot(100, this, [ = ] { slotquitApp(); });
            return;
        }

        QList< QUrl > selectpath = dialog.selectedUrls();
        qDebug() << selectpath;
        QString curpath = selectpath.at(0).toLocalFile();

        QStringList pathlist = files;
        pathlist.removeLast();
        m_UnCompressPage->SetDefaultFile(fileinfo);
        m_decompressfilename = fileinfo.fileName();
        m_UnCompressPage->setdefaultpath(curpath);
        m_decompressfilepath = curpath;
        m_pageid = PAGE_UNZIPPROGRESS;
        m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
        refreshPage();

        BatchExtract *batchJob = new BatchExtract();
        batchJob->setAutoSubfolder(true);
        batchJob->setDestinationFolder(curpath);
        batchJob->setPreservePaths(true);

        for (const QString &url : qAsConst(pathlist)) {
            batchJob->addInput(QUrl::fromLocalFile(url));
        }

        connect(batchJob, SIGNAL(batchProgress(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)));
        connect(batchJob, &KJob::result, this, &MainWindow::slotExtractionDone);
        connect(batchJob, &BatchExtract::sendCurFile, this, &MainWindow::slotBatchExtractFileChanged);
        connect(batchJob, &BatchExtract::sendFailFile, this, &MainWindow::slotBatchExtractError);
        //        connect(batchJob, &BatchExtract::sigExtractJobPassword,
        //                this, &MainWindow::SlotNeedPassword, Qt::QueuedConnection);
        //        connect(batchJob, &BatchExtract::sigExtractJobPassword,
        //                m_encryptionpage, &EncryptionPage::wrongPassWordSlot);
        connect(batchJob,
                SIGNAL(batchFilenameProgress(KJob *, const QString &)),
                this,
                SLOT(SlotProgressFile(KJob *, const QString &)));
        //        connect(batchJob, &BatchExtract::sigCancelled,
        //                this, &MainWindow::sigquitApp);

        qDebug() << "Starting job";
        batchJob->start();
    } else if (files.last() == QStringLiteral("compress")) {
        QStringList pathlist = files;
        pathlist.removeLast();
        emit sigZipSelectedFiles(pathlist);
        m_pageid = PAGE_ZIPSET;
        setCompressDefaultPath();
        refreshPage();
    } else if (files.last() == QStringLiteral("extract_here_split")) {
        if (files.at(0).contains(".7z.")) {
            QString filepath = files.at(0);

            transSplitFileName(filepath);

            QFileInfo fileinfo(filepath);

            if (fileinfo.exists()) {
                m_isrightmenu = true;
                QFileInfo fileinfo(files.at(0));
                m_decompressfilename = fileinfo.fileName();
                m_UnCompressPage->SetDefaultFile(fileinfo);
                m_UnCompressPage->setdefaultpath(fileinfo.path());
                loadArchive(filepath);
                m_pageid = PAGE_UNZIPPROGRESS;
                m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
                refreshPage();
            } else {
                m_CompressFail->setFailStrDetail(tr("Damaged file, unable to extract"));
                m_pageid = PAGE_UNZIP_FAIL;
                refreshPage();
            }
        }
    } else if (files.last() == QStringLiteral("extract_split")) {
        QString filepath = files.at(0);
        filepath = filepath.left(filepath.length() - 3) + "001";
        QFileInfo fileinfo(filepath);

        if (fileinfo.exists()) {
            m_decompressfilename = fileinfo.fileName();
            m_UnCompressPage->SetDefaultFile(fileinfo);
            if ("" != m_settingsDialog->getCurExtractPath() && m_UnCompressPage->getExtractType() != EXTRACT_HEAR) {
                m_UnCompressPage->setdefaultpath(m_settingsDialog->getCurExtractPath());
            } else {
                m_UnCompressPage->setdefaultpath(fileinfo.path());
            }

            loadArchive(filepath);
        } else {
            m_CompressFail->setFailStrDetail(tr("Damaged file, unable to extract"));
            m_pageid = PAGE_UNZIP_FAIL;
            refreshPage();
        }
    } else if (files.count() == 1 && Utils::isCompressed_file(files.at(0))) {
        QString filename;
        filename = files.at(0);

        //        if (filename.contains(".7z.")) {
        //            filename = filename.left(filename.length() - 3) + "001";
        //        }

        transSplitFileName(filename);

        QFileInfo fileinfo(filename);
        m_decompressfilename = fileinfo.fileName();
        m_UnCompressPage->SetDefaultFile(fileinfo);
        if ("" != m_settingsDialog->getCurExtractPath() && m_UnCompressPage->getExtractType() != EXTRACT_HEAR) {
            m_UnCompressPage->setdefaultpath(m_settingsDialog->getCurExtractPath());
        } else {
//            m_UnCompressPage->setdefaultpath(fileinfo.path());
//            m_UnCompressPage->setdefaultpath("/home/hushiwei/Documents");//只需要在这里把路径设置为第一级窗口的解压路径，而不是临时路径。
            if (strChildMndExtractPath == nullptr) {
                strChildMndExtractPath = new QString(fileinfo.path());
            }
            m_UnCompressPage->setdefaultpath(*strChildMndExtractPath);
        }
        m_pageid = Page_ID::PAGE_LOADING;
        loadArchive(filename);
    } else {
        emit sigZipSelectedFiles(files);
        m_pageid = PAGE_ZIPSET;
        setCompressDefaultPath();
        refreshPage();
    }
}

void MainWindow::slotLoadingFinished(KJob *job)
{
    m_homePage->spinnerStop();
    m_workstatus = WorkNone;
    if (job->error()) {
        int errorCode = job->error();
        if (errorCode == KJob::OpenFailedError) {
            if (job->mType == KJob::ENUM_JOBTYPE::LOADJOB) {
                LoadJob *pLoadJob = dynamic_cast<LoadJob *>(job);
                ReadOnlyArchiveInterface *pFace = pLoadJob->archiveInterface();
                QString fileName = pFace->filename();
                QString tipError = tr("Failed to open archive: %1").arg(fileName);
                m_CompressFail->setFailStrDetail(tipError);
                m_pageid = PAGE_UNZIP_FAIL;
                refreshPage();
            }
        } else {
            m_CompressFail->setFailStrDetail(tr("Damaged file, unable to extract"));
            m_pageid = PAGE_UNZIP_FAIL;
            refreshPage();
        }

        if (m_pJob) {
            m_pJob->deleteLater();
            m_pJob = nullptr;
        }
        return;
    }

    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterKeyColumn(0);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_UnCompressPage->setModel(m_filterModel);

    if (!m_isrightmenu) {
        m_pageid = PAGE_UNZIP;
        refreshPage();
        if (m_pJob) {
            m_pJob->deleteLater();
            m_pJob = nullptr;
        }
    } else {
        slotextractSelectedFilesTo(m_UnCompressPage->getDecompressPath());
    }
}

bool MainWindow::isWorkProcess()
{
    return m_workstatus == WorkProcess;
}

void MainWindow::loadArchive(const QString &files)
{
    QString transFile = files;
    transSplitFileName(transFile);

    WatcherFile(transFile);

    m_workstatus = WorkProcess;
    m_loadfile = transFile;
    m_UnCompressPage->getFileViewer()->setLoadFilePath(m_loadfile);
    m_encryptiontype = Encryption_Load;
    m_pageid = PAGE_LOADING;
    m_pJob = m_model->loadArchive(transFile, "", m_model);
    if (m_pJob == nullptr) {
        return;
    }
    LoadJob *pLoadJob = dynamic_cast<LoadJob *>(m_pJob);
    connect(pLoadJob, &LoadJob::sigLodJobPassword, this, &MainWindow::SlotNeedPassword);
    connect(pLoadJob, &LoadJob::sigWrongPassword, this, &MainWindow::SlotNeedPassword);

    m_pJob->start();
    m_homePage->spinnerStart(this, static_cast<pMember_callback>(&MainWindow::isWorkProcess));
    refreshPage();
}

void MainWindow::WatcherFile(const QString &files)
{
    SAFE_DELETE_ELE(m_fileManager);

    m_fileManager = new DFileWatcher(files, this);
    m_fileManager->startWatcher();
    qDebug() << m_fileManager->startWatcher() << "=" << files;
    m_UnCompressPage->setRootPathIndex(); //解决解压后再次打开压缩包出现返回上一级
    connect(m_fileManager, &DFileWatcher::fileMoved, this, [ = ]() { //监控压缩包，重命名时提示
        DDialog *dialog = new DDialog(this);
        dialog->setFixedWidth(440);
        QIcon icon = Utils::renderSVG(":assets/icons/deepin/builtin/icons/compress_warning_32px.svg", QSize(32, 32));
        dialog->setIcon(icon);
        dialog->setMessage(tr("The archive was changed on the disk, please import it again."));
        dialog->addButton(tr("OK"), true, DDialog::ButtonNormal);
        QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect();
        effect->setOffset(0, 4);
        effect->setColor(QColor(0, 145, 255, 76));
        effect->setBlurRadius(4);
        dialog->getButton(0)->setFixedWidth(340);
        //        dialog->getButton(0)->setGraphicsEffect(effect);
        dialog->exec();
        SAFE_DELETE_ELE(dialog);

        SAFE_DELETE_ELE(m_fileManager);

        m_pageid = PAGE_HOME;
        m_UnCompressPage->setRootPathIndex();
        this->refreshPage();
    });

}

void MainWindow::slotextractSelectedFilesTo(const QString &localPath)
{
//    m_pageid = PAGE_UNZIPPROGRESS;
//    refreshPage();
//    m_progressdialog->setProcess(0);
//    m_Progess->setprogress(0);
    m_progressdialog->setProcess(0);

    m_workstatus = WorkProcess;
    m_encryptiontype = Encryption_Extract;
    if (nullptr == m_model) {
        return;
    }

    if (nullptr == m_model->archive()) {
        return;
    }

    if (m_pJob) {
        m_pJob->deleteLater();
        m_pJob = nullptr;
    }

    ExtractionOptions options;
    QVector< Archive::Entry * > files;

    QString userDestination = localPath;
    QString destinationDirectory;

    m_pathstore = userDestination;
    //m_compressDirFiles = CheckAllFiles(m_pathstore);

    options.setAutoCreatDir(m_settingsDialog->isAutoCreatDir());
    if (pSettingInfo == nullptr) {
        pSettingInfo = new Settings_Extract_Info();
    }
    options.pSettingInfo = pSettingInfo;
    pSettingInfo->b_isAutoCreateDir = m_settingsDialog->isAutoCreatDir();
    pSettingInfo->str_defaultPath = userDestination;

    QString detectedSubfolder = "";
    if (m_settingsDialog->isAutoCreatDir()) {                   //自动创建文件夹
        if (m_model->archive()->hasMultipleTopLevelEntries()) { //如果是顶级多个目录，则创建文件夹
            detectedSubfolder = m_model->archive()->subfolderName();

            pSettingInfo->str_CreateFolder = detectedSubfolder;
            if (!userDestination.endsWith(QDir::separator())) {
                userDestination.append(QDir::separator());
            }
            destinationDirectory = userDestination + detectedSubfolder;
            QDir(userDestination).mkdir(detectedSubfolder);

            m_CompressSuccess->setCompressNewFullPath(destinationDirectory);
        } else {                        //如果是顶级单个目录，则不创建文件夹
            destinationDirectory = userDestination;
            auto rootEntry = this->m_model->getRootEntry();
            if (rootEntry->entries().length() > 0) {
                pSettingInfo->str_CreateFolder = rootEntry->entries().at(0)->name();
            } else {
                pSettingInfo->str_CreateFolder = detectedSubfolder;
            }

        }
    } else {
        destinationDirectory = userDestination;
        auto rootEntry = this->m_model->getRootEntry();
        if (rootEntry->entries().length() == 1) {
            pSettingInfo->str_CreateFolder = rootEntry->entries().at(0)->name();
        } else {
            pSettingInfo->str_CreateFolder = detectedSubfolder;
        }
    }

    qDebug() << "destinationDirectory:" << destinationDirectory;

    m_pJob = m_model->extractFiles(files, destinationDirectory, options);
    if (m_pJob == nullptr || m_pJob->mType != Job::ENUM_JOBTYPE::EXTRACTJOB) {
        qDebug() << "ExtractJob new failed.";
        return;
    }

    ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
    pExtractJob->archiveInterface()->extractTopFolderName = m_model->archive()->subfolderName();
    connect(pExtractJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)));
    connect(pExtractJob, &KJob::result, this, &MainWindow::slotExtractionDone);
    connect(pExtractJob, &ExtractJob::sigExtractJobPassword, this, &MainWindow::SlotNeedPassword, Qt::QueuedConnection);
    connect(pExtractJob, &ExtractJob::sigExtractJobPassword, m_encryptionpage, &EncryptionPage::wrongPassWordSlot);

    connect(pExtractJob, &ExtractJob::sigExtractJobPwdCheckDown, this, &MainWindow::slotShowPageUnzipProgress);
    connect(pExtractJob,
            SIGNAL(percentfilename(KJob *, const QString &)),
            this,
            SLOT(SlotProgressFile(KJob *, const QString &)));
    connect(pExtractJob, &ExtractJob::sigCancelled, this, &MainWindow::slotClearTempfile);
    connect(pExtractJob, &ExtractJob::updateDestFile, this, &MainWindow::onUpdateDestFile);

    m_decompressfilepath = destinationDirectory;

    /*if(m_model->archive()->property("isPasswordProtected").toBool() == true)
    {
        if (PAGE_ENCRYPTION != m_pageid)
        {
            m_pageid = PAGE_ENCRYPTION;
            refreshPage();
        }
        return;
    }*/
    m_Progess->pInfo()->startTimer();
    pExtractJob->archiveInterface()->destDirName = "";
    m_pJob->start();
}

void MainWindow::SlotProgress(KJob * /*job*/, unsigned long percent)
{

    calSpeedAndTime(percent);

    if (Encryption_SingleExtract == m_encryptiontype || Encryption_DRAG == m_encryptiontype) {
        if (percent < 100 && WorkProcess == m_workstatus) {
            if (!m_progressdialog->isshown()) {
                if (m_pageid != PAGE_UNZIP) {
                    m_pageid = PAGE_UNZIP;
                    refreshPage();
                }
                m_progressdialog->showdialog();
            }
            m_progressdialog->setProcess(percent);
        }
        return;
    } /*else if (Encryption_TempExtract_Open_Choose == m_encryptiontype || Encryption_TempExtract == m_encryptiontype) {
        m_pageid = PAGE_LOADING;
        m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
        refreshPage();
    }*/

    // 该函数尽量使用m_pageid来判断刷新到哪种界面，尽量不要根据m_encryptiontype去判断。
    if (m_pageid == PAGE_LOADING) {
        refreshPage();
    } else if (PAGE_ZIPPROGRESS == m_pageid || PAGE_UNZIPPROGRESS == m_pageid || PAGE_DELETEPROGRESS == m_pageid) {
        m_Progess->setprogress(percent);
    } else if ((PAGE_UNZIP == m_pageid || PAGE_ENCRYPTION == m_pageid) && (percent < 100) && m_pJob) {
        m_pageid = PAGE_UNZIPPROGRESS;
        m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
        refreshPage();
    } else if ((PAGE_ZIPSET == m_pageid) && (percent < 100)) {
        m_pageid = PAGE_ZIPPROGRESS;
        m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSING);
        refreshPage();
    }
}

void MainWindow::SlotProgressFile(KJob * /*job*/, const QString &filename)
{
    m_progressdialog->setCurrentFile(filename);
    m_Progess->setProgressFilename(filename);
}

void MainWindow::slotBatchExtractFileChanged(const QString &name)
{
    qDebug() << name;
    m_Progess->setFilename(name);
}

void MainWindow::slotBatchExtractError(const QString &name)
{
    m_CompressFail->setFailStrDetail(name + ":" + +" " + tr("Wrong password"));
    m_pageid = PAGE_UNZIP_FAIL;
    refreshPage();
}

void MainWindow::removeFromParentInfo(MainWindow *CurMainWnd)
{
    if (CurMainWnd->pCurAuxInfo != nullptr) {
        MainWindow_AuxInfo *parentInfo = CurMainWnd->pCurAuxInfo->parentAuxInfo;
        QString strWId = QString::number(CurMainWnd->winId());
        if (parentInfo) {
            QMap<QString, OpenInfo *>::iterator iter;
            QString key;
            for (iter = parentInfo->information.begin(); iter != parentInfo->information.end();) {
                //先存key
                key = iter.key();
                //指针移至下一个位置
                iter++;
                if (parentInfo->information[key]->strWinId == strWId) {
                    //删除当前位置数据
                    OpenInfo *p = parentInfo->information.take(key);
                    SAFE_DELETE_ELE(p);
                }
            }
        }

    }
}

void MainWindow::slotExtractionDone(KJob *job)
{
    m_workstatus = WorkNone;
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_NONE);
    Archive::Entry *pExtractWorkEntry = nullptr;
    if (m_pJob && m_pJob->mType == Job::ENUM_JOBTYPE::EXTRACTJOB) {
        ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
        pExtractWorkEntry = pExtractJob->getWorkEntry();

        if (this->m_pWatcher != nullptr) {
            this->m_pWatcher->finishWork();
            disconnect(this->m_pWatcher, &TimerWatcher::sigBindFuncDone, pExtractJob, &ExtractJob::slotWorkTimeOut);
            SAFE_DELETE_ELE(m_pWatcher);
        }

        int errcode = this->m_pJob->error();

        m_pJob->deleteLater();
        m_pJob = nullptr;

        if (errcode == KJob::NopasswordError) { //如果需要输入密码
            m_pageid = PAGE_ENCRYPTION;
            refreshPage();
            return;
        }

        if (errcode == 0 && m_encryptiontype != Encryption_SingleExtract && m_encryptiontype != Encryption_DRAG
                /*&& m_encryptiontype != Encryption_TempExtract_Open_Choose*/) {
            if (this->pCurAuxInfo == nullptr || this->pCurAuxInfo->information.size() == 0) {
                m_pageid = PAGE_UNZIP_SUCCESS;
                m_CompressSuccess->setstringinfo(tr("Extraction successful"));
                refreshPage();
                return;
            }
        }
    }

    int errorCode = job->error();

    if (m_pageid == PAGE_LOADING) {
        m_pOpenLoadingPage->stop();
    }
    if (m_pageid == PAGE_UNZIP/*  && m_encryptiontype != Encryption_TempExtract_Open_Choose*/) { // 如果是解压界面，则返回
        if (m_progressdialog->isshown()) {
            m_progressdialog->hide();
            // m_progressdialog->reject();
        }

        if (m_encryptiontype == Encryption_SingleExtract || m_encryptiontype == Encryption_DRAG) {
            if (errorCode == KJob::UserSkiped) {
                m_CompressSuccess->setstringinfo(tr("Skip all files"));
            } else {
                m_progressdialog->setFinished(m_decompressfilepath);
            }
        }

        return;
    } else if ((PAGE_ENCRYPTION == m_pageid) && (errorCode && (errorCode != KJob::KilledJobError && errorCode != KJob::UserSkiped)))   {

        // do noting:wrong password
    } else if (errorCode && (errorCode != KJob::KilledJobError && errorCode != KJob::UserSkiped)) {
        if (m_progressdialog->isshown()) {
            m_progressdialog->hide();
            // m_progressdialog->reject();
        }

        if (m_pathstore.left(6) == "/media" && getMediaFreeSpace() <= 50) {
            m_CompressFail->setFailStrDetail(tr("Insufficient space, please clear and retry"));
        } else if (getDiskFreeSpace() <= 50) {
            m_CompressFail->setFailStrDetail(tr("Insufficient space, please clear and retry"));
        } else {
            m_CompressFail->setFailStrDetail(tr("Damaged file, unable to extract"));
        }
        if (KJob::UserFilenameLong == errorCode) {
            m_CompressFail->setFailStrDetail(tr("Filename is too long, unable to extract"));
        } else if (KJob::OpenFailedError == errorCode) {
            m_CompressFail->setFailStrDetail(tr("Failed to open archive: %1"));
        } else if (KJob::WrongPsdError == errorCode) {
            m_CompressFail->setFailStrDetail(tr("Wrong password") + "," + tr("unable to extract"));
        }

        m_pageid = PAGE_UNZIP_FAIL;
//        if (KJob::NopasswordError == errorCode) {
//            m_pageid = PAGE_ENCRYPTION;
//        }
        refreshPage();
        return;
    } else if (Encryption_TempExtract == m_encryptiontype) {

        m_pOpenLoadingPage->stop();

        QStringList arguments;
        QString programName = "xdg-open";
        /*for (int i = 0; i < m_extractSimpleFiles.count(); i++)*/
//        {
        QString firstFileName = m_extractSimpleFiles.at(0)->name();
        bool isCompressedFile = Utils::isCompressed_file(firstFileName);
        if (isCompressedFile == true) {
            programName = "deepin-compressor";
        }
        QFileInfo file(firstFileName);
        if (file.fileName().contains("%")/* && file.fileName().contains(".png")*/) {

            QProcess p;
//                QString tempFileName = QString("%1.png").arg(openTempFileLink);
            QString tempFileName = QString("%1").arg(openTempFileLink) + "." + file.suffix();
            openTempFileLink++;
            QString commandCreate = "ln";
            QStringList args;
            args.append(DStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "tempfiles"
                        + QDir::separator() + firstFileName);
            args.append(DStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "tempfiles"
                        + QDir::separator() + tempFileName);
            arguments << DStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "tempfiles"
                      + QDir::separator() + tempFileName;   //the first arg is filePath
            p.execute(commandCreate, args);
        } else {
            QString destPath = DStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "tempfiles"
                               + QDir::separator() + firstFileName;
            if (pExtractWorkEntry != nullptr) {
                this->m_model->mapFilesUpdate.insert(destPath, pExtractWorkEntry);
            }

            arguments << destPath;  //the first arg is filePath
            if (isCompressedFile == true) {
                if (pMapGlobalWnd == nullptr) {
                    pMapGlobalWnd = new GlobalMainWindowMap();
                }
                pMapGlobalWnd->insert(QString::number(this->winId()), this);
                arguments << HEADBUS + QString::number(this->winId());//the second arg
                if (pExtractWorkEntry == nullptr) {
                    return;
                }
                QModelIndex index = this->m_model->indexForEntry(pExtractWorkEntry);
                QString strIndex = modelIndexToStr(index);
                arguments << strIndex;  //the third arg
            }
        }
//        }

        qDebug() << arguments;
        startCmd(programName, arguments);
//        KProcess *cmdprocess = new KProcess;
//        cmdprocess->setOutputChannelMode(KProcess::MergedChannels);
//        cmdprocess->setNextOpenMode(QIODevice::ReadWrite | QIODevice::Unbuffered | QIODevice::Text);
//        cmdprocess->setProgram(programPath, arguments);
//        cmdprocess->start();
        m_pageid = PAGE_UNZIP;
        refreshPage();
    } else if (Encryption_SingleExtract == m_encryptiontype || m_encryptiontype == Encryption_DRAG) {
        if (errorCode == KJob::UserSkiped) {
            m_isrightmenu = false;
            m_progressdialog->setMsg(tr("Skip all files"));
        } else {
            m_progressdialog->setFinished(m_decompressfilepath);
        }

        m_pageid = PAGE_UNZIP;
        refreshPage();

        if (m_encryptiontype != Encryption_DRAG) {
            QString fullpath = m_decompressfilepath + "/" + m_extractSimpleFiles.at(0)->property("name").toString();
            QFileInfo fileinfo(fullpath);
            if (fileinfo.exists()) {
//                DDesktopServices::showFolder(fullpath);
            }
        }
    } else if (Encryption_TempExtract_Open_Choose == m_encryptiontype) {
        m_pOpenLoadingPage->stop();
        QString ppp = program;
        if (program != tr("Choose default programma")) {
            OpenWithDialog::chooseOpen(program, QString(DEFAUTL_PATH) + m_extractSimpleFiles.at(0)->property("name").toString());
        } else {
            OpenWithDialog *dia = new OpenWithDialog(DUrl(QString(DEFAUTL_PATH) + m_extractSimpleFiles.at(0)->property("name").toString()), this);
            dia->exec();
        }

        m_pageid = PAGE_UNZIP;
        refreshPage();
    } else if (Encryption_NULL == m_encryptiontype) {
        qDebug() << "do nothing";
    } else {
        m_pageid = PAGE_UNZIP_SUCCESS;
        if (errorCode == KJob::UserSkiped) {
            m_isrightmenu = false;
            m_CompressSuccess->setstringinfo(tr("Skip all files"));
        } else {
            m_CompressSuccess->setstringinfo(tr("Extraction successful"));
        }
        refreshPage();
    }
}

void MainWindow::slotShowPageUnzipProgress()
{
    if (Encryption_TempExtract_Open_Choose == m_encryptiontype || Encryption_TempExtract == m_encryptiontype) {
        m_pageid = PAGE_LOADING;
        m_Progess->settype(Progress::OP_DECOMPRESSING);
        refreshPage();
    } else if (m_encryptiontype != Encryption_SingleExtract && m_encryptiontype != Encryption_DRAG) {
        m_pageid = PAGE_UNZIPPROGRESS;
        refreshPage();
    }
    m_progressdialog->setProcess(0);
    m_Progess->setprogress(0);
}

void MainWindow::SlotNeedPassword()
{
    if (PAGE_ENCRYPTION != m_pageid) {
        m_pageid = PAGE_ENCRYPTION;
        refreshPage();
    }
}

void MainWindow::SlotExtractPassword(QString password)
{
    m_progressdialog->clearprocess();
    // m_progressTransFlag = false;
    if (Encryption_Load == m_encryptiontype) {
        m_pageid = PAGE_LOADING;
        refreshPage();
        LoadPassword(password);
    } else if (Encryption_Extract == m_encryptiontype) {
        ExtractPassword(password);
    } else if (Encryption_SingleExtract == m_encryptiontype || Encryption_TempExtract == m_encryptiontype || Encryption_TempExtract_Open_Choose == m_encryptiontype || Encryption_DRAG == m_encryptiontype) {
        ExtractSinglePassword(password);
    }
}

void MainWindow::ExtractSinglePassword(QString password)
{
    m_workstatus = WorkProcess;
    if (m_pJob != nullptr) {
        m_pJob->deleteLater();
        m_pJob = nullptr;
    }



    if (m_pJob) {
        // first  time to extract
        ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
        pExtractJob->archiveInterface()->setPassword(password);
        pExtractJob->start();
    } else {
        // second or more  time to extract
        ExtractionOptions options;
        options.setDragAndDropEnabled(true);
        m_pJob = m_model->extractFiles(m_extractSimpleFiles, m_decompressfilepath, options);
        ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
        pExtractJob->archiveInterface()->setPassword(password);
        connect(pExtractJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)));
        connect(pExtractJob, &KJob::result, this, &MainWindow::slotExtractionDone);
        //
        connect(pExtractJob, &ExtractJob::sigExtractJobPwdCheckDown, this, &MainWindow::slotShowPageUnzipProgress);
        connect(pExtractJob, &ExtractJob::sigExtractJobPassword, this, &MainWindow::SlotNeedPassword);
        connect(
            pExtractJob, &ExtractJob::sigExtractJobPassword, m_encryptionpage, &EncryptionPage::wrongPassWordSlot);
        connect(pExtractJob,
                SIGNAL(percentfilename(KJob *, const QString &)),
                this,
                SLOT(SlotProgressFile(KJob *, const QString &)));
        connect(pExtractJob, &ExtractJob::updateDestFile, this, &MainWindow::onUpdateDestFile);

        m_pJob = pExtractJob;
        m_pJob->start();
    }
}

void MainWindow::ExtractPassword(QString password)
{
    m_workstatus = WorkProcess;

    ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
    if (pExtractJob) {
        // first  time to extract
        pExtractJob->archiveInterface()->setPassword(password);

        pExtractJob->start();
    } else {
        // second or more  time to extract
        ExtractionOptions options;
        options.setAutoCreatDir(m_settingsDialog->isAutoCreatDir());
        if (pSettingInfo == nullptr) {
            pSettingInfo = new Settings_Extract_Info();
        }
        options.pSettingInfo = pSettingInfo;

        QVector< Archive::Entry * > files;

        pExtractJob = m_model->extractFiles(files, m_decompressfilepath, options);
        pExtractJob->archiveInterface()->setPassword(password);
        connect(pExtractJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)));
        connect(pExtractJob, &KJob::result, this, &MainWindow::slotExtractionDone);
        //
        connect(pExtractJob, &ExtractJob::sigExtractJobPwdCheckDown, this, &MainWindow::slotShowPageUnzipProgress);
        connect(pExtractJob, &ExtractJob::sigExtractJobPassword, this, &MainWindow::SlotNeedPassword);
        connect(
            pExtractJob, &ExtractJob::sigExtractJobPassword, m_encryptionpage, &EncryptionPage::wrongPassWordSlot);
        connect(pExtractJob,
                SIGNAL(percentfilename(KJob *, const QString &)),
                this,
                SLOT(SlotProgressFile(KJob *, const QString &)));
        connect(pExtractJob, &ExtractJob::sigCancelled, this, &MainWindow::slotClearTempfile);
        connect(pExtractJob, &ExtractJob::updateDestFile, this, &MainWindow::onUpdateDestFile);

        m_pJob = pExtractJob;
        m_pJob->start();
    }
}
void MainWindow::LoadPassword(QString password)
{
    m_workstatus = WorkProcess;
    m_encryptiontype = Encryption_Load;
    m_pJob = m_model->loadArchive(m_loadfile, "", m_model);
    LoadJob *pLoadJob = dynamic_cast<LoadJob *>(m_pJob);
    connect(pLoadJob, &LoadJob::sigWrongPassword, this, &MainWindow::slotLoadWrongPassWord);
    pLoadJob->archiveInterface()->setPassword(password);
    if (m_pJob) {
        m_pJob->start();
    }
}

void MainWindow::setCompressDefaultPath()
{
    QString path;
    QStringList fileslist = m_CompressPage->getCompressFilelist();
    m_CompressSetting->setFilepath(fileslist);

    QFileInfo fileinfobase(fileslist.at(0));

    QString savePath = fileinfobase.path();

    for (int loop = 1; loop < fileslist.count(); loop++) {
        QFileInfo fileinfo(fileslist.at(loop));
        if (fileinfo.path() != fileinfobase.path()) {
            savePath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            break;
        }
    }

    m_CompressSetting->setDefaultPath(savePath);

    if (1 == fileslist.count()) {
        if (fileinfobase.isDir()) {
            m_CompressSetting->setDefaultName(fileinfobase.fileName());
        } else {
            m_CompressSetting->setDefaultName(fileinfobase.completeBaseName());
        }

//        m_CompressSetting->setDefaultName(fileinfobase.completeBaseName());
    } else {
        m_CompressSetting->setDefaultName(tr("Create New Archive"));
    }
}

void MainWindow::onCompressNext()
{
    m_pageid = PAGE_ZIPSET;
    setCompressDefaultPath();
    refreshPage();
}

void MainWindow::onCompressPressed(QMap< QString, QString > &Args)
{
    m_progressdialog->setProcess(0);
    m_Progess->setprogress(0);
    IsAddArchive = false;

    QStringList filesToAdd = m_CompressPage->getCompressFilelist();

    if (!filesToAdd.size()) {
        filesToAdd.push_back(Args[QStringLiteral("sourceFilePath")]);
        filesToAdd.push_back(Args[QStringLiteral("ToCompressFilePath")]);
    }

    QSet< QString > globalWorkDirList;
    foreach (QString file, filesToAdd) {
        QString globalWorkDir = file;
        if (globalWorkDir.right(1) == QLatin1String("/")) {
            globalWorkDir.chop(1);
        }
        globalWorkDir = QFileInfo(globalWorkDir).dir().absolutePath();
        globalWorkDirList.insert(globalWorkDir);
    }

    if (globalWorkDirList.count() == 1 || 0 == Args[QStringLiteral("createtar7z")].compare("true")) {
        creatArchive(Args);
    } else if (globalWorkDirList.count() > 1) {
        QMap< QString, QStringList > compressmap;
        foreach (QString workdir, globalWorkDirList) {
            QStringList filelist;
            foreach (QString file, filesToAdd) {
                QString globalWorkDir = file;
                if (globalWorkDir.right(1) == QLatin1String("/")) {
                    globalWorkDir.chop(1);
                }
                globalWorkDir = QFileInfo(globalWorkDir).dir().absolutePath();
                if (workdir == globalWorkDir) {
                    filelist.append(file);
                }
            }
            compressmap.insert(workdir, filelist);
        }

        qDebug() << compressmap;
        creatBatchArchive(Args, compressmap);
    } else {
        qDebug() << "Compress file count error!";
    }
}

void MainWindow::onUncompressStateAutoCompress(QMap<QString, QString> &Args)
{
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSDRAGADD);
    m_progressdialog->setProcess(0);
    m_Progess->setprogress(0);
    IsAddArchive = true;
    qDebug() << "开始添加压缩文件";
    addArchive(Args);
}

void MainWindow::onUncompressStateAutoCompressEntry(QMap<QString, QString> &Args, Archive::Entry *pWorkEntry)
{
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSDRAGADD);
    m_progressdialog->setProcess(0);
    m_Progess->setprogress(0);
    IsAddArchive = true;
    qDebug() << "开始添加压缩文件";
    addArchiveEntry(Args, pWorkEntry);
}

void MainWindow::creatBatchArchive(QMap< QString, QString > &Args, QMap< QString, QStringList > &filetoadd)
{
    m_pJob = new BatchCompress();
    BatchCompress *pBatchCompress = dynamic_cast<BatchCompress *>(m_pJob);
    pBatchCompress->setCompressArgs(Args);

    for (QString &key : filetoadd.keys()) {
        pBatchCompress->addInput(filetoadd.value(key));
    }

    connect(pBatchCompress, SIGNAL(batchProgress(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)));
    connect(m_pJob, &KJob::result, this, &MainWindow::slotCompressFinished);
    connect(pBatchCompress,
            SIGNAL(batchFilenameProgress(KJob *, const QString &)),
            this,
            SLOT(SlotProgressFile(KJob *, const QString &)));

    qDebug() << "Starting job";
    m_decompressfilename = Args[QStringLiteral("filename")];
    m_CompressSuccess->setCompressPath(Args[QStringLiteral("localFilePath")]);
    m_CompressSuccess->setCompressFullPath(Args[QStringLiteral("localFilePath")] + QDir::separator()
                                           + Args[QStringLiteral("filename")]);
    m_pathstore = Args[QStringLiteral("localFilePath")];
    //m_compressDirFiles = CheckAllFiles(m_pathstore);

    m_pageid = PAGE_ZIPPROGRESS;
    m_jobState = JOB_BATCHCOMPRESS;
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSING);
    refreshPage();
    m_pJob->start();
}

void MainWindow::addArchiveEntry(QMap<QString, QString> &Args, Archive::Entry *pWorkEntry)
{
    if (!IsAddArchive) {
        return;
    }
    if (!m_model) {
        return;
    }

    QString sourceArchivePath = Args[QStringLiteral("sourceFilePath")];
    QString filesToAddStr = Args[QStringLiteral("ToCompressFilePath")];
    QStringList filesToAdd = filesToAddStr.split("--");

    const QString fixedMimeType = Args[QStringLiteral("fixedMimeType")];
    const QString password = Args[QStringLiteral("encryptionPassword")];
    const QString enableHeaderEncryption = Args[QStringLiteral("encryptHeader")];
    createCompressFile_ = Args[QStringLiteral("localFilePath")] + QDir::separator() + Args[QStringLiteral("filename")];
    m_decompressfilename = Args[QStringLiteral("filename")];
    m_CompressSuccess->setCompressPath(Args[QStringLiteral("localFilePath")]);

    ReadOnlyArchiveInterface *pIface = Archive::createInterface(createCompressFile_, fixedMimeType);

    if (createCompressFile_.isEmpty()) {
        qDebug() << "filename.isEmpty()";
        return;
    }

    //renameCompress(createCompressFile_, fixedMimeType);
    m_decompressfilename = QFileInfo(createCompressFile_).fileName();
    m_CompressSuccess->setCompressFullPath(createCompressFile_);
    qDebug() << createCompressFile_;

    CompressionOptions options;
    options.setCompressionLevel(Args[QStringLiteral("compressionLevel")].toInt());
    //    options.setCompressionMethod(Args[QStringLiteral("compressionMethod")]);
    options.setEncryptionMethod(Args[QStringLiteral("encryptionMethod")]);
    options.setVolumeSize(Args[QStringLiteral("volumeSize")].toULongLong());

    QVector< Archive::Entry * > all_entries;

    foreach (QString file, filesToAdd) {
        Archive::Entry *entry = new Archive::Entry();

        QFileInfo fi(file);
        QString externalPath = fi.path() + QDir::separator();

        QString parentPath = "";
        if (m_model->getParentEntry() != nullptr) {
//            parentPath = m_model->getParentEntry()->property("fullPath").toString();
            parentPath = pWorkEntry->getParent()->property("fullPath").toString();
        }
//        QString tempFile = file;
        entry->setFullPath(parentPath + fi.fileName());//remove external path,added by hsw
//        entry->setParent(m_model->getParentEntry());
        entry->setParent(pWorkEntry->getParent());
        if (fi.isDir()) {
            entry->setIsDirectory(true);
            QHash<QString, QIcon> *map = new QHash<QString, QIcon>();
            Archive::CreateEntry(fi.absoluteFilePath(), entry, externalPath, map);
            m_model->appendEntryIcons(*map);
            map->clear();
            delete map;
            map = nullptr;
        } else {
            entry->setProperty("size", fi.size());
        }
        entry->setFullPath(file);
        all_entries.append(entry);
        m_addFile = file;
    }

    if (all_entries.isEmpty()) {
        qDebug() << "all_entries.isEmpty()";
        return;
    }

    QFileInfo fi(sourceArchivePath);
    Archive::Entry *sourceEntry  = nullptr;
    if (fi.isAbsolute()) {
        sourceEntry = new Archive::Entry();
        if (fi.isDir()) {
            sourceEntry->setIsDirectory(true);
        }

        QString globalWorkDir = sourceArchivePath;
        if (globalWorkDir.right(1) == QLatin1String("/")) {
            globalWorkDir.chop(1);
        }
        globalWorkDir = QFileInfo(globalWorkDir).dir().absolutePath();
        options.setGlobalWorkDir(globalWorkDir);
    } else {
        if (!m_UnCompressPage) {
            return;
        }
        if (fileViewer *pFViewer = m_UnCompressPage->getFileViewer()) {
            if (MyTableView *pTableView = pFViewer->getTableView()) {
                if (!pTableView->selectionModel()) {
                    return;
                }
                for (const auto &iter :  pTableView->selectionModel()->selectedRows()) {
                    sourceEntry = m_model->entryForIndex(iter);
                    if (sourceEntry->name() == sourceArchivePath) {
                        break;
                    }
                }
            }
        }
        if (!sourceEntry) {
            return;
        }
        sourceEntry->setIsDirectory(false);
        options.setGlobalWorkDir(sourceArchivePath);
    }

//    if (m_model->getParentEntry() != sourceEntry) {
//        sourceEntry = pWorkEntry->getParent();
//    }
    if (pWorkEntry->getParent() != sourceEntry) {
        sourceEntry = pWorkEntry->getParent();
    }

    resetMainwindow();
//    calSelectedTotalEntrySize(all_entries);
    qint64 size = 0;
    sourceEntry->calAllSize(size);
//    m_ProgressIns += size;
    m_Progess->pInfo()->getTotalSize() += size;

    m_pJob = m_model->addFiles(all_entries, sourceEntry, pIface, options);//this added by hsw
    if (!m_pJob) {
        return;
    }

    connect(m_pJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)), Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, &CreateJob::percentfilename, this, &MainWindow::SlotProgressFile, Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, &KJob::result, this, &MainWindow::slotJobFinished, Qt::ConnectionType::UniqueConnection);

    m_pageid = PAGE_ZIPPROGRESS;
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSDRAGADD);
    m_Progess->setProgressFilename(QFileInfo(filesToAddStr).fileName());
    m_jobState = JOB_ADD;
    refreshPage();
    m_pJob->start();
    m_workstatus = WorkProcess;
}

void MainWindow::addArchive(QMap<QString, QString> &Args)
{
    if (!IsAddArchive) {
        return;
    }
    if (!m_model) {
        return;
    }

//    m_encryptiontype = Encryption_SingleExtract;
    //m_progressdialog->setCurrentFile();

    QString sourceArchivePath = Args[QStringLiteral("sourceFilePath")];
    QString filesToAddStr = Args[QStringLiteral("ToCompressFilePath")];
    QStringList filesToAdd = filesToAddStr.split("--");

    const QString fixedMimeType = Args[QStringLiteral("fixedMimeType")];
    const QString password = Args[QStringLiteral("encryptionPassword")];
    const QString enableHeaderEncryption = Args[QStringLiteral("encryptHeader")];
    createCompressFile_ = Args[QStringLiteral("localFilePath")] + QDir::separator() + Args[QStringLiteral("filename")];
    m_decompressfilename = Args[QStringLiteral("filename")];
    m_CompressSuccess->setCompressPath(Args[QStringLiteral("localFilePath")]);

    ReadOnlyArchiveInterface *pIface = Archive::createInterface(createCompressFile_, fixedMimeType);
    if (pIface == nullptr) {
        qDebug() << "init plugin failed.";
        return;
    }
    if (createCompressFile_.isEmpty()) {
        qDebug() << "filename.isEmpty()";
        return;
    }

    //renameCompress(createCompressFile_, fixedMimeType);
    m_decompressfilename = QFileInfo(createCompressFile_).fileName();
    m_CompressSuccess->setCompressFullPath(createCompressFile_);
    //qDebug() << createCompressFile_;

    CompressionOptions options;
    options.setCompressionLevel(Args[QStringLiteral("compressionLevel")].toInt());
    //    options.setCompressionMethod(Args[QStringLiteral("compressionMethod")]);
    options.setEncryptionMethod(Args[QStringLiteral("encryptionMethod")]);
    options.setVolumeSize(Args[QStringLiteral("volumeSize")].toULongLong());

    QVector< Archive::Entry * > all_entries;
    foreach (QString file, filesToAdd) {
        Archive::Entry *entry = new Archive::Entry();

        QFileInfo fi(file);
        QString externalPath = fi.path() + QDir::separator();

        QString parentPath = "";
        if (m_model->getParentEntry() != nullptr) {
            parentPath = m_model->getParentEntry()->property("fullPath").toString();
        }

        entry->setFullPath(parentPath + fi.fileName());//remove external path,added by hsw
        entry->setParent(m_model->getParentEntry());
        if (fi.isDir()) {
            entry->setIsDirectory(true);
            QHash<QString, QIcon> *map = new QHash<QString, QIcon>();
            Archive::CreateEntryNew(fi.filePath(), entry, externalPath, map);
            m_model->appendEntryIcons(*map);
            map->clear();
            delete map;
            map = nullptr;
        } else {
            entry->setProperty("size", fi.size());
        }
        entry->setFullPath(file);
        all_entries.append(entry);
        m_addFile = file;

    }
    if (all_entries.isEmpty()) {
        qDebug() << "all_entries.isEmpty()";
        return;
    }
    QFileInfo fi(sourceArchivePath);
    Archive::Entry *sourceEntry  = nullptr;
    if (fi.isAbsolute()) {
        sourceEntry = new Archive::Entry();
        if (fi.isDir()) {
            sourceEntry->setIsDirectory(true);
        }
        QString globalWorkDir = sourceArchivePath;
        if (globalWorkDir.right(1) == QLatin1String("/")) {
            globalWorkDir.chop(1);
        }
        QFileInfo fileInfo(globalWorkDir);
        if (fileInfo.isDir()) {
            globalWorkDir = fileInfo.filePath();
        } else {
            globalWorkDir = fileInfo.absolutePath();
        }
        options.setGlobalWorkDir(globalWorkDir);
    } else {
        if (!m_UnCompressPage) {
            return;
        }
        if (fileViewer *pFViewer = m_UnCompressPage->getFileViewer()) {
            if (MyTableView *pTableView = pFViewer->getTableView()) {
                if (!pTableView->selectionModel()) {
                    return;
                }
                for (const auto &iter :  pTableView->selectionModel()->selectedRows()) {
                    sourceEntry = m_model->entryForIndex(iter);
                    if (sourceEntry->name() == sourceArchivePath) {
                        break;
                    }
                }
            }
        }
        if (!sourceEntry) {
            return;
        }
        sourceEntry->setIsDirectory(false);
        options.setGlobalWorkDir(sourceArchivePath);
    }
    qDebug() << "开始执行添加任务12";
    if (m_model->getParentEntry() != nullptr && m_model->getParentEntry() != sourceEntry) {
        //m_model->mapFilesUpdate;//根据这个获取当前位于那个sourceEntry中
        sourceEntry = m_model->getParentEntry();
    }

    resetMainwindow();

    m_pJob = m_model->addFiles(all_entries, sourceEntry, pIface, options);//this added by hsw
    if (!m_pJob) {
        return;
    }

    AddJob *pAddJob = dynamic_cast<AddJob *>(m_pJob);
    if (pAddJob->archiveInterface()->mType == ReadOnlyArchiveInterface::ENUM_PLUGINTYPE::PLUGIN_CLIINTERFACE) {//7z的计算目标大小
        calSelectedTotalEntrySize(all_entries);
    } else {
        calSelectedTotalEntrySize(all_entries);
        sourceEntry->calAllSize(m_Progess->pInfo()->getTotalSize());
    }

    connect(m_pJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)), Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, &CreateJob::percentfilename, this, &MainWindow::SlotProgressFile, Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, &KJob::result, this, &MainWindow::slotJobFinished, Qt::ConnectionType::UniqueConnection);
    m_pageid = PAGE_ZIPPROGRESS;
//    m_Progess->settype(COMPRESSING);
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSDRAGADD);
    m_Progess->setProgressFilename(QFileInfo(filesToAddStr).fileName());
    m_jobState = JOB_ADD;
    refreshPage();
    //m_pathstore = Args[QStringLiteral("localFilePath")];
    m_pJob->start();
    m_workstatus = WorkProcess;
}

void MainWindow::removeEntryVector(QVector<Archive::Entry *> &vectorDel, bool isManual)
{
    if (vectorDel.isEmpty()) {
        qDebug() << "all_entries.isEmpty()";
        return;
    }

    m_pJob =  m_model->deleteFiles(vectorDel);
    if (!m_pJob) {
        return;
    }

    connect(m_pJob, &KJob::result, this, &MainWindow::slotJobFinished, Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, &DeleteJob::percentfilename, this, &MainWindow::SlotProgressFile, Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)), Qt::ConnectionType::UniqueConnection);

    m_pageid = PAGE_DELETEPROGRESS;
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DELETEING);


    //m_Progess->settype(DECOMPRESSING);
    if (isManual) {
        m_jobState = JOB_DELETE_MANUAL;
    } else {
        m_jobState = JOB_DELETE;
    }

    //重置进度条
    resetMainwindow();

    DeleteJob *pDeleteJob = dynamic_cast<DeleteJob *>(m_pJob);
    if (pDeleteJob->archiveInterface()->mType == ReadOnlyArchiveInterface::ENUM_PLUGINTYPE::PLUGIN_READWRITE_LIBARCHIVE) {//该插件(tar格式)，计算总大小时，需要减去待删除的文件的大小
        //Archive::Entry *pRootEntry = this->m_model->getRootEntry();
        qint64 size = 0;
        this->m_model->getRootEntry()->calAllSize(size);//added by hsw for valid total size
        m_Progess->pInfo()->getTotalSize() += size;
        Archive::Entry *pFirstEntry = vectorDel[0];
        qint64 sizeCountWillDel = 0;
        pFirstEntry->calAllSize(sizeCountWillDel);
        m_Progess->pInfo()->getTotalSize() -= sizeCountWillDel;
    } else if (pDeleteJob->archiveInterface()->mType == ReadOnlyArchiveInterface::ENUM_PLUGINTYPE::PLUGIN_CLIINTERFACE) {//7z的
        calSelectedTotalEntrySize(vectorDel);
    } else { //其他格式的是否需要减去，删除子项的大小，还待调试优化。
        //Archive::Entry *pRootEntry = this->m_model->getRootEntry();
        qint64 size = 0;
        this->m_model->getRootEntry()->calAllSize(size);
        m_Progess->pInfo()->setTotalSize(size);//设置总大小
    }

    refreshPage();
    qDebug() << "delete job start";
    m_pJob->start();
    m_workstatus = WorkProcess;
}

void MainWindow::moveToArchive(QMap<QString, QString> &Args)
{
    if (!m_model) {
        return;
    }

    QString sourceArchivePath = Args[QStringLiteral("sourceFilePath")];
    QString filesToAddStr = Args[QStringLiteral("ToCompressFilePath")];
    QStringList filesToAdd = filesToAddStr.split("--");

    const QString fixedMimeType = Args[QStringLiteral("fixedMimeType")];
    const QString password = Args[QStringLiteral("encryptionPassword")];
    const QString enableHeaderEncryption = Args[QStringLiteral("encryptHeader")];
    createCompressFile_ = Args[QStringLiteral("localFilePath")] + QDir::separator() + Args[QStringLiteral("filename")];
    m_decompressfilename = Args[QStringLiteral("filename")];
    m_CompressSuccess->setCompressPath(Args[QStringLiteral("localFilePath")]);//relative path to base archive

    if (createCompressFile_.isEmpty()) {
        qDebug() << "filename.isEmpty()";
        return;
    }

    m_decompressfilename = QFileInfo(createCompressFile_).fileName();
    m_CompressSuccess->setCompressFullPath(createCompressFile_);
    qDebug() << createCompressFile_;

    CompressionOptions options;
    options.setCompressionLevel(Args[QStringLiteral("compressionLevel")].toInt());
    //    options.setCompressionMethod(Args[QStringLiteral("compressionMethod")]);
    options.setEncryptionMethod(Args[QStringLiteral("encryptionMethod")]);
    options.setVolumeSize(Args[QStringLiteral("volumeSize")].toULongLong());

    QVector< Archive::Entry * > all_entries;

    foreach (QString file, filesToAdd) {
        Archive::Entry *entry = new Archive::Entry();
        entry->setFullPath(file);
        QFileInfo fi(file);
        if (fi.isDir()) {
            entry->setIsDirectory(true);
        }

        all_entries.append(entry);
        m_addFile = file;
    }

    if (all_entries.isEmpty()) {
        qDebug() << "all_entries.isEmpty()";
        return;
    }

    QFileInfo fi(sourceArchivePath);
    Archive::Entry *sourceEntry  = nullptr;

    if (!m_UnCompressPage) {
        return;
    }
    if (fileViewer *pFViewer = m_UnCompressPage->getFileViewer()) {
        if (MyTableView *pTableView = pFViewer->getTableView()) {
            if (!pTableView->selectionModel()) {
                return;
            }
            for (const auto &iter :  pTableView->selectionModel()->selectedRows()) {
                sourceEntry = m_model->entryForIndex(iter);
                if (sourceEntry->name() == sourceArchivePath) {
                    break;
                }
            }
        }
    }
    if (!sourceEntry) {
        return;
    }
    sourceEntry->setIsDirectory(false);
    //options.setGlobalWorkDir(sourceArchivePath);


    qDebug() << "开始执行移动任务";
    m_pJob =  m_model->moveFiles(all_entries, sourceEntry, options);
    if (!m_pJob) {
        return;
    }

    connect(m_pJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)), Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, &KJob::percentfilename, this, &MainWindow::SlotProgressFile, Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, &KJob::result, this, &MainWindow::slotJobFinished, Qt::ConnectionType::UniqueConnection);

    m_pageid = PAGE_ZIPPROGRESS;
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSING);

    m_jobState = JOB_MOVE;
    refreshPage();
    m_pathstore = Args[QStringLiteral("localFilePath")];
    qDebug() << "开始执行移动任务13";
    m_pJob->start();
    m_workstatus = WorkProcess;
}

void MainWindow::transSplitFileName(QString &fileName)  // *.7z.003 -> *.7z.001
{
    QRegExp reg("^([\\s\\S]*.)[0-9]{3}$");

    if (reg.exactMatch(fileName) == false) {
        return;
    }

    QFileInfo fi(reg.cap(1) + "001");

    if (fi.exists() == true) {
        fileName = reg.cap(1) + "001";
    }
}

void MainWindow::renameCompress(QString &filename, QString fixedMimeType)
{
    QString localname = filename;
    int num = 2;

    if (m_CompressSetting->onSplitChecked()) {   // 7z分卷压缩
        QFileInfo file(filename);
        bool isFirstFileExist = false;
        bool isOtherFileExist = false;

        // 以文件名为1.7z.001验证
        // 过滤 该路径下 1*.7z.*的文件
        QStringList nameFilters;
        nameFilters << file.baseName() + "*.7z.*";
        QDir dir(file.path());
        QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Dirs | QDir::Readable, QDir::Name);

        //  循环判断 files列表 1.7z文件是否存在
        foreach (QFileInfo firstFile, files) {
            if (firstFile.baseName() == file.baseName()) {
                isFirstFileExist = true;
                break;
            } else {
                continue;
            }
        }

        if (isFirstFileExist) {  // 1.7z文件已存在  文件名为1(2).7z ...
            for (int newCount = 0; newCount < files.count(); newCount++) {
                newCount += 2;
                int count = 0;
                filename = localname.remove("." + QMimeDatabase().mimeTypeForName(fixedMimeType).preferredSuffix()) + "(" + "0"
                           + QString::number(newCount) + ")" + "."
                           + QMimeDatabase().mimeTypeForName(fixedMimeType).preferredSuffix();
                for (int i = 0; i < files.count(); i++) {
                    if (files.at(i).contains(file.baseName() + "(0" + QString::number(newCount) + ").7z.")) {
                        filename = localname.remove("." + QMimeDatabase().mimeTypeForName(fixedMimeType).preferredSuffix()) + "(" + "0"
                                   + QString::number(newCount + 1) + ")" + "."
                                   + QMimeDatabase().mimeTypeForName(fixedMimeType).preferredSuffix();

                        isOtherFileExist = true;
                        break;
                    } else {
                        count++;
                        continue;
                    }
                }

                if (isOtherFileExist) {
                    isOtherFileExist = false;
                    continue;
                }
                if (files.count() == count) {
                    break;
                }
            }
        }
    } else {
        while (QFileInfo::exists(filename)) {
            filename = localname.remove("." + QMimeDatabase().mimeTypeForName(fixedMimeType).preferredSuffix()) + "(" + "0"
                       + QString::number(num) + ")" + "."
                       + QMimeDatabase().mimeTypeForName(fixedMimeType).preferredSuffix();
            num++;
        }
    }
}

QStringList MainWindow::CheckAllFiles(QString path)
{
    QDir dir(path);
    QStringList nameFilters;
    QStringList entrys = dir.entryList(nameFilters, QDir::AllEntries | QDir::Readable, QDir::Name);

    for (int i = 0; i < entrys.count(); i++) {
        entrys.replace(i, path + QDir::separator() + entrys.at(i));
    }
    return entrys;
}

bool clearTempFiles(const QString &temp_path)
{
    bool ret = false;
    //    qDebug()<<temp_path;
    //    QDir dir(temp_path);
    //    if(dir.isEmpty())
    //    {
    //        qDebug()<<"dir.isEmpty()";
    //        return false;
    //    }
    //    QStringList filter; //过滤器
    //    filter.append("*");
    //    QDirIterator it(temp_path, filter, QDir::Dirs | QDir::Files, QDirIterator::NoIteratorFlags);
    //    while(it.hasNext()) { //若容器中还有成员，继续执行删除操作
    //        if(it.next().contains("/..") || it.next().contains("/.") || it.next().toStdString() == "")
    //        {
    //            continue;
    //        }

    //        QFileInfo fileinfo(it.next());
    //        qDebug()<<it.next();
    //        if(fileinfo.isDir())
    //        {
    //            clearTempFiles(it.next());
    //            ret = dir.rmpath(it.next());
    //            if(false == ret)
    //            {
    //                qDebug()<<"error"<<it.next();
    //            }
    //        }
    //        else {
    //            ret = dir.remove(it.next());Q
    //            if(false == ret)
    //            {
    //                qDebug()<<"error"<<it.next();
    //            }
    //        }
    //    }
    //    qDebug()<<ret;
    QProcess p;
    QString command = "rm";
    QStringList args;
    args.append("-fr");
    args.append(temp_path);
    p.execute(command, args);
    p.waitForFinished();
    return ret;
}

void MainWindow::deleteCompressFile(/*QStringList oldfiles, QStringList newfiles*/)
{
    if (m_Progess->getType() == Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSDRAGADD) {
        QFile fi(createCompressFile_ + ".tmp");
        if (fi.exists()) {
            fi.remove();
        }
        return;
    }

    QFile fi(createCompressFile_);  // 没有判断 7z分卷压缩的 文件名
    if (fi.exists()) {
        fi.remove();
    }

    if (m_CompressSetting->onSplitChecked()) {  // 7z分卷压缩
        QFileInfo file(createCompressFile_);
        QStringList nameFilters;
        nameFilters << file.fileName() + ".0*";
        QDir dir(file.path());
        QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

        foreach (QFileInfo fi, files) {
            QFile fiRemove(fi.filePath());
            if (fiRemove.exists()) {
                fiRemove.remove();
            }
        }
    }

//    if (newfiles.count() <= oldfiles.count()) {
//        qDebug() << "No file to delete";
//        return;
//    }

//    QStringList deletefile;
//    foreach (QString newpath, newfiles) {
//        int count = 0;
//        foreach (QString oldpath, oldfiles) {
//            if (oldpath == newpath) {
//                break;
//            }
//            count++;
//        }
//        if (count == oldfiles.count()) {
//            deletefile << newpath;
//        }
//    }

//    foreach (QString path, deletefile) {
//        QFileInfo fileInfo(path);
//        if (fileInfo.isFile() || fileInfo.isSymLink()) {
//            QFile::setPermissions(path, QFile::WriteOwner);
//            if (!QFile::remove(path)) {
//                qDebug() << "delete error!!!!!!!!!";
//            }
//        } else if (fileInfo.isDir()) {
//            clearTempFiles(path);
//            qDebug() << "delete ok!!!!!!!!!!!!!!";
//            if (fileInfo.exists()) {
//                clearTempFiles(path);
//            }
//        }
    //    }
}

//解压取消时删除临时文件,这个函数好像不太安全，尽量不要使用
void MainWindow::deleteDecompressFile(QString destDirName)
{
    qDebug() << "deleteDecompressFile" << m_decompressfilepath << m_decompressfilename << m_UnCompressPage->getDeFileCount() << m_model->archive()->isSingleFile() << m_model->archive()->isSingleFolder();
    bool bAutoCreatDir = m_settingsDialog->isAutoCreatDir();
    QString tmpDecompressfilepath = m_decompressfilepath;
    if (!tmpDecompressfilepath.isEmpty()) {
        if (!tmpDecompressfilepath.endsWith(QDir::separator())) {
            tmpDecompressfilepath += QDir::separator();
        }
        if (m_UnCompressPage->getDeFileCount() > 1) {
            if (bAutoCreatDir) {
                QDir fi(tmpDecompressfilepath);  //注意：若tmpDecompressfilepath为空字符串，则使用（"."）构造目录，后面会删除整个当前目录!!!
                if (fi.exists()) {
                    fi.removeRecursively();
                }
            } /*else {      //不自动创建文件夹，顶级多文件(夹)，未做删除临时文件处理
                auto rootEntry = this->m_model->getRootEntry();
                int rootEntriesNum = rootEntry->entries().length();
                for (int i = 0; i < rootEntriesNum; i++) {
                    qDebug() << rootEntry->entries().at(i)->name();
                    QDir fi(tmpDecompressfilepath + rootEntry->entries().at(i)->name());
                    if (fi.exists()) {
                        fi.removeRecursively();
                    }
                }
            }*/
        } else if (m_UnCompressPage->getDeFileCount() == 1) {
            if (!m_model->archive()->isSingleFile()) { //单个文件还是文件夹？
                QDir fi(tmpDecompressfilepath + m_model->archive()->subfolderName());
                if (fi.exists()) {
                    fi.removeRecursively();
                }
            } else {
                QFile fi(tmpDecompressfilepath + destDirName);
                if (fi.exists()) {
                    fi.remove();
                }
            }
        }
    }
}

bool MainWindow::startCmd(const QString &executeName, QStringList arguments)
{
    QString programPath = QStandardPaths::findExecutable(executeName);
    if (programPath.isEmpty()) {
        qDebug() << "error can't find xdg-mime";
        return false;
    }
    KProcess *cmdprocess = new KProcess;
    cmdprocess->setOutputChannelMode(KProcess::MergedChannels);
    cmdprocess->setNextOpenMode(QIODevice::ReadWrite | QIODevice::Unbuffered | QIODevice::Text);
    cmdprocess->setProgram(programPath, arguments);
    auto func = [ = ](int)->void {
        if (cmdprocess != nullptr)
        {
            QObject::disconnect(cmdprocess);
            delete cmdprocess;
        }
    };

    QObject::connect(cmdprocess, QOverload< int, QProcess::ExitStatus >::of(&QProcess::finished), func);
    cmdprocess->start();
    return true;
}

void MainWindow::creatArchive(QMap< QString, QString > &Args)
{
    const QStringList filesToAdd = m_CompressPage->getCompressFilelist();
    const QString fixedMimeType = Args[QStringLiteral("fixedMimeType")];
    const QString password = Args[QStringLiteral("encryptionPassword")];
    const QString enableHeaderEncryption = Args[QStringLiteral("encryptHeader")];
    createCompressFile_ = Args[QStringLiteral("localFilePath")] + QDir::separator() + Args[QStringLiteral("filename")];
    m_decompressfilename = Args[QStringLiteral("filename")];
    m_CompressSuccess->setCompressPath(Args[QStringLiteral("localFilePath")]);

    if (createCompressFile_.isEmpty()) {
        qDebug() << "filename.isEmpty()";
        return;
    }

    renameCompress(createCompressFile_, fixedMimeType);
    m_decompressfilename = QFileInfo(createCompressFile_).fileName();
    m_CompressSuccess->setCompressFullPath(createCompressFile_);
    qDebug() << createCompressFile_;

    CompressionOptions options;
    options.setCompressionLevel(Args[QStringLiteral("compressionLevel")].toInt());
    //    options.setCompressionMethod(Args[QStringLiteral("compressionMethod")]);
    options.setEncryptionMethod(Args[QStringLiteral("encryptionMethod")]);
    options.setVolumeSize(Args[QStringLiteral("volumeSize")].toULongLong());
    options.setIsTar7z(0 == Args[QStringLiteral("createtar7z")].compare("true"));
    options.setFilesSize(Args[QStringLiteral("selectFilesSize")].toLongLong());

    QVector< Archive::Entry * > all_entries;

    foreach (QString file, filesToAdd) {
        Archive::Entry *entry = new Archive::Entry(this);
        entry->setFullPath(file);



        QFileInfo fi(file);
        if (fi.isDir()) {
            entry->setIsDirectory(true);
        }

        all_entries.append(entry);
    }

    if (all_entries.isEmpty()) {
        qDebug() << "all_entries.isEmpty()";
        return;
    }

    QString globalWorkDir = filesToAdd.first();
    if (globalWorkDir.right(1) == QLatin1String("/")) {
        globalWorkDir.chop(1);
    }
    globalWorkDir = QFileInfo(globalWorkDir).dir().absolutePath();
    options.setGlobalWorkDir(globalWorkDir);

#ifdef __aarch64__ // 华为arm平台 zip压缩 性能提升. 在多线程场景下使用7z,单线程场景下使用libarchive
    double maxFileSizeProportion = static_cast<double>(maxFileSize_) / static_cast<double>(selectedTotalFileSize);
    m_createJob = Archive::create(createCompressFile_, fixedMimeType, all_entries, options, this, maxFileSizeProportion > 0.6);
#else
    m_pJob = Archive::create(createCompressFile_, fixedMimeType, all_entries, options, this);
#endif
//    m_createJob = Archive::create(createCompressFile_, fixedMimeType, all_entries, options, this);

    if (!password.isEmpty()) {
        if (m_pJob->mType == Job::ENUM_JOBTYPE::CREATEJOB) {
            CreateJob *pCreateJob = dynamic_cast<CreateJob *>(m_pJob);
            pCreateJob->enableEncryption(password, enableHeaderEncryption.compare("true") ? false : true);
        }
    }

    connect(m_pJob, &KJob::result, this, &MainWindow::slotCompressFinished, Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)), Qt::ConnectionType::UniqueConnection);
    connect(m_pJob, &CreateJob::percentfilename, this, &MainWindow::SlotProgressFile, Qt::ConnectionType::UniqueConnection);

    m_pageid = PAGE_ZIPPROGRESS;
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSING);
    m_jobState = JOB_CREATE;
    refreshPage();

    m_pathstore = Args[QStringLiteral("localFilePath")];
    //m_compressDirFiles = CheckAllFiles(m_pathstore);

    m_pJob->start();
    m_workstatus = WorkProcess;
}

void MainWindow::slotCompressFinished(KJob *job)
{
    qDebug() << "job finished" << job->error();
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_NONE);
    m_workstatus = WorkNone;
    if (job->error() && (job->error() != KJob::KilledJobError)) {
        if (m_pathstore.left(6) == "/media") {
            if (getMediaFreeSpace() <= 50) {
                m_CompressFail->setFailStrDetail(tr("Insufficient space, please clear and retry"));
            } else {
                m_CompressFail->setFailStrDetail(tr("Damaged file"));
            }
        } else {
            if (getDiskFreeSpace() <= 50) {
                m_CompressFail->setFailStrDetail(tr("Insufficient space, please clear and retry"));
            } else {
                m_CompressFail->setFailStrDetail(tr("Damaged file"));
            }
        }
        m_pageid = PAGE_ZIP_FAIL;
        refreshPage();
        return;
    }

    createCompressFile_.clear();
    m_pageid = PAGE_ZIP_SUCCESS;
    refreshPage();

    if (m_pJob) {
        m_pJob->deleteLater();
        m_pJob = nullptr;
    }
}
void MainWindow::slotJobFinished(KJob *job)
{
    qDebug() << "job finished" << job->error();
    m_workstatus = WorkNone;
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_NONE);
    if (job->error() && (job->error() != KJob::KilledJobError)) {
        if (m_pathstore.left(6) == "/media") {
            if (getMediaFreeSpace() <= 50) {
                m_CompressFail->setFailStrDetail(tr("Insufficient space, please clear and retry"));
            } else {
                m_CompressFail->setFailStrDetail(tr("Damaged file"));
            }
        } else {
            if (getDiskFreeSpace() <= 50) {
                m_CompressFail->setFailStrDetail(tr("Insufficient space, please clear and retry"));
            } else {
                m_CompressFail->setFailStrDetail(tr("Damaged file"));
            }
        }
        if (!IsAddArchive && m_jobState == JOB_CREATE) {
            m_pageid = PAGE_ZIP_FAIL;
        } else {
            m_pageid = PAGE_UNZIP;
        }
        refreshPage();
        return;
    }
    switch (m_jobState) {
    case JOB_CREATE:
        createCompressFile_.clear();
        if (!IsAddArchive) {
            m_pageid = PAGE_ZIP_SUCCESS;
        }
        if (m_pJob) {
            m_pJob->deleteLater();
            m_pJob = nullptr;
        }
//        if (m_createJob) {
//            m_createJob->deleteLater();
//            m_createJob = nullptr;
//        }
        refreshPage();
        break;
    case JOB_ADD: {
        createCompressFile_.clear();
        if (IsAddArchive) {
            m_pageid = PAGE_UNZIP;
        }

        //emit sigUpdateTableView(m_addFile);
        //reload package archive
        QString filename =   m_model->archive()->fileName();
        QStringList ArchivePath = QStringList() << filename;

        //onSelected(ArchivePath);

        if (m_pJob) {
            if (m_pJob->mType == KJob::ENUM_JOBTYPE::ADDJOB) {
                AddJob *pJob = dynamic_cast<AddJob *>(m_pJob);
                auto res = pJob->entries();
                if (res.length() > 0) {
                    this->m_UnCompressPage->getFileViewer()->selectRowByEntry(res[0]);
                }
            }
            m_pJob->deleteLater();
            m_pJob = nullptr;
        }
        refreshPage();
        emit sigTipsWindowPopUp(SUBACTION_MODE::ACTION_DRAG, ArchivePath);
    }

    break;
    case JOB_DELETE: {
        m_pageid = PAGE_UNZIP;
        //reload package archive
        QString filename =   m_model->archive()->fileName();
        QStringList ArchivePath = QStringList() << filename;

        if (m_pJob && m_pJob->mType == Job::ENUM_JOBTYPE::DELETEJOB) {
            DeleteJob *pDeleteJob = nullptr;
            pDeleteJob = dynamic_cast<DeleteJob *>(m_pJob);
            this->m_UnCompressPage->getFileViewer()->getTableView()->clearSelection();// delete 后清除选中
            Archive::Entry *pWorkEntry = pDeleteJob->getWorkEntry();
            m_pJob->deleteLater();
            m_pJob = nullptr;

            refreshPage();
            //refresh valid begin
            m_filterModel->clear();
            m_filterModel->setSourceModel(m_model);
            //refresh valid end
            qDebug() << "自动删除完成信号" << ArchivePath;
            emit deleteJobComplete(pWorkEntry);
        }

    }
    break;
    case JOB_DELETE_MANUAL: {
        m_pageid = PAGE_UNZIP;
        //reload package archive
        QString filename =   m_model->archive()->fileName();
        QStringList ArchivePath = QStringList() << filename;
        if (m_pJob->mType == Job::ENUM_JOBTYPE::DELETEJOB) {
            this->m_UnCompressPage->getFileViewer()->getTableView()->clearSelection();// delete 后清除选中
            m_pJob->deleteLater();
            m_pJob = nullptr;
        }
        refreshPage();
        //refresh valid begin
        m_filterModel->clear();
        m_filterModel->setSourceModel(m_model);
        //refresh valid end
        qDebug() << "手动删除完成信号" << ArchivePath;
//        emit deleteJobComplete();
        emit sigTipsWindowPopUp(SUBACTION_MODE::ACTION_DELETE, ArchivePath);
    }
    break;
    case JOB_LOAD:
        break;
    case JOB_COPY:
        break;
    case JOB_BATCHEXTRACT:
        break;
    case JOB_EXTRACT:
        break;
    case JOB_TEMPEXTRACT:
        break;
    case JOB_MOVE: {
        m_pageid = PAGE_UNZIP;
        //reload package archive
        if (m_pJob) {
            m_pJob->deleteLater();
            m_pJob = nullptr;
        }
        refreshPage();
    }
    break;
    case JOB_COMMENT:
        break;
    case JOB_BATCHCOMPRESS:
        break;
    case JOB_NULL:
        break;
    }
}

QString MainWindow::modelIndexToStr(const QModelIndex &index)
{
    return QString::number(index.row()) + QString::number(index.column()) + QString::number(index.internalId());
}

void MainWindow::slotExtractSimpleFiles(QVector< Archive::Entry * > fileList, QString path, EXTRACT_TYPE type)
{
    m_Progess->pInfo()->startTimer();

    resetMainwindow();

    if (m_model->archive()->fileName().endsWith(".zip") || m_model->archive()->fileName().endsWith(".jar")) {
        if (ReadOnlyArchiveInterface *pinterface = m_model->getPlugin()) {
            if (pinterface->isAllEntry()) {
                foreach (Archive::Entry *p, fileList) {
                    m_Progess->pInfo()->getTotalSize() += p->property("size").toLongLong();
                }
            } else {
                m_Progess->pInfo()->getTotalSize() = pinterface->extractSize(fileList);
            }
        }
    } else {
        foreach (Archive::Entry *p, fileList) {
            m_Progess->pInfo()->getTotalSize() += p->property("size").toLongLong();
        }
    }

//    if (type == EXTRACT_TO) {// 传递的是顶节点
//        foreach (Archive::Entry *p, fileList) {
//            p->calAllSize(m_Progess->pInfo()->getTotalSize());
//        }
//    } else {// 传递的是所有节点
//        foreach (Archive::Entry *p, fileList) {
//            m_Progess->pInfo()->getTotalSize() += p->getSize();
//        }
//    }

    m_progressdialog->setProcess(0);
    m_Progess->setprogress(0);

    m_workstatus = WorkProcess;
    m_pathstore = path;

    m_progressdialog->clearprocess();
    if (!m_model) {
        return;
    }

    if (m_pJob) {
        m_pJob->deleteLater();
        m_pJob = nullptr;
    }

    ExtractionOptions options;
    options.setDragAndDropEnabled(true);
    m_extractSimpleFiles = fileList;
    QString destinationDirectory = path;
    //m_compressDirFiles = CheckAllFiles(path);

    Archive::Entry *pDestEntry = fileList[0];

    QString programName = "";

    if (type == EXTRACT_TEMP) {
        m_encryptiontype = Encryption_TempExtract;
        m_pageid = Page_ID::PAGE_LOADING;

        // m_openType = true;
        m_Progess->setopentype(true);
        if (pCurAuxInfo == nullptr) {
            pCurAuxInfo = new MainWindow_AuxInfo();
        }

        OpenInfo *pNewInfo = nullptr;
        QModelIndex index = this->m_model->indexForEntry(fileList[0]);
        QString key = modelIndexToStr(index);
        if (pCurAuxInfo->information.contains(key) == false) {
            pNewInfo = new OpenInfo;
        } else {
            if (this->pMapGlobalWnd != nullptr) {
                MainWindow *pChild = this->pMapGlobalWnd->getOne(pCurAuxInfo->information[key]->strWinId);
                if (pChild != nullptr) {
                    QApplication::setActiveWindow(pChild);  // 置顶
                    m_workstatus = WorkNone;
                    return;
                }
            }

            SAFE_DELETE_ELE(pCurAuxInfo->information[key]);
            pCurAuxInfo->information.remove(key);
            pNewInfo = new OpenInfo;
        }
        pCurAuxInfo->information.insert(key, pNewInfo);

    } else if (type == EXTRACT_TEMP_CHOOSE_OPEN) {
        m_encryptiontype =  Encryption_TempExtract_Open_Choose;
        m_Progess->setopentype(true);
    } else if (type == EXTRACT_DRAG) {
        m_encryptiontype =  Encryption_DRAG;
    } else if (type == EXTRACT_HEAR) {
        programName = "deepin-compressor";
        m_pageid = PAGE_UNZIPPROGRESS;
        m_encryptiontype = Encryption_SingleExtract;
        //m_pathstore = QFileInfo(m_model->archive()->fileName()).absolutePath();
        m_pathstore = *strParentArchivePath;
        destinationDirectory = m_pathstore;
    } else if (type == EXTRACT_TO) {
        programName = "deepin-compressor";
        m_pageid = PAGE_UNZIPPROGRESS;
        m_encryptiontype = Encryption_SingleExtract;
    } else {
        programName = "deepin-compressor";
        m_encryptiontype = Encryption_SingleExtract;
    }

    if (!destinationDirectory.endsWith(QDir::separator())/*destinationDirectory.right(1) != QDir::separator()*/) {
        destinationDirectory = destinationDirectory + QDir::separator();
    }
    QString destEntryPath = destinationDirectory + pDestEntry->name();
    QFileInfo fileInfo(destEntryPath);

    if (fileInfo.exists() && programName == "" && (type == EXTRACT_TEMP || type == EXTRACT_TEMP_CHOOSE_OPEN)) { //判断解压文件是否已经在目标路径下已经解压出来，如果解压出来，则不再解压
        qint64 size = pDestEntry->getSize();
        qint64 size1 = calFileSize(destEntryPath);
        if (size == size1) {
            QString programName = "xdg-open";
            QString firstFileName = m_extractSimpleFiles.at(0)->name();
            bool isCompressedFile = Utils::isCompressed_file(pDestEntry->fullPath());


            QStringList arguments;
            arguments << destEntryPath;//the first arg

            if (pMapGlobalWnd == nullptr) {
                pMapGlobalWnd = new GlobalMainWindowMap();
            }
            pMapGlobalWnd->insert(QString::number(this->winId()), this);
            if (isCompressedFile == true) {

                programName = "deepin-compressor";
                arguments << HEADBUS + QString::number(this->winId());//the second arg
                QModelIndex index = this->m_model->indexForEntry(pDestEntry);
                QString strIndex = modelIndexToStr(index);
                arguments << strIndex;//the third arg
            }

            startCmd(programName, arguments);
            return;
        } else {
            clearTempFiles(destEntryPath);//if file exists but diff in size,so delete it and extract again.
        }

    }
    refreshPage();
    m_pJob = m_model->extractFiles(fileList, destinationDirectory, options);
    if (m_pJob == nullptr || m_pJob->mType != Job::ENUM_JOBTYPE::EXTRACTJOB) {
        qDebug() << "ExtractJob new failed.";
        return;
    }
    ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
    pExtractJob->archiveInterface()->bindProgressInfo(this->m_Progess->pInfo());
    if (this->m_pWatcher == nullptr) {
        this->m_pWatcher = new TimerWatcher();
        connect(this->m_pWatcher, &TimerWatcher::sigBindFuncDone, pExtractJob, &ExtractJob::slotWorkTimeOut);
    }

    pExtractJob->resetTimeOut();
    this->m_pWatcher->bindFunction(this, static_cast<pMember_callback>(&MainWindow::isWorkProcess));
    this->m_pWatcher->beginWork(100);

    connect(pExtractJob, SIGNAL(percent(KJob *, ulong)), this, SLOT(SlotProgress(KJob *, ulong)));
    connect(pExtractJob, &KJob::result, this, &MainWindow::slotExtractionDone);
//
    connect(pExtractJob, &ExtractJob::sigExtractJobPwdCheckDown, this, &MainWindow::slotShowPageUnzipProgress);
    connect(
        pExtractJob, &ExtractJob::sigExtractJobPassword, this, &MainWindow::SlotNeedPassword, Qt::QueuedConnection);
    connect(pExtractJob, &ExtractJob::sigExtractJobPassword, m_encryptionpage, &EncryptionPage::wrongPassWordSlot);
    connect(pExtractJob,
            SIGNAL(percentfilename(KJob *, const QString &)),
            this,
            SLOT(SlotProgressFile(KJob *, const QString &)));

    pExtractJob->start();
    m_decompressfilepath = destinationDirectory;

    QFileInfo file(m_loadfile);
    m_progressdialog->setCurrentTask(file.fileName());
}

void MainWindow::slotExtractSimpleFilesOpen(const QVector<Archive::Entry *> &fileList, const QString &programma)
{
    QString tmppath = DStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "tempfiles";
    QDir dir(tmppath);
    if (!dir.exists()) {
        dir.mkdir(tmppath);
    }

    program = programma;
    //lastPercent = 0;
    slotExtractSimpleFiles(fileList, tmppath, EXTRACT_TEMP_CHOOSE_OPEN);
}

void MainWindow::slotKillExtractJob()
{
//    m_openType = false;
    m_workstatus = WorkNone;
    if (m_pJob) {
//        m_encryptionjob->Killjob();
//        m_encryptionjob = nullptr;
        m_pJob->kill();
        m_pJob = nullptr;
    }
    //deleteCompressFile(m_compressDirFiles, CheckAllFiles(m_decompressfilepath));
}

void MainWindow::slotFailRetry()
{
    if (PAGE_ZIP_FAIL == m_pageid) {
        m_pageid = PAGE_ZIPSET;
        refreshPage();
    } else if (Encryption_Load == m_encryptiontype) {
        m_pageid = PAGE_HOME;
        refreshPage();
        loadArchive(m_loadfile);
    } else if (Encryption_Extract == m_encryptiontype) {
        slotextractSelectedFilesTo(m_UnCompressPage->getDecompressPath());
    } else if (Encryption_SingleExtract == m_encryptiontype) {
    }
}

void MainWindow::slotStopSpinner()
{
    if (pEventloop != nullptr) {
        pEventloop->quit();
    }
    if (m_spinner != nullptr) {
        m_spinner->stop();
        m_spinner->hide();
    }
    if (m_pJob) {
//        ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
//        disconnect(pExtractJob, &ExtractJob::sigExtractSpinnerFinished, this, &MainWindow::slotStopSpinner);
        Job *pJob = dynamic_cast<Job *>(m_pJob);
        disconnect(pJob, &ExtractJob::sigExtractSpinnerFinished, this, &MainWindow::slotStopSpinner);
    } /*else  {

    }*/

}

void MainWindow::slotWorkTimeOut()
{
    qDebug() << "slotWorkTimeOut";
}

void MainWindow::deleteFromArchive(const QStringList &files, const QString &/*archive*/)
{
    if (!m_UnCompressPage) {
        return;
    }
    Archive::Entry *pEntry = nullptr;
    if (fileViewer *pFViewer = m_UnCompressPage->getFileViewer()) {
        if (MyTableView *pTableView = pFViewer->getTableView()) {
            if (!pTableView->selectionModel()) {
                return;
            }
            for (const auto &iter :  pTableView->selectionModel()->selectedRows()) {
                pEntry = m_model->entryForIndex(iter);
                break;
            }
        }
    }
    if (!pEntry) {
        return;
    }


    QVector< Archive::Entry * > all_entries;

    foreach (QString file, files) {
        Archive::Entry *entry = new Archive::Entry();
        entry->setFullPath(file);

        QFileInfo fi(file);
        if (fi.isDir()) {
            entry->setIsDirectory(true);
        }

        all_entries.append(entry);
    }

    if (all_entries.isEmpty()) {
        qDebug() << "all_entries.isEmpty()";
        return;
    }

    m_pJob =  m_model->deleteFiles(all_entries);
    if (!m_pJob) {
        return;
    }

    connect(m_pJob, &KJob::result, this, &MainWindow::slotJobFinished, Qt::ConnectionType::UniqueConnection);
    m_pageid = PAGE_DELETEPROGRESS;
    m_Progess->settype(Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING);
    m_jobState = JOB_DELETE;

    m_pJob->start();
    m_workstatus = WorkProcess;
}
/**
 * @brief MainWindow::closeExtractJobSafe
 * @see 安全地退出解压过程并关闭
 */
void MainWindow::closeExtractJobSafe()
{
    slotResetPercentAndTime();
    m_isrightmenu = false;
    if (m_pJob && m_pJob->mType == Job::ENUM_JOBTYPE::EXTRACTJOB) {
        if (pEventloop == nullptr) {
            pEventloop = new QEventLoop(this->m_Progess);
        }
        m_encryptiontype = Encryption_NULL;
        ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
        pExtractJob->archiveInterface()->extractPsdStatus = ReadOnlyArchiveInterface::ExtractPsdStatus::Canceled;
        if (pEventloop->isRunning() == false) {
            connect(pExtractJob, &ExtractJob::sigExtractSpinnerFinished, this, &MainWindow::slotStopSpinner);
            m_pJob->kill();
            m_pJob = nullptr;
//            pEventloop->exec(QEventLoop::ExcludeUserInputEvents);
        } else {
            m_pJob->kill();
            m_pJob = nullptr;
        }
    }

    //deleteDecompressFile();
}

void MainWindow::slotLoadWrongPassWord()
{
    if (Encryption_Load == m_encryptiontype) {
        m_pOpenLoadingPage->stop();
        m_pageid = PAGE_ENCRYPTION;
        m_mainLayout->setCurrentIndex(7);
    }

    m_encryptionpage->setInputflag(true);
    m_encryptionpage->wrongPassWordSlot();
}

//void MainWindow::addToArchive(const QStringList &files, const QString &archive)
//{
//    qDebug() << "执行添加操作" << "向" << archive << "添加文件";
//    if (!m_CompressSetting) return;
//    if (!m_model) return;


//    //add to source archive
//    qDebug() << "添加路径为：" <<  m_model->archive()->fileName();
//    m_CompressSetting->autoCompress(m_model->archive()->fileName(), files);

//    //move files to archive
//    m_CompressSetting->autoMoveToArchive(files, archive);

//}

void MainWindow::onCancelCompressPressed(Progress::ENUM_PROGRESS_TYPE compressType)
{
//    m_compressType = compressType;
    slotResetPercentAndTime();
    m_isrightmenu = false;
    m_pageid = PAGE_UNZIP;
    QString destDirName;
    if (m_pJob && m_pJob->mType == Job::ENUM_JOBTYPE::EXTRACTJOB) { // 解压取消
        ExtractJob *pExtractJob = dynamic_cast<ExtractJob *>(m_pJob);
        destDirName = pExtractJob->archiveInterface()->destDirName;
//        if (pExtractJob->archiveInterface()->mType == ReadOnlyArchiveInterface::ENUM_PLUGINTYPE::PLUGIN_CLIINTERFACE) {
//            //append the spiner animation to the eventloop, so can play the spinner animation
//            if (pEventloop == nullptr) {
//                pEventloop = new QEventLoop(this->m_Progess);
//            }


//            pExtractJob->archiveInterface()->extractPsdStatus = ReadOnlyArchiveInterface::ExtractPsdStatus::Canceled;
//            if (pEventloop->isRunning() == false) {
//                connect(pExtractJob, &ExtractJob::sigExtractSpinnerFinished, this, &MainWindow::slotStopSpinner);
//                if (m_spinner == nullptr) {
//                    m_spinner = new DSpinner(this->m_Progess);
//                    m_spinner->setFixedSize(40, 40);
//                }
//                m_spinner->move(this->m_Progess->width() / 2 - 20, this->m_Progess->height() / 2 - 20);
//                m_spinner->hide();
//                m_spinner->start();
//                m_spinner->show();
//                m_pJob->kill();
//                m_pJob = nullptr;
//                pEventloop->exec(QEventLoop::ExcludeUserInputEvents);
//            } else {
//                m_pJob->kill();
//                m_pJob = nullptr;
//            }
//        } else {
        if (pEventloop == nullptr) {
            pEventloop = new QEventLoop(this->m_Progess);
        }
        pExtractJob->archiveInterface()->extractPsdStatus = ReadOnlyArchiveInterface::ExtractPsdStatus::Canceled;
        if (pEventloop->isRunning() == false) {
            connect(pExtractJob, &ExtractJob::sigExtractSpinnerFinished, this, &MainWindow::slotStopSpinner);
            if (m_spinner == nullptr) {
                m_spinner = new DSpinner(this->m_Progess);
                m_spinner->setFixedSize(40, 40);
            }
            m_spinner->move(this->m_Progess->width() / 2 - 20, this->m_Progess->height() / 2 - 20);
            m_spinner->hide();
            m_spinner->start();
            m_spinner->show();
            m_pJob->kill();
            m_pJob = nullptr;
            pEventloop->exec(QEventLoop::ExcludeUserInputEvents);
        } else {
            m_pJob->kill();
            m_pJob = nullptr;
        }
//        }
    } else if (m_pJob && m_pJob->mType == Job::ENUM_JOBTYPE::DELETEJOB) {
        DeleteJob *pDeleteJob = dynamic_cast<DeleteJob *>(m_pJob);
        if (pDeleteJob->archiveInterface()->mType == ReadOnlyArchiveInterface::ENUM_PLUGINTYPE::PLUGIN_CLIINTERFACE) {
            m_pJob->kill();
            m_pJob = nullptr;
            m_pageid = PAGE_UNZIP;
        }
    } else {
        if (m_pJob) {
            m_pJob->kill();
            m_pJob = nullptr;
            m_pageid = PAGE_UNZIP;
        }
    }

    deleteCompressFile(/*m_compressDirFiles, CheckAllFiles(m_pathstore)*/);
    deleteDecompressFile(destDirName);

    if (compressType == Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSING) {
        if (m_pJob) {
//            m_createJob->deleteLater();
            m_pJob->kill();
            m_pJob = nullptr;
        } else if (m_pJob != nullptr) {
//            batchJob->doKill();
            m_pJob->kill();
            m_pJob = nullptr;
        }
        m_pageid = PAGE_ZIP;
    } else if (compressType == Progress::ENUM_PROGRESS_TYPE::OP_DECOMPRESSING) {
        m_pageid = PAGE_UNZIP;
    } else if (compressType == Progress::ENUM_PROGRESS_TYPE::OP_COMPRESSDRAGADD) {
        if (m_pJob) {
//            m_createJob->deleteLater();
            m_pJob->kill();
            m_pJob = nullptr;
        }
        m_pageid = PAGE_UNZIP;
    }
    refreshPage();
    // emit sigquitApp();
    slotResetPercentAndTime();
    m_Progess->setprogress(0);

}

void MainWindow::slotClearTempfile()
{
    openTempFileLink = 0;
    QProcess p;
    QString command = "rm";
    QStringList args;
    args.append("-rf");
    args.append(DStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "tempfiles");
    p.execute(command, args);
    p.waitForFinished();
}

void MainWindow::slotquitApp()
{
    --m_windowcount;

    if (m_windowcount == 0) {
        QProcess p;
        QString command = "rm";
        QStringList args;
        args.append("-rf");
        args.append(DStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "tempfiles");
        p.execute(command, args);
        p.waitForFinished();

        emit sigquitApp();
    }

}

void MainWindow::onUpdateDestFile(QString destFile)
{
    m_CompressSuccess->setCompressFullPath(destFile);
}

void MainWindow::onCompressPageFilelistIsEmpty()
{
    m_pageid = PAGE_HOME;
    refreshPage();
}

void MainWindow::slotCalDeleteRefreshTotalFileSize(const QStringList &files)
{
    resetMainwindow();

    calSelectedTotalFileSize(files);
}

//void MainWindow::slotUncompressCalDeleteRefreshTotalFileSize(const QStringList &files)
//{
//    resetMainwindow();

//    calSelectedTotalFileSize(files);

//    removeFromArchive(files);
//}

void MainWindow::slotUncompressCalDeleteRefreshTotoalSize(QVector<Archive::Entry *> &vectorDel, bool isManual)
{
//    resetMainwindow();
////    calSelectedTotalEntrySize(vectorDel);
//    Archive::Entry *pRootEntry = this->m_model->getRootEntry();
//    this->m_model->getRootEntry()->calAllSize(selectedTotalFileSize);//added by hsw for valid total size
    removeEntryVector(vectorDel, isManual);
}

void MainWindow::resetMainwindow()
{
//    selectedTotalFileSize = 0;
//    lastPercent = 0;

//#ifdef __aarch64__
//    maxFileSize_ = 0;
//#endif
    m_Progess->pInfo()->resetProgress();
    m_Progess->setprogress(0);
    m_progressdialog->setProcess(0);
}

void MainWindow::slotBackButtonClicked()
{
    resetMainwindow();

    slotResetPercentAndTime();
    m_encryptionpage->resetPage();
    m_CompressSuccess->clear();

    if (m_pageid == PAGE_ZIP_SUCCESS || m_pageid == PAGE_UNZIP_SUCCESS) {
        m_CompressPage->clearFiles();
        m_Progess->setprogress(0);
        m_progressdialog->setProcess(0);
    }

    m_pageid = PAGE_HOME;
    refreshPage();
}

void MainWindow::slotResetPercentAndTime()
{
    m_Progess->setopentype(false);
    m_Progess->pInfo()->resetProgress();
}

void MainWindow::slotFileUnreadable(QStringList &pathList, int fileIndex)
{
    pathList.removeAt(fileIndex);
    if (m_pageid != PAGE_ZIP) {
        m_pageid = PAGE_ZIP;
        refreshPage();
    }
    m_CompressPage->onRefreshFilelist(pathList);
    if (pathList.isEmpty()) {
        m_pageid = PAGE_HOME;
        refreshPage();
    }
}

void MainWindow::onTitleButtonPressed()
{
    switch (m_pageid) {
    case PAGE_ZIP:
        emit sigZipAddFile();
        break;
    case PAGE_ZIPSET:
        emit sigZipReturn();
        m_CompressSetting->clickTitleBtnResetAdvancedOptions();
        m_pageid = PAGE_ZIP;
        refreshPage();
        break;
    case PAGE_ZIP_SUCCESS:
    case PAGE_ZIP_FAIL:
        m_CompressSuccess->clear();
        m_pageid = PAGE_ZIP;
        refreshPage();
        break;
    case PAGE_UNZIP:
        //addArchive();
        emit sigCompressedAddFile();
        break;
    case PAGE_UNZIP_SUCCESS:
    case PAGE_UNZIP_FAIL:
        m_CompressSuccess->clear();

        if (m_UnCompressPage->getFileCount() < 1) {
            m_pageid = PAGE_HOME;
        } else {
            m_pageid = PAGE_UNZIP;
        }
        refreshPage();
        break;
    default:
        break;
    }
    return;
}

void MainWindow::onCompressAddfileSlot(bool status)
{
    if (false == status) {
        m_titlebutton->setVisible(false);
        m_openAction->setEnabled(false);
        //        setAcceptDrops(false);
    } else {
        m_titlebutton->setIcon(DStyle::StandardPixmap::SP_IncreaseElement);
        m_titlebutton->setVisible(true);
        m_openAction->setEnabled(true);
        //        setAcceptDrops(true);
    }
}

bool MainWindow::checkSettings(QString file)
{
    QString fileMime = Utils::judgeFileMime(file);
    bool hasSetting = true;

    bool existMime;
    if (fileMime.size() == 0) {
        existMime = true;
    } else {
        existMime = Utils::existMimeType(fileMime);
    }

    if (existMime) {
        QString defaultCompress = getDefaultApp(fileMime);

        if (defaultCompress.startsWith("dde-open.desktop")) {
            setDefaultApp(fileMime, "deepin-compressor.desktop");
        }
    } else {
        QString defaultCompress = getDefaultApp(fileMime);
        if (defaultCompress.startsWith("deepin-compressor.desktop")) {
            setDefaultApp(fileMime, "dde-open.desktop");
        }

        int re = promptDialog();
        if (re != 1) {
            hasSetting = false;
        }
    }

    return hasSetting;
}

QString MainWindow::getDefaultApp(QString mimetype)
{
    QString outInfo;
    QProcess p;
    QString command3 = "xdg-mime query default %1";
    p.start(command3.arg("application/" + mimetype));
    p.waitForFinished();
    outInfo = QString::fromLocal8Bit(p.readAllStandardOutput());

    return  outInfo;
}

void MainWindow::setDefaultApp(QString mimetype, QString desktop)
{
    QProcess p;
    QString command3 = "xdg-mime default %1 %2";
    p.start(command3.arg(desktop).arg("application/" + mimetype));
    p.waitForFinished();
}

int MainWindow::promptDialog()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenRect =  screen->availableVirtualGeometry();

    DDialog *dialog = new DDialog(this);
    QPixmap pixmap = Utils::renderSVG(":assets/icons/deepin/builtin/icons/compress_warning_32px.svg", QSize(32, 32));
    dialog->setIcon(pixmap);
    dialog->setMinimumSize(380, 140);
    dialog->addButton(tr("OK"), true, DDialog::ButtonNormal);
    dialog->move(((screenRect.width() / 2) - (dialog->width() / 2)), ((screenRect.height() / 2) - (dialog->height() / 2)));
    DLabel *pContent = new DLabel(tr("Please open the Archive Manager and set the file association type"), dialog);

    pContent->setAlignment(Qt::AlignmentFlag::AlignHCenter);
    DPalette pa;
    pa = DApplicationHelper::instance()->palette(pContent);
    pa.setBrush(DPalette::Text, pa.color(DPalette::ButtonText));
    DFontSizeManager::instance()->bind(pContent, DFontSizeManager::T6, QFont::Medium);
    pContent->setMinimumSize(293, 20);

    QVBoxLayout *mainlayout = new QVBoxLayout;
    mainlayout->setContentsMargins(0, 0, 0, 0);
    mainlayout->addWidget(pContent, 0, Qt::AlignHCenter | Qt::AlignVCenter);
    mainlayout->addSpacing(15);

    DWidget *widget = new DWidget(dialog);
    widget->setLayout(mainlayout);
    dialog->addContent(widget);
    int res = dialog->exec();
    SAFE_DELETE_ELE(dialog);

    return res;
}

