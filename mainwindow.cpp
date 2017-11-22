#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <math.h>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

#include <chrono>
#include <thread>

QImageUpdateWorker::QImageUpdateWorker(QObject* parent,MotionCorrectionWorker *mcw):QObject(parent),mcw_(mcw){
    channel_parameters.resize(3);
    for(auto&& ch:channel_parameters){
        ch.push_back(0);
        ch.push_back(8192);
        ch.push_back(1);
    }
    timer = new QTimer(this);
    timer->setInterval(50);
    connect(timer, SIGNAL(timeout()),this,SLOT(show()));
}

QImageUpdateWorker::~QImageUpdateWorker(){
    delete timer;
}

void QImageUpdateWorker::setLimits(int ch, int type, int value){
    qDebug()<<"QIUW::setLimits::setting " << ch << ", " << type << ", " << value;
    if(ch<0) return;
    if(ch>2) return;
    if(type<0) return;
    if(type>2) return;

    if(channel_parameters[ch][type]== value) return;
    channel_parameters[ch][type]=value;
    qDebug()<<"QIUW::setLimits::set " << ch << ", " << type << ", " << value;
}

void QImageUpdateWorker::show(){
    if(!mcw_) return;

    static QElapsedTimer staticET;
    if(staticET.isValid() && staticET.elapsed() < 10)
        return;
    staticET.start();

//    QElapsedTimer et;
//    et.start();
//    qDebug()<<"QIUW::";

    /*std::deque<std::vector<cv::Mat>> deque_raw;
    std::deque<std::vector<cv::Mat>> deque_shifted;
    std::deque<int> deque_frame_tag;
    mcw_->getDeque(deque_raw,deque_shifted,deque_frame_tag);
    if(last_frame_tag == deque_frame_tag[0]) return;
    else last_frame_tag = deque_frame_tag[0];
    if(deque_raw.empty() || deque_shifted.empty()||deque_frame_tag.empty()) return;*/


    std::vector<cv::Mat> raw_frame;
    std::vector<cv::Mat> shifted_frame;
    int processed_frame_tag;
    mcw_->getMeans(raw_frame,shifted_frame,processed_frame_tag);

    if(raw_frame.empty()||shifted_frame.empty()){
        qDebug()<<"QIUW::average empty";
        return;
    }
    if(processed_frame_tag==last_frame_tag) return;
    last_frame_tag = processed_frame_tag;

    struct remap_c{
        inline unsigned int remap_to_uint8(double x, std::vector<int>& params){
            return params.size()>2&&params[2]?cv::saturate_cast<unsigned char>(256.0*(x-params[0])/(params[1]-params[0])):0;
        }
        QImage operator ()(std::vector<cv::Mat> &frame, std::vector<std::vector<int>>& channel_parameters){
            QImage qimg(frame[0].cols,frame[0].rows,QImage::Format_RGB32);
            qDebug()<<frame[0].depth();
            if(frame[0].depth()==5){
                for(int y=0;y<frame[0].rows;y++){
                    for(int x=0;x<frame[0].cols;x++){
                        qimg.setPixel(y,x,
                           qRgb(frame.size()==1||channel_parameters[1][2]==0?0:remap_to_uint8(frame[1].at<float>(y,x),channel_parameters[1]),
                                remap_to_uint8(frame[0].at<float>(y,x),channel_parameters[0]),
                                0));
                    }
                }
            }else{
                for(int y=0;y<frame[0].rows;y++){
                    for(int x=0;x<frame[0].cols;x++){
                        qimg.setPixel(y,x,
                           qRgb(frame.size()==1||channel_parameters[1][2]==0?0:remap_to_uint8(frame[1].at<int16_t>(y,x),channel_parameters[1]),
                                remap_to_uint8(frame[0].at<int16_t>(y,x),channel_parameters[0]),
                                0));
                    }
                }
            }

            return qimg;
        }
    } remap;

    QImage qimg_raw = remap(raw_frame, channel_parameters);
    QImage qimg_shifted = remap(shifted_frame, channel_parameters);
//    qDebug()<<"QIUW::shifted:"<<et.elapsed();
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
    for(int i=0;i<2;i++){
        QWidget* w= i==0?ui->widgetNew:ui->widgetCurrent;
        intManager[i] = new QtIntPropertyManager(w);
        QtSpinBoxFactory *spinBoxFactory = new QtSpinBoxFactory(w);
        propertyBrowser[i] = new QtTreePropertyBrowser(w);
        if(i==0) propertyBrowser[i]->setFactoryForManager(intManager[i], spinBoxFactory);
        auto property=intManager[i]->addProperty("Factor");
        intProperties[i].push_back(property);
        intManager[i]->setRange(property,0,64);
        intManager[i]->setValue(property,2);
        propertyBrowser[i]->addProperty(property);
        property=intManager[i]->addProperty("Margin");
        intProperties[i].push_back(property);
        intManager[i]->setRange(property,0,128);
        intManager[i]->setValue(property,32);
        propertyBrowser[i]->addProperty(property);
        property=intManager[i]->addProperty("S_smooth");
        intProperties[i].push_back(property);
        intManager[i]->setRange(property,0,128);
        intManager[i]->setValue(property,0);
        propertyBrowser[i]->addProperty(property);
        property=intManager[i]->addProperty("S_norm");
        intProperties[i].push_back(property);
        intManager[i]->setRange(property,0,128);
        intManager[i]->setValue(property,0);
        propertyBrowser[i]->addProperty(property);
        property=intManager[i]->addProperty("Norm_offset");
        intProperties[i].push_back(property);
        intManager[i]->setRange(property,-8192,8192);
        intManager[i]->setValue(property,0);
        propertyBrowser[i]->addProperty(property);
        property=intManager[i]->addProperty("Histgram equal");
        intProperties[i].push_back(property);
        intManager[i]->setRange(property,0,1);
        intManager[i]->setValue(property,0);
        propertyBrowser[i]->addProperty(property);
        propertyBrowser[i]->setResizeMode(propertyBrowser[i]->Stretch);
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
        }else{
            if(parser.isSet(chOpt)){
                QString chStr=parser.value(chOpt);
                qDebug()<<chStr;
                mcw->setCh(chStr.toInt());
            }
            QString baseFileName;
            if(args.size()==0){
                qDebug()<<"using moving average as template";
                ui->labelTiff->setText("Moving average");
                logFileName = "";
            }else{
                qDebug()<<args[0];
                mcw->setTemplate(args[0]);
                ui->labelTiff->setText(args[0]);
                baseFileName = args[0];
                auto q = QFileInfo(baseFileName);
                std::ostringstream ss;
                auto t = std::time(nullptr);
                auto tm = *std::localtime(&t);
                ss << std::put_time(&tm, "_%y%m%d_%H%M%S") << ".log";
                logFileName = (q.canonicalPath() + QDir::separator()+q.baseName()).toStdString()+ss.str();
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
    MotionCorrectionWorker* mcw_ = mcw;  // necessary for closure
    connect(ui->comboBoxAverage,&QComboBox::currentTextChanged,[mcw_](QString value){mcw_->setNumAverage(value.toInt());});
    mcw_->setNumAverage(ui->comboBoxAverage->currentText().toInt());

    qimage_update_thread=new QThread(this);
    qiuw=new QImageUpdateWorker(NULL, mcw);
    QImageUpdateWorker* qiuw_=qiuw; // necessary for closure
    for(int ch=0;ch<3;ch++){
        connect(minSliders[ch],&QSlider::valueChanged,[qiuw_, ch](int value){qiuw_->setLimits(ch,0,value);});
        connect(maxSliders[ch],&QSlider::valueChanged,[qiuw_, ch](int value){qiuw_->setLimits(ch,1,value);});
        connect(checkBoxes[ch],&QCheckBox::stateChanged, [qiuw_, ch](int value){qiuw_->setLimits(ch,2,value);});
        qiuw->setLimits(ch,0,minSliders[ch]->value());
        qiuw->setLimits(ch,1,maxSliders[ch]->value());
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

    motion_correction_thread->start();
    qimage_update_thread->start();
    qDebug()<<"Started thread";

    connect(ui->pushButton,SIGNAL(clicked()),this, SLOT(updateParameters()));
    connect(this,SIGNAL(parametersUpdated(double,int,double,double,double,int)),
            mcw,SLOT(setParameters(double,int,double,double,double,int)));
    updateParameters();
}
void MainWindow::updated(QImage qimg_raw, QImage qimg_shifted){
    qDebug()<<"MW::updating images";

    pixmapItemRaw->setPixmap(QPixmap::fromImage(qimg_raw));
    //graphicsSceneRaw->setSceneRect(pixmapItemRaw->pixmap().rect());

    pixmapItemShifted->setPixmap(QPixmap::fromImage(qimg_shifted));
    //graphicsSceneShifted->setSceneRect(pixmapItemShifted->pixmap().rect());

    qDebug()<<"MW::updated";
//    delete scene;
    return;
}

void MainWindow::updateParameters(){
    int val[6];
    for(int j=0;j<intManager[0]->properties().size();j++){
        val[j]=intManager[0]->value(intProperties[0][j]);
        intManager[1]->setValue(intProperties[1][j],val[j]);
    }
    emit parametersUpdated(val[0],val[1],val[2],val[3],val[4],val[5]);
    if(!logFileName.empty()){
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        bool done=false;
        for(int j=0;j<5 && !done;j++){
            try{
                std::ofstream outfile(logFileName,std::ios_base::app);
                outfile<<std::put_time(&tm, "\"%Y/%m/%d_%H:%M:%S\"");
                for(int i=0;i<6;i++) outfile << ", " << val[i];
                outfile << std::endl;
                done=true;
            }catch(std::exception &e){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
}


MainWindow::~MainWindow()
{
    qimage_update_thread->exit();
    qimage_update_thread->wait();
    motion_correction_thread->exit();
    motion_correction_thread->wait();
    delete ui;
}

