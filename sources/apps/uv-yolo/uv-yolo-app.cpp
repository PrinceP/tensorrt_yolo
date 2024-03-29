/**
MIT License

Copyright (c) 2018 NVIDIA CORPORATION. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*
*/

#include <stdlib.h>
#include "ds_image.h"
#include "network_config.h"
#include "trt_utils.h"
#include "yolo.h"

#include "json.hpp"



#ifdef MODEL_V2
#include "yolov2.h"
#endif

#ifdef MODEL_V2_TINY
#include "yolov2-tiny.h"
#endif

#ifdef MODEL_V3
#include "yolov3.h"
#endif

#ifdef MODEL_V3_TINY
#include "yolov3-tiny.h"
#endif

#include <experimental/filesystem>
#include <gflags/gflags.h>
#include <string>
#include <sys/time.h>


#include <json/json.h>
#include <zmq.h>
#include "zmq.hpp"
#include<vector>

#include <chrono>
#include <thread>

#include <cpr/cpr.h>

#include <algorithm>

using namespace nlohmann;

namespace fs = std::experimental::filesystem;


DEFINE_bool(decode, true, "Decode the detections");
DEFINE_int32(batch_size, 1, "Batch size for the inference engine.");
DEFINE_uint64(seed, std::time(0), "Seed for the random number generator");

//Engine Name
DEFINE_string(engine, "sources/lib/models/yolov3-tiny-kHALF-batch1.engine", "Engine type");
//NMS Threshold
DEFINE_double(NMSThresh, 0.2f, "NMS Threshold");
//Confidence
//DEFINE_double(ProbThresh, 0.2f, "Prob Threshold");

//CONFIG_IP
DEFINE_string(configip, "0.0.0.0", "IP address for config");

//CONFIG_PORT
DEFINE_string(configport, "1234", "Port address for config");

//REC_ADD_PULLPORT_INPUT
DEFINE_string(inputport, "7501", "Input Port - Pull address for config");

//REC_REV_PUSHPORT
DEFINE_string(outputport, "7601", "Output Port - Push address for config");

//INPUT_TASK
DEFINE_string(task, "task1", "Input Task Name");





static bool ValidateEngine(const char* flagname, const std::string& engine1 )
{       fs::path engine = engine1; 
        if(fs::exists(engine))
                 { std::cout << "File exists :" << engine1 << std::endl ; return true; }
                
        else
                 { std::cout << "File not exists :" << engine1 << std::endl ;  return false; }
                
}
DEFINE_validator(engine, &ValidateEngine);


struct FLAGS
{
bool person;
bool  bag;
bool  head;
bool  shelf;
bool  movable_obj;
bool face;
bool race;
bool age;
bool  gender;
bool eye_acc;
bool face_mask;
bool head_gear;
bool hand_position;
bool upper_body_clothing;
bool lower_body_clothing;
double thresh_person;
double thresh_bag;
double thresh_head;
double thresh_shelf;
double thresh_movable_obj;
double thresh_face;

bool person_age;
bool person_gender;
bool pose;

int gpu_all_in_one_det;
int gpu_shelf_det;
int gpu_head_face_det;
int gpu_head_face_class;
int gpu_person_class;

bool profile_time = true;
}FLAGS;

