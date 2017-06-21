#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include <deque>
#include <vector>
#include "opencv2/opencv.hpp"
#include "opencv2/opencv_lib.hpp"

class MovingAverage
{
public:
    bool add(std::vector<cv::Mat> x, int tag);
    void resize(int n);
    std::vector<cv::Mat> mean();
    int last_tag();
    MovingAverage(int n = 10);
private:
    int last_tag_;
    std::deque<std::vector<cv::Mat>> buf;
    int max_size;
    std::vector<cv::Mat> sum;
    std::vector<cv::Mat> tmp_sum;
    int tmp_count;
    static void add(std::vector<cv::Mat> &x, std::vector<cv::Mat> y);
    static void subtract(std::vector<cv::Mat> &x, std::vector<cv::Mat> y);
};
#endif // MOVINGAVERAGE_H
