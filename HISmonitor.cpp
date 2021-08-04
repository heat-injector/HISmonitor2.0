#include "Particle.h"
#include "tracker_config.h"
#include "tracker.h"
SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);
PRODUCT_ID(TRACKER_PRODUCT_ID);
PRODUCT_VERSION(TRACKER_PRODUCT_VERSION);
STARTUP(
    Tracker::startup();
);
    float sleepDelay = 5;           //Time delay to sleep
    long sleepTime = 0;             //Used for sleep logic
    long heatStartTime = 0;         //recorded millis time when flame turns on
    long heatStopTime = 0;          //recorded millis time when flame turns off
    long deltaHeatTime = 0;         //calculated delta between off and on
    const int collectHours = D9;    //TrackerSOM PIND9 - Input_PULLDOWN 3.3v signal for flame ON
    const int isPoweredOn = D8;     //TrackerSOM PIND8 - Input_PULLDOWN signal for power to unit
    int shutdownUnit = D1;          //TrackerSOM PIND1 - "shutdownHeat" is "set" on latching relay, NO contact. When High for 1sec it will shutdown heat.
    int enableUnit = D0;            //TrackerSOM PIND0 - "enableHeat" is "reset" on latching relay, NC contact. When High for 1sec it will Enable heat.
    int adminRelay = D3;            //TrackerSOM PIND3 - relay to toggle when the VCC hits the latching relay.
    int shutdown(String command);   //Declaring Shutdown int
    int enable(String command);     //Declaring Enable int
    int keepAlive(String command);    //Declaring Keepalive int
    bool flameOn = false;           //Is flame ON?
    bool powerOn = false;           //Is there Power?
    bool updateMode = false;           //Is keepAlive enabled
    int previousPowerOn = 0;        //look logic for Power
    int previousFlameOn = 0;        //loop logic for flame ON
    int previousSleep = 0;            //Look logic for KeepAlive function
    String heatTime = "";           //String for deltaHeatTime
    String powerOnPub = "";         //String for Power On
    String powerOffPub = "";        //String for Power Off
    String keepOnline = "";         //String for KeepOnline Mode

SerialLogHandler logHandler(115200, LOG_LEVEL_TRACE, {
    { "app.gps.nmea", LOG_LEVEL_INFO },
    { "app.gps.ubx",  LOG_LEVEL_INFO },
    { "ncp.at", LOG_LEVEL_INFO },
    { "net.ppp.client", LOG_LEVEL_INFO },
});

void setup()
{
    Tracker::instance().init();                 //Internal Tracker Command
    TrackerSleep::instance().wakeFor(D8,RISING);//Wakes Tracker up with powerOn  
    pinMode(CAN_PWR, OUTPUT);                   //Set 5v to the CAN VCC
    digitalWrite(CAN_PWR, HIGH);                //Set 5v to the CAN VCC
    pinMode(collectHours, INPUT_PULLDOWN);      //Setting pinMode for collectHours as input_pulldown
    pinMode(isPoweredOn, INPUT_PULLDOWN);       //Setting pinMode for power as input_pulldown
    pinMode(shutdownUnit, OUTPUT);              //Shutdown unit output
    digitalWrite(shutdownUnit, LOW);            //sets the pin to low
    pinMode(enableUnit, OUTPUT);                //enable unit output
    digitalWrite(enableUnit, LOW);              //sets the pin to low
    pinMode(adminRelay, OUTPUT);                //VCC Relay for Latch
    digitalWrite(adminRelay, LOW);              //sets the pin to low
    Particle.function("TurnOff", shutdown);     //Function - Shutdown Heat Injector
    Particle.function("TurnOn", enable);        //Function - Enable Heat Injector
    Particle.function("keepAlive", keepAlive);  //Function - Enter update mode
    Particle.variable("flameOn", flameOn);      //Variable - flameOn boolean
    Particle.variable("powerOn", powerOn);      //Variable - power boolean 
    Particle.variable("KeepAlive", updateMode); //Variable - power boolean         
    Particle.connect();
}

