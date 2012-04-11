// Copyright http://schitaytesami.org, 2012
// Author: Victor Lempitsky

#include "VideoSpeedup.h"

#include <iomanip>

using namespace std;


VideoSpeedup::VideoSpeedup(const string& invideoname, const string& inpolyname)
{
	in = VideoCapture(invideoname);
	nFrames = in.get(CV_CAP_PROP_FRAME_COUNT);

	//nFrames = 30000;

	printf("Starting on file %s. Frames: %d\n", invideoname.c_str(), nFrames);
	signal = Mat(nFrames,2,CV_32F);

	vector<Point2f> poly[2];

	ifstream polyin;
	polyin.open(inpolyname.c_str(), ifstream::in);
	dual = false;

	for(;;)
	{
		Point2f p;
		polyin >> p.x;
		polyin >> p.y;

		if(!p.x && !p.y)
			break;

		if(poly[0].size() == 4)
			poly[1].push_back(p);
		else
			poly[0].push_back(p);

		if(polyin.eof())
			break;
	}
	polyin.close();

	dual = poly[1].size() > 0;

	printf("%d\n", poly[0].size());

	maskArea[0] = contourArea(poly[0]);
	if(dual)	maskArea[1] = contourArea(poly[1]);

	printf("Mask area: %f %f\n", maskArea[0], maskArea[1]);
	bbox[0] = boundingRect(poly[0]);
	if(dual)	bbox[1] = boundingRect(poly[1]);

		
	Mat frame;

	in >> frame;
	Mat fullmask(frame.size(),CV_8U);
	printf("Width: %d, Height: %d\n", frame.size().width, frame.size().height);


	for(int j = 0; j < 1 + (dual? 1 : 0);  j++)
	{
		Point *pts = new Point[poly[j].size()];
		for(int i=0; i< poly[j].size(); i++)
			pts[i] = poly[j][i];
		int sz[] = { poly[j].size() };
		const Point *ppt[1] = {pts};
		fillPoly(fullmask, ppt, sz, 1, Scalar(255));
		delete[] pts;
	}

	rectangle(fullmask, Point(0,0), Point(240,20), Scalar(255), -1); //corner timestamp
	
	subtract(Scalar(255), fullmask, strongBlurMask);
	dilate(strongBlurMask, mildBlurMask, Mat::ones(3,3,CV_8U));

	erode(strongBlurMask, strongBlurMask, Mat::ones(15,15,CV_8U));
	
	for(int j = 0; j < 1 + (dual? 1 : 0);  j++)
	{
		Mat mask_ = Mat(fullmask, Range(bbox[j].y,bbox[j].y+bbox[j].height), Range(bbox[j].x,bbox[j].x+bbox[j].width));
		subtract(Scalar(255),mask_,mask[j]);
		multiply(mask[j],Scalar(255),mask[j]);
	}
	
		//subtractor.bShadowDetection = 0;
	in.set(CV_CAP_PROP_POS_FRAMES,0);
}


void VideoSpeedup::ProcessVideo(int erodeRadius, int skipFrames, const char *signalFile, bool reuseSignal)
{
	Mat frame;
	Mat crop;
	Mat motion, motionE;
	ifstream signalin;

	if(signalFile && reuseSignal)
		signalin.open(signalFile.c_str(), ios::in | ios::binary);


	if(signalFile && reuseSignal && signalin.good())
	{
		float *signalArray = new float[nFrames*2];
			
		signalin.read((char *)signalArray, sizeof(float)*nFrames*2);
		signalin.close();

		for(int k = 0; k < 1 + (dual? 1 : 0); k++)
			for(int j = 0; j < nFrames; j++)
				signal.at<float>(j,k) = signalArray[j+k*nFrames];

		delete[] signalArray;
	}
	else {
		cout << "Processing the input video\n";
		for(int i = 0; i < nFrames; )
		{
			if(! (i%1000) )			cout << i << " out of " << nFrames << '\n';
			in >> frame;

			if(frame.rows == 0)
			{
				signal.at<float>(i,0) = signal.at<float>(max(i-1,0),0);
				signal.at<float>(i,1) = signal.at<float>(max(i-1,0),1);
				i++;
				continue;
			}

			float val[2];
			for(int j = 0; j < 1 + (dual? 1 : 0);  j++)
			{
				crop = Mat(frame, Range(bbox[j].y,bbox[j].y+bbox[j].height), Range(bbox[j].x,bbox[j].x+bbox[j].width));
				subtractor[j](crop,motion);
				erode(motion,motionE,Mat::ones(erodeRadius,erodeRadius,CV_8U));
				motionE.setTo(0,mask[j]);
				compare(motionE,254,motionE,CMP_GT);
				val[j] = float(countNonZero(motionE))/maskArea[j];
				signal.at<float>(i,j) = val[j];
			}

			if(skipFrames) {
				int j;
				for(j=i+1; j <=i+skipFrames; j++)
				{
					signal.at<float>(j,0) = val[0];
					signal.at<float>(j,1) = val[1];
				}
				in.set(CV_CAP_PROP_POS_FRAMES,j);
				i = j;
			}
			else i++;

		}
	}

	if(signalFile && !reuseSignal)
	{
		float *signalArray = new float[nFrames*2];
		for(int k = 0; k < 1 + (dual? 1 : 0); k++)
			for(int j = 0; j < nFrames; j++)
				signalArray[j+k*nFrames] = signal.at<float>(j,k);
		ofstream signalout;
		signalout.open(signalFile.c_str(), ios::out | ios::binary);
		if(signalout.good())
		{
			signalout.write((char *)signalArray, sizeof(float)*nFrames*2);
			signalout.close();
		}
		else {
			cout << "Error saving the signal!\n";
		}
		delete[] signalArray;
	}
	in.set(CV_CAP_PROP_POS_FRAMES,0);
}

