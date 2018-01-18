#include <services/Console.h>
#include <stream/VGAStream.h>
#include <subsystem/ChildManager.h>


using namespace nre;


static ConsoleSession console("console",1,"Critial_System_2");
static VGAStream view(console,0);


int main(){
    
    view <<"ACC/ACCM module!!\n";

    view <<"\n";

    view <<"Simulation Program\n\n\n\n";

    view << "Running Simulation ........ ";

    Sm sm(0);
    sm.down();
    return 0;
}