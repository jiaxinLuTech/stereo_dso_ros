/**
* This file is part of DSO.
* 
* Copyright 2016 Technical University of Munich and Intel.
* Developed by Jakob Engel <engelj at in dot tum dot de>,
* for more information see <http://vision.in.tum.de/dso>.
* If you use this code, please cite the respective publications as
* listed on the above website.
*
* DSO is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* DSO is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with DSO. If not, see <http://www.gnu.org/licenses/>.
*/





#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "util/settings.h"
#include "FullSystem/FullSystem.h"
#include "util/Undistort.h"
#include "IOWrapper/Pangolin/PangolinDSOViewer.h"
#include "IOWrapper/OutputWrapper/SampleOutputWrapper.h"


#include <ros/ros.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <geometry_msgs/PoseStamped.h>
#include "cv_bridge/cv_bridge.h"

#include <message_filters/subscriber.h> 
#include <message_filters/time_synchronizer.h> 

std::string calib = "";
std::string vignetteFile = "";
std::string gammaFile = "";
bool useSampleOutput=false;
int mode=0;

using namespace sensor_msgs;
using namespace message_filters;
using namespace dso;
void parseArgument(char* arg)
{
	int option;
	char buf[1000];

	if(1==sscanf(arg,"sampleoutput=%d",&option))
	{
		if(option==1)
		{
			useSampleOutput = true;
			printf("USING SAMPLE OUTPUT WRAPPER!\n");
		}
		return;
	}

	if(1==sscanf(arg,"quiet=%d",&option))
	{
		if(option==1)
		{
			setting_debugout_runquiet = true;
			printf("QUIET MODE, I'll shut up!\n");
		}
		return;
	}


	if(1==sscanf(arg,"nolog=%d",&option))
	{
		if(option==1)
		{
			setting_logStuff = false;
			printf("DISABLE LOGGING!\n");
		}
		return;
	}

	if(1==sscanf(arg,"nogui=%d",&option))
	{
		if(option==1)
		{
			disableAllDisplay = true;
			printf("NO GUI!\n");
		}
		return;
	}
	if(1==sscanf(arg,"nomt=%d",&option))
	{
		if(option==1)
		{
			multiThreading = false;
			printf("NO MultiThreading!\n");
		}
		return;
	}
	if(1==sscanf(arg,"calib=%s",buf))
	{
		calib = buf;
		printf("loading calibration from %s!\n", calib.c_str());
		return;
	}
	if(1==sscanf(arg,"vignette=%s",buf))
	{
		vignetteFile = buf;
		printf("loading vignette from %s!\n", vignetteFile.c_str());
		return;
	}

	if(1==sscanf(arg,"gamma=%s",buf))
	{
		gammaFile = buf;
		printf("loading gammaCalib from %s!\n", gammaFile.c_str());
		return;
	}
	if(1==sscanf(arg,"mode=%d",&option))
	{
		mode = option;
		if(option==0)
		{
			printf("PHOTOMETRIC MODE WITH CALIBRATION!\n");
		}
		if(option==1)
		{
			printf("PHOTOMETRIC MODE WITHOUT CALIBRATION!\n");
			setting_photometricCalibration = 0;
			setting_affineOptModeA = 0; //-1: fix. >=0: optimize (with prior, if > 0).
			setting_affineOptModeB = 0; //-1: fix. >=0: optimize (with prior, if > 0).
		}
		if(option==2)
		{
			printf("PHOTOMETRIC MODE WITH PERFECT IMAGES!\n");
			setting_photometricCalibration = 0;
			setting_affineOptModeA = -1; //-1: fix. >=0: optimize (with prior, if > 0).
			setting_affineOptModeB = -1; //-1: fix. >=0: optimize (with prior, if > 0).
            setting_minGradHistAdd=3;
		}
		return;
	}

	printf("could not parse argument \"%s\"!!\n", arg);
}




FullSystem* fullSystem = 0;
Undistort* undistorter = 0;
int frameID = 0;

