
#include "Particle.h"
#include "tracker_config.h"
#include "tracker.h"

    long lastPublish = 0;           // Used to keep track of the last time we published data
    int delayMinutes = 1;           // How many minutes between publishes? 10+ recommended for long-time continuous publishing!
    int sleepDelay = 6;             //(Hrs)Time delay to sleep
    long sleepTime = 0;             //Used for sleep logic

    long heatStartTime = 0;         //recorded millis time when flame turns on
    long heatStopTime = 0;          //recorded millis time when flame turns off
    long deltaHeatTime = 0;         //calculated delta between off and on

    const int collectHours = D4;    //TrackerSOM PIND4 - Input_PULLDOWN 3.3v signal for flame ON
    int isPoweredOn = D5;         //TrackerSOM PIND5 - Input_PULLDOWN signal for power to unit
    int shutdownHeat = D1;          //TrackerSOM PIND1 - "shutdownHeat" is "set" on latching relay, NO contact. When High for 1sec it will shutdown heat.
    int enableHeat = D0;            //TrackerSOM PIND0 - "enableHeat" is "reset" on latching relay, NC contact. When High for 1sec it will Enable heat.
    int adminRelay = D3;            //TrackerSOM PIND3 - relay to toggle when the VCC hits the latching relay.

    bool flameOn = false;           //Is flame ON?
    bool powerOn = false;           //Is there Power?

    int previousPowerOn = 0;        //look logic for Power
    int previousFlameOn = 0;        //loop logic for flame ON
    String heatTime = "";           //String for deltaHeatTime
    float get_temperature();


SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);
PRODUCT_ID(TRACKER_PRODUCT_ID);
PRODUCT_VERSION(TRACKER_PRODUCT_VERSION);
SerialLogHandler logHandler(115200, LOG_LEVEL_TRACE, 
{
    { "app.gps.nmea", LOG_LEVEL_INFO },
    { "app.gps.ubx",  LOG_LEVEL_INFO },
    { "ncp.at", LOG_LEVEL_INFO },
    { "net.ppp.client", LOG_LEVEL_INFO },
});

void setup()
{
    Tracker::instance().init();
    
    pinMode(CAN_PWR, OUTPUT);               //Set 5v to the CAN VCC
    digitalWrite(CAN_PWR, HIGH);            //Set 5v to the CAN VCC

    pinMode(collectHours, INPUT_PULLUP);  //Setting pinMode for collectHours as input_pulldown
    pinMode(isPoweredOn, INPUT_PULLUP);       //Setting pinMode for power as input_pulldown

    pinMode(shutdownHeat, OUTPUT);          //Shutdown output
    digitalWrite(shutdownHeat, LOW);        //sets the pin to low

    pinMode(enableHeat, OUTPUT);            //Shutdown Reset output
    digitalWrite(enableHeat, LOW);          //sets the pin to low

    pinMode(adminRelay, OUTPUT);            //VCC Relay for Latch
    digitalWrite(adminRelay, LOW);          //sets the pin to low

    //Particle.function("shutdownHeat", shutdown);      //Function - Shutdown Heat
   
    //Particle.function("enableHeat", enable);          //Function - Enable Heat

    Particle.variable("flameOn", flameOn);            //Variable - flameOn boolian
    Particle.variable("powerOn", powerOn);            //Variable - power boolian         
    Particle.connect();
}


void loop()
{
    Tracker::instance().loop();

    flameOn = (digitalRead(A4));  //boolian for if flame is on
    delay(5);
    powerOn = (digitalRead(A5));
    //powerOn = digitalRead(D5);  //boolian for if power on
    //powerOn = digitalRead(poweredOn);  //Test for digitalRead 

    if (digitalRead(D4) == HIGH)  //Run this sub loop while flame is on
    {
        delay(15000);
        if (previousFlameOn == LOW)     //only run once 
        {
            
            heatStartTime = millis();   //Store current millis for flame on
            delay(250);
        }
        
        if (millis() - lastPublish > delayMinutes*60*1000) // if the current time - the last time we published is greater than your set delay...
            {
                    lastPublish = millis();
                    waitFor(Particle.connected, 10000);
                    delayMinutes = 600;
            }
            
    }
    
    else
    {
        if (previousFlameOn == HIGH)    //only run once
        {
            heatStopTime = millis();    //Store current millis for flame off
            deltaHeatTime = heatStopTime - heatStartTime;   //calculate delta heat time
            deltaHeatTime = ((deltaHeatTime / 1000) / 60);
            heatTime = String(deltaHeatTime);
            waitFor(Particle.connected, 10000);
            Particle.publish("hrs", heatTime, PRIVATE);  //publish delta heat time
            delay(3000);
            delayMinutes = 1;
        }
        
        if (isPoweredOn == LOW)
            {
            if (previousPowerOn == HIGH)
                {
                sleepTime = millis();
                delayMinutes = 1;
                }
            
            if (sleepTime + (sleepDelay*60*60*1000) < millis())
                {
                TrackerSleep::instance().wakeFor(D5,RISING);            //OLD code System.sleep(D5,RISING);
                delay(1000);
                System.reset();
                }
        
            }    
        
    }

    previousFlameOn = flameOn;  //change flame on state
    previousPowerOn = powerOn;  //change power on state
    
}

int shutdown(String command)   //Command to Shutdown with the "set" on latching relay
    {
        if (command == "1")                     //requests are case sensitive...Program accepts "off", "Off", or "OFF" to shutdown heat
        {
            digitalWrite(adminRelay, HIGH);     //Power the VCC relay 
            delay(1000);
            digitalWrite(shutdownHeat, HIGH);   //Power to latching relay
            delay(50);
            digitalWrite(shutdownHeat, LOW);    //Turn off latching signal
            digitalWrite(adminRelay, LOW);      //Turn off VCC relay
        }
        return 1;                               //If heat was shutdown, return "1"
    }
    
 int enable(String command)   //Command to Shutdown with the "set" on latching relay
    {
        if (command == "1")                     //requests are case sensitive...Program accepts "off", "Off", or "OFF" to shutdown heat
        {
            digitalWrite(adminRelay, HIGH);     //Power the VCC relay
            delay(1000);
            digitalWrite(enableHeat, HIGH);     //Power the latching relay
            delay(50);
            digitalWrite(enableHeat, LOW);      //Turn off latching signal
            digitalWrite(adminRelay, LOW);      //Turn off the VCC relay                       
        }
        return 1;                               //If heat was shutdown, return "1"
    }
    

