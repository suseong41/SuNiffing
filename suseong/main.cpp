#include "../suseongtrace.h"
#include <stdio.h>
#include <string>
#include "../runner.h"

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        TRACE("No Device Detected");
        return -1;
    }

    std::string devName = argv[1];

    Runner runner;
    runner.run(devName);

    return 0;
}
