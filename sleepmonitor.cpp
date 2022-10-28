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
#include <opencv2/videoio.hpp>

#define FRAMERATE 1
#define BITRATE 10000000
#define RECORD_TIME 1      //IN SECONDS
#define HEIGHT 480
#define WIDTH 640
#define PARTS 1

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace Spinnaker::Video;
using namespace std;
using namespace cv;

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
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

// This function prepares, saves, and cleans up an video from a vector of images.
int InitializeCamera(INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;

    SetConsoleTextAttribute(hConsole, 9);

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
        ptrFramerate->SetValue(10.0);
        cout << "----- Acquisition framerate set to 10... -----" << endl;
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    SetConsoleTextAttribute(hConsole, 15);

    return result;
}


int AcquireImages(CameraPtr pCam, INodeMap& nodeMap)
{
    int result = 0;

    SetConsoleTextAttribute(hConsole, 11);

    cout << endl << endl << "\t*** IMAGE ACQUISITION ***" << endl;

    try
    {
        // Calculate required number of frames for video
        const unsigned int numImages = (FRAMERATE * RECORD_TIME) + 24;

        // Begin acquiring images
        pCam->BeginAcquisition();
        cout << "Acquisition started..." << endl << endl;

        SetConsoleTextAttribute(hConsole, 15);


        // *** NOTES ***
        // By default, if no specific color processing algorithm is set, the image
        // processor will default to NEAREST_NEIGHBOR method.
        ImageProcessor processor;
        processor.SetColorProcessing(HQ_LINEAR);

        SpinVideo video;
        const unsigned int k_videoFileSize_MB = 4096;
        video.SetMaximumFileSize(k_videoFileSize_MB);

        Video::H264Option option;

        option.frameRate = FRAMERATE;
        option.bitrate = BITRATE;
        option.height = HEIGHT;
        option.width = WIDTH;

        for (int part = 0; part < PARTS; part++)
        {
            string videoFilename = "Video_" + to_string(part+1) + "_" + to_string(time(0));
            video.Open(videoFilename.c_str(), option);

            vector<ImagePtr> images;


            // *** IMPORTAN NOTE TO SELF ***
            // The first 24 frames in the MP4 file won't be buffered
            for (int imageCnt = 0; imageCnt < numImages; imageCnt++)
            {
                try
                {

                    // Retrieve the next received image
                    ImagePtr pResultImage = pCam->GetNextImage(1000);

                    if (pResultImage->IsIncomplete())
                    {
                        SetConsoleTextAttribute(hConsole, 12);
                        cout << "Image "<< imageCnt << " is incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl
                            << endl;
                    }
                    else
                    {
                        SetConsoleTextAttribute(hConsole, 10);

                        cout << "------------------------" << endl;
                        cout << "Grabbed image " << imageCnt + 1 << "/" << numImages << endl;

                        Mat cvimg = cv::Mat(480, 640, CV_16UC1, pResultImage->GetData(), pResultImage->GetStride());
                        cvimg = cvimg - 24000;
                        cvimg = cvimg * 50;

                        // Deep copy image into mp4 file
                        video.Append(processor.Convert(pResultImage, PixelFormat_Mono8));
                        cout << "Appended image " << imageCnt + 1 << "/" << numImages << " to part:" << part + 1 << endl;
                        cout << "------------------------" << endl;


                    }
                    images.push_back(processor.Convert(pResultImage, PixelFormat_Mono8));

                    //video.Append(processor.Convert(pResultImage, PixelFormat_Mono8));

                    // Release image
                    pResultImage->Release();
                }

                catch (Spinnaker::Exception& e)
                {
                    cout << "Error: " << e.what() << endl;
                    result = -1;
                }
            }

            //for (unsigned int imageCnt = 0; imageCnt < images.size(); imageCnt++)
            //{
            //    video.Append(images[imageCnt]);
            //
            //    cout << "\tAppended image " << imageCnt << "..." << endl;
            //}

            video.Close();
            cout << endl << "Video saved at " << videoFilename << ".avi" << endl;
            //video.release();
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

// This function acts as the body of the example; please see NodeMapInfo example
// for more in-depth comments on setting up cameras.
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

        result = result | InitializeCamera(nodeMap, nodeMapTLDevice);

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

// Example entry point; please see Enumeration example for more in-depth
// comments on preparing and cleaning up the system.
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
