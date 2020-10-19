#pragma once

#include "ultrasound.h"

#include "morphsnakes.h"


struct sortBasedYCoordinate {
	bool operator() (cv::Point2f pt1, cv::Point2f pt2) { return (pt1.y < pt2.y); }
} sortBasedYCoordinate;


struct EuclideanDistance
{
	EuclideanDistance(const Point2f& _p) : p(_p) {}

	bool operator()(const Point2f& lhs, const Point2f& rhs) const
	{
		return sqrt(pow(lhs.x - p.x, 2) + pow(lhs.y - p.y, 2)) < sqrt(pow(rhs.x - p.x, 2) + pow(rhs.y - p.y, 2));
	}

private:
	Point2f p;
};

bool Longest(const std::vector<Point>& lhs, const std::vector<Point>& rhs)
{
	return lhs.size() < rhs.size();
}

ultrasound::ultrasound() {}
ultrasound::~ultrasound() {}

ultrasound::ultrasound(string dicomPath, string outputPath) {

	this->dicomPath = dicomPath;
	this->outputPath = outputPath;

	string nameAndExt = experimental::filesystem::path(dicomPath).filename().generic_string();

	size_t lastindex = nameAndExt.find_last_of(".");
	this->filename = nameAndExt.substr(0, lastindex);

	
	wstring dcm_str(dicomPath.length(), L' ');
	wstring out_str(dicomPath.length(), L' ');
	copy(dicomPath.begin(), dicomPath.end(), dcm_str.begin());
	copy(outputPath.begin(), outputPath.end(), out_str.begin());

	CDicomReader *reader = new CDicomReader();
	this->tags = reader->GetDicomInfo(dcm_str.c_str()); //read dicom tags
    
	this->images = reader->dcmimage_Mat(dcm_str.c_str(), tags[2], tags[3], tags[4], tags[5]);
	//reader->dcmimage_bmp(dcm_str.c_str(), out_str.c_str()); // read the given dicom and save images into the given folder

	reader->~CDicomReader();
}

void ultrasound::exportImages(string dicomPath) {
	
	this->dicomPath = dicomPath;
	string nameAndExt = experimental::filesystem::path(dicomPath).filename().generic_string();

	size_t lastindex = nameAndExt.find_last_of(".");
	this->filename = nameAndExt.substr(0, lastindex);
	//create dir for all outputs

	wstring dcm_str(dicomPath.length(), L' ');
	copy(dicomPath.begin(), dicomPath.end(), dcm_str.begin());

	//clear folder of images
	for (const auto& entry : std::experimental::filesystem::directory_iterator(this->outputImagesDir))
		std::experimental::filesystem::remove_all(entry.path());
	
	//clear memory of images
	vector<Mat>().swap(images);

	CDicomReader *reader = new CDicomReader();
	this->tags = reader->GetDicomInfo(dcm_str.c_str()); //read dicom tags
	this->images = reader->dcmimage_Mat(dcm_str.c_str(), this->outputImagesDir, tags[2], tags[3], tags[4], tags[5]);
	//reader->dcmimage_bmp(dcm_str.c_str(), out_str.c_str()); // read the given dicom and save images into the given folder

	reader->~CDicomReader();

}


void ultrasound::creatDirectories() {
	//Create all neccessary directories
	this->studyDir = this->mainOutputDirectory; //this->filename;
	_mkdir(studyDir.c_str());

	this->outputImagesDir = studyDir + separator() + "ultrasound_images";
	_mkdir(outputImagesDir.c_str());

	this->outputSegmentedImagesDir = studyDir + separator() +  "ultrasound_segmented_images";
	_mkdir(outputSegmentedImagesDir.c_str());

	this->outputPointsDir = studyDir + separator() + "ultrasound_points";
	_mkdir(outputPointsDir.c_str());

	/*this->loggertxt = this->studyDir + "/logger.txt";
	if(this->enableLogging) this->logFile.open(this->loggertxt);*/
}



template<class T> morphsnakes::NDImage<T, 2> cimg2ndimage(CImg<T>& img)
{
	morphsnakes::Shape<2> shape = { img.height(), img.width() };
	morphsnakes::Stride<2> stride = { img.width() * sizeof(T), sizeof(T) };

	return morphsnakes::NDImage<T, 2>(img.data(), shape, stride);
}

