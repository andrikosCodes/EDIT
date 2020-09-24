#ifndef PHOTOACOUSTIC_H
#define PHOTOACOUSTIC_H

#pragma once

#define _SCL_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <direct.h>
#include <iostream>
#include <numeric>
#include <string>
#include <locale>
#include <codecvt>
#include <experimental/filesystem>
#include "DicomReader.h"


#include <opencv2/core/core.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/opencv.hpp>


// ----------------------------VTK Triangulation----------------------
#include "vtk-9.0/vtkSmartPointer.h"
#include "vtk-9.0/vtkCardinalSpline.h"
#include "vtk-9.0/vtkPoints.h"
#include "vtk-9.0/vtkPolyData.h"
#include "vtk-9.0/vtkDoubleArray.h"
//---------------------------------------------------------------------

using namespace std;
using namespace cv;

class photoAcoustic {
public:
	photoAcoustic(); //constructor
	~photoAcoustic(); //destructor

	void exportImages(string dicomPath);
	void thicknessExtraction(int frame = 0); //frame = 0 means that the process will be performed for all photoAcoustic sequenence
	void finalizeAllThicknessContours(vector<vector<Point2f>> thicknessContours);
	void writeThicknessPoints();

	void setMainOutputDirectory(string mainOutputDirectory) {
		this->mainOutputDirectory = mainOutputDirectory;
	}

	string getOutputImagesDir() {
		return outputImagesDir;
	}

	string getStudyDir() {
		return studyDir;
	}

	vector<Mat> getImages() {
		return images;
	}

	Mat getIthImage(int i) {
		return images[i];
	}

	vector<double> getTags() {
		return tags;
	}

	string getDicomPath() {
		return dicomPath;
	}

	string getOutputPath() {
		return outputPath;
	}

	string getFilename() {
		return filename;
	}

	void openLogger() {
		this->loggertxt = this->studyDir + "/photoAcoustic_logger.txt";
		this->logFile.open(this->loggertxt);
	}

	void closeLogger() {
		this->logFile.close();
	}

	vector<vector<Point2f>> getThicknessPoints() {
		return thicknessPoints;
	}

	vector<Point2f> getContourForFix() {
		return this->contourForFix;
	}


	vector<double> getMeanThickness() {
		return meanThickness;
	}

	vector<vector<Point2f>> getFinalThicknessPoints() {
		return finalThicknessPoints;
	}

	//------------------------------------------THESE SHOULD BE GIVEN BY ULTRASOUND PART---------------------------
	void setInitialFrame(int initialFrame){
		this->initialFrame = initialFrame;
	}

	void setLastFrame(int lastFrame) {
		this->lastFrame = lastFrame;
	}

	void setlumenPoints(vector<vector<Point2f>> lumenPoints) {
		this->lumenPoints = lumenPoints;
	}

	void setContourForFix(vector<Point2f> contourForFix) {
		this->contourForFix = contourForFix;
	}

	//-------------------------------------------------------------------------------------------------------------

	void clearThicknessProcess() {
		vector<vector<Point2f>>().swap(this->thicknessPoints);
		vector<vector<Point2f>>().swap(this->lumenPoints);
		vector<vector<Point2f>>().swap(this->finalThicknessPoints);
		vector<Point2f>().swap(this->contourForFix);
		vector<double>().swap(meanThickness);
		LoggerMessage("The Segmentation process was repeated");
	}


	//variables
	bool enableLogging = true;
	ofstream logFile;

private:
	//functions
	enum totalSequenceOrCorrecton { TOTAL, CORRECTION};
	void creatDirectories();
	void process(int frame, totalSequenceOrCorrecton type);
	vector<Point2f> smoothContour(vector<Point2f> contour, int num_spline = 0);
	void interpolatePointsOfContourIsGoingToBeFixed();
	vector<Point2f> smoothCurve(vector<vector<vector<double>>> centerline, int num_spline = 0);
	Point2f findCenterOfContour(vector<Point2f> contour);

	void LoggerMessage(string message) {
		if (this->logFile.is_open()) this->logFile << " --> " << message << endl;
	}

	//variables
	string mainOutputDirectory;
	string dicomPath;
	string studyDir;
	string outputImagesDir;
	string outputSegmentedImagesDir;
	string outputPointsDir;
	string outputPath;
	string filename;
	vector<Mat> images;
	vector<double> tags;
	int initialFrame;
	int lastFrame;
	vector<vector<Point2f>> thicknessPoints;
	vector<vector<Point2f>> finalThicknessPoints;
	vector<double> meanThickness;
	vector<vector<Point2f>> lumenPoints;
	vector<Point2f> contourForFix;
	string loggertxt;

};



#endif