/*
void Run_Parser()
{
        INIReader reader("test.ini");
        if (reader.ParseError() < 0) {
                std::cout << "Can't load desired\n";
                INIReader reader("default.ini");
        }
        FLAGS.person = reader.GetBoolean("Detectors", "person", false);
        FLAGS.bag = reader.GetBoolean("Detectors", "bag", false);
        FLAGS.head = reader.GetBoolean("Detectors", "head", false);
        FLAGS.shelf = reader.GetBoolean("Detectors", "shelf", false);
        FLAGS.movable_obj = reader.GetBoolean("Detectors", "movable_obj", false);
        FLAGS.face = reader.GetBoolean("Detectors", "face", false);

        if(FLAGS.face){
                FLAGS.age = reader.GetBoolean("Classifiers","age", false);
                FLAGS.race = reader.GetBoolean("Classifiers","race", false);
                FLAGS.gender = reader.GetBoolean("Classifiers","gender", false);
                FLAGS.eye_acc = reader.GetBoolean("Classifiers","eye_acc", false);
                FLAGS.face_mask = reader.GetBoolean("Classifiers","face_mask", false);

        }

        if(FLAGS.person){
                FLAGS.hand_position = reader.GetBoolean("Classifiers","hand_position", false);
                FLAGS.upper_body_clothing = reader.GetBoolean("Classifiers","upper_body_clothing", false);
                FLAGS.lower_body_clothing = reader.GetBoolean("Classifiers","lower_body_clothing", false);
                FLAGS.person_age = reader.GetBoolean("Classifiers","person_age", false);
                FLAGS.person_gender = reader.GetBoolean("Classifiers","person_gender", false);

        }

        if(FLAGS.head){
                FLAGS.head_gear = reader.GetBoolean("Classifiers", "head_gear", false);
        }

        //Parse Detector Options
        FLAGS.thresh_person = reader.GetReal("Thresholds","person", -1);
        FLAGS.thresh_bag = reader.GetReal("Thresholds","bag", -1);
        FLAGS.thresh_head = reader.GetReal("Thresholds","head", -1);
        FLAGS.thresh_shelf = reader.GetReal("Thresholds","shelf", -1);
        FLAGS.thresh_movable_obj = reader.GetReal("Thresholds","movable_obj", -1);
        FLAGS.thresh_face = reader.GetReal("Thresholds","face", -1);

        //Parse Detector_GPU options
        FLAGS.gpu_all_in_one_det = reader.GetInteger("Detectors_GPU","all_in_one_det",0 );
        FLAGS.gpu_shelf_det = reader.GetInteger("Detectors_GPU","shelf_det",  0);
        FLAGS.gpu_head_face_det = reader.GetInteger("Detectors_GPU","head_face_det", 0);

        //Parse Classifier_GPU options
        FLAGS.gpu_head_face_class = reader.GetInteger("Classifiers_GPU","head_face_cls", 0);
        FLAGS.gpu_person_class = reader.GetInteger("Classifiers_GPU","person_cls", 0);
        FLAGS.pose = FLAGS.face || FLAGS.head;
}
*/


std::string timestamp()
{
        char timeString[100];
        struct timeval tv;
        gettimeofday(&tv, NULL);
        long long int seconds2 = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
        sprintf(timeString, "%lld", seconds2);
        return std::string(timeString);
}

struct OutPred
{
 std::string classname;
 int label;
 double confidence;
 int xmin;
 int xmax;
 int width;
 int height;
};

