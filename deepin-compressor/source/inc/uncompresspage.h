/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     dongsen <dongsen@deepin.com>
 *
 * Maintainer: dongsen <dongsen@deepin.com>
 *             AaronZhang <ya.zhang@archermind.com>
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

#ifndef SINGLEFILEPAGE_H
#define SINGLEFILEPAGE_H

#include "DWidget"
#include <DFileDialog>
#include <DPushButton>
#include <DLineEdit>
#include "DLabel"
#include "lib_edit_button.h"
#include <DCommandLinkButton>
#include <DPalette>
#include "archivesortfiltermodel.h"
#include <DApplicationHelper>
#include "fileViewer.h"

DWIDGET_USE_NAMESPACE

class UnCompressPage : public DWidget
{
    Q_OBJECT

public:
    UnCompressPage(QWidget *parent = nullptr);

    void setModel(ArchiveSortFilterModel *model);
    QString getDecompressPath();
    void setdefaultpath(QString path);
    void SetDefaultFile(QFileInfo info);
    int getFileCount();
    int getDeFileCount();
    int showWarningDialog(const QString &msg);
    EXTRACT_TYPE getExtractType();
    void setRootPathIndex();
    void slotCompressedAddFile();
    fileViewer *getFileViewer();
    int showReplaceDialog(QString name);

signals:
    void sigDecompressPress(const QString &localPath);
    void sigextractfiles(QVector<Archive::Entry *>, QString path, EXTRACT_TYPE type);
    void sigOpenExtractFile(const QVector<Archive::Entry *> &fileList, const QString &programma);
    void sigFilelistIsEmpty();
//    void sigRefreshFileList(const QStringList &files);
    /**
     * @brief sigRefreshEntryVector
     * @param vectorDel
     * @param isManual,true:by action clicked; false: by message emited.
     */
    void sigRefreshEntryVector(QVector<Archive::Entry *> &vectorDel, bool isManual);
    void sigAutoCompress(const QString &, const QStringList &);
    void sigAutoCompressEntry(const QString &, const QStringList &, Archive::Entry *pWorkEntry); //added by hsw 20200525
    void sigUpdateUnCompreeTableView(const QFileInfo &);
    void sigSelectedFiles(QStringList &files);
    void subWindowTipsPopSig(int, const QStringList &);
    void subWindowTipsUpdateEntry(int, QVector<Archive::Entry *> &vectorDel);
    void sigDeleteArchiveFiles(const QStringList &files, const QString &);
    void sigAddArchiveFiles(const QStringList &files, const QString &);

    void sigDeleteJobFinished(Archive::Entry *pWorkEntry);

public slots:
    void oneCompressPress();
    void onPathButoonClicked();
    void onextractfilesSlot(QVector<Archive::Entry *> fileList, EXTRACT_TYPE type, QString path);
//    void onRefreshFilelist(const QStringList &filelist);
    /**
     * @brief onRefreshEntryList
     * @param vectorDel
     * @param isManual,true:by action clicked; false: by message emited.
     */
    void onRefreshEntryList(QVector<Archive::Entry *> &vectorDel, bool isManual);
    void onextractfilesOpenSlot(const QVector<Archive::Entry *> &fileList, const QString &programma);
    void onAutoCompress(const QStringList &path, Archive::Entry *pWorkEntry);
    void slotSubWindowTipsPopSig(int, const QStringList &);

    void slotDeleteJobFinished(Archive::Entry *pWorkEntry);

private:
    QString getAndDisplayPath(QString path);
private:

    fileViewer *m_fileviewer;
    DPushButton *m_nextbutton;
    QStringList m_filelist;
    QVector<Archive::Entry *> m_vectorDel;
    DCommandLinkButton *m_extractpath;
    DLabel *m_pixmaplabel;
    Lib_Edit_Button *m_pathbutton;
    QString m_pathstr;
    QFileInfo m_info;

    ArchiveSortFilterModel *m_model;
    EXTRACT_TYPE extractType;

    QStringList m_inputlist;
};
#endif
