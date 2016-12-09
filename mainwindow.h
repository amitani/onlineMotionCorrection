#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QCommandLineParser>
#include <QThread>
#include <vector>
#include <deque>
#include <motioncorrectionthread.h>

#include <QImage>
class QImageUpdateWorker:public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QImageUpdateWorker)

public:
    explicit QImageUpdateWorker(QObject* parent = 0, MotionCorrectionWorker* mcw=NULL);
    ~QImageUpdateWorker();

public slots:
    void setLimits(int ch, int type, int value);
    void setNumAverage(int n);
    void show();

signals:
    void updated(QImage raw, QImage corrected);

private:
    //std::deque<std::vector<cv::Mat>> deque_raw, deque_shifted;
    MotionCorrectionWorker* mcw_;
    std::vector<std::vector<int>> channel_parameters; //channel_parameters[ch][type] type:0:min, 1:max, 2:enabled
    int n_;

    static const unsigned int N_DEQUE = 256;
};


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void putRawImage(QImage* im);
    void putCorrectedImage(QImage* im);

public slots:
    void updated(QImage raw, QImage shifted);

private:
    Ui::MainWindow *ui;
    std::vector<QSlider*> minSliders;
    std::vector<QSlider*> maxSliders;
    std::vector<QSpinBox*> minSpinBoxes;
    std::vector<QSpinBox*> maxSpinBoxes;
    std::vector<QCheckBox*> checkBoxes;

    QThread* motion_correction_thread;
    QThread* qimage_update_thread;
    MotionCorrectionWorker* mcw;
    QImageUpdateWorker* qiuw;

};

#endif // MAINWINDOW_H
