#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utils/logger.hpp>
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <string>
#include "SpinVideo.h"


#define FRAMERATE 10
#define BITRATE 10000000
#define RECORD_TIME 30      //IN SECONDS
#define HEIGHT 480
#define WIDTH 640
#define PARTS 1

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace Spinnaker::Video;
using namespace std;
using namespace cv;

int PrintDeviceInfo(INodeMap& nodeMap)
{
    int result = 0;

    cout << endl << "*** DEVICE INFORMATION ***" << endl << endl;

    try
    {
        FeatureList_t features;
        CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
        if (IsAvailable(category) && IsReadable(category))
        {
            category->GetFeatures(features);

            FeatureList_t::const_iterator it;
            for (it = features.begin(); it != features.end(); ++it)
            {
                CNodePtr pfeatureNode = *it;
                cout << pfeatureNode->GetName() << " : ";
                CValuePtr pValue = (CValuePtr)pfeatureNode;
                cout << (IsReadable(pValue) ? pValue->ToString() : "Node not readable");
                cout << endl;
            }
        }
        else
        {
            cout << "Device control information not available." << endl;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

int ConfigureCamera(INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;

    try
    {
        cout << endl << endl << "\t*** CONFIGURING CAMERA SETTINGS ***" << endl;
        // Set acquisition mode to continuous
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous (node retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to set acquisition mode to continuous (entry 'continuous' retrieval). Aborting..." << endl
                << endl;
            return -1;
        }

        int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();
        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);
        cout << "----- Acquisition mode set to continuous... -----" << endl;

        CFloatPtr ptrFramerate = nodeMap.GetNode("AcquisitionFrameRate");
        ptrFramerate->SetValue(FRAMERATE);
        cout << "----- Acquisition framerate set to 10... -----" << endl;
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}


int AcquireImages(CameraPtr pCam, INodeMap& nodeMap)
{
    int result = 0;

    cout << endl << endl << "\t*** IMAGE ACQUISITION ***" << endl;

    try
    {
        int recordTime = 60;
        int recordParts = 1;
        cout << "Recording time (seconds): ";
        cin >> recordTime;
        cout << endl << "Number of video files: ";
        cin >> recordParts; 



        // Calculate required number of frames for 1 video file
        const unsigned int numImages = ((FRAMERATE * recordTime)/recordParts) + 24;

        // Begin acquiring images
        pCam->BeginAcquisition();
        cout << "Acquisition started..." << endl << endl;

        // *** NOTES ***
        // By default, if no specific color processing algorithm is set, the image
        // processor will default to NEAREST_NEIGHBOR method.
        ImageProcessor processor;
        processor.SetColorProcessing(HQ_LINEAR);

        SpinVideo video;
        //const unsigned int k_videoFileSize_MB = 4096;
        //video.SetMaximumFileSize(k_videoFileSize_MB);

        Video::H264Option option;
        option.frameRate = FRAMERATE;
        option.bitrate = BITRATE;
        option.height = HEIGHT;
        option.width = WIDTH;

        for (int part = 1; part <= recordParts; part++)
        {
            string videoFilename = "Video_" + to_string(part) + "_" + to_string(time(0));
            video.Open(videoFilename.c_str(), option);


            //cout << "Appending first 24 frames to MP4 file..." << endl;
            //for (int frame = 0; frame < 24; frame++)
            //{
            //    ImagePtr pResultImage = pCam->GetNextImage(1000);
            //    Mat cvimg = cv::Mat(480, 640, CV_16UC1, pResultImage->GetData(), pResultImage->GetStride());
            //    cvimg = cvimg - 24000;
            //    cvimg = cvimg * 50;
            //    video.Append(processor.Convert(pResultImage, PixelFormat_Mono8));
            //}

            // *** IMPORTAN NOTE TO SELF ***
            // The first 24 frames in the MP4 file won't be buffered
            for (int imageCnt = 1; imageCnt <= numImages; imageCnt++)
            {
                try
                {

                    // Retrieve the next received image
                    ImagePtr pResultImage = pCam->GetNextImage(1000);

                    if (pResultImage->IsIncomplete())
                    {
                        //SetConsoleTextAttribute(hConsole, 12);
                        cout << "Image "<< imageCnt << " is incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl
                            << endl;
                    }
                    else
                    {
                        //SetConsoleTextAttribute(hConsole, 10);

                        cout << "------------------------" << endl;
                        cout << "Grabbed image " << imageCnt << "/" << numImages << endl;

                        Mat cvimg = cv::Mat(480, 640, CV_16UC1, pResultImage->GetData(), pResultImage->GetStride());
                        cvimg = cvimg - 24000;
                        cvimg = cvimg * 50;

                        // Deep copy image into mp4 file
                        video.Append(processor.Convert(pResultImage, PixelFormat_Mono8));
                        cout << "Appended image " << imageCnt << "/" << numImages << " to part:" << part << endl;
                        cout << "------------------------" << endl;

                        // Release image
                        pResultImage->Release();
                    }
                }

                catch (Spinnaker::Exception& e)
                {
                    cout << "Error: " << e.what() << endl;
                    result = -1;
                }
            }
            video.Close();
            cout << endl << "Video saved at " << videoFilename << ".avi" << endl;
        }
        // End acquisition
        pCam->EndAcquisition();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

int RunSingleCamera(CameraPtr pCam)
{
    int result = 0;
    int err = 0;

    try
    {
        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        result = PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Configure camera settings (fps, acquisition mode, etc.)
        result = result | ConfigureCamera(nodeMap, nodeMapTLDevice);

        err = AcquireImages(pCam, nodeMap);
        if (err < 0)
        {
            return err;
        }

        // Deinitialize camera
        pCam->DeInit();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

int main(int /*argc*/, char** /*argv*/)
{
    int result = 0;

    cv::utils::logging::setLogLevel(cv::utils::logging::LogLevel::LOG_LEVEL_SILENT);

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version    ## optional
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
         << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
         << endl;

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << endl << endl;

    // Finish if there are no cameras
    if (numCameras == 0)
    {
        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << "Not enough cameras!" << endl;
        cout << "Done! Press Enter to exit..." << endl;
        getchar();

        return -1;
    }

    // Run example on each camera
    for (unsigned int i = 0; i < numCameras; i++)
    {
        cout << endl << "Running example for camera " << i << "..." << endl;

        result = result | RunSingleCamera(camList.GetByIndex(i));

        cout << "Camera " << i << " example complete..." << endl << endl;
    }

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}
