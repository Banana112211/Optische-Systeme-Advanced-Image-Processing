//INCLUDES============================================================================
#include "stdafx.h"


//Variablen und Parameter==============================================================
//Mat Dateien
Mat original_frame;//Originalbild
Mat gray_frame;//Graubild
Mat gauss_frame;//Bild nach Gaussfilterung
Mat binary_frame;//Binärbild
Mat canny_frame;//Cannybild
Mat binary_dilatet_frame;//Dilatiertes Binärbild
Mat binary_dilatet_eroded_frame;//Erodiertes Binärbild
unsigned int microseconds;

//Variablen
vector<vector<Point> > contours;//Konturvektor
vector<Vec4i> hierarchy;//Hierarchievektor
vector<vector<Point> > letter_contours;//Konturen der Buchstaben in den Schildern
vector<Rect> letter_boxes;
vector<Rect> sorted_letter_boxes;
string text;

//Parameter
int adaptive_threshold_subtract = 2;//Anpassung des adaptive Threshold
int dilation_elem = 0;//Dilationsmodus
int dilation_size = 0;//Dilationsgröße
int erosion_elem = 2;//Erosionsmodus
int erosion_size = 0;//Erosionsgröße
int max_thresh = 255;
int block_size = 5;
int Optimaler_Einstellung = 0;
int rForm_min = 30;//minimaler Formfaktor*100
int rForm_max = 80;//maximaler Formfaktor*100
int Webcam_ein = 0;//0=Es wird ein gespeichertes Bild verarbeitet. 1=Es werden Frames von der Webcam eingelesen
int Buchstab_Erk = 1;//0=Keine Buchstabenerkennung, 1=Buchstabenerkennung
int n_childs = 0;//Anzahl der Childs, die eine Kontur hat
int correlation_coeff_maximum=85;/////Wenn eine Übereinstimmung größer 85% erreicht wurde, wir der Buchstabe zurückgegeben
int binare_Mode = 0; // Auswahl des Methode fuer Binarisierung des Images. 0=OTSU, 1= adaptiveThreshold
bool n_childs_smaller_min=0;//Gibt an, ob Anzahl Kinder <5
bool size_roadsign_bigger_minsize = 0;//Gibt an, ob minimale Schildgröße 1% des Gesamtbildes
bool size_roadsign_not_fullscreen = 0;//Gibt an, ob maximale Schildgröße 95% des Gesamtbildes
bool form_factor_smaller_rFormmax=0;//Gibt an, ob Formfaktor < rForm_max (Schieberegler)
bool form_factor_bigger_rFormmin = 0;//Gibt an, ob Formfaktor > rForm_min (Schieberegler)

