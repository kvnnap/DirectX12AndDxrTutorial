#include "App.h"

#include "core/anvil.h"
#include "visualisation/basic_visualiser.h"

#include <iostream>
#include <memory>

using namespace std;
using feanor::anvil::Anvil;
using feanor::anvil::visualisation::BasicVisualiser;

int main()
{
	App app;

    Anvil& anvil = Anvil::getInstance();

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

    anvil.clear();

    return ret;
}