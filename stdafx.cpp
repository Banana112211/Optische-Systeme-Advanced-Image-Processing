// stdafx.cpp : Quelldatei, die nur die Standard-Includes einbindet.
// ConsoleApplication1.pch ist der vorkompilierte Header.
// stdafx.obj enthält die vorkompilierten Typinformationen.

#include "stdafx.h"

// TODO: Auf zusätzliche Header verweisen, die in STDAFX.H
// und nicht in dieser Datei erforderlich sind.

Mat rotate_eigene(Mat src, double angle)
{
	cv::Mat dst;
	
	// get rotation matrix for rotating the image around its center
	cv::Point2f center(src.cols / 2.0, src.rows / 2.0);
	cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
	// determine bounding rectangle
	cv::Rect bbox = cv::RotatedRect(center, src.size(), angle).boundingRect();
	// adjust transformation matrix
	rot.at<double>(0, 2) += bbox.width / 2.0 - center.x;
	rot.at<double>(1, 2) += bbox.height / 2.0 - center.y;
	warpAffine(src, dst, rot, bbox.size(), INTER_LINEAR,BORDER_CONSTANT,Scalar(255, 255, 255));

	return dst;
}

char recognize_letter(Mat binary_frame, Rect box, string plate_type,int correlation_coeff_maximum, templates_letter temp[], int number_loop, float angle)
{
	//Sicherstellen, dass Binärbild keine Grauwerte enthält
	threshold(binary_frame, binary_frame, 0, 255, CV_THRESH_OTSU);	
	//Wenn es sich um ein Autobahnschild handelt, Template invertieren (schwarz wird zu weiß und umgekehrt)
	if (plate_type == AUTOBAHNSCHILD)
	{
		Mat alloc = ~binary_frame;
		//alloc.setTo(255, binary_frame < 127);
		binary_frame = alloc;
	}
	//===Winkel verschieben!FUnktioniert nicht=======================
	//if (angle != 0) // Rotation soll also nur erfolgen falls verschiebung vorliegt
	//{
	//	imshow("binary_frame", binary_frame);
	//	imshow("binary_frame_bevor", binary_frame(box));
	//	binary_frame(box) = rotate_eigene(binary_frame(box), angle);
	//	binary_frame = rotate_eigene(binary_frame, angle);
	//}

	//=================

	double y = box.height; //Höhe der Bounding Box
	double alpha = (template_height / (y)); //Skalierungsfaktor 
	int new_width = round(alpha*box.width); //Neue Breite des Buchstabens normiert auf die Template-Höhe
	//Mat Datei für den Buchstaben in Größe der Buchstaben-Templates 
	Mat matLetterResized = Mat::zeros(template_height, template_width, CV_8UC1);
	cv::resize(binary_frame(box), matLetterResized, cv::Size(new_width, template_height));
	//Leeres Bild in der Größe der Templates initialisieren 
	Mat matEmpty = Mat::zeros(template_height, template_width, CV_8UC1);
	
	//Alle Pixel durchgehen und den Inhalt des gefundenen Buchstabens in das leere Bild speichern
	for (size_t row = 0; row < template_height; row++)
	{
		for (size_t column = 0; column < new_width; column++)
		{
			if (column < template_width)
			{
				matEmpty.at<char>(row, column) = matLetterResized.at<char>(row, column);
			}
		}
	}

	//imwrite("letter_resized.png", matEmpty);
	//Mit Template abgleichen---------------------------------------------------------------------
	double correlation_coeff_max = 0;//maximaler Korrelationskoeffizient
	char letter_result = ' ';//Ergebnis der Buchstabenklassifikation
	//Templates nacheinander durchgehen
	for (int template_nr = 0; template_nr < number_loop; template_nr++)
	{
		double correlation_coeff;	//Korrelationskoeffizient (wie gut stimmt Template mit Letter überein?)
		double pixel_nonzero;		//Gibt die Anzahl der Pixel an, deren Wert nicht 0 isst
		double template_size = template_height*template_width;		//Größe der Templates
		Mat res_xor = Mat::zeros(template_height, template_width, CV_8UC1); //Mat für XOR Bild initialisieren
		Mat res_xnor = Mat::zeros(template_height, template_width, CV_8UC1);//Mat für XNOR Bild initialisieren

		//Bitweise XNOR-Verknüpfung zwischen Template und Letter	
		Mat template_letter_Mat = temp[template_nr].template_letter_Mat.clone();
		bitwise_xor(template_letter_Mat, matEmpty, res_xor);
		bitwise_not(res_xor, res_xnor);//xor wird durch not zu xnor;
		//Zählen der Pixel, deren Wert im XOR-Image nicht 0 ist
		pixel_nonzero = countNonZero(res_xnor);
		//Korrelationskoeffizient ausrechenen (teilen durch Größe der Templates)
		correlation_coeff = pixel_nonzero / template_size;
		//Finden der größten Übereinstimmung bzw. Korrelation 
		if (correlation_coeff > correlation_coeff_max)
		{
			correlation_coeff_max = correlation_coeff;
			letter_result = temp[template_nr].letter;
			//imwrite("xnor.png", res_xnor);
		}
	}
	//Ergebnis in String wandeln
	stringstream letter_result_ss;
	letter_result_ss << letter_result;
	string letter_result_str = letter_result_ss.str();	
	//Abspeichern im Ordner letter, damit die Tenplates manuell optimiert werden können;
	imwrite("Letter/Unkown"+ letter_result_str + ".jpg", matEmpty); 
	//Wenn eine Übereinstimmung größer 85% erreicht wurde, wir der Buchstabe zurückgegeben
	if ((correlation_coeff_max*100) > correlation_coeff_maximum)
	{
		return letter_result;
	}
	else return ' ';
}


