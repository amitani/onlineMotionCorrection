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
#include "movingaverage.h"

Q_DECLARE_METATYPE(cv::Mat)

void SetMatInMMap(std::shared_ptr<MMap<SI4Image>> mmap, std::vector<cv::Mat> mat, int frame_tag);

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
    void setParameters(double factor, int margin_h, int margin_w, double sigma_smoothing,
                       double sigma_normalization, double normalization_offset, int threshold, int replacement);
    void setNumAverage(int n);

public:
    void setTemplate(QString tifffilename);
    void setCh(int ch);
    void initImageRegistrator();
    void getMeans(std::vector<cv::Mat> &raw_frame, std::vector<cv::Mat> &shifted_frame, int &last_frame_tag);

signals:
    void processed();

private:
    QTimer* timer;
    std::shared_ptr<MMap<SI4Image>> mmap_raw;
    std::shared_ptr<MMap<SI4Image>> mmap_shifted;
    std::shared_ptr<MMap<SI4Image>> mmap_average;
    std::shared_ptr<MMap<SI4Image>> mmap_summed;
    std::shared_ptr<MMap<SmallDoubleMatrix>> mmap_dislocation;
    cv::Mat template_image;
    int ch;
    std::shared_ptr<ImageRegistrator> ir;
    std::shared_ptr<SI4Image> temporary_data;


/*
    static const int max_deque_size = 200;
    static const int default_n_template = 30;
    int n_template;
    std::deque<std::vector<cv::Mat>> deque_raw_;
    std::deque<std::vector<cv::Mat>> deque_shifted_;
    std::deque<int> deque_frame_tag_;
*/
    static const int n_for_xyz_correction = 200;
    static const int n_for_template = 50;
    static const int n_for_summed = 50;

    MovingAverage raw;
    MovingAverage shifted;
    MovingAverage tmpl; //template
    MovingAverage xyz; // for xyz correction
    MovingAverage summed; // for xyz correction

    QMutex mutex_;

    int threshold_ = 80;
    int replacement_ = 35;
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
