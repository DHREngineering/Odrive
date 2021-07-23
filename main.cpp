#include <unistd.h>
#include "odrive.h"

int main(int argc, char **argv){
		
        //Initialize the odrive object
        dhr::odrive od;
        od.init(0x2075378E5753);    
        Json::Value json;
       
        //Predefine variables
        uint32_t state1 = 3, state2 = 8;
        float vel = 7.0, vel1 = 0,vel_es=0,pos_es=0;

        //Retrieve the json config
        dhr::getJson(&od, &json);
        
        //Callibrate axis0
        dhr::writeOdriveData(&od, json, "axis0.requested_state", state1);
        sleep(10); //Some sleep time is required

        //Enter CONTROL_MODE_VELOCITY_CONTROL
        dhr::writeOdriveData(&od, json, "axis0.requested_state", state2);
        sleep(10);
        
        // WRITE to the Odrives first velocity
        dhr::writeOdriveData(&od, json, "axis0.controller.input_vel", vel);


        // Loop to read the position and velocity of the axis0
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
        // Write 0 to the axis0.cotroller.input_vel in order to stop the motor
        dhr::writeOdriveData(&od, json, "axis0.controller.input_vel", vel1);

		return 0;
}
