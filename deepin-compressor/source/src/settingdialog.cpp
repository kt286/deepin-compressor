#include "settingdialog.h"
#include <QStandardItemModel>
#include <qsettingbackend.h>
#include <QDebug>
#include <DFileDialog>
#include <QBoxLayout>
#include <DComboBox>
#include <DLabel>

SettingDialog::SettingDialog(QWidget *parent):
    DSettingsDialog(parent)
{
    m_associtionlist << "file_association.file_association_type.x-7z-compressed"
                   << "file_association.file_association_type.x-archive"
                   << "file_association.file_association_type.x-bcpio"
                   << "file_association.file_association_type.x-bzip"
                   << "file_association.file_association_type.x-cpio"
                   << "file_association.file_association_type.x-cpio-compressed"
                   << "file_association.file_association_type.vnd.debian.binary-package"
                   << "file_association.file_association_type.gzip"
                   << "file_association.file_association_type.x-java-archive"
                   << "file_association.file_association_type.x-lzma"
                   << "file_association.file_association_type.vnd.ms-cab-compressed"
                   << "file_association.file_association_type.vnd.rar"
                   << "file_association.file_association_type.x-rpm"
                   << "file_association.file_association_type.x-sv4cpio"
                   << "file_association.file_association_type.x-sv4crc"
                   << "file_association.file_association_type.x-tar"
                   << "file_association.file_association_type.x-bzip-compressed-tar"
                   << "file_association.file_association_type.x-compressed-tar"
                   << "file_association.file_association_type.x-lzip-compressed-tar"
                   << "file_association.file_association_type.x-lzma-compressed-tar"
                   << "file_association.file_association_type.x-tzo"
                   << "file_association.file_association_type.x-xz-compressed-tar"
                   << "file_association.file_association_type.x-tarz"
                   << "file_association.file_association_type.x-xar"
                   << "file_association.file_association_type.x-xz"
                   << "file_association.file_association_type.zip"
                   << "file_association.file_association_type.x-cd-image"
                   << "file_association.file_association_type.x-iso9660-appimage"
                   << "file_association.file_association_type.x-source-rpm";

    m_valuelist.clear();

    initUI();
    initConnect();

    foreach(QString key, m_associtionlist)
    {
        m_valuelist.append(m_settings->value(key).toBool());

    }
    m_valuelisttemp = m_valuelist;
}

void SettingDialog::initUI()
{


    this->widgetFactory()->registerWidget("custom-button", [this] (QObject *obj) -> QWidget* {
        if (DSettingsOption *option = qobject_cast<DSettingsOption*>(obj)) {
            QWidget* buttonwidget = new QWidget();
            QHBoxLayout* layout = new QHBoxLayout();
            QPushButton *button1 = new DPushButton(tr("全选"));
            QPushButton *button2 = new DPushButton(tr("取消全选"));
            button1->setFixedSize(100, 36);
            button2->setFixedSize(100, 36);
            layout->addStretch();
            layout->addWidget(button1);
            layout->addStretch();
            layout->addWidget(button2);
            layout->addStretch();
            buttonwidget->setLayout(layout);

            connect(button1, &QPushButton::clicked, this, &SettingDialog::selectpressed);
            connect(button2, &QPushButton::clicked, this, &SettingDialog::cancelpressed);
            return buttonwidget;
        }

        return nullptr;
    });

    this->widgetFactory()->registerWidget("pathbox", [this] (QObject *obj) -> QWidget* {
        if (m_comboboxoption = qobject_cast<DSettingsOption*>(obj)) {
            QWidget* widget = new QWidget();
            QHBoxLayout* layout = new QHBoxLayout();

            DLabel* label = new DLabel;
            label->setText(tr("默认解压位置") + ":");

            DComboBox* combobox = new DComboBox;
            combobox->setFixedWidth(300);
            combobox->setEditable(true);
            QStringList list;
            list << tr("当前目录")<<tr("桌面")<<tr("其他目录");
            combobox->addItems(list);
            qDebug()<<m_comboboxoption->value();
            if(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) == m_comboboxoption->value())
            {
                combobox->setCurrentIndex(1);
                m_curpath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            }
            else if("" == m_comboboxoption->value())
            {
                combobox->setCurrentIndex(0);
                m_curpath = "";
            }
            else {
                combobox->setCurrentIndex(2);
                m_curpath = m_comboboxoption->value().toString();
                combobox->setEditText(m_curpath);
            }

            layout->addWidget(label,0 , Qt::AlignLeft);
            layout->addWidget(combobox,0 , Qt::AlignLeft);

            widget->setLayout(layout);

            connect(combobox, &DComboBox::currentTextChanged, this, &SettingDialog::comboxindexchanged);
            connect(this, &SettingDialog::sigeditText ,combobox, &DComboBox::setEditText);


            qDebug()<<m_curpath;
            return widget;
        }

        return nullptr;
    });

    const QString confDir = DStandardPaths::writableLocation(
                    QStandardPaths::AppConfigLocation);
    const QString confPath = confDir + QDir::separator() + "deepin-compressor.conf";

    // 创建设置项存储后端
    auto backend = new QSettingBackend(confPath);

    // 通过json文件创建DSettings对象
    m_settings = DSettings::fromJsonFile(":/data/deepin-compressor.json");
    // 设置DSettings存储后端
    m_settings->setBackend(backend);

    // 通过DSettings对象构建设置界面
    updateSettings(m_settings);

    moveToCenter();
}

