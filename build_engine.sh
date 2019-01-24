#/bin/bash


# to generate a new engine 
# by default uses :  - > data/yolov3-tiny.cfg 
# -> data/yolov3-tiny.weights 
# to generate engine at location sources/lib/models/yolov3-tiny-kFLOAT-batch1.engine
# path of input files are mentioned in 


# 
# Configure file : sources/lib/network_config.cpp 
# for changing below settings
#
#const std::string kPRECISION = "kFLOAT";
#const std::string kINPUT_BLOB_NAME = "data";
#const uint kINPUT_H = 864;
#const uint kINPUT_W = 864;
#const uint kINPUT_C = 3;
#const uint64_t kINPUT_SIZE = kINPUT_C * kINPUT_H * kINPUT_W;
#const uint kOUTPUT_CLASSES = 6;
#const std::vector<std::string> kCLASS_NAMES   =  {"Person", "Backpack", "Handbag", "Suitcase", "Stroller", "Wheelchair" };
#
#const std::string kDS_LIB_PATH = "sources/lib/";
#const std::string kMODELS_PATH = kDS_LIB_PATH + "models/";
#const std::string kDETECTION_RESULTS_PATH = "data/detections/";
#const std::string kCALIBRATION_SET = "data/calibration_images.txt";
#const std::string kTEST_IMAGES = "data/test_images.txt";





rm sources/lib/models/yolov3-tiny-kFLOAT-batch1.engine
cd sources/apps/trt-yolo/
make -j4
cd -
./sources/apps/trt-yolo/trt-yolo-app 

# copy engine to app 
cp -v sources/lib/models/yolov3-kHALF-batch1.engine /app/models/det/aio.engine
# sources/lib/models/yolov3-tiny-kFLOAT-batch1.engine

