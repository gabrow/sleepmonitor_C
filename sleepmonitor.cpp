#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utils/logger.hpp>
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include "SpinVideo.h"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>


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

int exp_part = 0;

int PrintDeviceInfo(INodeMap& nodeMap)
{
    int result = 0;

    cout << "\n" << "*** DEVICE INFORMATION ***" << "\n" << "\n";

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
                cout << "\n";
            }
        }
        else
        {
            cout << "Device control information not available." << "\n";
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << "\n";
        result = -1;
    }

    return result;
}

int ConfigureCamera(INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;

    try
    {
        cout << "\n" << "\n" << "\t*** CONFIGURING CAMERA SETTINGS ***" << "\n";
        // Set acquisition mode to continuous
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous (node retrieval). Aborting..." << "\n" << "\n";
            return -1;
        }

        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to set acquisition mode to continuous (entry 'continuous' retrieval). Aborting..." << "\n"
                << "\n";
            return -1;
        }

        int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();
        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);
        cout << "----- Acquisition mode set to continuous... -----" << "\n";

        CFloatPtr ptrFramerate = nodeMap.GetNode("AcquisitionFrameRate");
        ptrFramerate->SetValue(FRAMERATE);
        cout << "----- Acquisition framerate set to 10... -----" << "\n";
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << "\n";
        result = -1;
    }

    return result;
}


int AcquireImages(CameraPtr pCam, INodeMap& nodeMap, int recordLength, int part)
{
    int result = 0;

    cout << "\n" << "\n" << "\t*** IMAGE ACQUISITION ***" << "\n";

    try
    {
        // Calculate required number of frames for 1 video file
        const int numImages = (FRAMERATE * recordLength) +24;

        // Begin acquiring images
        pCam->BeginAcquisition();
        cout << "Acquisition started..." << "\n" << "\n";

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

        chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point elapsed;

        string videoFilename = "Video_" + to_string(part) + "_" + to_string(time(0));
        video.Open(videoFilename.c_str(), option);

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
                    cout << "Image "<< imageCnt << " is incomplete with image status " << pResultImage->GetImageStatus() << "..." << "\n"
                        << "\n";
                }
                else
                {
                    //SetConsoleTextAttribute(hConsole, 10);

                    cout << "------------------------" << "\n";
                    cout << "Grabbed image " << imageCnt << "/" << numImages << "\n";

                    Mat cvimg = cv::Mat(480, 640, CV_16UC1, pResultImage->GetData(), pResultImage->GetStride());

                    cvimg = cvimg - 23800;
                    cvimg = cvimg * 50;

                    // Deep copy image into mp4 file
                    video.Append(processor.Convert(pResultImage, PixelFormat_Mono8));
                    cout << "Appended image " << imageCnt << "/" << numImages << " to part:" << part << "\n";

                    // Release image
                    pResultImage->Release();

                    elapsed = std::chrono::steady_clock::now();
                    cout << "Recording time elapsed: "
                              << std::chrono::duration_cast<std::chrono::seconds>(elapsed - begin).count()
                              << "s" << "\n";

                    // progressbar just for a nicer look
                    int barWidth = 100;
                    float progress = static_cast<float>(imageCnt) / numImages;

                    std::cout << "[";
                    int pos = barWidth * progress;
                    for (int i = 0; i < barWidth; ++i) {
                        if (i <= pos) std::cout << "#";
                        //else if (i == pos) std::cout << ">";
                        else std::cout << " ";
                    }
                    std::cout << "] " << int(progress * 100.0) << " %\n";

                    cout << "------------------------" << "\n";
                }
            }

            catch (Spinnaker::Exception& e)
            {
                cout << "Error: " << e.what() << "\n";
                result = -1;
            }
        }
        video.Close();
        
        //const char* avi_filename = videoFilename.c_str() + '.avi';
        //const char* mp4_filename = videoFilename.c_str() + '.mp4';
        //rename(avi_filename, mp4_filename);
        cout << "\n" << "Video saved at " << videoFilename << ".avi" << "\n";

        // End acquisition
        pCam->EndAcquisition();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << "\n";
        result = -1;
    }

    return result;
}

int RecordTimeInput()
{
    string input;
    cin >> input;

    std::size_t length = input.length();
    if (length != 0)
    {
        int value = std::stoi(input.substr(0, length - 1));
        if (input[length - 1] == 's') return value;
        else if (input[length - 1] == 'm') return value * 60;
        else if (input[length - 1] == 'h') return value * 3600;
        else
        {
            cout << "Invalid input. Try again.\n";
            return RecordTimeInput();
        }
    }
    return 0;
}

int RunSingleCamera(CameraPtr pCam)
{
    int result = 0;
    int err = 0;

    try
    {
        //int secondsToRecord = 60;
        cout << "Recording time in seconds (1 hour = 3600s, 8 hours = 28800): ";
		int secondsToRecord = 0;
		cin >> secondsToRecord;
        //int secondsToRecord = RecordTimeInput();
        cout << "\n" << "Number of video files: ";
        int partsToRecord;
        cin >> partsToRecord; 

        for (int part = 1; part <= partsToRecord; part++)
        {
            // Retrieve TL device nodemap and print device information
            INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
            if (part == 1) result = PrintDeviceInfo(nodeMapTLDevice);

            // Initialize camera
            pCam->Init();

            // Retrieve GenICam nodemap
            INodeMap& nodeMap = pCam->GetNodeMap();

            // Configure camera settings (fps, acquisition mode, etc.)
            result = result | ConfigureCamera(nodeMap, nodeMapTLDevice);

            err = AcquireImages(pCam, nodeMap, secondsToRecord, part);
            if (err < 0)
            {
                return err;
            }

            // Deinitialize camera
            pCam->DeInit();
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << "\n";
        result = -1;
    }

    return result;
}

int main(int /*argc*/, char** /*argv*/)
{
    int result = 0;

    cv::utils::logging::setLogLevel(cv::utils::logging::LogLevel::LOG_LEVEL_SILENT);

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << "\n" << "\n";

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version    ## optional
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
         << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << "\n"
         << "\n";

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << "\n" << "\n";

    // Finish if there are no cameras
    if (numCameras == 0)
    {
        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << "Not enough cameras!" << "\n";
        cout << "Done! Press Enter to exit..." << "\n";
        getchar();

        return -1;
    }



    // Run example on each camera
    for (unsigned int i = 0; i < numCameras; i++)
    {
        cout << "\n" << "Running example for camera " << i << "..." << "\n";

        result = result | RunSingleCamera(camList.GetByIndex(i));

        cout << "Camera " << i << " example complete..." << "\n" << "\n";
    }

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << "\n" << "Done! Press Enter to exit..." << "\n";
    getchar();

    return result;
}
