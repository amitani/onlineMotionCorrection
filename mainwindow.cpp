#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

#include <QElapsedTimer>

QImageUpdateWorker::QImageUpdateWorker(QObject* parent):QObject(parent), ch1min_(0),ch1max_(8192),ch2min_(0),ch2max_(8192){
}

QImageUpdateWorker::~QImageUpdateWorker(){
}


void QImageUpdateWorker::setLimits(int ch, int type, int value){
    qDebug()<<"QIUW::setLimits::" << ch << ", " << type << ", " << value;
    switch(ch){
    case 0:
        if(type) ch1max_=value;
        else  ch1min_=value;
        break;
    case 1:
        if(type) ch2max_=value;
        else  ch2min_=value;
        break;
    case 2:
        if(type) ch3max_=value;
        else  ch3min_=value;
        break;
    }
}

void QImageUpdateWorker::processed(std::vector<cv::Mat> raw, std::vector<cv::Mat> shifted){
    struct remap_to_uint8_c{
        unsigned int operator()(double x, double min, double max){
            return cv::saturate_cast<unsigned char>(256.0*(x-min)/(max-min));
        }
    } remap_to_uint8;

    qDebug()<<"QIUW::";
    if(raw.empty()) return;
/*    qDebug() << raw.size();
    qDebug() << raw[0].cols;
    qDebug() << raw[0].rows;
    qDebug() << raw[0].at<int16_t>(5,5);
    return;*/
    if(raw.size()!=shifted.size()){
        qDebug()<<"QIUW::Ch discrepancy between raw and shifted";
        return;
    }
    if(raw[0].cols!=shifted[0].cols||raw[0].rows!=shifted[0].rows){
        qDebug()<<"QIUW::Ch discrepancy between raw and shifted";
        return;
    }
    for(int i=0;i<raw.size();i++){
        if(raw[0].cols!=raw[i].cols||raw[0].cols!=shifted[i].cols){
            qDebug()<<"QIUW::size discrepancy";
            return;
        }
        if(raw[0].rows!=raw[i].rows||raw[0].rows!=shifted[i].rows){
            qDebug()<<"QIUW::size discrepancy";
            return;
        }
    }

    QElapsedTimer et;
    et.start();


    QImage qimg_raw(raw[0].cols,raw[0].rows,QImage::Format_RGB32);
    for(int y=0;y<raw[0].rows;y++){
        for(int x=0;x<raw[0].cols;x++){
            if(raw.size()>1)
                qimg_raw.setPixel(y,x,
                   qRgb(remap_to_uint8(raw[1].at<int16_t>(y,x),ch2min_,ch2max_),
                                  remap_to_uint8(raw[0].at<int16_t>(y,x),ch1min_,ch1max_),0));
            else
                qimg_raw.setPixel(y,x,
                   qRgb(0,remap_to_uint8(raw[0].at<int16_t>(y,x),ch2min_,ch2max_),0));
        }
    }
    qDebug()<<"QIUW::raw:"<<et.elapsed();
    //qimg_raw=qimg_raw.mirrored(true,true);
    //qDebug()<<et.elapsed();

    QImage qimg_shifted(shifted[0].cols,shifted[0].rows,QImage::Format_RGB32);
    for(int y=0;y<raw[0].rows;y++){
        for(int x=0;x<raw[0].cols;x++){
            if(raw.size()>1)
                qimg_shifted.setPixel(y,x,
                   qRgb(remap_to_uint8(shifted[1].at<float>(y,x),ch2min_,ch2max_),
                                      remap_to_uint8(shifted[0].at<float>(y,x),ch1min_,ch1max_),0));
            else
                qimg_shifted.setPixel(y,x,
                   qRgb(0,remap_to_uint8(shifted[0].at<float>(y,x),ch2min_,ch2max_),0));
        }
    }
    //qDebug()<<et.elapsed();
    //qimg_shifted=qimg_shifted.mirrored(true,true);
    qDebug()<<"QIUW::shifted:"<<et.elapsed();

    emit updated(qimg_raw,qimg_shifted);
    qDebug()<<"QIUW::done";
}



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    minSliders.push_back(ui->sliderCh1Min);
    minSliders.push_back(ui->sliderCh2Min);
    minSliders.push_back(ui->sliderCh3Min);
    maxSliders.push_back(ui->sliderCh1Max);
    maxSliders.push_back(ui->sliderCh2Max);
    maxSliders.push_back(ui->sliderCh3Max);
    minSpinBoxes.push_back(ui->spinBoxCh1Min);
    minSpinBoxes.push_back(ui->spinBoxCh2Min);
    minSpinBoxes.push_back(ui->spinBoxCh3Min);
    maxSpinBoxes.push_back(ui->spinBoxCh1Max);
    maxSpinBoxes.push_back(ui->spinBoxCh2Max);
    maxSpinBoxes.push_back(ui->spinBoxCh3Max);
    for(int i=0;i<3;i++){
        QSlider* slider;
        connect(minSliders[i],SIGNAL(valueChanged(int)),minSpinBoxes[i],SLOT(setValue(int)));
        connect(minSpinBoxes[i],SIGNAL(valueChanged(int)),minSliders[i],SLOT(setValue(int)));
        slider=maxSliders[i];
        connect(minSliders[i],&QSlider::valueChanged,[slider](int value){slider->setMinimum(value);});
        connect(maxSliders[i],SIGNAL(valueChanged(int)),maxSpinBoxes[i],SLOT(setValue(int)));
        connect(maxSpinBoxes[i],SIGNAL(valueChanged(int)),maxSliders[i],SLOT(setValue(int)));
        slider=minSliders[i];
        connect(maxSliders[i],&QSlider::valueChanged,[slider](int value){slider->setMaximum(value);});
    }

    mcw=new MotionCorrectionWorker();
    QStringList argv=QCoreApplication::arguments();
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument("template", "Template image file for motion correction.");
    QCommandLineOption chOpt((QStringList(QString("ch")),"Specify channel for correction, 0 for auto(last)","0|1-9", "0"));
    parser.addOption(chOpt);
    if(!parser.parse(argv)){
        for(int i=0;i<parser.unknownOptionNames().count();i++){
            qDebug()<<"illegal option --\"" + parser.unknownOptionNames().at(i) + "\"";
        }
        exit(1);
    }else{
        parser.process(argv);
        const QStringList args = parser.positionalArguments();
        if(args.size()>1){
            qDebug()<<"too many arguments";
            exit(1);
        }else if(args.size()==0){
            qDebug()<<"too few arguments";
            exit(1);
        }else{
            qDebug()<<args[0];
            mcw->setTemplate(args[0]);
            if(parser.isSet(chOpt)){
                QString chStr=parser.value(chOpt);
                qDebug()<<chStr;
                mcw->setCh(chStr.toInt());
            }
            qDebug()<<"Init";
            mcw->initImageRegistrator();
            //mcw->loadParameters();
        }
    }
    qDebug()<<"Starting thread";
    motion_correction_thread=new QThread(this);
    mcw->moveToThread(motion_correction_thread);
    connect(motion_correction_thread, SIGNAL(started()), mcw, SLOT(start()));
    connect(motion_correction_thread, SIGNAL(finished()), mcw, SLOT(deleteLater()));
    connect(motion_correction_thread, SIGNAL(finished()), motion_correction_thread, SLOT(deleteLater()));

    qimage_update_thread=new QThread(this);
    qiuw=new QImageUpdateWorker();
    QImageUpdateWorker* qiuw_=qiuw;
    for(int ch=0;ch<3;ch++){
        connect(minSliders[ch],&QSlider::valueChanged,[qiuw_, ch](int value){qiuw_->setLimits(ch,0,value);});
        connect(maxSliders[ch],&QSlider::valueChanged,[qiuw_, ch](int value){qiuw_->setLimits(ch,1,value);});
    }
    qiuw->moveToThread(qimage_update_thread);
    connect(qimage_update_thread, SIGNAL(started()), qiuw, SLOT(start()));
    connect(qimage_update_thread, SIGNAL(finished()), qiuw, SLOT(deleteLater()));
    connect(qimage_update_thread, SIGNAL(finished()), qimage_update_thread, SLOT(deleteLater()));

    qRegisterMetaType<std::vector<cv::Mat>>();
    connect(mcw,SIGNAL(processed(std::vector<cv::Mat>,std::vector<cv::Mat>)),qiuw,SLOT(processed(std::vector<cv::Mat>,std::vector<cv::Mat>)));
    connect(qiuw,SIGNAL(updated(QImage,QImage)),this,SLOT(updated(QImage,QImage)));


    motion_correction_thread->start();
    qimage_update_thread->start();
    qDebug()<<"Started thread";
}
void MainWindow::updated(QImage qimg_raw, QImage qimg_shifted){
    qDebug()<<"MW::updating images";
    QPixmap pixmap;
    QGraphicsScene* scene;

    scene = new QGraphicsScene();
    pixmap.convertFromImage(qimg_raw);
    scene->addPixmap(pixmap);
    scene->setSceneRect(qimg_raw.rect());
    ui->graphicsViewRaw->setScene(scene);

    scene = new QGraphicsScene();
    pixmap.convertFromImage(qimg_shifted);
    scene->addPixmap(pixmap);
    scene->setSceneRect(qimg_shifted.rect());
    ui->graphicsViewCorrected->setScene(scene);
    qDebug()<<"MW::updated";
    return;
}



MainWindow::~MainWindow()
{
    motion_correction_thread->exit();
    qimage_update_thread->exit();
    motion_correction_thread->wait();
    qimage_update_thread->wait();
    delete ui;
}

