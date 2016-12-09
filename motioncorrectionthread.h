#ifndef MOTIONCORRECTIONTHREAD_H
#define MOTIONCORRECTIONTHREAD_H

#include <QObject>
#include <QTimer>
#include <QMetaObject>
#include <QMutex>
#include <vector>
#include <memory>
#include "mmap.h"
#include "image_registrator.h"

Q_DECLARE_METATYPE(cv::Mat)

class MotionCorrectionWorker:public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(MotionCorrectionWorker)

public:
    explicit MotionCorrectionWorker(QObject* parent = 0);
    ~MotionCorrectionWorker();

public slots:
    void start();
    void stop();
    void check();

public:
    void setTemplate(QString tifffilename);
    void setCh(int ch);
    void initImageRegistrator();
    void loadParameters(QString xmlfilename);
    void getDeque(std::deque<std::vector<cv::Mat>>& deque_raw, std::deque<std::vector<cv::Mat>>& deque_shifted);

signals:
    void processed();

private:
    QTimer* timer;
    std::shared_ptr<MMap<SI4Image>> mmap_raw;
    std::shared_ptr<MMap<SI4Image>> mmap_shifted;
    std::vector<cv::Mat> template_image;
    int ch;
    std::shared_ptr<ImageRegistrator> ir;
    std::shared_ptr<SI4Image> temporary_data;

    static const int max_deque_size = 200;

    std::deque<std::vector<cv::Mat>> deque_raw_;
    std::deque<std::vector<cv::Mat>> deque_shifted_;
    QMutex mutex_;

    //cv::Mat shift(SI4Image* source, double i0, double j0);
};


/*
class MotionCorrectionThread:public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(MotionCorrectionThread)

public:
    explicit MotionCorrectionThread(QObject* parent = 0);
    ~MotionCorrectionThread();

public slots:
    void start();
    void stop();

signals:
   // void processed(SI4Image raw, SI4Image corrected);

private:
    QThread* thread;
    MotionCorrectionLoop* mcl;
};
*/
#endif // MOTIONCORRECTIONTHREAD_H
