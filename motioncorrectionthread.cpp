#include "motioncorrectionthread.h"
#include <QtDebug>
#include <QElapsedTimer>

/*
#define cimg_use_tiff
#include "CImg.h"
//#pragma comment(lib, "libtiff.lib")
using namespace cimg_library;
*/

MotionCorrectionWorker::MotionCorrectionWorker(QObject* parent):QObject(parent), n_template(default_n_template)
{
    timer = new QTimer(this);
    timer->setInterval(10);
    //connect(timer, &QTimer::timeout,this,&MotionCorrectionWorker::check);
    connect(timer, SIGNAL(timeout()),this,SLOT(check()));
    ch=0;
    mmap_raw=std::make_shared<MMap<SI4Image>>("SI4_RAW");
    mmap_shifted=std::make_shared<MMap<SI4Image>>("MC_SHIFTED");
    mmap_average=std::make_shared<MMap<SI4Image>>("MC_AVERAGE");
    mmap_dislocation=std::make_shared<MMap<SmallDoubleMatrix>>("MC_DISLOCATION");
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
                cv::Mat tmp;
                cv::Mat(temporary_data->height,temporary_data->width,CV_16S,
                        temporary_data->data+temporary_data->height*temporary_data->width*i).convertTo(tmp,CV_32F);
                raw_frame.push_back(tmp);
            }
            qDebug()<<"MCW::"<<et.elapsed()<<":copied";

            int ch_to_align= ch && ch<temporary_data->n_ch?ch-1:temporary_data->n_ch-1;
            qDebug()<<"MCW::ch "<<ch_to_align;
            cv::Point2d d;
            cv::Mat heatmap;
            if(template_image.empty()){
                mutex_.lock();
                auto deque_raw = deque_raw_;
                mutex_.unlock();

                qDebug()<<"MCW::deque_size:"<<deque_raw.size();
                if(!deque_raw.empty() && temporary_data->n_ch == deque_raw[0].size()){
                    qDebug()<<"MCW::averaging for template";
                    cv::Mat tmp(deque_raw[0][ch_to_align].rows,deque_raw[0][ch_to_align].cols,CV_32F);
                    int n=deque_raw.size()>n_template?n_template:deque_raw.size();
                    qDebug()<<"MCW::averaging frames:" << n;
                    for(int i=0;i<n;i++) tmp+=deque_raw[i][ch_to_align];
                    tmp/=(float)n;
                    qDebug()<<"MCW::averaged for template";
                    ir->SetTemplate(tmp);
                    ir->Init();
                    ir->Align(raw_frame[ch_to_align],NULL,&d,&heatmap);
                    qDebug()<<"MCW::aligned to average";
                }
            }else{
                ir->Align(raw_frame[ch_to_align],NULL,&d,&heatmap);
                qDebug()<<"MCW::aligned";
            }

            /*cv::Mat tmp=raw_frame[ch_to_align];
            for(int y=0;y<tmp.rows;y++){
                for(int x=0;x<tmp.cols;x++){
                    qDebug()<<tmp.at<int16_t>(y,x);
                }
            }
            exit(1);*/


            qDebug()<<"MCW::"<<heatmap.size[0]<<heatmap.size[1];
            if(heatmap.size[0]>40 && heatmap.size[1]>40) qDebug()<<"MCW::corr="<<heatmap.at<float>(33,33);
