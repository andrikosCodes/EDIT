#ifndef PROCESS_3D_H
#define PROCESS_3D_H

#pragma once

#define _SCL_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <iostream>
#include <direct.h>
#include <string>
#include <locale>
#include <codecvt>
#include <experimental/filesystem>
#include <opencv2/core/core.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

// ----------------------------VTK Triangulation----------------------
#include "vtk-9.0/vtkCardinalSpline.h"
#include "vtk-9.0/vtkPoints.h"
#include "vtk-9.0/vtkDoubleArray.h"
#include <vtk-9.0/vtkSmartPointer.h>
#include <vtk-9.0/vtkPolyData.h>
#include <vtk-9.0/vtkIntersectionPolyDataFilter.h>
#include <vtk-9.0/vtkLine.h>
#include <vtk-9.0/vtkOBBTree.h>
#include <vtk-9.0/vtkModifiedBSPTree.h>
#include <vtk-9.0/vtkCellArray.h>
#include <vtk-9.0/vtkTriangle.h>
#include <vtk-9.0/vtkSTLWriter.h>
#include <vtk-9.0/vtkSmoothPolyDataFilter.h>
#include <vtk-9.0/vtkPolyDataNormals.h>
#include <vtk-9.0/vtkBooleanOperationPolyDataFilter.h>
#include <vtk-9.0/vtkFillHolesFilter.h>
#include <vtk-9.0/vtkPolyDataMapper.h>
#include <vtk-9.0/vtksubdivisionFilter.h>
#include <vtk-9.0/vtkAdaptiveSubdivisionFilter.h>
#include <vtk-9.0/vtkLinearSubdivisionFilter.h>
#include <vtk-9.0/vtkButterflySubdivisionFilter.h>
#include <vtk-9.0/vtkLoopSubdivisionFilter.h>
//---------------------------------------------------------------------


using namespace std;
using namespace cv;


class process_3D {
public:
	//functions
	process_3D(); //constructor
	~process_3D(); //destructor

	enum STLType { BLADDER, SKIN, THICKNESS};

	string triangulation(vector<vector<Point2f>> point_cloud, STLType type); //string path
	string surface_smoothing(vtkSmartPointer<vtkPolyData> surface, STLType type, vector<Point2f> firstContour, vector<Point2f> lastContour); //string path

	void setStudyDir(string studyDir) {
		this->studyDir = studyDir;

		this->outputObjectsDir = this->studyDir + "/stl_objects";
		_mkdir(outputObjectsDir.c_str());
	}

	void openLogger() {
		this->loggertxt = this->studyDir + "/process_3D_logger.txt";
		this->logFile.open(this->loggertxt);
	}

	void closeLogger() {
		this->logFile.close();
	}

	//variables
	double xspace;
	double yspace;
	double distanceBetweenFrames;
	ofstream logFile;
	Point2f imageCenter;

private:
	//variable
	string studyDir;
	string outputObjectsDir;
	string loggertxt;	

	//functions
	string getSTLName(STLType type);

	void LoggerMessage(string message) {
		if (this->logFile.is_open()) this->logFile << " --> " << message << endl;
	}

};

#endif 