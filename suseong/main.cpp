#include "../suseongtrace.h"
#include <stdio.h>
#include <string>

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        TRACE("No Device Detected");
        return 1;
    }

    std::string devName = argv[1];

    return 0;
}
