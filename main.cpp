#include <unistd.h>
#include "odrive.h"

int main(int argc, char **argv){
		dhr::odrive od;
        od.init(0x2075378E5753);    
        Json::Value json;
       
        uint32_t state1 = 6, state2 = 8;
        float vel = 7.0, vel1 = 0,vel_es=0,pos_es=0;

        dhr::getJson(&od, &json);
        
        dhr::writeOdriveData(&od, json, "axis0.requested_state", state1);
        sleep(10);

        dhr::writeOdriveData(&od, json, "axis0.requested_state", state2);
        sleep(10);
        std::cout << "Gotov sam da machkam!" << std::endl;
        
        dhr::writeOdriveData(&od, json, "axis0.controller.input_vel", vel);

        int i = 0;
        while (true){
            dhr::readOdriveData(&od, json, "axis0.encoder.vel_estimate",vel_es);
            dhr::readOdriveData(&od, json, "axis0.encoder.pos_estimate",pos_es);

            std::cout << "velocity:" <<  vel_es;
            std::cout << "  position:" << pos_es << std::endl;
            sleep(1);
            if(i == 10) break;
            i-=-1;
        }
        std::cout << "AI Chaoooo" << std::endl;
        
        dhr::writeOdriveData(&od, json, "axis0.controller.input_vel", vel1);

		return 0;
}
