#include "motioncorrectionthread.h"
#include <QtDebug>
//#include <QElapsedTimer>

void SetMatInMMap(std::shared_ptr<MMap<SI4Image>> mmap, std::vector<cv::Mat> mat, int frame_tag){
    static std::shared_ptr<SI4Image> si = std::make_shared<SI4Image>();
    if(mat.empty()) return;
    si->frame_tag=frame_tag;
    int cols=mat[0].cols;
    int rows=mat[0].rows;
    si->width=cols;
    si->height=rows;
    si->n_ch = mat.size();
    assert(cols*rows*mat.size()<SI4Image::max_size);
    for(int i=0;i<mat.size();i++){
        if(mat[i].cols!=cols) return;
        if(mat[i].rows!=rows) return;
        cv::Mat m;
        mat[i].convertTo(m,CV_16S); // todo: check whether this a shallow copy if no conversion is needed
        memcpy(si->data+i*cols*rows,m.data,sizeof(int16_t)*cols*rows);
    }
    mmap->set(*si);
    return;
}

MotionCorrectionWorker::MotionCorrectionWorker(QObject* parent):QObject(parent), xyz(n_for_xyz_correction),summed(n_for_summed), tmpl(n_for_template)
{
    timer = new QTimer(this);
    timer->setInterval(10);
    //connect(timer, &QTimer::timeout,this,&MotionCorrectionWorker::check);
    connect(timer, SIGNAL(timeout()),this,SLOT(check()));
    ch=0;
    mmap_raw=std::make_shared<MMap<SI4Image>>("SI4_RAW");
    mmap_shifted=std::make_shared<MMap<SI4Image>>("MC_SHIFTED");
    mmap_average=std::make_shared<MMap<SI4Image>>("MC_AVERAGE");
    mmap_summed=std::make_shared<MMap<SI4Image>>("MC_SUMMED");
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
//            QElapsedTimer et;
//            et.start();
            mmap_raw->get(*temporary_data);
            qDebug()<<"MCW::n_ch"<<temporary_data->n_ch;
//            qDebug()<<"MCW::"<<et.elapsed()<<":get";
            last_frame_tag=temporary_data->frame_tag;
            if(temporary_data->n_ch<1) return;
            qDebug()<<"MCW::"<<temporary_data->frame_tag;
            std::vector<cv::Mat> raw_frame;
            for(int i=0;i<temporary_data->height*temporary_data->width*temporary_data->n_ch;i++){
                if(temporary_data->data[i]<threshold_)temporary_data->data[i]=replacement_;
            }
            for(int i=0;i<temporary_data->n_ch;i++){
                cv::Mat tmp;
                cv::Mat(temporary_data->height,temporary_data->width,CV_16S,
                        temporary_data->data+temporary_data->height*temporary_data->width*i).convertTo(tmp,CV_32F);
                raw_frame.push_back(tmp);
            }
//            qDebug()<<"MCW::"<<et.elapsed()<<":copied";

            int ch_to_align= ch && ch<temporary_data->n_ch?ch-1:temporary_data->n_ch-1;
            qDebug()<<"MCW::ch "<<ch_to_align;
            cv::Point2d d;
            if(template_image.empty()){
                mutex_.lock();
                auto ma_template = tmpl.mean();
                mutex_.unlock();
                if(!ma_template.empty()){
                    ir->SetTemplate(ma_template[0]);
                    ir->Init();
                    ir->Align(raw_frame[ch_to_align],NULL,&d,NULL);
                    qDebug()<<"MCW::Moving average template initialized";
                }
           }else{
                ir->Align(raw_frame[ch_to_align],NULL,&d,NULL);
                qDebug()<<"MCW::aligned";
            }

            qDebug()<<"MCW::"<<d.x<<d.y;
//            qDebug()<<"MCW::"<<et.elapsed()<<":aligned";
            cv::Point2d center;
            center.x = temporary_data->width / 2.0 - 0.5 + d.x;
            center.y = temporary_data->height / 2.0 - 0.5 + d.y;
            qDebug()<<"MCW::"<<center.x<<center.y;
            std::vector<cv::Mat> shifted_frame;   // Todo: Technically we can reuse this without allocating everytime to save some time.
            for(int i=0;i<temporary_data->n_ch;i++){
                cv::Mat shifted_ch;
                cv::Mat tmp;
                raw_frame[i].convertTo(tmp, CV_32FC1);
                getRectSubPix(tmp, cv::Size(temporary_data->width, temporary_data->height), center, shifted_ch);
                shifted_ch.convertTo(tmp, CV_16S);
                shifted_frame.push_back(tmp);
            }
            SetMatInMMap(mmap_shifted,shifted_frame,last_frame_tag);
//            qDebug()<<"MCW::"<<et.elapsed()<<":shifted";

            SmallDoubleMatrix sdm;
            sdm.height=3;sdm.width=1;sdm.data[0]=d.y;sdm.data[1]=d.x;sdm.data[2]=last_frame_tag;
            mmap_dislocation->set(sdm);

            mutex_.lock();
            raw.add(raw_frame,last_frame_tag);
            shifted.add(shifted_frame,last_frame_tag);
            if(template_image.empty())
                tmpl.add(std::vector<cv::Mat>(raw_frame.begin()+ch_to_align,raw_frame.begin()+ch_to_align+1),last_frame_tag);
//            xyz.add(shifted_frame,last_frame_tag);
//            auto mean_for_xyz = xyz.mean();
            summed.add(shifted_frame,last_frame_tag);
            auto mean_for_summed = summed.mean();
            mutex_.unlock();
            emit processed();
//            SetMatInMMap(mmap_average,mean_for_xyz,last_frame_tag);
            SetMatInMMap(mmap_summed,mean_for_summed,last_frame_tag);

//            qDebug()<<"MCW::"<<et.elapsed()<<":emitted";
        }else{
            qDebug()<<"MCW::no update";
        }
    }else{
        qDebug()<<"MCW::no IR";
    }
}
void MotionCorrectionWorker::getMeans(std::vector<cv::Mat> &raw_frame, std::vector<cv::Mat> &shifted_frame, int &last_frame_tag){
    mutex_.lock();
    raw_frame=raw.mean();
    shifted_frame=shifted.mean();
    last_frame_tag=raw.last_tag();
    mutex_.unlock();
}

