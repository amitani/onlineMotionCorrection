#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <math.h>
#include <QElapsedTimer>

QImageUpdateWorker::QImageUpdateWorker(QObject* parent,MotionCorrectionWorker *mcw):QObject(parent),n_(1),mcw_(mcw){
    channel_parameters.resize(3);
    for(auto&& ch:channel_parameters){
        ch.push_back(0);
        ch.push_back(8192);
        ch.push_back(1);
    }
}

QImageUpdateWorker::~QImageUpdateWorker(){
}

void QImageUpdateWorker::setNumAverage(int n){
    if(n<=0) return;
    if(n>=200) return;
    n_=n;
    return;
}

void QImageUpdateWorker::setLimits(int ch, int type, int value){
    qDebug()<<"QIUW::setLimits::setting " << ch << ", " << type << ", " << value;
    if(ch<0) return;
    if(ch>2) return;
    if(type<0) return;
    if(type>2) return;

    channel_parameters[ch][type]=value;
    qDebug()<<"QIUW::setLimits::set " << ch << ", " << type << ", " << value;
    show();
    qDebug()<<"QIUW::setLimits::shown " << ch << ", " << type << ", " << value;
}

void QImageUpdateWorker::show(){
    if(!mcw_) return;
    QElapsedTimer et;
    et.start();
    qDebug()<<"QIUW::";
    std::deque<std::vector<cv::Mat>> deque_raw;
    std::deque<std::vector<cv::Mat>> deque_shifted;
    std::deque<int> deque_frame_tag;
    mcw_->getDeque(deque_raw,deque_shifted,deque_frame_tag);

    if(deque_raw.empty() || deque_shifted.empty()||deque_frame_tag.empty()) return;

    if(last_frame_tag == deque_frame_tag[0]) return;
    else last_frame_tag = deque_frame_tag[0];

    std::vector<cv::Mat> raw = deque_raw[0];
    std::vector<cv::Mat> shifted = deque_shifted[0];

    struct remap_to_uint8_c{
        unsigned int operator()(double x, std::vector<int> params){
            return params.size()>2&&params[2]?cv::saturate_cast<unsigned char>(256.0*(x-params[0])/(params[1]-params[0])):0;
        }
    } remap_to_uint8;

    if(deque_raw.empty()){
        qDebug()<<"QIUW::deque empty";
        return;
    }
    int n=std::min(n_,(int)deque_raw.size());
    qDebug()<<"QIUW::averaging " << n_ << ", " << deque_raw.size() << ", " << N_DEQUE;
    qDebug()<<"QIUW::averaging " << n << " frames";

    std::vector<cv::Mat>raw_average(deque_raw[0].size());
    for(int ch=0;ch<deque_raw[0].size();ch++){
        raw_average[ch]=deque_raw[0][ch];
        for(int i=1;i<n;i++) raw_average[ch]+=deque_raw[i][ch];
        raw_average[ch]/=(float)n;
    }
    QImage qimg_raw(raw_average[0].cols,raw_average[0].rows,QImage::Format_RGB32);
    for(int y=0;y<raw_average[0].rows;y++){
        for(int x=0;x<raw_average[0].cols;x++){
            if(raw_average.size()>1)
                qimg_raw.setPixel(y,x,
                   qRgb(remap_to_uint8(raw_average[1].at<float>(y,x),channel_parameters[1]),
                                  remap_to_uint8(raw_average[0].at<float>(y,x),channel_parameters[0]),0));
            else
                qimg_raw.setPixel(y,x,
                   qRgb(0,remap_to_uint8(raw_average[0].at<float>(y,x),channel_parameters[1]),0));
        }
    }
    qDebug()<<"QIUW::raw:"<<et.elapsed();

    std::vector<cv::Mat>shifted_average(deque_shifted[0].size());
    for(int ch=0;ch<deque_shifted[0].size();ch++){
        shifted_average[ch]=deque_shifted[0][ch];
        for(int i=1;i<n;i++)shifted_average[ch]+=deque_shifted[i][ch];
        shifted_average[ch]/=(float)n;
    }
    QImage qimg_shifted(shifted_average[0].cols,shifted_average[0].rows,QImage::Format_RGB32);
    for(int y=0;y<shifted_average[0].rows;y++){
        for(int x=0;x<shifted_average[0].cols;x++){
            if(shifted_average.size()>1)
                qimg_shifted.setPixel(y,x,
                   qRgb(remap_to_uint8(shifted_average[1].at<float>(y,x),channel_parameters[1]),
                                      remap_to_uint8(shifted_average[0].at<float>(y,x),channel_parameters[0]),0));
            else
                qimg_shifted.setPixel(y,x,
                   qRgb(0,remap_to_uint8(shifted_average[0].at<float>(y,x),channel_parameters[1]),0));
        }
    }
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

    checkBoxes.push_back(ui->checkBoxCh1);
    checkBoxes.push_back(ui->checkBoxCh2);
    checkBoxes.push_back(ui->checkBoxCh3);

    ui->graphicsViewRaw->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsViewRaw->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsViewCorrected->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsViewCorrected->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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
        }else{
            if(parser.isSet(chOpt)){
                QString chStr=parser.value(chOpt);
                qDebug()<<chStr;
                mcw->setCh(chStr.toInt());
            }
            if(args.size()==0){
                qDebug()<<"using moving average as template";
                ui->labelTiff->setText("Moving average");
            }else{
                qDebug()<<args[0];
                mcw->setTemplate(args[0]);
                ui->labelTiff->setText(args[0]);
            }
            qDebug()<<"Init";
            mcw->initImageRegistrator();
        }
    }
    qDebug()<<"Starting thread";
    motion_correction_thread=new QThread(this);
    mcw->moveToThread(motion_correction_thread);
    connect(motion_correction_thread, SIGNAL(started()), mcw, SLOT(start()));
    connect(motion_correction_thread, SIGNAL(finished()), mcw, SLOT(deleteLater()));
    connect(motion_correction_thread, SIGNAL(finished()), motion_correction_thread, SLOT(deleteLater()));

    qimage_update_thread=new QThread(this);
    qiuw=new QImageUpdateWorker(NULL, mcw);
    QImageUpdateWorker* qiuw_=qiuw;
    for(int ch=0;ch<3;ch++){
        connect(minSliders[ch],&QSlider::valueChanged,[qiuw_, ch](int value){qiuw_->setLimits(ch,0,value);});
        connect(maxSliders[ch],&QSlider::valueChanged,[qiuw_, ch](int value){qiuw_->setLimits(ch,1,value);});
        connect(checkBoxes[ch],&QCheckBox::stateChanged, [qiuw_, ch](int value){qiuw_->setLimits(ch,2,value);});
    }
    qiuw->moveToThread(qimage_update_thread);
    graphicsSceneRaw=new QGraphicsScene();
    graphicsSceneShifted=new QGraphicsScene();
    pixmapItemRaw=graphicsSceneRaw->addPixmap(QPixmap());
    pixmapItemShifted=graphicsSceneShifted->addPixmap(QPixmap());
    ui->graphicsViewRaw->setScene(graphicsSceneRaw);
    ui->graphicsViewCorrected->setScene(graphicsSceneShifted);

    connect(qimage_update_thread, SIGNAL(finished()), qiuw, SLOT(deleteLater()));
    connect(qimage_update_thread, SIGNAL(finished()), qimage_update_thread, SLOT(deleteLater()));
    connect(qimage_update_thread, SIGNAL(finished()), graphicsSceneRaw, SLOT(deleteLater()));
    connect(qimage_update_thread, SIGNAL(finished()), graphicsSceneShifted, SLOT(deleteLater()));
    //qRegisterMetaType<std::vector<cv::Mat>>();
    connect(mcw,SIGNAL(processed()),qiuw,SLOT(show()));
    connect(qiuw,SIGNAL(updated(QImage,QImage)),this,SLOT(updated(QImage,QImage)));
    connect(ui->comboBoxAverage,&QComboBox::currentTextChanged,[qiuw_](QString value){qiuw_->setNumAverage(value.toInt());});

    motion_correction_thread->start();
    qimage_update_thread->start();
    qDebug()<<"Started thread";
}
void MainWindow::updated(QImage qimg_raw, QImage qimg_shifted){
    qDebug()<<"MW::updating images";

    pixmapItemRaw->setPixmap(QPixmap::fromImage(qimg_raw));
    graphicsSceneRaw->setSceneRect(pixmapItemRaw->pixmap().rect());

    pixmapItemShifted->setPixmap(QPixmap::fromImage(qimg_shifted));
    graphicsSceneShifted->setSceneRect(pixmapItemShifted->pixmap().rect());

    qDebug()<<"MW::updated";
//    delete scene;
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

