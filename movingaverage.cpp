#include "movingaverage.h"

void MovingAverage::add(std::vector<cv::Mat> &x, std::vector<cv::Mat> y){
    if(x.empty()){
        for(int i=0;i<y.size();i++){
            cv::Mat tmp;
            y[i].convertTo(tmp,CV_32F);
            x.push_back(tmp);
        }
    }
    else{
        for(int i=0;i<y.size();i++) cv::add(x[i],y[i],x[i],cv::Mat(),CV_32F);
    }
}

void MovingAverage::subtract(std::vector<cv::Mat> &x, std::vector<cv::Mat> y){
    for(int i=0;i<y.size();i++) cv::subtract(x[i],y[i],x[i],cv::Mat(),CV_32F);
}

MovingAverage::MovingAverage(int n) : max_size(n), tmp_count(0){

}
bool MovingAverage::add(std::vector<cv::Mat> x, int  tag) {
    // todo: check shape
    while (buf.size() >= max_size) {
        subtract(sum, buf.front());
        buf.pop_front();
    }
    add(sum,x);
    buf.push_back(x);
    last_tag_ = tag;
    //todo tmp_sum

    return true;
}

std::vector<cv::Mat> MovingAverage::mean() {
    std::vector<cv::Mat> res;
    for(int i=0;i<sum.size();i++){
        res.push_back(sum[i]/buf.size());
    }
    return res;
}
int  MovingAverage::last_tag(){ return last_tag_;}

void MovingAverage::resize(int n) {
    max_size = n;
    while (buf.size() > max_size) {
        subtract(sum, buf.front());
        buf.pop_front();
    }
    if(tmp_count >= max_size){
        tmp_count=0;
        tmp_sum.clear();
    }
}
