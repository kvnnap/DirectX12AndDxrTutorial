#include "App.h"

#include <iostream>
#include <memory>

using namespace std;

int main()
{
	App app;

    try {
        // Initialise ECS sytems
        //anvil.addSystem(make_shared<BasicVisualiser>("CornellBox-Original.obj"));
        //anvil.addSystem(make_shared<BasicVisualiser>("CornellBox-Original.obj"));

        //run();

        //anvil.clear();
    }
    catch (const exception& e) {
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }

	auto ret = app.execute();

    return ret;
}