void MotionCorrectionWorker::setTemplate(QString tifffilename){
    template_image = cv::imread(tifffilename.toStdString(),CV_LOAD_IMAGE_ANYDEPTH).t();
    cv::Mat tmp;
    template_image.convertTo(tmp,CV_8U);
    tmp=tmp.t();
    cv::imshow(tifffilename.toStdString(),tmp);
}

void MotionCorrectionWorker::setCh(int ch){
    this->ch=ch;
}

void MotionCorrectionWorker::initImageRegistrator(){
    if(!ir){
        qDebug()<<"IR::initializing";
        ir=std::make_shared<ImageRegistrator>();
        //ir->SetParameters(2,64,0,0,0,0); default parameter is set in ImageRegister
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

void MotionCorrectionWorker::setParameters(double factor, int margin_h,int margin_w, double sigma_smoothing,
                                           double sigma_normalization, double normalization_offset, int threshold, int replacement){
    ir->SetParameters(factor,margin_h,margin_w,sigma_smoothing,sigma_normalization,normalization_offset);
    threshold_ = threshold;
    replacement_ = replacement;
    if(!template_image.empty()){
        ir->Init();
    }
}
void MotionCorrectionWorker::setNumAverage(int n){
    mutex_.lock();
    if(n<1) n=1;
    if(n>200) n=200;
    raw.resize(n);
    shifted.resize(n);
    mutex_.unlock();
}

MotionCorrectionWorker::~MotionCorrectionWorker()
{
    delete timer;
}
