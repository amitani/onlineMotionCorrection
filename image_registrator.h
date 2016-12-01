#ifndef IMAGEREGISTRATOR_H
#define IMAGEREGISTRATOR_H

#include "opencv2/opencv.hpp"
#include "opencv2/opencv_lib.hpp"

class ImageRegistrator
{
public:
	bool Init(); // to be called after both parameters and template are set
	void SetTemplate(const cv::Mat &template_mat);
	void SetParameters(double factor, int margin, double sigma_smoothing, double sigma_normalization,
		double normalization_offset, int to_equalize_histogram);
	bool LoadParameters(cv::String xml_filename, bool to_load_template = false);
	bool SaveParameters(cv::String xml_filename, bool to_save_template = false) const;
    bool Align(const cv::Mat &in, cv::Mat *out, cv::Point2d *d = NULL, cv::Mat *heatmap = NULL) const;

	void Preprocess(const cv::Mat &in, cv::Mat &out) const;
private:

	const double ver_ = 0.01;
	bool initialized_ = false;
	double factor_ = 2;  // factor to shrink for processing
	int margin_ = 64;  // number of pixels at the edge. same as maximum pixels to correct.
	double sigma_smoothing_ = 0;
	double sigma_normalization_ = 0;
	double normalization_offset_ = 0;
	int to_equalize_histogram_ = 0;
	cv::Mat original_template_;
	cv::Mat processed_template_center_;
};

#endif // IMAGEREGISTRATOR_H