//          emit processed(std::vector<cv::Mat>(1,heatmap),std::vector<cv::Mat>(1,heatmap));
//          return;

            qDebug()<<"MCW::"<<d.x<<d.y;
            qDebug()<<"MCW::"<<et.elapsed()<<":aligned";
            cv::Point2d center;
            center.x = temporary_data->width / 2.0 - 0.5 + d.x;
            center.y = temporary_data->height / 2.0 - 0.5 + d.y;
            qDebug()<<"MCW::"<<center.x<<center.y;
            std::vector<cv::Mat> shifted_frame;
            std::shared_ptr<SI4Image> si = std::make_shared<SI4Image>();
            si->frame_tag=last_frame_tag;
            si->height=temporary_data->height;
            si->width=temporary_data->width;
            si->n_ch=temporary_data->n_ch;
            for(int i=0;i<temporary_data->n_ch;i++){
                cv::Mat shifted_ch;
                cv::Mat tmp;
                raw_frame[i].convertTo(tmp, CV_32FC1);
                getRectSubPix(tmp, cv::Size(temporary_data->width, temporary_data->height), center, shifted_ch);
                shifted_frame.push_back(shifted_ch);
                shifted_ch.convertTo(tmp, CV_16S);
                memcpy(si->data+i*tmp.cols*tmp.rows,tmp.data,sizeof(int16_t)*tmp.cols*tmp.rows);
            }
            mmap_shifted->set(*si);
            qDebug()<<"MCW::"<<et.elapsed()<<":shifted";

            SmallDoubleMatrix sdm;
            sdm.height=2;sdm.width=1;sdm.data[0]=d.y;sdm.data[1]=d.x;
            mmap_dislocation->set(sdm);




            mutex_.lock();
            if(deque_raw_.size()>0){
                if(deque_raw_[0].size()!=raw_frame.size()){
                    qDebug()<<"QIUW::size discrepancy (ch), clearing deque";
                    deque_raw_.clear();
                    deque_shifted_.clear();
                    deque_frame_tag_.clear();
                }else if(deque_raw_[0][0].cols!=raw_frame[0].cols){
                    qDebug()<<"QIUW::size discrepancy (cols), clearing deque";
                    deque_raw_.clear();
                    deque_shifted_.clear();
                    deque_frame_tag_.clear();
                }else if(deque_raw_[0][0].rows!=raw_frame[0].rows){
                    qDebug()<<"QIUW::size discrepancy (rows), clearing deque";
                    deque_raw_.clear();
                    deque_shifted_.clear();
                    deque_frame_tag_.clear();
                }
            }
            deque_raw_.push_front(raw_frame);
            while(deque_raw_.size()>max_deque_size)deque_raw_.pop_back();
            deque_shifted_.push_front(shifted_frame);
            while(deque_shifted_.size()>max_deque_size)deque_shifted_.pop_back();
            deque_frame_tag_.push_front(last_frame_tag);
            while(deque_frame_tag_.size()>max_deque_size)deque_frame_tag_.pop_back();
            mutex_.unlock();
            emit processed();

            qDebug()<<"MCW::"<<et.elapsed()<<":emitted";
        }else{
            qDebug()<<"MCW::no update";
        }
    }else{
        qDebug()<<"MCW::no IR";
    }
}
void MotionCorrectionWorker::getDeque(std::deque<std::vector<cv::Mat>> &deque_raw, std::deque<std::vector<cv::Mat>> &deque_shifted, std::deque<int> &deque_frame_tag){
    mutex_.lock();
    deque_raw=deque_raw_;
    deque_shifted=deque_shifted_;
    deque_frame_tag=deque_frame_tag_;
    mutex_.unlock();
}

void MotionCorrectionWorker::setTemplate(QString tifffilename){
    /*CImgList<int16_t> cimglist;
    cimglist.load_tiff(tifffilename.toStdString().c_str(), 0);
    template_image.reserve(cimglist.size());
    qDebug()<<template_image.size();
    for(int i=0;i<cimglist.size();i++){
        CImg<int16_t> cimg = cimglist.at(i);
        qDebug()<<cimg.height()<<cimg.width();
        cimg.transpose();
        cv::Mat tmp(cimg.height(), cimg.width(), CV_16SC1, cimg.data());
        template_image.push_back(tmp.clone());
    }*/

    template_image = cv::imread(tifffilename.toStdString(),CV_LOAD_IMAGE_ANYDEPTH);
    cv::Mat tmp;
    //template_image.convertTo(tmp,CV_8U);
    //cv::imshow(tifffilename.toStdString(),tmp);
}

void MotionCorrectionWorker::setCh(int ch){
    this->ch=ch;
}

void MotionCorrectionWorker::initImageRegistrator(){
    if(!ir){
        qDebug()<<"IR::initializing";
        ir=std::make_shared<ImageRegistrator>();
        ir->SetParameters(2,64,0,0,0,0);
        qDebug()<<"IR::initialized";
    }
    if(!template_image.empty()){
        qDebug()<<"IR::setting template";
        qDebug()<<"IR::template_size=" << template_image.size;
        ir->SetTemplate(template_image);
        qDebug()<<"IR::set template";
        ir->Init();
    }
}

void MotionCorrectionWorker::setParameters(double factor, int margin, double sigma_smoothing,
                                           double sigma_normalization, double normalization_offset, int to_equalize_histogram){
    ir->SetParameters(factor,margin,sigma_smoothing,sigma_normalization,normalization_offset,to_equalize_histogram);
    ir->Init();
}

MotionCorrectionWorker::~MotionCorrectionWorker()
{
    delete timer;
}
