#include "App.h"
#include <iostream>

int main(int argc, char** argv)
{
    try
    {
        bdc::app::App app{argc, argv};
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
