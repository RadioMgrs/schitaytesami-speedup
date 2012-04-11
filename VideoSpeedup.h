#ifndef VIDEOSPEEDUP_H
#define VIDEOSPEEDUP_H

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/background_segm.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "opencv2/objdetect/objdetect.hpp"


using namespace cv;
using namespace std;


class VideoSpeedup {

	bool dual;

	VideoCapture in;
	Rect bbox[2];
	Mat mask[2];
	Mat signal;
	int nFrames;
	float maskArea[2];
	Mat strongBlurMask;
	Mat mildBlurMask;
	BackgroundSubtractorMOG2 subtractor[2];
	
public:
	VideoSpeedup(){ }
	VideoSpeedup(const string& invideoname, const string& inpolyname);
	~VideoSpeedup() { }

	void ProcessVideo(int erodeRadius, int skipFrames, const char *signalFile = NULL, bool reuseSignal = false);
	void SpeedupVideo(const string& dir, const string& prefix, float chunkLength,
		int slowSpeed, int fastSpeed, float level, float minSpan, float meanSpan, int startTime);
};




#endif