Mat dilatieren(Mat input, int dilation_elem, int dilation_size)
{
	Mat output;

	int dil_type;
	if (dilation_elem == 0) { dil_type = MORPH_RECT; }
	else if (dilation_elem == 1) { dil_type = MORPH_CROSS; }
	else if (dilation_elem == 2) { dil_type = MORPH_ELLIPSE; }

	Mat dil_element = getStructuringElement(dil_type,
		Size(dilation_size + 1, dilation_size + 1),
		Point(dilation_size, dilation_size));

	dilate(input, output, dil_element);

	return output;
}

Mat erodieren(Mat input, int erosion_elem, int erosion_size)
{
	Mat output;

	int er_type;
	if (erosion_elem == 0) { er_type = MORPH_RECT; }
	else if (erosion_elem == 1) { er_type = MORPH_CROSS; }
	else if (erosion_elem == 2) { er_type = MORPH_ELLIPSE; }

	Mat er_element = getStructuringElement(er_type,
		Size(erosion_size + 1, erosion_size + 1),
		Point(erosion_size, erosion_size));

	erode(input, output, er_element);

	return output;
}

void test(templates_letter temp[])
{
	imshow("test", temp[0].template_letter_Mat);
	waitKey(0);
}
int count_childs(vector<Vec4i> aiHierarchy, int i)
{
	int n_childs = 0;
	auto index_first_child = aiHierarchy[i][2];
	if (index_first_child >= 0) {
		auto index_next_child = aiHierarchy[index_first_child][0];
		while (index_next_child >= 0) {
			index_next_child = aiHierarchy[index_next_child][0];
			++n_childs;
		}
		return n_childs;
	}
	else {
		return 0;
	}
}


string maximum(double blue, double yellow, double white)
{
	if (blue > yellow)
	{
		if (blue > white)
		{
			return AUTOBAHNSCHILD;
		}
	}
	if (yellow > blue)
	{
		if (yellow > white)
		{
			return ORTSSCHILD_PFEILWEGWEISER;
		}
	}
	if (white > blue)
	{
		if (white > yellow)
		{
			return INNERORTS;
		}
	}
	return "Kein Schildtyp erkannt";
}