std::string MessageJson( std::string timestamp, int cam_id, std::string filename, int dimensions[], std::string dtype , std::vector<OutPred> Pred, json config_json_new)
{

        json_object *MainPrediction = json_object_new_object();
        json_object *jfilename = json_object_new_string(filename.c_str());
	json_object *jtimestamp = json_object_new_double(  atof(timestamp.c_str()) );
	json_object *jdtype = json_object_new_string(dtype.c_str());
	json_object *jcamid = json_object_new_int64(cam_id);
	json_object *jinfo = json_object_new_object();

	
	
	json_object_object_add(jinfo, "cam_id", jcamid);
	json_object_object_add(jinfo,"filename", jfilename);

	json_object *jarray_shape = json_object_new_array();
	for (int i = 0; i < 3; i++)
	{	
		json_object *shapeone = json_object_new_int64(dimensions[i]);
		json_object_array_add(jarray_shape, shapeone);
	 	
	}
	

        json_object_object_add(MainPrediction, "info", jinfo);
	json_object_object_add(MainPrediction, "dtype", jdtype);
	json_object_object_add(MainPrediction, "time_stamp", jtimestamp);
	json_object_object_add(MainPrediction, "shape", jarray_shape);
	



                json_object *jarray_faces = json_object_new_array();
                json_object *jarray_people = json_object_new_array();
                json_object *jarray_objects = json_object_new_array();
                json_object *jarray_heads = json_object_new_array();
                json_object *jarray_rest = json_object_new_array();

 	for(int i = 0; i < Pred.size(); i++ ){

                json_object *onePrediction = json_object_new_object();

                json_object *jtype = json_object_new_string(Pred[i].classname.c_str());
                json_object *jobjCoordinates = json_object_new_object();
                json_object_object_add(jobjCoordinates, "xmin", json_object_new_int64(Pred[i].xmin));
                json_object_object_add(jobjCoordinates, "ymin", json_object_new_int64(Pred[i].xmax));
                json_object_object_add(jobjCoordinates, "width", json_object_new_int64(Pred[i].width));
                json_object_object_add(jobjCoordinates, "height", json_object_new_int64(Pred[i].height));
                json_object *jconfidence = json_object_new_double(Pred[i].confidence);

                //json_object_object_add(onePrediction, "name", jtype);

                //json_object_object_add(onePrediction, "coordinates", jobjCoordinates);
                //json_object_object_add(onePrediction, "confidence", jconfidence);
                //json_object_object_add(onePrediction, "properties", json_object_new_string(""));
		//Changed for release 27.11.2018
                
                    
		if(Pred[i].classname == "Person"){

			json_object *jobjBody = json_object_new_object();

			json_object *jproperties = json_object_new_object();

			json_object_object_add(jobjBody, "coordinates", jobjCoordinates);
			json_object_object_add(jobjBody, "confidence", jconfidence);



			json_object_object_add(jobjBody, "properties", jproperties);

			json_object_object_add(onePrediction, "body", jobjBody);
                }
		//--

                //Change for the release 13.12.2018
                
		if(Pred[i].classname != "Person"){
                  json_object_object_add(onePrediction, "name", jtype);   
                  json_object_object_add(onePrediction, "coordinates", jobjCoordinates);
                  json_object_object_add(onePrediction, "confidence", jconfidence);
                                  
                }
                //
               
		
                if(Pred[i].classname == "Person" &&  Pred[i].confidence >= config_json_new["thresh_person_lt"] )
                {
                        json_object_array_add(jarray_people, onePrediction);
                }
                if((Pred[i].classname == "Suitcase" || Pred[i].classname == "Handbag" || Pred[i].classname == "Backpack") && config_json_new["bag"] && Pred[i].confidence >= config_json_new["thresh_bag"])
                {
                        json_object_array_add(jarray_objects, onePrediction);
                }
                if((Pred[i].classname == "Stroller" || Pred[i].classname == "Wheelchair") && config_json_new["movable_obj"] && Pred[i].confidence >= config_json_new["thresh_movable_obj"])
                {
                        json_object_array_add(jarray_objects, onePrediction);
                }

                json_object_array_add(jarray_rest, onePrediction);


        }
	json_object *jpeople = json_object_new_object();
	json_object *jobject = json_object_new_object();
	
	json_object *jdetections =  json_object_new_object();
         
	json_object *jdetection =  json_object_new_object();
  
	json_object_object_add(jdetection, "people", jarray_people);
        json_object_object_add(jdetection, "objects", jarray_objects);
        
        
        //json_object_array_add(jdetection, jpeople);
	
        //json_object_array_add(jdetection, jobject);


        json_object_object_add(jdetections, "detections", jdetection);
   

        json_object_object_add(MainPrediction,"data", jdetections);
        //json_object_object_add(MainPrediction,"people", jarray_people);

         

        //json_object_object_add(MainPrediction,"heads", jarray_heads);
        //json_object_object_add(MainPrediction,"rest", jarray_rest);


    return json_object_to_json_string(MainPrediction);

}




