#ifndef STRUCTS_H
#define STRUCTS_H
#include <QDebug>
#include <QElapsedTimer>

/**
 * @brief The ProgressAssistant struct
 * @see 进度条助手
 */
class ProgressAssistant: public QObject
{
public:
    explicit ProgressAssistant(QObject *parent = nullptr);
    void startTimer();

    void restartTimer();
    /**
     * @brief resetProgress
     * @see 重置进度条各项数据（总大小，耗时，百分值）
     */
    void resetProgress();

    void setTotalSize(qint64 size);

    qint64 &getTotalSize();

    double getSpeed(unsigned long percent);

    qint64 getLeftTime(unsigned long percent);

private:
    qint64 consumeTime;                 //消耗时间
    QElapsedTimer m_timer;
    unsigned long m_lastPercent = 0;
    qint64 m_totalFileSize = 0;         //处理的文件总大小
};

#endif // STRUCTS_H