#define PI 3.1415
#define OUTWIDTH 640
#define OUTHEIGHT 480

void VideoSpeedup::SpeedupVideo(const string& dir, const string& prefix, float chunkLength,
	int slowSpeed, int fastSpeed, float level, float minSpan, float meanSpan, int startTime)
{
	Mat frame;

	cout << "Writing the output video\n";
	int inrate = in.get(CV_CAP_PROP_FPS);
	int startmsec = in.get(CV_CAP_PROP_POS_MSEC);


	int mind = ceil(minSpan*inrate/2);
	int meand = ceil(meanSpan*inrate/2);


	Mat tmp = signal;
	Mat tmp2(tmp);
	dilate(tmp,tmp2,Mat::ones(mind,1,CV_32F));
	
	for(int i = 0; i < nFrames; i++)
	{
		for(int k = 0; k < 1 + (dual? 1 : 0); k++)
		{
			float val = tmp2.at<float>(i,k);
			float ratio = val/level;
			if(ratio >= 1.0)
				ratio = 1.0;
			if(ratio <= 0)
				ratio = 0;
		
			tmp.at<float>(i,k) = slowSpeed*ratio+fastSpeed*(1-ratio);
		}
	}
	//blur(tmp,tmp2,Size(meand,1));

	int MaxChunkFramesWritten = chunkLength*inrate;
	int lastWritten = -1000000;
	int nFramesWritten = 0;
	Mat blurStrong;
	Mat blurWeak;
	
	for(int i = 0, chunkIndex = 0; i < nFrames; chunkIndex++)
	{
		cout << "Starting chunk " << chunkIndex << ". \n";

		ostringstream outvideoname;
		outvideoname << dir << prefix << "_" << chunkIndex << ".avi";
		cout << "1";
		VideoWriter out(outvideoname.str(), CV_FOURCC('D','I','V','X'), inrate, Size(OUTWIDTH,OUTHEIGHT), 1);
		cout << "2";

		ofstream timelineout;
		string timelineoutName = outvideoname.str()+".timeline";
		timelineout.open(timelineoutName.c_str());

		if( !out.isOpened() || !timelineout.good() ) {
					   printf("VideoWriter failed to open!\n");
					   exit (-1);
		}

		int chunkFramesWritten = 0;
		lastWritten = -1000000;
		for(; i < nFrames; i++)
		{
			if(chunkFramesWritten > MaxChunkFramesWritten && tmp2.at<float>(i,0) > fastSpeed*0.8 && (tmp2.at<float>(i,1) > fastSpeed*0.8 || !dual))
				break;
			in >> frame;

			if(frame.rows == 0)
				continue;
			
			if(i-lastWritten < tmp2.at<float>(i,0) && (!dual || i-lastWritten < tmp2.at<float>(i,1)))
				continue;


			blur(frame, blurStrong, Size(int(frame.cols/40),int(frame.cols/40)));
			blur(frame, blurWeak, Size(int(frame.cols/80),int(frame.cols/80)));

			int msec = in.get(CV_CAP_PROP_POS_MSEC)-startmsec+startTime*1000;
			int hours = msec/(3600*1000);
			int minutes = (msec % (3600*1000))/(60*1000);
			int seconds = (msec % (60*1000))/(1000);

			blurWeak.copyTo(frame, mildBlurMask);
			blurStrong.copyTo(frame, strongBlurMask);

			rectangle(frame, bbox[0], CV_RGB(255, 0, 0));
			rectangle(frame, bbox[1], CV_RGB(255, 0, 0));

			Mat frame_;
			resize(frame, frame_, Size(OUTWIDTH, OUTHEIGHT));

			stringstream ss;
			ss << setfill ('0') << setw(2) <<  hours << ':' << setfill ('0') << setw(2) << minutes << ':'<< setfill ('0') << setw(2) << seconds;
			if(startTime)
				putText(frame_, ss.str(), Point(8,frame_.size().height-16), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,255,255), 2);

			out << frame_;
			lastWritten = i;

			
			int outmsec = (1000*nFramesWritten/inrate );
			int outhours = outmsec/(3600*1000);
			int outminutes = (outmsec % (3600*1000))/(60*1000);
			int outseconds = (outmsec % (60*1000))/(1000);

			stringstream ssout;
			ssout << setfill ('0') << setw(2) <<  outhours << ':' << setfill ('0') << setw(2) << outminutes << ':'<< setfill ('0') << setw(2) << outseconds;
			

			timelineout << chunkFramesWritten << " " << i << " " << ssout.str() << " " << ss.str() << "\n";

			nFramesWritten++;
			chunkFramesWritten++;
		}
		cout << chunkFramesWritten << " frames written." << endl;
		timelineout.close();
	}
	cout << "Number of written frames " << nFramesWritten;
	
	
}