void loop()
{ //Loop in
    Tracker::instance().loop();                             //Required

    flameOn = digitalRead(D9);                              //boolean for if flame is on
    delay(5);
    powerOn = digitalRead(D8);                              //boolean for if power is on

    if (digitalRead(D9) == HIGH)                            //Run this sub loop while flame is on
    {   //If flame is on - Run once
        delay(15000);                                       //15second delay used to void failed starts.
        if (previousFlameOn == LOW)                         //Loop logic : only run once while flame is on 
        {   //Record start time 
            heatStartTime = millis();                       //Store current millis for flame start time
            delay(250);                                 
        }   //Record start time
    }   //If flame is on - Run once
    else
    {   //If flame is off - Run once
        if (previousFlameOn == HIGH)                        //Loop Logic only run once after flame goes away
        {   //Record stop time and send hrs
            heatStopTime = millis();                        //Store current millis for flame stop time
            deltaHeatTime = heatStopTime - heatStartTime;   //calculate delta heat time between flame on and flame off
            deltaHeatTime = ((deltaHeatTime / 1000) / 60);  //Convert millisecond to minutes
            
            if (deltaHeatTime > 0)
            {
            heatTime = String(deltaHeatTime);               //Store delta minues in heatTime 
            waitFor(Particle.connected, 10000);             //Give the partice a few minutes to connect
            Particle.publish("hrs", heatTime, PRIVATE);     //publish delta heat time as hrs
            delay(3000);
            }    
        }   //Record stop time and send hrs
    }   //If flame is off - Run once
    
    previousFlameOn = flameOn;  //change flame on state
    
    if (digitalRead(isPoweredOn) == HIGH)
    {  
            if (previousPowerOn == LOW)   
            {
                updateMode = false;
                powerOnPub = String("PluggedIn");
                Particle.publish("pwrOn", powerOnPub, PRIVATE);
                TrackerSleep::instance().pauseSleep();
            }
    }

    if (digitalRead(isPoweredOn) == LOW)
    {
            if (previousPowerOn == HIGH)
            {
                powerOffPub = String("Unplugged");
                Particle.publish("pwrOff", powerOffPub, PRIVATE);
                sleepTime = millis();
                sleepDelay = 5;

            }
            if (sleepTime + (sleepDelay * 60 * 60 * 1000) <= millis()) //Originl Math (sleepDelay * 60 * 60 * 1000) <= millis())
            {
                TrackerSleep::instance().resumeSleep();            //Allows Tracker to go back to sleep
            }
    }
    
    previousPowerOn = powerOn;  //change power on state
    previousSleep = updateMode;  //Change keepAlive status
} //Loop Out

int shutdown(String command)   //Command to Shutdown with the "set" on latching relay
    {
        if (command == "1")                     //requests are case sensitive...Program accepts "off", "Off", or "OFF" to shutdown heat
        {
            digitalWrite(adminRelay, HIGH);     //Power the VCC relay
            delay(250);
            digitalWrite(shutdownUnit, HIGH);     //Power the VCC relay
            delay(150);
            digitalWrite(shutdownUnit, LOW);      //Turn off VCC relay
            delay(150);
            digitalWrite(adminRelay, LOW);    //Turn off latching signal
            
        }
        return 1;                              //If heat was shutdown, return "1"
    }
    
 int enable(String command)   //Command to Shutdown with the "set" on latching relay
    {
        if (command == "1")                     //requests are case sensitive...Program accepts "off", "Off", or "OFF" to shutdown heat
        {
            digitalWrite(adminRelay, HIGH);     //Power the VCC relay
            delay(250);
            digitalWrite(enableUnit, HIGH);     //Power the latching relay
            delay(150);
            digitalWrite(enableUnit, LOW);      //Turn off the VCC relay
            delay(150);
            digitalWrite(adminRelay, LOW);      //Turn off latching signal
                        
        }
        return 1;                               //If heat was shutdown, return "1"
    }

int keepAlive(String command)   //Command to force online mode for 24 hours
    {
        if (command == "1")                     
        {
            TrackerSleep::instance().extendExecution(86400); //Add 24 hours to the sleep time, one time only
            updateMode = true;
            keepOnline = String("KeepAlive");
            Particle.publish("updateMode", keepOnline, PRIVATE);
        }
        return 1;                               //If keepAlive was enabled, return "1"
    }
