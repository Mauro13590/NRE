/*NOVA Manager:

This file read configuration and run module on nova.


*/
#include "nova_manager.h"

using namespace nre;

static ConsoleSession console("console",0,"NovaManager"); 
static SList<VMConfig> configs;
static ChildManager child_manager;
static VGAStream view_manager(console,0);
static Cycler<CPU::iterator> cpucyc(CPU::begin(), CPU::end());

//variable Maurizio Sepe
size_t n_mod = 0;
struct Module *module;


static bool check_module(SList<VMConfig> configs){
    if(configs.length() == 0){ 
        return false;
    }else { 
        module = (Module*)malloc(sizeof(Module)*configs.length());
        //view_manager << sizeof(module)/8 << " " << configs.length() <<"\n";
        return true;
    }
}    

static void configures(){
   const Hip &hip = Hip::get();
    for(auto mem=hip.mem_begin(); mem != hip.mem_end(); ++mem){
        //view_manager <<"[nova_manager]: " << mem->cmdline()<< " "<<mem->addr<<" "<<mem->size<< "\n";
        if(strstr(mem->cmdline(), ".vmconfig")){
            VMConfig *cfg = new VMConfig(mem->addr,mem->size,mem->cmdline());
            configs.append(cfg);
        }
    }
}

static void show_configures(SList<VMConfig> configs){
    n_mod++;
    for(auto it = configs.begin(); it != configs.end(); ++it, ++n_mod)
        view_manager << "  [" << n_mod << "] " << it->name() << "\n";
    n_mod = 0;    
}

static void parsing_configures(SList<VMConfig> configs){
    for(auto it = configs.begin(); it != configs.end(); ++it, ++n_mod){
        String _cmdline = it->name();
        const char *str = _cmdline.str();
        const char *start = str;
        for(size_t i = 0; i <= _cmdline.length(); ++i ){
            if (i == _cmdline.length() || str[i] == ' '){
                size_t len = str + i - start;
                //view_manager <<str<< len << "\n";
                if(strncmp(start,"mode=System",len) == 0) module[n_mod]._mode = 0;
                else if(strncmp(start,"mode=User",len) == 0){
                        //view_manager << "User\n";
                        start = start + len + 1;
                        if(strncmp(start,"ncpu=",6) == 0){
                            module[n_mod]._ncpu = start[6] - '0';
                            module[n_mod]._mode = 1;
                            //view_manager << module[n_mod].ncpu << "\n";
                        }
                    } 
                
                start = str + i + 1;
            }
        }    
    }
    n_mod = 0;
}

static void start_modules(SList<VMConfig> configs){

    RunningVMList &vml = RunningVMList::get();
    for(auto it = configs.begin(); it != configs.end(); ++it, ++n_mod){
        if(module[n_mod]._mode == USER) vml.add(child_manager,&*it,module[n_mod]._ncpu);
        if(module[n_mod]._mode == SYSTEM) vml.add(child_manager,&*it,cpucyc.next()->log_id());
    }
    n_mod = 0;
    view_manager <<"\n START MODULE \n";
}

int main(){

    view_manager <<"\t \t \t Welcome to the Nova Manager! \n \n";
    configures();
    view_manager <<"Module to be loaded on Nova-NRE: \n\n";

    
    if (!check_module(configs)) view_manager <<"Module Not Present \n";
    else{ 
        //view_manager <<"MODULE ARE : \n\n";
        show_configures(configs);
        parsing_configures(configs);
        start_modules(configs);
    }
    Sm sm(0);
    sm.down();

    return 0;

}