struct Param{
	int erodeParam;
	int skipFrames;
	int slowSpeed;
	int fastSpeed;

	float sensitivity;
	float minSpan;
	float meanSpan;

	float chunkLength;
	bool saveSignal;
	bool reuseSignal;
	int startTime; //in seconds from midnight

	Param() {
		erodeParam = 2;
		skipFrames = 0;
		slowSpeed = 2;
		fastSpeed = 16;
		sensitivity = 0.005;
		minSpan = 1;
		meanSpan = 0.5;
		chunkLength = 12000;
		saveSignal = true;
		reuseSignal = false;
		startTime = 0;
	}
		
};




//C:\temp\0027c176-465e-11e1-a9f1-047d7b49312.avi C:\Observer\copy4vadim\done\catenated\out\test.txt C:\Observer\copy4vadim\done\catenated\temp\

int main(int argc, char * argv[])
{
	Param params;

	int n = 5;

	for(int i = 0; i < argc; i++)
		printf("%d    %s\n", i, argv[i]);

	while(n < argc-1) {
		if(!strcmpi(argv[n], "-erode")) {
			sscanf(argv[n+1], "%d", &params.erodeParam);
		}
		if(!strcmpi(argv[n], "-skip")) {
			sscanf(argv[n+1], "%d", &params.skipFrames);
		}
		if(!strcmpi(argv[n], "-slow")) {
			sscanf(argv[n+1], "%d", &params.slowSpeed);
		}
		if(!strcmpi(argv[n], "-fast")) {
			sscanf(argv[n+1], "%d", &params.fastSpeed);
		}
		if(!strcmpi(argv[n], "-sensitivity")) {
			sscanf(argv[n+1], "%f", &params.sensitivity);
		}
		if(!strcmpi(argv[n], "-minspan")) {
			sscanf(argv[n+1], "%f", &params.minSpan);
		}
		if(!strcmpi(argv[n], "-meanspan")) {
			sscanf(argv[n+1], "%f", &params.meanSpan);
		}
		if(!strcmpi(argv[n], "-chunk")) {
			sscanf(argv[n+1], "%f", &params.chunkLength);
		}
		if(!strcmpi(argv[n], "-savesignal")) {
			int t;
			sscanf(argv[n+1], "%d", &t);
			params.saveSignal = t > 0;
		}
		if(!strcmpi(argv[n], "-reusesignal")) {
			int t;
			sscanf(argv[n+1], "%d", &t);
			params.reuseSignal = t > 0;
		}

		if(!strcmpi(argv[n], "-starttime")) {
			int t;
			sscanf(argv[n+1], "%d", &t);
			params.startTime = t;
		}


		n += 2;
	}
	
	string str(argv[1]);
	str += ".signal";

	VideoSpeedup vs = VideoSpeedup(argv[1], argv[2]);
	vs.ProcessVideo(params.erodeParam, params.skipFrames, (params.saveSignal || params.reuseSignal)? str.c_str() : NULL, params.reuseSignal);
	vs.SpeedupVideo(argv[3], argv[4], params.chunkLength, params.slowSpeed,
		params.fastSpeed, params.sensitivity, params.minSpan, params.meanSpan, params.startTime);

	return 0;
}