int main(int argc, char** argv)
{

    srand(unsigned(FLAGS_seed));
    ::google::ParseCommandLineFlags(&argc, &argv, true);



        // Send request and get a result. 
	
	std::string CONFIG_IP = FLAGS_configip ;
	std::string CONFIG_PORT = FLAGS_configport;
	
	//std::cout << "IP = " << "http://"+CONFIG_IP+":"+CONFIG_PORT+"/get_config_flags" << std::endl; 
	std::string conf; 
    
	while(1){
	
                // http://0.0.0.0:1234/get_config_flags	
		auto response = cpr::Get // or cpr::Head
		(
    		cpr::Url{"http://"+CONFIG_IP+":"+CONFIG_PORT+"/get_config_flags"},
    		cpr::Header{{"accept", "text/html"}},
    		cpr::Timeout{4 * 1000}
		);	
		
	
	 	if(response.status_code == 200){
			conf = response.text;
				
	std::cout << response.status_code << std::endl;
			break;
		}
	
	}
   
	//std::cout <<  "Here " << conf << std::endl;
    
	int n = conf.length();
        char char_array[n+1];
        strcpy(char_array, conf.c_str());
         

//New json         
        json config_json_new = json::parse(conf);
//

        json_object *config_json = json_tokener_parse(char_array);
	
	char* node_name;
  	node_name = getenv ("node_name");
	
	std::string task_name = FLAGS_task;
	//char* task_name_c[task_name.length() + 1];
	//strcpy(task_name_c,   task_name.c_str());  
	//std::cout << cx  << std::endl;	
	
	if (!(config_json_new["person"] || config_json_new["bag"] || config_json_new["movable_obj"]) || !json_object_get_boolean ( json_object_object_get ( json_object_object_get (json_object_object_get(config_json, "tx2_tasks"), node_name ), task_name.c_str()) )  ) 
	{
		
		std::cout << "Exiting the detector, config is false"  << std::endl;	
		exit(0);
	}

	std::cout << "Intializing the detector, config is true"  << std::endl;



//Initialization of demo.py variables
        //bool FLAG_ALL_IN_ONE = FLAGS.movable_obj || FLAGS.person || FLAGS.bag ;

    std::string REC_REV = FLAGS_outputport; 
    std::string REC_ADD = FLAGS_inputport;	 

//End   	


        std::unique_ptr<Yolo> inferNet{nullptr};

#ifdef MODEL_V2
    inferNet = std::unique_ptr<Yolo>{new YoloV2(FLAGS_batch_size)};
#endif

#ifdef MODEL_V2_TINY
    inferNet = std::unique_ptr<Yolo>(new YoloV2Tiny(FLAGS_batch_size));
#endif

#ifdef MODEL_V3
    inferNet = std::unique_ptr<Yolo>{new YoloV3(FLAGS_batch_size, FLAGS_engine)};
#endif

#ifdef MODEL_V3_TINY
    inferNet = std::unique_ptr<Yolo>{new YoloV3Tiny(FLAGS_batch_size, FLAGS_engine)};
#endif
     
    //Setting the values
        float thresh_person_lt = config_json_new["thresh_person_lt"];
        float thresh_bag = config_json_new["thresh_bag"];
        float thresh_movable_obj  = config_json_new["thresh_movable_obj"];
        
        float prob_thresh = std::min({thresh_person_lt, thresh_bag ,thresh_movable_obj});   
   
        inferNet->setNMSThresh(FLAGS_NMSThresh);
        inferNet->setProbThresh(prob_thresh);
    //End

     // ZMQ
                zmq::context_t context(1);

    //  Socket to receive messages on
    zmq::socket_t receiver(context, ZMQ_PULL);
    receiver.connect("tcp://127.0.0.1:"+REC_ADD);

    //  Socket to send messages to
    zmq::socket_t sender(context, ZMQ_PUSH);
 
    sender.bind("tcp://127.0.0.1:"+REC_REV);

        // End  

	while(1){
		
            


	  // ZMQ Recieve


        //Got Json
        zmq::message_t message1;
        receiver.recv(&message1);
        std::string smessage1(static_cast<char*>(message1.data()), message1.size());
        std::cout << smessage1 << std::endl;
        int n = smessage1.length();
        char char_array[n+1];
        strcpy(char_array, smessage1.c_str());
        json_object *received_json = json_tokener_parse(char_array);
        json_object *jfilename, *jinfo, *jcamid, *jtype;
	
        int dimensions[3];
        json_object *jarray;
        jinfo = json_object_object_get(received_json, "info");
	jfilename = json_object_object_get(jinfo, "filename");
	jcamid = json_object_object_get(jinfo, "cam_id");
        jarray = json_object_object_get(received_json, "shape");
        std::string filename = json_object_get_string(jfilename);
	int camid = json_object_get_int64 (jcamid);
        int arraylen = json_object_array_length(jarray); /*Getting the length of the array*/
        json_object * jvalue;

        for(int i=0; i< arraylen; i++){
                jvalue = json_object_array_get_idx(jarray, i); /*Getting the array element at position i*/
                dimensions[i] = json_object_get_int(jvalue);
        }
	std::string dtype = json_object_get_string(json_object_object_get(received_json, "dtype"));



        //Make Image

        zmq::message_t message2;
        receiver.recv(&message2);
std::printf("Received image: %d bytes \n", static_cast<int>(message2.size()));
	//std::cout << dimensions[0]  << dimensions[1] << dimensions[2] << std::endl;  
        cv::Mat received_img(dimensions[0], dimensions[1], CV_8UC3, message2.data());

   //End
	

	 //Loading Image
	double inferElapsed = 0;
	//std::cout << "Starting Loading" << inferNet->getInputH() <<" "<< inferNet->getInputW() << std::endl;
    DsImage img =  DsImage(received_img, inferNet->getInputH(), inferNet->getInputW());
    std::vector<DsImage> dsImages(1);
    dsImages.at(0) = img;
	struct timeval inferStart, inferEnd;
	std::cout << "Starting inference" << std::endl; 
	gettimeofday(&inferStart, NULL);
    cv::Mat trtInput = blobFromDsImages(dsImages, inferNet->getInputH(), inferNet->getInputW());

    inferNet->doInference(trtInput.data);
	gettimeofday(&inferEnd, NULL);
    //Running loop
	inferElapsed += ((inferEnd.tv_sec - inferStart.tv_sec)
                         + (inferEnd.tv_usec - inferStart.tv_usec) / 1000000.0)
            * 1000 / FLAGS_batch_size;

	std::cout << inferElapsed << " ms = Time Taken by Model" << std::endl;
    std :: string message ;
    for(int imageIdx = 0; imageIdx < FLAGS_batch_size; ++imageIdx)
            {
                auto curImage = dsImages.at(imageIdx);
                auto binfo = inferNet->decodeDetections(imageIdx, curImage.getImageHeight(),
                                                        curImage.getImageWidth());
                auto remaining = nonMaximumSuppression(inferNet->getNMSThresh(), binfo);
                std::vector<OutPred> vector_OutPred;
                for (auto b : remaining)
                {
                    {
                        //std::cout << " label:" << b.label << "(" << inferNet->getClassName(b.label) << ")" << " confidence:" << b.prob << " xmin:" << b.box.x1 << " ymin:" << b.box.y1
                        //            << " xmax:" << b.box.x2 << " ymax:" << b.box.y2 << std::endl;
                        vector_OutPred.push_back( { inferNet->getClassName(b.label), b.label ,b.prob, b.box.x1, b.box.y1, b.box.x2 - b.box.x1, b.box.y2 - b.box.y1 } );

                    }
                }
 //               std :: cout << MessageJson( timestamp(),  camid, filename, dimensions, dtype,  vector_OutPred, config_json_new) << std::endl;
                message.append(MessageJson( timestamp(),  camid, filename, dimensions, dtype,  vector_OutPred, config_json_new ));
            }
    
        
    int length = message.size();
    zmq::message_t request(length);
    memcpy((void *)request.data(), message.c_str(), (size_t)length);
    sender.send(request);

    std :: cout<<"Sent result json" <<  message << std::endl;

    }
  		
    return 0;
}
