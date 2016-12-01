#include "motioncorrectionthread.h"
#include <QtDebug>
#include <QElapsedTimer>

#define cimg_use_tiff
#include "CImg.h"
using namespace cimg_library;

MotionCorrectionWorker::MotionCorrectionWorker(QObject* parent):QObject(parent)
{
    timer = new QTimer(this);
    timer->setInterval(5);
    //connect(timer, &QTimer::timeout,this,&MotionCorrectionWorker::check);
    connect(timer, SIGNAL(timeout()),this,SLOT(check()));
    ch=0;
    mmap_raw=std::make_shared<MMap<SI4Image>>("SI4_RAW");
    mmap_shifted=std::make_shared<MMap<SI4Image>>("SI4_SHIFTED");
    qDebug()<<"MCW::Constructed";
    temporary_data=std::make_shared<SI4Image>();
}

void MotionCorrectionWorker::start(){
    timer->start();
    qDebug()<<"MCW::Started";
}

void MotionCorrectionWorker::stop(){
    timer->stop();
}

void MotionCorrectionWorker::check(){
    qDebug()<<"MCW::check";
    static int last_frame_tag;
    if(ir){
        if(last_frame_tag!=mmap_raw->get_pointer()->frame_tag){
            QElapsedTimer et;
            et.start();
            mmap_raw->get(*temporary_data);
            qDebug()<<"MCW::n_ch"<<temporary_data->n_ch;
            qDebug()<<"MCW::"<<et.elapsed()<<":get";
            last_frame_tag=temporary_data->frame_tag;
            if(temporary_data->n_ch<1) return;
            qDebug()<<"MCW::"<<temporary_data->frame_tag;
            std::vector<cv::Mat> raw_frame;
            for(int i=0;i<temporary_data->height*temporary_data->width*temporary_data->n_ch;i++){
                if(temporary_data->data[i]<80)temporary_data->data[i]=35;
            }
            for(int i=0;i<temporary_data->n_ch;i++){
                raw_frame.push_back(cv::Mat(temporary_data->height,temporary_data->width,CV_16SC1,
                                            temporary_data->data+temporary_data->height*temporary_data->width*i).clone());
            }

            qDebug()<<"MCW::"<<et.elapsed()<<":copied";
            int ch_to_align= ch && ch<temporary_data->n_ch?ch:temporary_data->n_ch-1;
            cv::Point2d d;
            qDebug()<<"MCW::ch "<<ch_to_align;

            //ir->Align(raw_frame[ch_to_align],NULL,&d);

            cv::Mat heatmap;
            ir->Align(raw_frame[ch_to_align],NULL,&d,&heatmap);

            /*cv::Mat tmp=raw_frame[ch_to_align];
            for(int y=0;y<tmp.rows;y++){
                for(int x=0;x<tmp.cols;x++){
                    qDebug()<<tmp.at<int16_t>(y,x);
                }
            }
            exit(1);*/



            qDebug()<<"MCW::"<<heatmap.size[0]<<heatmap.size[1];
            qDebug()<<"MCW::corr="<<heatmap.at<float>(33,33);
//          emit processed(std::vector<cv::Mat>(1,heatmap),std::vector<cv::Mat>(1,heatmap));
//          return;

            qDebug()<<"MCW::"<<d.x<<d.y;
            qDebug()<<"MCW::"<<et.elapsed()<<":aligned";
            cv::Point2d center;
            center.x = temporary_data->width / 2.0 - 0.5 + d.x;
            center.y = temporary_data->height / 2.0 - 0.5 + d.y;
            qDebug()<<"MCW::"<<center.x<<center.y;
            std::vector<cv::Mat> shifted_frame;
            for(int i=0;i<temporary_data->n_ch;i++){
                cv::Mat shifted_ch;
                cv::Mat tmp;
                raw_frame[i].convertTo(tmp, CV_32FC1);
                getRectSubPix(tmp, cv::Size(temporary_data->width, temporary_data->height), center, shifted_ch);
                shifted_frame.push_back(shifted_ch);
            }
            qDebug()<<"MCW::"<<et.elapsed()<<":shifted";
            emit processed(raw_frame,shifted_frame);
            qDebug()<<"MCW::"<<et.elapsed()<<":emitted";
            qDebug()<<"MCW::processed";
        }else{
            qDebug()<<"MCW::no update";
        }
    }else{
        qDebug()<<"MCW::no IR";
    }
}

void MotionCorrectionWorker::setTemplate(QString tifffilename){
    CImgList<int16_t> cimglist;
    cimglist.load_tiff(tifffilename.toStdString().c_str(), 0);
    template_image.reserve(cimglist.size());
    qDebug()<<template_image.size();
    for(int i=0;i<cimglist.size();i++){
        CImg<int16_t> cimg = cimglist.at(i);
        qDebug()<<cimg.height()<<cimg.width();
        cimg.transpose();
        cv::Mat tmp(cimg.height(), cimg.width(), CV_16SC1, cimg.data());


        template_image.push_back(tmp.clone());
    }


    /*qDebug()<<"loaded template";
    qDebug()<<template_image.size();
    qDebug()<<template_image[0].rows<<template_image[0].cols;

    for(int ch=0;ch<template_image.size();ch++){
        cv::Mat tmp=template_image[ch];
        for(int y=0;y<tmp.rows;y++){
            for(int x=0;x<tmp.cols;x++){
                qDebug()<<tmp.at<int16_t>(y,x);
            }
        }
    }
    exit(1);*/
}

void MotionCorrectionWorker::setCh(int ch){
    this->ch=ch;
}

void MotionCorrectionWorker::initImageRegistrator(){
    if(template_image.size()){
        if(!ir){
            qDebug()<<"IR::initializing";
            ir=std::make_shared<ImageRegistrator>();
            qDebug()<<"IR::initialized";
        }
        qDebug()<<"IR::setting template";
        int template_ch= ch>1 && template_image.size()>ch-1?ch-1:template_image.size()-1;
        qDebug()<<"IR::template_ch=" << template_ch;
        qDebug()<<"IR::template_size=" << template_image.size();
        ir->SetTemplate(template_image[template_ch]);
        qDebug()<<"IR::set template";
        ir->SetParameters(2,64,0,0,0,0);
        ir->Init();
    }else{
        ir=NULL;
    }
}

void MotionCorrectionWorker::loadParameters(QString xmlfilename){}

MotionCorrectionWorker::~MotionCorrectionWorker()
{
}
