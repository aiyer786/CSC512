
#include "FeatureDetector.h"
#include "KeyPointsCollector.h"
#include <iostream>
#include <fstream>

int main( int argc, char *argv[] )
{
    std::string filename;
    std::cout << "Enter file name: ";
    std::cin >> filename;

    // Debugger on or off
    bool debug;
    std::string debugStr;
    std::cout << "Want the debugger on? (y/n): ";
    std::cin >> debugStr;

    if ( debugStr == "y" ) {
        debug = true;
    } else if ( debugStr == "n" ) {
        debug = false;
    }

    FeatureDetector detector( filename, debug );
    detector.cursorFinder();

    return EXIT_SUCCESS;

}