void SettingDialog::initConnect()
{
    connect(m_settings, &DSettings::valueChanged,
            this, &SettingDialog::settingsChanged);
}

void SettingDialog::done(int status)
{
    Q_UNUSED(status);
    QDialog::done(status);

    int loop = 0;
    foreach(bool value, m_valuelisttemp)
    {
        if(m_valuelist.at(loop) != value)
        {
            QString key = m_associtionlist.at(loop);
            QString mimetype = "application/" + key.remove("file_association.file_association_type.");
            startcmd(mimetype, m_valuelisttemp.at(loop));
        }

        loop++;
    }
    m_valuelist = m_valuelisttemp;
}


QString SettingDialog::getCurExtractPath()
{
    qDebug()<<m_curpath;
    return m_curpath;
}

bool SettingDialog::isAutoCreatDir()
{
    return m_settings->value("base.decompress.create_folder").toBool();
}

bool SettingDialog::isAutoOpen()
{
    return m_settings->value("base.decompress.open_folder").toBool();
}


void SettingDialog::settingsChanged(const QString &key, const QVariant &value)
{
    qDebug()<<key<<value;

    if(key.contains("file_association_type"))
    {
        int index = m_associtionlist.indexOf(key);
        if(index > -1)
        {
            m_valuelisttemp.replace(index, value.toBool());
        }
    }
}


void SettingDialog::selectpressed()
{
    foreach(QString key, m_associtionlist)
    {
        m_settings->setOption(key, true);
    }
}
void SettingDialog::cancelpressed()
{

    foreach(QString key, m_associtionlist)
    {
        m_settings->setOption(key, false);
    }
}

void SettingDialog::comboxindexchanged(const QString & index)
{
    if(tr("其他目录") == index)
    {
        DFileDialog dialog;
        dialog.setAcceptMode(DFileDialog::AcceptOpen);
        dialog.setFileMode(DFileDialog::Directory);
        dialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));

        const int mode = dialog.exec();

        if (mode != QDialog::Accepted) {
            return;
        }

        QList<QUrl> pathlist = dialog.selectedUrls();
        QString curpath = pathlist.at(0).toLocalFile();

        emit sigeditText(curpath);
        m_curpath = curpath;
    }
    else if(tr("桌面") == index)
    {
        m_curpath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }
    else {
        m_curpath = "";
    }

    m_comboboxoption->setValue(m_curpath);


}

void SettingDialog::startcmd(QString &mimetype, bool state)
{
    if(!m_process)
    {
        m_process = new KProcess;
    }

    QString programPath = QStandardPaths::findExecutable("xdg-mime");
    if (programPath.isEmpty()) {
        qDebug()<<"error can't find xdg-mime";
        return;
    }

    QStringList arguments;

    if(state)
    {
        arguments<<"default"<<"deepin-compressor.desktop"<<mimetype;
    }
    else {
        arguments<<"default"<<".desktop"<<mimetype;
    }

    qDebug()<<mimetype<<arguments;

    m_process->setOutputChannelMode(KProcess::MergedChannels);
    m_process->setNextOpenMode(QIODevice::ReadWrite | QIODevice::Unbuffered | QIODevice::Text);
    m_process->setProgram(programPath, arguments);
    m_process->start();
    m_process->waitForStarted();
    m_process->waitForFinished();
}