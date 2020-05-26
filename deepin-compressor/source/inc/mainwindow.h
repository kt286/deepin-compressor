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

#pragma once

#include <DMainWindow>
#include <QSettings>
#include <DTitlebar>
#include <DFileWatcher>
#include <QElapsedTimer>
#include <DIconButton>
#include <log4qt/logger.h>

#include "homepage.h"
#include "uncompresspage.h"
#include "compresspage.h"
#include "compresssetting.h"
#include "progress.h"
#include "compressor_success.h"
#include "compressor_fail.h"
#include "archive_manager.h"
#include "archivemodel.h"
#include "encryptionpage.h"
#include "progressdialog.h"
#include "extractpausedialog.h"
#include "settingdialog.h"
#include "encodingpage.h"
#include <DIconButton>
#include "archivesortfiltermodel.h"
#include "batchextract.h"
#include "batchcompress.h"
#include <DFileWatcher>
#include <QElapsedTimer>
#include <QQueue>


#define TITLE_FIXED_HEIGHT 50
#define HEADBUS "/QtDusServer/registry"

DWIDGET_USE_NAMESPACE

enum Page_ID {
    PAGE_HOME,
    PAGE_UNZIP,
    PAGE_ZIP,
    PAGE_ZIPSET,
    PAGE_ZIPPROGRESS,
    PAGE_UNZIPPROGRESS,
    PAGE_ZIP_SUCCESS,
    PAGE_ZIP_FAIL,
    PAGE_UNZIP_SUCCESS,
    PAGE_UNZIP_FAIL,
    PAGE_ENCRYPTION,
    PAGE_DELETEPROGRESS,
    PAGE_MAX
};

enum EncryptionType {
    Encryption_NULL,
    Encryption_Load,
    Encryption_Extract,
    Encryption_SingleExtract,
    Encryption_ExtractHere,
    Encryption_TempExtract,
    Encryption_TempExtract_Open,
    Encryption_TempExtract_Open_Choose,
    Encryption_DRAG
};

enum WorkState {
    WorkNone,
    WorkProcess,
};

class QStackedLayout;
enum JobState {
    JOB_NULL,
    JOB_ADD,
    JOB_DELETE,
    JOB_DELETE_MANUAL,//手动delete，而非消息通知的delete
    JOB_CREATE,
    JOB_LOAD,
    JOB_COPY,
    JOB_BATCHEXTRACT,
    JOB_EXTRACT,
    JOB_TEMPEXTRACT,
    JOB_MOVE,
    JOB_COMMENT,
    JOB_BATCHCOMPRESS,
};

class MainWindow;

/**
 * this can help us to get the map of all mainwindow created.
 * @brief The GlobalMainWindowMap struct
 */
struct GlobalMainWindowMap {
public:
    void insert(const QString &strWinId, MainWindow *wnd)
    {
        if (this->mMapGlobal.contains(strWinId) == false) {
            this->mMapGlobal.insert(strWinId, wnd);
        }
    }

    MainWindow *getOne(const QString &strWinId)
    {
        if (this->mMapGlobal.contains(strWinId) == false) {
            return nullptr;
        } else {
            return this->mMapGlobal[strWinId];
        }
    }

    void remove(const QString &strWinId)
    {
        if (this->mMapGlobal.contains(strWinId) == true) {
            this->mMapGlobal.remove(strWinId);
        }
    }

    void clear()
    {
        this->mMapGlobal.clear();
    }

    /**
     * @brief mMapGlobal
     * @ key: winId
     * @ value: pointer of mainWindow
     */
    QMap<QString, MainWindow *> mMapGlobal;
};

struct OpenInfo {
    QString strWinId = "";//open view the winId
    bool open = false;
};

struct MainWindow_AuxInfo {
    /**
     * @brief infomation
     * @ key :strModexIndex,see as modelIndexToStr()
     * @ value :the pointer of open info
     */
    QMap<QString, OpenInfo *> information;
    /**
     * @brief childAuxInfo
     */
//    MainWindow_AuxInfo *childAuxInfo = nullptr;
    /**
     * @brief parentAuxInfo
     */
    MainWindow_AuxInfo *parentAuxInfo = nullptr;
};