void callback(const sensor_msgs::ImageConstPtr& msg_left,const sensor_msgs::ImageConstPtr& msg_right)
{
	cv_bridge::CvImagePtr cv_ptr_left = cv_bridge::toCvCopy(msg_left, sensor_msgs::image_encodings::MONO8);	
	assert(cv_ptr_left->image.type() == CV_8U);
	assert(cv_ptr_left->image.channels() == 1);
	cv_bridge::CvImagePtr cv_ptr_right = cv_bridge::toCvCop(msg_right,sensor_msgs::image_encodings::MONO8);	
	assert(cv_ptr_right->image.type() == CV_8U);
	assert(cv_ptr_right->image.channels() == 1);
	if(setting_fullResetRequested)
	{
		std::vector<IOWrap::Output3DWrapper*> wraps = fullSystem->outputWrapper;
		delete fullSystem;
		for(IOWrap::Output3DWrapper* ow : wraps) ow->reset();
		fullSystem = new FullSystem();
		fullSystem->linearizeOperation=false;
		fullSystem->outputWrapper = wraps;
	        if(undistorter->photometricUndist != 0)
	    	fullSystem->setGammaFunction(undistorter->photometricUndist->getG());
		setting_fullResetRequested=false;
	}

	MinimalImageB minImg_L((int)cv_ptr_left->image.cols, (int)cv_ptr_left->image.rows,(unsigned char*)cv_ptr_left->image.data);
	MinimalImageB minImg_R((int)cv_ptr_right->image.cols, (int)cv_ptr_right->image.rows,(unsigned char*)cv_ptr_right->image.data);
	
	ImageAndExposure* undistImg_L = undistorter->undistort<unsigned char>(&minImg_L, 1,0, 1.0f);
	ImageAndExposure* undistImg_R = undistorter->undistort<unsigned char>(&minImg_R, 1,0, 1.0f);
	fullSystem->addActiveFrame(undistImg_L, undistImg_R, frameID);
	frameID++;
	delete undistImg_L;
	delete undistImg_R;
}


int main( int argc, char** argv )
{
	ros::init(argc, argv, "stereo_dso_live");

	for(int i=1; i<argc;i++) parseArgument(argv[i]);

	setting_desiredImmatureDensity = 1000;
	setting_desiredPointDensity = 1200;
	setting_minFrames = 5;
	setting_maxFrames = 7;
	setting_maxOptIterations=4;
	setting_minOptIterations=1;
	setting_logStuff = false;
	setting_kfGlobalWeight = 1.3;


	//printf("MODE WITH CALIBRATION, but without exposure times!\n");

    	undistorter = Undistort::getUndistorterForFile(calib, gammaFile, vignetteFile);
	

   
	setGlobalCalib(
	    (int)undistorter->getSize()[0],
	    (int)undistorter->getSize()[1],
	    undistorter->getK().cast<float>());
	baseline = undistorter->getBl();


	fullSystem = new FullSystem();
	fullSystem->linearizeOperation=false;


	if(!disableAllDisplay)
	    fullSystem->outputWrapper.push_back(new IOWrap::PangolinDSOViewer(
	    		 (int)undistorter->getSize()[0],
	    		 (int)undistorter->getSize()[1]));


	if(useSampleOutput)
	fullSystem->outputWrapper.push_back(new IOWrap::SampleOutputWrapper());


	if(undistorter->photometricUndist != 0)
	fullSystem->setGammaFunction(undistorter->photometricUndist->getG());

	ros::NodeHandle nh;

	message_filters::Subscriber<sensor_msgs::Image> imageL_sub(nh, "image_L", 1);  
	message_filters::Subscriber<sensor_msgs::Image> imageR_sub(nh, "image_R", 1);  
	TimeSynchronizer<sensor_msgs::Image, sensor_msgs::Image> sync(imageL_sub, imageR_sub, 10);
	sync.registerCallback(boost::bind(&callback, _1, _2)); 
	ros::spin();

	for(IOWrap::Output3DWrapper* ow : fullSystem->outputWrapper)
	{
	ow->join();
	delete ow;
	}

	delete undistorter;
	delete fullSystem;

	return 0;
}

