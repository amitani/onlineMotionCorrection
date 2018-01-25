#include "image_registrator.h"
#include "CV_SubPix.h"

using cv::Mat;
using cv::Point2d;
using cv::String;
using cv::Range;
using cv::Size;
using cv::Point;
using cv::FileStorage;

bool ImageRegistrator::Init()
{
	Mat tmp;
	Preprocess(original_template_, tmp);
    processed_template_center_ = tmp(Range(margin_w_ / factor_, original_template_.rows / factor_ - margin_w_ / factor_),
                                     Range(margin_h_ / factor_, original_template_.cols / factor_ - margin_h_ / factor_)).clone();
    // flipped because Matlab uses different data order h and w corresponds to col and row, respectively.
	return initialized_ = true;
}

void ImageRegistrator::SetTemplate(const cv::Mat & template_mat)
{
	initialized_ = false;
    template_mat.copyTo(original_template_);
    return;
}

void ImageRegistrator::SetParameters(double factor, int margin_h, int margin_w, double sigma_smoothing,
    double sigma_normalization, double normalization_offset)
{
	initialized_ = false;
    if(factor>=0) factor_ = factor;
    if (margin_h >= 0) margin_h_ = margin_h;
    if (margin_w >= 0) margin_w_ = margin_w;
	if (sigma_smoothing >= 0) sigma_smoothing_ = sigma_smoothing;
	if (sigma_normalization >= 0) sigma_normalization_ = sigma_normalization;
    if (normalization_offset >= 0) normalization_offset_ = normalization_offset;
	return;
}

bool ImageRegistrator::LoadParameters(const cv::String xml_filename, bool to_load_template)
{
	initialized_ = false;
	try {
		FileStorage fs(xml_filename, FileStorage::READ);
        fs["Factor"] >> factor_;
        fs["MarginH"] >> margin_h_;
        fs["MarginW"] >> margin_w_;
		fs["SigmaSmoothing"] >> sigma_smoothing_;
		fs["SigmaNormalization"] >> sigma_normalization_;
        fs["NormalizationOffset"] >> normalization_offset_;
		if (to_load_template) fs["Template"] >> original_template_;
		return true;
	}catch (cv::Exception e) {
		return false;
	}
}

bool ImageRegistrator::SaveParameters(const String xml_filename, bool to_save_template) const
{
	if (!initialized_) return false;
	try {
		FileStorage fs(xml_filename, FileStorage::WRITE);
		fs << "Version" << ver_;
        fs << "Factor" << factor_;
        fs << "MarginH" << margin_h_;
        fs << "MarginW" << margin_w_;
		fs << "SigmaSmoothing" << sigma_smoothing_;
		fs << "SigmaNormalization" << sigma_normalization_;
        fs << "NormalizationOffset" << normalization_offset_;
		if (to_save_template) fs << "Template" << original_template_;
		fs.release();
		return true;
	}catch (cv::Exception e){
		return false;
	}
}

bool ImageRegistrator::Align(const Mat &in, Mat *out, Point2d *d, Mat *heatmap) const{
	if (!initialized_) return false;
	Mat tmp;
	Preprocess(in, tmp);
    Mat result(2 * std::round(margin_w_ / factor_) + 1, 2 * std::round(margin_h_ / factor_) + 1, CV_32FC1);
	matchTemplate(tmp, processed_template_center_, result, CV_TM_CCOEFF_NORMED);
	if (heatmap) {
		*heatmap = result;
	}

	Point maxLoc;
	minMaxLoc(result, NULL, NULL, NULL, &maxLoc);
	Point2d maxLocSubPix;
	minMaxLocSubPix(&maxLocSubPix, result, &maxLoc);
	if (d || out) {
        d->x = maxLocSubPix.x * factor_ - margin_h_;
        d->y = maxLocSubPix.y * factor_ - margin_w_;
	}
	if (out) {
		Point2d center;
		center.x = original_template_.cols / 2.0 - 0.5 + d->x;
		center.y = original_template_.rows / 2.0 - 0.5 + d->y;
		Mat imResult;
		in.convertTo(tmp, CV_32FC1);
		getRectSubPix(tmp, Size(original_template_.cols, original_template_.rows), center, imResult);
		imResult.convertTo(*out, in.depth());
	}
	return true;
}

void ImageRegistrator::Preprocess(const cv::Mat & in, cv::Mat & out) const
{
	in.convertTo(out, CV_32FC1, 1, normalization_offset_);
	resize(out, out, Size(), 1.0 / factor_, 1.0 / factor_);
	Mat tmp1;
	if (sigma_smoothing_)
		GaussianBlur(out, tmp1, Size(0, 0), sigma_smoothing_ / factor_);
		//blur(out, tmp1, Size(sigma_smoothing_ / factor_, sigma_smoothing_ / factor_));
	else
		tmp1 = out;
	if (sigma_normalization_) {
		tmp1.convertTo(tmp1, -1, 1, 0);
		Mat tmp2;
		GaussianBlur(out, tmp2, Size(0, 0), sigma_normalization_ / factor_);
		//blur(out, tmp2, Size(sigma_smoothing_ / factor_, sigma_smoothing_ / factor_));
		tmp2.convertTo(tmp2, CV_32FC1, normalization_offset_);
		normalize(tmp2, tmp2, 1, cv::NORM_L1);
		divide(tmp1, tmp2, out);
	} else {
		out = tmp1;
	}
	if (to_equalize_histogram_) {
		normalize(out, out, 0, 255, cv::NORM_MINMAX);
		out.convertTo(out, CV_8UC1);
		equalizeHist(out, out);
	}
	return;
}
