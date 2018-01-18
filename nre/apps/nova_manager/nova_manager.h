#include <subsystem/ChildManager.h>
#include <services/Console.h>
#include <stream/Serial.h>
#include <stream/VGAStream.h>
#include <collection/Cycler.h>
#include <Hip.h>
#include <String.h>



#include "RunningVM.h"
#include "RunningVMList.h"
#include "VMConfig.h"
#include "VMMngService.h"



#define SYSTEM 0
#define USER 1


struct Module{
    int _ncpu;
    int _mode = 0;
};