//Verbessung:
// - Auslagerung des Datentyps
// - Bitmap
// - Perspektiven
Mat rotate(Mat src, double angle)
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
	cv::warpAffine(src, dst, rot, bbox.size());
	return dst;
}
//----------------------------------------------------------------------------------------------------------
//Main
//----------------------------------------------------------------------------------------------------------
int main(int, char**)
{
	//1.Step:Trackbars initialisieren=================================================================
	// Einstellungsfenster
	namedWindow("Einstellungen", CV_WINDOW_FREERATIO);
	//Slider fürs BINARYTHRESHHOLD	
	createTrackbar("Adap.ThreshS", "Einstellungen", &adaptive_threshold_subtract, 32);
	//Slider fürs Dilatieren
	createTrackbar("Dil.Modus", "Einstellungen", &dilation_elem, 6);
	createTrackbar("DilSize", "Einstellungen", &dilation_size, 21);
	//Slider fürs Erodieren
	createTrackbar("Er.Modus", "Einstellungen", &erosion_elem, 2);

	createTrackbar("Er.Size", "Einstellungen", &erosion_size, 21);
	//Slider für den Canny-Filter
	createTrackbar("rForm_min", "Einstellungen", &rForm_min, 100);
	createTrackbar("rForm_max", "Einstellungen", &rForm_max, 100);
	createTrackbar("Webcam", "Einstellungen", &Webcam_ein, 1);
	createTrackbar("Buchstaben erkennen", "Einstellungen", &Buchstab_Erk, 1); 
	createTrackbar("Correlation_coeff_maximum", "Einstellungen", &correlation_coeff_maximum, 100);
	createTrackbar("Binar_Mode", "Einstellungen", &binare_Mode, 5);
	createTrackbar("Optimaler_Einstellung", "Einstellungen", &Optimaler_Einstellung, 3);
	//1.1.Step:Einmalige init. der Templates=============
	char Letter_Lookup[63] = { 'z',	'y','x','w','v','u','9','8','7','6','5','4','3','2','1','s','r','q','p','o','n'
		,'m','g','e','c','a','j','i','t','l','k','h','f','d','b','0','Z','Y','X','W','V','U','T','S','R','Q','P','O','N',
		'M','L','K','J','i','H','G','F','E','D','C','B','A', ' ' };

	templates_letter temp_letter[63]; //Instanz eines object structs
	int number_loop = sizeof(Letter_Lookup);
	for (int template_nr = 0; template_nr <63; template_nr++)
	{
		temp_letter[template_nr].letter = Letter_Lookup[template_nr];
		stringstream template_nr_ss;
		template_nr_ss << template_nr;
		string template_nr_string = template_nr_ss.str();
		Mat template_letter_unbearbeitet = Mat::zeros(template_height, template_width, CV_8UC1);//Mat für XNOR Bild initialisieren
		template_letter_unbearbeitet = imread("Template/" + template_nr_string + ".jpg", CV_8UC1);
		//Sicherstellen, dass Binärbild keine Grauwerte enthält
		// Dadurch setzte er alles was nicht schwarz ist auf weis
		Mat Mask;

		cv::inRange(template_letter_unbearbeitet, 50, 255, Mask);
		template_letter_unbearbeitet.setTo(255, Mask);
		//======
		threshold(template_letter_unbearbeitet, template_letter_unbearbeitet, 0, 255, CV_THRESH_OTSU);
		temp_letter[template_nr].template_letter_Mat = template_letter_unbearbeitet;
	}
	//=============
	for (;;)
	{
		//1.2Step: Aktualisierung der Werte des Trackbars=================================================================
		namedWindow("Einstellungen", CV_WINDOW_FREERATIO);
		//Slider fürs BINARYTHRESHHOLD	
		createTrackbar("Adap.ThreshS", "Einstellungen", &adaptive_threshold_subtract, 16);
		//Slider fürs Dilatieren
		createTrackbar("Dil.Modus", "Einstellungen", &dilation_elem, 6);
		createTrackbar("DilSize", "Einstellungen", &dilation_size, 21);
		//Slider fürs Erodieren
		createTrackbar("Er.Modus", "Einstellungen", &erosion_elem, 2);
		createTrackbar("Er.Size", "Einstellungen", &erosion_size, 21);
		//Slider für den Canny-Filter
		createTrackbar("rForm_min", "Einstellungen", &rForm_min, 100);
		createTrackbar("rForm_max", "Einstellungen", &rForm_max, 100);
		createTrackbar("Webcam", "Einstellungen", &Webcam_ein, 1);
		createTrackbar("Buchstaben erkennen", " Einstellungen", &Buchstab_Erk, 1);
		createTrackbar("Correlation_coeff_maximum", "Einstellungen", &correlation_coeff_maximum, 100);
		createTrackbar("Binar_Mode", "Einstellungen", &binare_Mode, 5);
		createTrackbar("Optimaler_Einstellung", "Einstellungen", &Optimaler_Einstellung, 3);

		if (Optimaler_Einstellung==1)// Optimale Einstellungen fur Pfeilortschilder
		{ 

			adaptive_threshold_subtract = 16;//Anpassung des adaptive Threshold
			dilation_elem = 2;//Dilationsmodus
			dilation_size = 2;//Dilationsgröße
			erosion_elem = 0;//Erosionsmodus
			erosion_size = 4;//Erosionsgröße
			max_thresh = 255;
			block_size = 81;
			rForm_min = 42;//minimaler Formfaktor*100
			rForm_max = 80;//maximaler Formfaktor*100
			correlation_coeff_maximum = 85;//Wenn eine Übereinstimmung größer 85% erreicht wurde, wir der Buchstabe zurückgegeben
			n_childs_smaller_min = 0;//Gibt an, ob Anzahl Kinder <5
			size_roadsign_bigger_minsize = 0;//Gibt an, ob minimale Schildgröße 1% des Gesamtbildes
			size_roadsign_not_fullscreen = 0;//Gibt an, ob maximale Schildgröße 95% des Gesamtbildes
			form_factor_smaller_rFormmax = 0;//Gibt an, ob Formfaktor < rForm_max (Schieberegler)
			form_factor_bigger_rFormmin = 0;//Gibt an, ob Formfaktor > rForm_min (Schieberegler)
			binare_Mode = 1;// Binarisierung mit OTSU
		    imshow("Einstellungen", CV_WINDOW_FREERATIO);
		}
		if (Optimaler_Einstellung == 2)// Autobahnschild
		{
			adaptive_threshold_subtract = 2;//Anpassung des adaptive Threshold
			dilation_elem = 0;//Dilationsmodus
			dilation_size = 0;//Dilationsgröße
			erosion_elem = 2;//Erosionsmodus
			erosion_size = 0;//Erosionsgröße
			max_thresh = 255;
			block_size = 5;
			rForm_min = 40;//minimaler Formfaktor*100
			rForm_max = 80;//maximaler Formfaktor*100
			n_childs = 0;//Anzahl der Childs, die eine Kontur hat
			correlation_coeff_maximum = 85;/////Wenn eine Übereinstimmung größer 85% erreicht wurde, wir der Buchstabe zurückgegeben
			n_childs_smaller_min = 0;//Gibt an, ob Anzahl Kinder <5
			size_roadsign_bigger_minsize = 0;//Gibt an, ob minimale Schildgröße 1% des Gesamtbildes
			size_roadsign_not_fullscreen = 0;//Gibt an, ob maximale Schildgröße 95% des Gesamtbildes
			form_factor_smaller_rFormmax = 0;//Gibt an, ob Formfaktor < rForm_max (Schieberegler)
			form_factor_bigger_rFormmin = 0;//Gibt an, ob Formfaktor > rForm_min (Schieberegler)
			binare_Mode = 0;// Binarisierung mit adaptiveThreshold
			imshow("Einstellungen", CV_WINDOW_FREERATIO);
		}
		if (Optimaler_Einstellung == 3)// Ortschild
		{
			adaptive_threshold_subtract = 2;//Anpassung des adaptive Threshold
			dilation_elem = 0;//Dilationsmodus
			dilation_size = 0;//Dilationsgröße
			erosion_elem = 2;//Erosionsmodus
			erosion_size = 0;//Erosionsgröße
			max_thresh = 255;
			block_size = 5;
			rForm_min = 30;//minimaler Formfaktor*100
			rForm_max = 80;//maximaler Formfaktor*100
			n_childs = 0;//Anzahl der Childs, die eine Kontur hat
			correlation_coeff_maximum = 85;/////Wenn eine Übereinstimmung größer 85% erreicht wurde, wir der Buchstabe zurückgegeben
			n_childs_smaller_min = 0;//Gibt an, ob Anzahl Kinder <5
			size_roadsign_bigger_minsize = 0;//Gibt an, ob minimale Schildgröße 1% des Gesamtbildes
			size_roadsign_not_fullscreen = 0;//Gibt an, ob maximale Schildgröße 95% des Gesamtbildes
			form_factor_smaller_rFormmax = 0;//Gibt an, ob Formfaktor < rForm_max (Schieberegler)
			form_factor_bigger_rFormmin = 0;//Gibt an, ob Formfaktor > rForm_min (Schieberegler)
			binare_Mode = 0;// Binarisierung mit adaptiveThreshold
			imshow("Einstellungen", CV_WINDOW_FREERATIO);
		}
	if (Webcam_ein == 1)
	{
		//2.Step:Quellen fur image waehlen===========================================================================
		//2.1.Step:Initialisieren der Kamera=====
		//wählen: 'cam_int' oder 'cam_ext
		int camera = cam_ext;
		VideoCapture cap(camera); // Kameraeingang wählen
		while(!cap.isOpened())  // Ueberprueft ob Kamera eingeschalten ist
		{
			cout << "Webcam wurde nicht gefunden!Bitte Webcam austecken und wieder einstecken";
			
			VideoCapture cap(0); // Kameraeingang wählen
		
		}
		Mat frame;
		cap >> frame;
		imwrite("webcam_frame.jpg", frame);
		original_frame = imread("webcam_frame.jpg", CV_LOAD_IMAGE_COLOR);
		if (!original_frame.data)
		{
			cout << "Die Datei konnte nicht geöffnet oder gefunden werden" << std::endl;
			return -1;
		}
	}
	else
	{
		original_frame = imread("webcam_frame.jpg", CV_LOAD_IMAGE_COLOR);  // Autobahn.jpg ; Baumerlenbach.jpg
	}

	    //------------------------------------------------------------------------------------------------------		
		//3.Step: Umwandlung des eingelesen Frames
		//3.1.Step: Umwandlung in ein Graubild 
			cvtColor(original_frame, gray_frame, COLOR_BGR2GRAY);
			imwrite("gray.jpg", gray_frame);  //Abspeichern
		//3.2.Step:Gaußfilter anwenden (Elimination von statistischen Messfehlern)
			GaussianBlur(gray_frame, gauss_frame, Size(3, 3), 1, 1);
		//3.4.Step: Binärbild erstellen
			if(binare_Mode==0)
			{
				threshold(gauss_frame, binary_frame, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
			}
			else if (binare_Mode == 1)
			{
			    adaptiveThreshold(gauss_frame, binary_frame, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, block_size, adaptive_threshold_subtract);
			}
			Mat binary_frame2 = binary_frame.clone();
			imwrite("binary_frame.jpg", binary_frame);  //Abspeichern
			imshow("binary", binary_frame);
	    //3.5.Step:Erodieren und Dilatieren
			//Dilatieren
			binary_dilatet_frame = dilatieren(binary_frame, dilation_elem, dilation_size);
			imwrite("binary_dilatet_frame.jpg", binary_dilatet_frame);  //Abspeichern
			//Erodieren
			binary_dilatet_eroded_frame = erodieren(binary_dilatet_frame, erosion_elem, erosion_size);
			imwrite("binary_dilatet_eroded_frame.jpg", binary_dilatet_eroded_frame);  //Abspeichern
		//3.6.Step:Konturen finden
		findContours(binary_dilatet_eroded_frame, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE, Point(0, 0));

	//------------------------------------------------------------------------------------------------------
	//4.Step:Ortsschilder und Pfeilwegweiser extrahieren
		Size framesize = binary_dilatet_eroded_frame.size();//Bildgröße
		double min_object_size = 0.01*framesize.height*framesize.width;//minimale Schildgröße
		double max_object_size = 0.80*framesize.height*framesize.width;//maximale Schildgröße		
		Mat contours_frame = Mat::zeros(binary_dilatet_eroded_frame.size(), CV_8UC3);//Konturenbild leeren
		vector<object> object_vector; //Vektor des structs 'object'
		object_vector.clear(); //Sicherstellen, dass Vektor leer
		int n_objects_fitted = 0; //Anzahl der gefundenen Objekte/Schilder
		//4.1.Step:Alle Konturen durchlaufen
		for (size_t i = 0; i < contours.size(); i++) 
		{
			object object; //Instanz eines object structs
			//Alle Konturen zeichnen
			drawContours(contours_frame, contours, (int)i, Scalar(0, 0, 255), 1, 8, hierarchy, 0, Point());
			//Eigenschaften dem Struct zuweisen
			object.aiHierarchy = hierarchy;
			object.contour_length = arcLength(contours[i], false); //Länge der Kontur
			object.area = contourArea(contours[i], false); //Fläche der Kontur
			//Formfaktor berechnen
			if (object.contour_length > 0.0) //Länge der Kontur >0
				object.form_factor = 4.0*CV_PI* object.area / (object.contour_length*object.contour_length); //Formfaktor berechnen
			//Anzahl der Childs der Kontur zählen (ausgelagert in eigene Funktion count_childs)
			object.n_childs = count_childs(object.aiHierarchy, i);

			//-------------------------------------------------------------------
				//Folgende Bedingungen werden abgefragt
			/*bool*/ n_childs_smaller_min = object.n_childs > 3; //Anzahl Kinder <5
			/*bool*/ size_roadsign_bigger_minsize = object.area > min_object_size; //Minimale Schildgröße 1% des Gesamtbildes
			/*bool*/ size_roadsign_not_fullscreen = object.area < max_object_size; //Minimale Schildgröße 90% des Gesamtbildes
			/*bool*/ form_factor_smaller_rFormmax = (object.form_factor < (((double)rForm_max) / 100)); //Formfaktor < rForm_max (Schieberegler)
			/*bool*/ form_factor_bigger_rFormmin = (object.form_factor > (((double)rForm_min) / 100));//Formfaktor > rForm_min (Schieberegler)
		//-------------------------------------------------------------------

			if (n_childs_smaller_min && size_roadsign_bigger_minsize && size_roadsign_not_fullscreen && form_factor_smaller_rFormmax && form_factor_bigger_rFormmin)
			{
				//Bounding Box um die gefundenen Objekte ziehen
				Rect box;
				box = boundingRect(contours[i]);
				RotatedRect rotatedRect =minAreaRect(contours[i]);
				
				rectangle(contours_frame, box, Scalar(0, 255, 0));
				//Anzahl der gefundenen Objekte hochzählen
				n_objects_fitted++;
				object.angle = rotatedRect.angle;
				if (rotatedRect.angle < -30) {
					object.angle=rotatedRect.angle + 90;
				}
				else {
					object.angle =rotatedRect.angle;
				}
				object.Object_ID = n_objects_fitted;
				object.Contour_ID = i;
				object.box = box;
				object.x = box.x;
				object.y = box.y;
				//Objekteigenschaften auf dem struct-Vektor ablegen bzw. speichern
				object_vector.push_back(object);// hier werden alle gefunden Schilder abgespeichert
				
				//=== Nach testen nachfolgender Abschnitt löschen! Zeigt den Winkel im Objekt an 
				// read center of rotated rect
				//cv::Point2f center = rotatedRect.center; // center

				//// draw rotated rect
				//float  angle = object.angle;
				//cv::Point2f rect_points[4];
				//rotatedRect.points(rect_points);
				//for (unsigned int j = 0; j < 4; ++j)
				//	cv::line(binary_dilatet_eroded_frame, rect_points[j], rect_points[(j + 1) % 4], cv::Scalar(0, 255, 0));
				//// draw center and print text
				//std::stringstream ss;   ss << angle; // convert float to string
				//cv::circle(binary_dilatet_eroded_frame, center, 5, cv::Scalar(0, 255, 0)); // draw center
				//cv::putText(binary_dilatet_eroded_frame, ss.str(), center + cv::Point2f(-25, 25), cv::FONT_HERSHEY_COMPLEX_SMALL, 1, cv::Scalar(255, 0, 255)); // print angle
				//cv::imshow("mask", binary_dilatet_eroded_frame);
				//=================
			}
		}

	//----------------------------------------------------------------------------------------------------------
	//5.Step:Schildtyp erkennen
		//5.1.Step:Iterieren über alle gefundenen Objekte
		Mat contours_frame_Result = contours_frame.clone();// Damit in der Uebersicht eine Variante mit großer Buchstaben fuer die Art des Schildes steht
		for (int j = 0; j < object_vector.size(); j++)
		{
			Mat bgr = original_frame(object_vector[j].box).clone(); //BGR Bild
			double pixel_amout = bgr.rows*bgr.cols; //Anzahl der Pixel im Bild
			Mat1b mask_yellow; //Maske für die Gelbanteile im Bild
			Mat1b mask_blue; //Maske für die Blauanteile im Bild
			Mat1b mask_white; //Maske für die Weißanteile im Bild			
			Mat3b hsv_inv;//HSV Bild
			//HSV Bild aus dem BGR-Bild 
			cvtColor(bgr, hsv_inv, COLOR_BGR2HSV); 
			//Berechnung_Blauanteil im Bild
			inRange(hsv_inv, Scalar(110, 50, 50), Scalar(130, 255, 255), mask_blue);
			double mask_blue_result = countNonZero(mask_blue) / pixel_amout * 100;
			//Berechnung_Gelbanteil im Bild
			inRange(hsv_inv, Scalar(5, 50, 50), Scalar(30, 255, 255), mask_yellow);
			double mask_yellow_result = countNonZero(mask_yellow) / pixel_amout * 100;
			//Berechnung_Weißanteil im Bild
			inRange(hsv_inv, Scalar(0, 0, 240), Scalar(0.7 * 255, 255, 255), mask_white);
			double mask_white_result = countNonZero(mask_white) / pixel_amout * 100;
			//Schildtyp erkennen mit der Funktion maximum
			auto plate_type = maximum(mask_blue_result, mask_yellow_result, mask_white_result);
			//Zurückgeben des Schildtyps in den Objektvektor
			object_vector[j].plate_type = plate_type;
		//5.2.Step:Am unteren Rand des Schilds im Konturenbild anzeigen, um welchen Schildtyp es sich handelt
			putText(contours_frame_Result, plate_type, Point2f(object_vector[j].box.x, object_vector[j].box.y + object_vector[j].box.height), FONT_HERSHEY_PLAIN, 10, Scalar(0, 204, 255, 255), 4);
			putText(contours_frame, plate_type, Point2f(object_vector[j].box.x, object_vector[j].box.y + object_vector[j].box.height), FONT_HERSHEY_PLAIN, 2, Scalar(0, 204, 255, 255),1);
			imwrite("contours_frame_Schildart.jpg", contours_frame_Result);
		}

	//----------------------------------------------------------------------------------------------------------
	//6.Step:Buchstaben extrahieren
		if (Buchstab_Erk == 1)
		{

			letter_contours.clear();
			letter_boxes.clear();
			for (int b = 0; b < n_objects_fitted; b++) //Iterieren über die Anzahl der gefundenen Objekte
			{
				if (object_vector[b].plate_type == "Kein Schildtyp erkannt")// Abbruch des Loops falls objekt kein Schild ist
				{
					continue; 
					cout << "abbruch loop";
				}

				Rect letter; //BoundingBox für die Buchstaben
				auto index_first_letter = object_vector[b].aiHierarchy[object_vector[b].Contour_ID][2];

				if (index_first_letter >= 0)
				{	//Ersten Buchstaben umrahmen und abspeichern	
					letter = boundingRect(contours[index_first_letter]);
					if (letter.height > 3 && letter.width > 3)
					{
						rectangle(contours_frame, letter, Scalar(255, 255, 0));
						letter_contours.push_back(contours[index_first_letter]);
						letter_boxes.push_back(letter);
					}
					//Nächsten Buchstaben finden
					auto index_next_letter = object_vector[b].aiHierarchy[index_first_letter][0];
					while (index_next_letter >= 0)
					{
						letter = boundingRect(contours[index_next_letter]);
						if (letter.height > 5 && letter.width > 5)
						{
							//Wenn Buchstabe gefunden, dann auch umrahmen						
							rectangle(contours_frame, letter, Scalar(255, 255, 0));
							letter_contours.push_back(contours[index_next_letter]);
							letter_boxes.push_back(letter);
						}
						index_next_letter = object_vector[b].aiHierarchy[index_next_letter][0];
					}
				}

				//----------------------------------------------------------------------------------------------------------
				//7.Step.Buchstaben erkennen (OCR)
					//7.1.Step:Buchstaben sortieren	
				if (letter_boxes.size() != 0)
				{
					Rect y_max;
					Rect y_min;
					y_min = letter_boxes[0];
					y_max = letter_boxes[0];
					for (int g = 0; g < letter_boxes.size() - 1; g++)
					{
						if (letter_boxes[g].y > y_max.y)
							y_max = letter_boxes[g];

						if (letter_boxes[g].y < y_min.y)
							y_min = letter_boxes[g];
					}

					for (int g = 0; g < letter_boxes.size(); g++)
					{
						for (int i = 0; i < letter_boxes.size(); i++)
						{
							if ((letter_boxes[g].x < letter_boxes[i].x))
							{
								Rect Platzhalter = letter_boxes[g];
								letter_boxes[g] = letter_boxes[i];
								letter_boxes[i] = Platzhalter;
							}
						}
					}
				}
				text = ' '; //Textfeld leeren
			//7.2.Step:Buchstaben aus dem Binärbild extrahieren
				for (int i = 0; i < letter_boxes.size(); i++) // iterate through each contour for first hierarchy level .
				{
					char letter_result;
					Rect box;
					box = letter_boxes[i];

					letter_result = recognize_letter(binary_frame, box, object_vector[b].plate_type, correlation_coeff_maximum,temp_letter, number_loop, object_vector[b].angle); //Eigene Funktion zum Buchstaben erkennen
					if (letter_result != ' ') //Wenn Buchstabe erkannt
					{
						//7.3.Step:Ergebnis in String wandeln
						stringstream letter_result_ss;
						letter_result_ss << letter_result;
						string letter_result_str = letter_result_ss.str();
						putText(contours_frame, letter_result_str, Point2f(box.x, box.y), FONT_HERSHEY_PLAIN, 2, Scalar(0, 204, 255, 255));
						imwrite("Letter/" + letter_result_str + ".jpg", binary_frame(box)); //Abspeichern;					
						text = text + letter_result;
						
					}
				}
				cout << text<<endl;
				//Text, der innerhalb des Schilds steht ausgeben
				putText(contours_frame, text, Point2f(object_vector[b].x, object_vector[b].y), FONT_HERSHEY_PLAIN, 2, Scalar(0, 204, 255, 255));
			}
			namedWindow("Konturen", WINDOW_AUTOSIZE);
			imshow("Konturen", contours_frame);
		}

		//=====Darstellung des gesamten Ergebnisses
		 // Abgespeicherte Images Einlesen
			Mat gray_frame_show = imread("gray.jpg", CV_LOAD_IMAGE_COLOR);   //als original_frame einlesen
			Mat binar_fame_show = imread("binary_frame.jpg", CV_LOAD_IMAGE_COLOR);   //als original_frame einlesen
			Mat binary_dilatet_frame_show = imread("binary_dilatet_frame.jpg", CV_LOAD_IMAGE_COLOR);   //als original_frame einlesen
			Mat binary_dilatet_eroded_frame_show = imread("binary_dilatet_eroded_frame.jpg", CV_LOAD_IMAGE_COLOR);   //als original_frame einlesen
			Mat contours_frame_show = imread("contours_frame_Schildart.jpg",CV_LOAD_IMAGE_COLOR);
			Mat Ergebnis = imread("Test5.jpg");
		//Zusammenfuehrung der Images damit diese gemeinsam dargestellt werden koennen
		cv::hconcat(original_frame, gray_frame_show, Ergebnis);
		cv::hconcat(Ergebnis, binar_fame_show, Ergebnis);
		cv::hconcat(Ergebnis, binary_dilatet_frame_show, Ergebnis);
		cv::hconcat(Ergebnis, binary_dilatet_eroded_frame_show, Ergebnis);
		
		String windowName1 = "Ergebnis"; //Name of the window
		//Init. der Abstaende fur die Bildbeschriftung
		int abstand_gray,abstand_binary,test, abstand_binary_dilatet_frame_show, abstand_binary_dilatet_eroded_frame_show;
		test = original_frame.cols;
		abstand_gray = gray_frame_show.cols;
		abstand_binary = abstand_gray + binar_fame_show.cols;
		abstand_binary_dilatet_frame_show = abstand_binary + binary_dilatet_frame_show.cols;
		abstand_binary_dilatet_eroded_frame_show = abstand_binary_dilatet_frame_show + binary_dilatet_eroded_frame_show.cols;
		Scalar Schriftfarbe= (0,200,200);
		namedWindow(windowName1, WINDOW_NORMAL); // Erstellung des Fensters
		putText(Ergebnis, "Original", Point(0, 70), FONT_HERSHEY_SIMPLEX, 3, Schriftfarbe, 8);
		putText(Ergebnis, "Gray", Point(abstand_gray, 70), FONT_HERSHEY_SIMPLEX, 3, Schriftfarbe, 8);
		putText(Ergebnis, "Binary", Point(abstand_binary, 70), FONT_HERSHEY_SIMPLEX, 3, Schriftfarbe, 8);
		putText(Ergebnis, "Dilatet", Point(abstand_binary_dilatet_frame_show, 70), FONT_HERSHEY_SIMPLEX, 3, Schriftfarbe, 8);
		putText(Ergebnis, "Dilatet_Eroded", Point(abstand_binary_dilatet_eroded_frame_show, 70), FONT_HERSHEY_SIMPLEX, 3, Schriftfarbe, 8);
		imshow(windowName1, Ergebnis);
																					 
		//====
		//waitkey zum beenden: Beliebige Taste
		if (waitKey(30)>= 0)
		{
			break;
		}
	}
	return 0;

}