static QVector<qint64> m_tempProcessId;
static Log4Qt::Logger *m_logger = nullptr;
class QStackedLayout;
static int m_windowcount = 1;
class MonitorAdaptor;
class MainWindow : public DMainWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.archive.mainwindow.monitor")

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void closeEvent(QCloseEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

    void InitUI();
    void InitConnection();
    void initTitleBar();
    QMenu *createSettingsMenu();
    void loadArchive(const QString &files);
    void creatArchive(QMap<QString, QString> &Args);
    void creatBatchArchive(QMap<QString, QString> &Args, QMap<QString, QStringList> &filetoadd);
    void addArchive(QMap<QString, QString> &Args);
    void addArchiveEntry(QMap<QString, QString> &args, Archive::Entry *pWorkEntry);
//    void removeFromArchive(const QStringList &removeFilePaths);
    /**
     * @brief removeEntryVector
     * @param vectorDel
     * @param isManual,true:by action clicked; false: by message emited.
     */
    void removeEntryVector(QVector<Archive::Entry *> &vectorDel, bool isManual);
    void moveToArchive(QMap<QString, QString> &Args);

    void transSplitFileName(QString &fileName); // *.7z.003 -> *.7z.001

    void ExtractPassword(QString password);
    void ExtractSinglePassword(QString password);
    void LoadPassword(QString password);
    void WatcherFile(const QString &files);
    void renameCompress(QString &filename, QString fixedMimeType);
    static QString getLoadFile();
    qint64 getDiskFreeSpace();
    qint64 getMediaFreeSpace();

    bool applicationQuit();
    QString getAddFile();


    //log
    void initalizeLog(QWidget *widget);
    void logShutDown();
    void bindAdapter();
//    static Log4Qt::Logger *getLogger();

private:
    void saveWindowState();
    void loadWindowState();
    QString modelIndexToStr(const QModelIndex &modelIndex);//added by hsw 20200525
protected:
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;
    void dragMoveEvent(QDragMoveEvent *event) override;

public slots:
    //accept subwindows drag files and return tips string
    bool onSubWindowActionFinished(int mode, const qint64 &pid, const QStringList &urls);

    bool popUpChangedDialog(const qint64 &pid);

    bool createSubWindow(const QStringList &urls);

private slots:
    void setEnable();
    void setDisable();
    void refreshPage();
    void onSelected(const QStringList &);
    void onRightMenuSelected(const QStringList &);
    void onCompressNext();
    void onCompressPressed(QMap<QString, QString> &Args);
    void onUncompressStateAutoCompress(QMap<QString, QString> &Args);
    // added by hsw 20200525
    void onUncompressStateAutoCompressEntry(QMap<QString, QString> &Args, Archive::Entry *pWorkEntry = nullptr);
    void onCancelCompressPressed(int compressType);
    void onTitleButtonPressed();
    void onCompressAddfileSlot(bool status);

    void slotLoadingFinished(KJob *job);
    void slotExtractionDone(KJob *job);
    void slotShowPageUnzipProgress();
    void slotextractSelectedFilesTo(const QString &localPath);
    void SlotProgress(KJob *job, unsigned long percent);
    void SlotProgressFile(KJob *job, const QString &filename);
    void SlotNeedPassword();
    void SlotExtractPassword(QString password);
    void slotCompressFinished(KJob *job);
    void slotJobFinished(KJob *job);
    void slotExtractSimpleFiles(QVector<Archive::Entry *> fileList, QString path, EXTRACT_TYPE type);
    void slotExtractSimpleFilesOpen(const QVector<Archive::Entry *> &fileList, const QString &programma);
    void slotKillExtractJob();
    void slotFailRetry();
    void slotBatchExtractFileChanged(const QString &name);
    void slotBatchExtractError(const QString &name);
    void slotClearTempfile();
    void slotquitApp();
    void onUpdateDestFile(QString destFile);
    void onCompressPageFilelistIsEmpty();

    void slotCalDeleteRefreshTotalFileSize(const QStringList &files);
//    void slotUncompressCalDeleteRefreshTotalFileSize(const QStringList &files);
    /**
     * @brief slotUncompressCalDeleteRefreshTotoalSize
     * @param vectorDel
     * @param isManual,true:by action clicked; false: by message emited.
     */
    void slotUncompressCalDeleteRefreshTotoalSize(QVector<Archive::Entry *> &vectorDel, bool isManual);

    void resetMainwindow();
    void slotBackButtonClicked();
    void slotResetPercentAndTime();
    void slotFileUnreadable(QStringList &pathList, int fileIndex);//compress file is unreadable or file is a link
    void slotStopSpinner();

    void deleteFromArchive(const QStringList &files, const QString &archive);
    void addToArchive(const QStringList &files, const QString &archive);

signals:
    void sigquitApp();
    void sigZipAddFile();
    void sigCompressedAddFile();
    void sigZipReturn();
    void sigZipSelectedFiles(const QStringList &files);
    void loadingStarted();
    void sigUpdateTableView(const QFileInfo &);
    void sigTipsWindowPopUp(int, const QStringList &);
    void sigTipsUpdateEntry(int, QVector<Archive::Entry *> &vectorDel);
    void deleteJobComplete();
    void deleteJobComplete1(Archive::Entry *pEntry);

private:
    Archive *m_archive_manager = nullptr;
    ArchiveModel *m_model = nullptr;
    ArchiveSortFilterModel *m_filterModel;
    QString m_decompressfilename;
    QString m_decompressfilepath;
    static QString m_loadfile;
    QString m_addFile;

    void setCompressDefaultPath();
    void setQLabelText(QLabel *label, const QString &text);
    QJsonObject creatShorcutJson();

    QStringList CheckAllFiles(QString path);
    void deleteCompressFile(/*QStringList oldfiles, QStringList newfiles*/);
    void deleteDecompressFile(QString destDirName = "");

private:
    DLabel *m_logo;
    QPixmap m_logoicon;
    QFrame *m_titleFrame;
    DLabel *m_titlelabel;
    DWidget *m_mainWidget;
    QStackedLayout *m_mainLayout;
    HomePage *m_homePage;
    UnCompressPage *m_UnCompressPage;
    CompressPage *m_CompressPage;
    CompressSetting *m_CompressSetting;
    Progress *m_Progess;
    Compressor_Success *m_CompressSuccess;
    Compressor_Fail *m_CompressFail;
    EncryptionPage *m_encryptionpage;
    ProgressDialog *m_progressdialog;
    SettingDialog *m_settingsDialog = nullptr;
    EncodingPage *m_encodingpage;
    QSettings *m_settings;
    Page_ID m_pageid;

    QVector<Archive::Entry *> m_extractSimpleFiles;

    DIconButton *m_titlebutton = nullptr;

    BatchCompress *batchJob = nullptr;
    ExtractJob *m_encryptionjob = nullptr;
    LoadJob *m_loadjob = nullptr;
    CreateJob *m_createJob = nullptr;
    AddJob *m_addJob = nullptr;
    MoveJob *m_moveJob = nullptr;
    DeleteJob *m_DeleteJob = nullptr;
    EncryptionType m_encryptiontype = Encryption_NULL;
    bool m_isrightmenu = false;
    WorkState m_workstatus = WorkNone;
    JobState m_jobState = JOB_NULL;

    int m_timerId = 0;
    //bool m_progressTransFlag = false;
    QAction *m_openAction;

    //QStringList m_compressDirFiles;
    QString createCompressFile_;

    QString m_pathstore;
    bool m_initflag = false;
    int m_startTimer = 0;
    int m_watchTimer = 0;

    DFileWatcher *m_fileManager = nullptr;
    int openTempFileLink = 0;
    QEventLoop *pEventloop = nullptr;
    DSpinner *m_spinner = nullptr;

    bool IsAddArchive = false;

    GlobalMainWindowMap *pMapGlobalWnd = nullptr;//added by hsw 20200521
    MonitorAdaptor *pAdapter = nullptr;//added by hsw 20200521
    MainWindow_AuxInfo *pCurAuxInfo = nullptr;//added by hsw 20200525
    int m_compressType;

private:
    void calSelectedTotalFileSize(const QStringList &files);
    void calSelectedTotalEntrySize(QVector<Archive::Entry *> &vectorDel);
    qint64 calFileSize(const QString &path);
    void calSpeedAndTime(unsigned long compressPercent);

    QElapsedTimer m_timer;
    unsigned long lastPercent = 0;
    qint64 selectedTotalFileSize = 0;
    qint64 compressTime = 0;
    QReadWriteLock m_lock;
    QString program;
    QMap<qint64, QStringList> m_subWinDragFiles;
    int m_mode = 0;
    qint64 m_curOperChildPid = 0;

#ifdef __aarch64__
    qint64 maxFileSize_ = 0;
#endif
};