void ultrasound::processing(int initialFrame, int lastFrame, Point clickPoint) {

	vector<vector<Point2f>>().swap(this->lumenPoints);
	vector<vector<Point2f>>().swap(this->skinPoints);
	vector<double>().swap(this->lumenArea);

	this->initialFrame = initialFrame;
	this->lastFrame = lastFrame;
	CImg<unsigned char> embedding;

	vector<Mat> imageOfinterest;
	for (int i = initialFrame; i <= lastFrame; i++) { //initialFrame - 1
		LoggerMessage(string("Working on Frame: " + to_string(i)));
		Mat lumen;
		if (applyEqualizeHist) {
			equalizeHist(this->images[i], lumen);
		}
		else {
			lumen = this->images[i];
		}

		CImg<double>  imgC = *cvImgToCImg(lumen);
		

		//Morphological ACWE
		if (i == this->initialFrame || i >=this->lastFrame-5) { // maybe 5 has to be configurable from gui
			//in the first as well as in the final 5 frames we apply circle levelset image and 200 repeats
			CImg<unsigned char> embedding = circle_levelset(imgC.height(), imgC.width(), { clickPoint.y , clickPoint.x }, this->levelsetSize);
			morphsnakes::MorphACWE<double, 2> macwe(cimg2ndimage(embedding), cimg2ndimage(imgC), this->smoothing, this->lamda1, this->lamda2);
			for (int i = 0; i < 200; ++i) {
				macwe.step();
			}
			//Back to Mat
			lumen = CImgtoCvImg((embedding * 255));
			this->levelsetImage = embedding;
		}
		else {
			//while in the rest images we use the segmentation of the previous image and 50 repeats
			morphsnakes::MorphACWE<double, 2> macwe(cimg2ndimage(this->levelsetImage), cimg2ndimage(imgC), this->smoothing, this->lamda1, this->lamda2);
			for (int i = 0; i < this->repeats; ++i) {
				macwe.step();
			}
			//Back to Mat
			lumen = CImgtoCvImg((this->levelsetImage * 255));
		}

		//Canny(lumen, lumen, 0, 0, 3);
	   //threshold(lumen, lumen, 100, 255, THRESH_BINARY);

		//---------------------------------------------Keep the longest contour--------------------------------------
		threshold(lumen, lumen, 100, 255, THRESH_BINARY);
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		Canny(lumen, lumen, 0, 0, 3);//with or without, explained later.
		findContours(lumen, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

		Mat drawing = Mat::zeros(lumen.size(), lumen.type());

		Scalar color(255, 0, 0);
		
		//Make sure that the longest contour is the 0
		drawContours(drawing, contours, findLongestVector(contours), color);

		lumen = drawing;

		//find white pixels and calculte their center and store points into vector!
		vector<Point2f> *lumenPoints = new vector<Point2f>();
		Point2f *lumenCenter = new Point2f();
		Point *highestWhitePixel = new Point();
		ResultOfProcess poitsWereFound = centerAndPointsOfContour(lumen, lumenPoints, lumenCenter, highestWhitePixel);
		
		//update the click point! This point is used for the segmentation of the final (almost 5) frames
		clickPoint = { (int)lumenCenter->x, (int)lumenCenter->y };


		if (poitsWereFound == ResultOfProcess::SUCCESS) {
			//int pix = findDistanceLumenFromSkin(images[i], highestWhitePixel);

			sortUsingPolarCoordinates(lumenPoints, i, lumenCenter, images[i], 0);

		}

		vector<Point2f>().swap(*lumenPoints);
		vector<vector<Point>>().swap(contours);
		vector<Vec4i>().swap(hierarchy);
		drawing.release();
		lumen.release();
	}

}


void ultrasound::finalizeAllBladderContours(vector<vector<Point2f>> points) {
	vector<vector<Point2f>>().swap(lumenPoints);
	for (int i = 0; i < points.size(); i++) {
	
		points[i] = smoothContour(points[i], 100);

		Point2f center = accumulate(points[i].begin(), points[i].end(), Point2f(0.0, 0.0));
		center.x /= points[i].size();
		center.y /= points[i].size();

		transform(points[i].begin(), points[i].end(), points[i].begin(), std::bind2nd(std::minus<Point2f>(), center));


		Mat xpts(points[i].size(), 1, CV_32F, &points[i][0].x, 2 * sizeof(float));
		Mat ypts(points[i].size(), 1, CV_32F, &points[i][0].y, 2 * sizeof(float));
		Mat magnitude, angle;
		cartToPolar(xpts, ypts, magnitude, angle);
		vector<Point2f> polarXY, sortedPolarXY;
		vector<double> polarAngles;
		for (int j = 0; j < points[i].size(); j++) {
			polarXY.push_back(Point2f(magnitude.at<Float32>(j), angle.at<Float32>(j)));
			polarAngles.push_back(angle.at<Float32>(j));
		}
		auto min = std::min_element(polarAngles.begin(), polarAngles.end());

		int minIndex = distance(polarAngles.begin(), min);
	
		if (minIndex != 0) {
			sortedPolarXY.insert(sortedPolarXY.end(), polarXY.begin() + minIndex, polarXY.end());
			polarXY.erase(polarXY.begin() + minIndex, polarXY.end());
		}
		sortedPolarXY.insert(sortedPolarXY.end(), polarXY.begin(), polarXY.end());
		
		Mat mag(sortedPolarXY.size(), 1, CV_32F, &sortedPolarXY[0].x, 2 * sizeof(float));
		Mat ang(sortedPolarXY.size(), 1, CV_32F, &sortedPolarXY[0].y, 2 * sizeof(float));
		Mat xnew, ynew;
		polarToCart(mag, ang, xnew, ynew);
		vector<Point2f> pp;

		for (int j = 0; j<points[i].size(); j++) {
			pp.push_back(Point2f(xnew.at<Float32>(j), ynew.at<Float32>(j)));
		}

		transform(pp.begin(), pp.end(), pp.begin(), std::bind2nd(std::plus<Point2f>(), center));
		this->lumenPoints.push_back(pp);
	
		xpts.release();
		ypts.release();
		mag.release();
		ang.release();
		xnew.release();
		ynew.release();
		vector<double>().swap(polarAngles);
		vector<Point2f>().swap(polarXY);
		vector<Point2f>().swap(sortedPolarXY);
		vector<Point2f>().swap(pp);
	}
	vector<vector<Point2f>>().swap(points);
}

void ultrasound::extractSkinPoints(vector<vector<Point2f>> bladderPoints) {

	vector<vector<Point2f>>().swap(skinPoints);
	finalizeAllBladderContours(bladderPoints);

	this->skinPoints = this->getlumenPoints();
	int imageCount = this->initialFrame;

	for (int i = 0; i< skinPoints.size(); i++) {

		Point2f center = accumulate(skinPoints[i].begin(), skinPoints[i].end(), Point2f(0.0, 0.0));
		center.x /= skinPoints[i].size();
		center.y /= skinPoints[i].size();

		transform(skinPoints[i].begin(), skinPoints[i].end(), skinPoints[i].begin(), std::bind2nd(std::minus<Point2f>(), center));

		Mat xpts(skinPoints[i].size(), 1, CV_32F, &skinPoints[i][0].x, 2 * sizeof(float));
		Mat ypts(skinPoints[i].size(), 1, CV_32F, &skinPoints[i][0].y, 2 * sizeof(float));
		Mat magnitude, angle;
		cartToPolar(xpts, ypts, magnitude, angle);

		vector<Point2f> SkinPolarXY;
		int index = 0;
		for (int j = 0; j < skinPoints[i].size(); j++) {
			if (abs((angle.at<Float32>(j) * 180 / CV_PI) - 270) < 5) {
				break;
			}
			index++;
		}

		for (int j = 0; j < skinPoints[i].size(); j++) {
			SkinPolarXY.push_back(Point2f(magnitude.at<Float32>(j), angle.at<Float32>(j)));
		}

	Mat image = images[imageCount].clone();
	medianBlur(image, image, 9);
	GaussianBlur(image, image, Size(9, 9), 0);
	threshold(image, image, 100, 255, THRESH_BINARY);
	
	int distBladderSkin = 0;

	//for (int j = skinPoints[i][index].y + center.y; j >= 0; j--) {
	for (int j = 0; j < skinPoints[i][index].y + center.y; j++) {
		//if ((int)image.at<uchar>(j, skinPoints[i][index].x + center.x) == 255) {
		if ((int)image.at<uchar>(j, skinPoints[i][index].x + center.x) == 0) {
			distBladderSkin++;
		}
		else {
			break; //find the first white (255) pixel
		}
	}

	int dist = skinPoints[i][index].y + center.y - distBladderSkin;

	//cout << dist << endl;

	for (int j = 0; j < SkinPolarXY.size(); j++) {
		SkinPolarXY[j].x += dist;
	}

	Mat mag(SkinPolarXY.size(), 1, CV_32F, &SkinPolarXY[0].x, 2 * sizeof(float));
	Mat ang(SkinPolarXY.size(), 1, CV_32F, &SkinPolarXY[0].y, 2 * sizeof(float));
	Mat xnew, ynew;
	polarToCart(mag, ang, xnew, ynew);
	vector<Point2f> pp;

	//cout << SkinPolarXY.size() << endl;
	for (int j = 0; j < SkinPolarXY.size(); j++) {
		pp.push_back(Point2f(xnew.at<Float32>(j) + center.x, ynew.at<Float32>(j) + center.y));
		//images[imageCount].at<uchar>(round(ynew.at<Float32>(j) + center.y), round(xnew.at<Float32>(j) + center.x)) = 255;
	}

	vector<Point2f>().swap(this->skinPoints[i]);

	this->skinPoints[i] = pp;


	imageCount++;

	//free memory
	image.release();
	xpts.release();
	ypts.release();
	mag.release();
	ang.release();
	xnew.release();
	ynew.release();
	vector<Point2f>().swap(SkinPolarXY);
	vector<Point2f>().swap(pp);
	}
}


void ultrasound::writePointsAndImages() {
	Mat img;
	for (int i = 0; i <= (this->lastFrame - this->initialFrame); i++) {
		String lumenhtxt = this->outputPointsDir + "/" + to_string(i + this->initialFrame) + ".txt";
		ofstream filelumen;
		filelumen.open(lumenhtxt);
		img = this->images[i + this->initialFrame];
		for (int j = 0; j < this->lumenPoints[i].size(); j++) { //p.size()
				filelumen << this->lumenPoints[i][j].x << " " << this->lumenPoints[i][j].y << endl;
				img.at<uchar>(round(this->lumenPoints[i][j].y), round(this->lumenPoints[i][j].x)) = 255;
		}

		for (int j = 0; j < this->skinPoints[i].size(); j++) { //p.size()
			img.at<uchar>(round(this->skinPoints[i][j].y), round(this->skinPoints[i][j].x)) = 255;
		}

		String pathbmp = this->outputSegmentedImagesDir + "/" + to_string(i+ this->initialFrame) + ".bmp";
		imwrite(pathbmp, img);
		filelumen.close();
	}
	img.release();
}

//--------------------------------------------------C O R E - F U N C T I O N S--------------------------------------------------------------


void ultrasound::sortUsingPolarCoordinates(vector<Point2f> *p, int iter, Point2f *center, Mat image, int skinDistance) {
	
	transform(p->begin(), p->end(), p->begin(), std::bind2nd(std::minus<Point2f>(), *center));

	Mat xpts(p->size(), 1, CV_32F, &p->at(0).x, 2 * sizeof(float));
	Mat ypts(p->size(), 1, CV_32F, &p->at(0).y, 2 * sizeof(float));

	Mat magnitude, angle;
	cartToPolar(xpts, ypts, magnitude, angle);

	vector<Point2f> polarXY;
	for (int i = 0; i < p->size(); i++) {
		polarXY.push_back(Point2f(magnitude.at<Float32>(i), angle.at<Float32>(i)));
	}

	sort(polarXY.begin(), polarXY.end(), sortBasedYCoordinate);

	Mat mag(polarXY.size(), 1, CV_32F, &polarXY[0].x, 2 * sizeof(float));
	Mat ang(polarXY.size(), 1, CV_32F, &polarXY[0].y, 2 * sizeof(float));

	Mat xnew, ynew, xnewSkin, ynewSkin;
	polarToCart(mag, ang, xnew, ynew);

	vector<Point2f> pp;

	for (int i = 0; i < p->size(); i++) {
		pp.push_back(Point2f(xnew.at<Float32>(i), ynew.at<Float32>(i)));
	}
	vector<Point2f> smoothed = smoothContour(sortBasedEuclideanDistance(pp), 0); //num_spline = 50;


	transform(smoothed.begin(), smoothed.end(), smoothed.begin(), std::bind2nd(std::plus<Point2f>(), *center));

	this->lumenPoints.push_back(smoothed);

	LoggerMessage("Bladder borders were detected successfully!");
	
	xpts.release();
	ypts.release();
	mag.release();
	ang.release();
	xnew.release();
	ynew.release();
	xnewSkin.release();
	ynewSkin.release();

	vector<Point2f>().swap(polarXY);
	vector<Point2f>().swap(pp);
}


ultrasound::ResultOfProcess ultrasound::centerAndPointsOfContour(Mat processed, vector<Point2f> *points, Point2f *center, Point *highestWhitePixel)
{
	Mat whitePixels;   // output, locations of non-zero pixels
	
	findNonZero(processed, whitePixels);

	if (whitePixels.empty()) {
		LoggerMessage("No Bladder points were detected");
		return ResultOfProcess::FAILURE;
	} 

	highestWhitePixel->x = whitePixels.at<Point>(0).x;
	highestWhitePixel->y = whitePixels.at<Point>(0).y;

	center->x = 0.0;
	center->y = 0.0;

	for (int i = 0; i < whitePixels.total(); i++) {
		Point lumen_p = Point(whitePixels.at<Point>(i).x, whitePixels.at<Point>(i).y);
			points->push_back(Point2f(whitePixels.at<Point>(i).x, whitePixels.at<Point>(i).y));
			center->x += whitePixels.at<Point>(i).x;
			center->y += whitePixels.at<Point>(i).y;
	}
	center->x /= whitePixels.total();
	center->y /= whitePixels.total();


	whitePixels.release();

	return ResultOfProcess::SUCCESS;
}



CImg<unsigned char>* ultrasound::cvImgToCImg(Mat &cvImg)
{
	CImg<unsigned char> * result = new CImg<unsigned char>(cvImg.cols, cvImg.rows);

	for (int x = 0; x < cvImg.cols; ++x)
		for (int y = 0; y < cvImg.rows; ++y)
			(*result)(x, y) = cvImg.at<uchar>(y, x);

	return result;
}

Mat ultrasound::CImgtoCvImg(CImg<unsigned char> img)
{
	Mat result = Mat(img.height(), img.width(), CV_8U);

	for (int x = 0; x < result.cols; ++x)
		for (int y = 0; y < result.rows; ++y)
			result.at<uchar>(y, x) = (img)(x, y);

	return result;
}


CImg<unsigned char> ultrasound::circle_levelset(int height, int width, const array<int, 2>& center, double radius, double scalerow)
{
	CImg<unsigned char> res(width, height);
	for (int i = 0; i < height; ++i)
	{
		for (int j = 0; j < width; ++j)
		{
			int diffy = (i - center[0]);
			int diffx = (j - center[1]);
			res(j, i) = (radius*radius - (diffx*diffx + diffy * diffy)) > 0;
		}
	}
	return res;
}

//sorted.push_back(sorted[0]);

vector<Point2f> ultrasound::sortBasedEuclideanDistance(vector<Point2f> points) {

	vector<Point2f> sorted;
	Point2f center = accumulate(points.begin(), points.end(), Point2f(0.0, 0.0));

	center.x /= points.size();
	center.y /= points.size();


	int count = 0;
	double dist = sqrt(pow(points[count].x - center.x, 2) + pow(points[count].y - center.y, 2));
	//keep as first pixel this has distance from center greater than 10. This was a problem due to ultrasound artifact
	while (dist < 10) {
		count++;
		dist = sqrt(pow(points[count].x - center.x, 2) + pow(points[count].y - center.y, 2));
	}

	sorted.push_back(points[count]);
	points.erase(points.begin() + count);

	int range = floor(points.size() / 3);

	while (!points.empty()) {
		Point2f po = sorted.back();
		 (points.size() > range) ? 
			 sort(points.begin(), points.begin() + range, EuclideanDistance(po)) : 
			 sort(points.begin(), points.begin() + range--, EuclideanDistance(po));
		 dist = sqrt(pow(points[0].x - po.x, 2) + pow(points[0].y - po.y, 2));
		 if (dist < 50)
			 sorted.push_back(points[0]);
		points.erase(points.begin());
	}

	//sorted.push_back(sorted[0]);

	LoggerMessage("Bladder points were sorted clickwise successfully!");
	
	return sorted;
}

int ultrasound::findLongestVector(vector<vector<Point>> vec) {
	int max = 0;
	for (int i = 1; i < vec.size(); i++) {
		if (vec[i].size() > vec[max].size()) {
			max = i;
		}
	}
	return max;
}
vector<Point2f> ultrasound::smoothContour(vector<Point2f> contour, int num_spline) {

	vector<vector<vector<double>>> cl_3D;
	vector<vector<double>> temp_cl_3D;
	vector<double> xx;
	vector<double> yy;
	vector<double> zz;

	for (int i = 0; i < contour.size(); i++) {
		vector<double> temp_temp_cl_3D;
		temp_temp_cl_3D.push_back(contour[i].x);
		temp_temp_cl_3D.push_back(contour[i].y);
		temp_cl_3D.push_back(temp_temp_cl_3D);
	}
	cl_3D.push_back(temp_cl_3D);

	contour = smoothCurve(cl_3D, num_spline);

	//free some memory
	vector<vector<vector<double>>>().swap(cl_3D);
	vector<vector<double>>().swap(temp_cl_3D);
	vector<double>().swap(xx);
	vector<double>().swap(yy);
	vector<double>().swap(zz);

	return contour;
}

// Cubic spline interpolation to smooth centerline
vector<Point2f> ultrasound::smoothCurve(vector<vector<vector<double>>> centerline, int num_spline) { //vector<vector<double>>
	vector<vector<vector<double>>> smoothCtr;
	for (int ii = 0; ii < centerline.size(); ++ii) {
		// Read centerline into vtk points
		vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
		for (int i = 0; i < centerline[ii].size(); ++i) {
			points->InsertNextPoint(centerline[ii][i][0], centerline[ii][i][1], centerline[ii][i][1]); // ATTENTION TO X-Y ORDER
		}
		// Create polydata using the set of path points
		vtkSmartPointer<vtkPolyData> data = vtkSmartPointer<vtkPolyData>::New();
		data->SetPoints(points);
		double resamplingParameter = 1; // 10 Sampling resolution
		double distance = 0; // distance from start to end of centerline
							 // Calculate distance
		for (int i = 0; i < data->GetNumberOfPoints() - 1; ++i) {
			double p1[3], p2[3];
			data->GetPoint(i, p1);
			data->GetPoint(i + 1, p2);
			double diff_x = pow(p2[0] - p1[0], 2.0);
			double diff_y = pow(p2[1] - p1[1], 2.0);
			double diff_z = pow(p2[2] - p1[2], 2.0);
			distance += sqrt(diff_x + diff_y + diff_z);
		}
		// Create arrays to store samples for each axis
		vtkSmartPointer<vtkDoubleArray> x, y, z;
		x = vtkSmartPointer<vtkDoubleArray>::New();
		x->SetNumberOfComponents(1);
		y = vtkSmartPointer<vtkDoubleArray>::New();
		y->SetNumberOfComponents(1);
		z = vtkSmartPointer<vtkDoubleArray>::New();
		z->SetNumberOfComponents(1);
		double p0[3];
		// Sample centerline and insert points to respectable arrays
		for (int i = 0; i < data->GetNumberOfPoints(); i = i + resamplingParameter) {
			data->GetPoint(i, p0);
			x->InsertNextValue(p0[0]);
			y->InsertNextValue(p0[1]);
			z->InsertNextValue(p0[2]);
		}
		// Create a spline object for each axis
		vtkSmartPointer<vtkCardinalSpline> xspline = vtkSmartPointer<vtkCardinalSpline>::New();
		xspline->ClosedOff(); // Curve is not closed
		vtkSmartPointer<vtkCardinalSpline> yspline = vtkSmartPointer<vtkCardinalSpline>::New();
		yspline->ClosedOff();
		vtkSmartPointer<vtkCardinalSpline> zspline = vtkSmartPointer<vtkCardinalSpline>::New();
		zspline->ClosedOff();
		double sampledPoints = (double)x->GetNumberOfTuples();
		// Spline interpolation
		// t represents the position of each point on the curve
		// e.g.: 0: start point, 1: end point, 0.5: mid point
		for (int i = 0; i < sampledPoints - 1; ++i) {
			double t = (double)i / sampledPoints;
			double xpoint = x->GetTuple1(i);
			xspline->AddPoint(t, xpoint); // x axis
			double ypoint = y->GetTuple1(i);
			yspline->AddPoint(t, ypoint); // y axis
			double zpoint = z->GetTuple1(i);
			zspline->AddPoint(t, zpoint); // z axis
		}
		// Set final point for interpolation
		double xpoint = x->GetTuple1(sampledPoints - 1);
		xspline->AddPoint(1, xpoint);
		double ypoint = y->GetTuple1(sampledPoints - 1);
		yspline->AddPoint(1, ypoint);
		double zpoint = z->GetTuple1(sampledPoints - 1);
		zspline->AddPoint(1, zpoint);
		double evaluationPoints = 10000, dt = 1 / evaluationPoints; // 10000
		// Create an array to store interpolated points
		double **p = new double *[evaluationPoints];
		for (int i = 0; i < evaluationPoints; ++i) {
			p[i] = new double[3];
		}
		int i = 0;
		double t = 0;
		// Evaluate splines at small intervals and store points
		while (i < evaluationPoints) {
			p[i][0] = xspline->Evaluate(t);
			p[i][1] = yspline->Evaluate(t);
			p[i][2] = zspline->Evaluate(t);
			i++; t = t + dt;
		}
		// Calculate distance between the spline points
		double smoothed_distance = 0;
		double * newdst = new double[evaluationPoints - 1];
		for (int i = 0; i < evaluationPoints - 1; ++i) {
			double diff_x = pow(p[i + 1][0] - p[i][0], 2.0);
			double diff_y = pow(p[i + 1][1] - p[i][1], 2.0);
			double diff_z = pow(p[i + 1][2] - p[i][2], 2.0);
			smoothed_distance += sqrt(diff_x + diff_y + diff_z);
			newdst[i] = sqrt(diff_x + diff_y + diff_z);
		}
		//cout << smoothed_distance << " ";
		double planeDistance;
		if (num_spline == 0) {
			 planeDistance = 13; // sampling distance 54
		}
		else {
			 planeDistance = ceil(smoothed_distance) / num_spline; //ceil(smoothed_distance) / num_spline
		}
		//double planeDistance = ceil(smoothed_distance) / num_spline;//0.5; // sampling distance 54
		//double planeDistance = ceil(dims[0] * 15 / 1024);

		double interval = 0;
		vector<int> ind; // vector to store indexes of equidistant points
		for (int i = 0; i < evaluationPoints - 1; ++i) {
			interval += newdst[i];
			if (interval >= planeDistance - 0.01) { // select points with equal distance on curve
				interval = 0;
				ind.push_back(i);
			}
		}
		// Store equidistant points
		vector<vector<double>> equidistantPoints;
		for (int i = 0; i < ind.size(); ++i) {
			vector<double> point;
			point.push_back(p[ind[i]][0]); point.push_back(p[ind[i]][1]); point.push_back(p[ind[i]][2]);
			equidistantPoints.push_back(point);
		}
		smoothCtr.push_back(equidistantPoints); // store interpolated centerline

		vector<vector<double>>().swap(equidistantPoints);
		vector<int>().swap(ind);
	}


	//convert point from double to Point (opencv)
	vector<Point2f> t;

	t.push_back(Point(centerline[0][0][0], centerline[0][0][1])); //push the first point into the vector

	for (int i = 0; i < smoothCtr[0].size(); i++) {
		t.push_back(Point2f(smoothCtr[0][i][0], smoothCtr[0][i][1]));
	}

	//free memory
	vector<vector<vector<double>>>().swap(smoothCtr);

	return t;// smoothCtr[0]; //return only the first centerline 
}


