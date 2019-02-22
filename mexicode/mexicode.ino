// This #include statement was automatically added by the Particle IDE.
#include "PhoBot.h"
//current version is trying to have data sent to ARB in influx (commented out at end) and readings are frequent 
//Took out fcl read function since don't need it for Mexico
//The code to run the sensor off the PhoBot and solenoid valve
//for sensors and pressure together

// don't actually know what the 9.0 are forPhoBot p = PhoBot(9.0, 9.0); some examples have it as that, others with ()

PhoBot p = PhoBot();

//#include <Wire.h>                 //enable I2C.
#define pH_address 99               //default I2C ID number for EZO pH Circuit.
#define ORP_address 98              //default I2C ID number for EZO ORP Circuit
#define EC_address 100              //default I2C ID number for EZO EC Circuit
#define RTD_address 102             //default I2C ID number for EZO Temp Circuit
#define pres_pin A0                //sets analog pin number for pressure sensor 

// Client to communicate to influx
TCPClient client;

#define POSTNAME ""
#define HOSTNAME ""
#define HOSTNUM 0

//creates a vector of the sensor address numbers
const int address[]={RTD_address,ORP_address,EC_address,pH_address};
// creates a string of the sensor names that will be how it's reported to grafana for FROM
String sensors[4] = {"Temp", "ORP", "EC", "pH"};
// creats a string for the different measurements the EC sensor takes and how it's reported to grafana for FROM
String econd[4] = {"EC", "TDS", "SAL", "SG"};


//This part came with the sample code
char computerdata[40];              //we make a 20 byte character array to hold incoming data from a pc/mac/other.
byte received_from_computer = 0;    //we need to know how many characters have been received.
byte code = 0;                      //used to hold the I2C response code.


char _data[200];                    //we make a 40-byte character array to hold incoming data from any circuit.
byte in_char = 0;                   //used as a 1 byte buffer to store inbound bytes from the pH Circuit.


char *ec;                       //char pointer used in string parsing.
char *tds;                      //char pointer used in string parsing.
char *sal;                      //char pointer used in string parsing.
char *sg;                       //char pointer used in string parsing.

double adc = 0;                 //variable used for analog to digital conversion
float mv=0;                     //millivolts measured from the ADC

//double volts = 0.0;             //For Phobot usage


//parameters to change as pleased for timing
/* easy change parameters
.
..
...
....
.....
......
.......
........
.......
......
.....
....
...
..
.
*/
int zzz = (5) * 60 - 5 - 2;        //sleep time for (n) MINUTE sleep (multiples by 60 to change it into minutes actually and takes into acount ~5 seconds to power on and connect with -5 and 2 seconds for pressure delay)
double flush = 5;               //initializes number of SECONDS to have valve on
int awake = (30);               //how many SECONDS the photon will remain awake to recieve updates or whatever
int p_thresh = 20;              //pressure (psi) threshold for valve to open 
String node_id = "MEX005";
/*
.
..
...
....
.....
......
.......
........
.......
......
.....
....
...
..
.
*/


void setup()                    
//hardware initialization.
{
    pinMode(pres_pin, INPUT);         //set pin as an input for the pressure transducer
    //pinMode(D7, OUTPUT);  //commented out because not using beef cake    //Sets digital pin 4 as output for the relay switch to motor
    Serial.begin(9600);           //enable serial port.
    Wire.begin();                 //enable I2C port.
    
    //sets sensors to sleep
    for (int i=0; i < 4; i++){    //Initialize for-loop to address all 4 (0,1,2,3) positions of the circuits address[] array (goes through all the sensor ID's)
            
            Wire.beginTransmission(address[i]); //call the circuit by its ID number.
            Wire.write("Sleep");                //transmit the command sleep to lower power consumption..
            Wire.endTransmission();             //end the I2C data transmission.
    }
    
}




void loop() {   
//the main loop basically just sets up to take readings and exchange water once an hour and to wake up every 15 minutes for updates
//tips: System.sleep takes in and argument of time in SECONDS so you don't have to *1000 but delay takes in argument of time in MILISECONDS, hence (time in seconds desired)*1000 to convert  
    
    for(int t = 0; t < 12; t++){         // sets hourly cycle (5 minute sleeps if t < 12, 15 minute sleeps if t < 4)
        //loop for 20 minutes (because 3 readings per minute * 20 minutes = 60), taking readings every 20 seconds
        p.setMotors("M3-B-100");    //send pulse to turn off valve
        delay(1000);
        p.setMotors("M3-S");
        
        if (t == 0) {
        //start of hour cycle
            p.setMotors("M1-F-100");            //Turns on pressure
            float press = take_pressure();                    //Takes pressure reading on the water pipes. Eventually if pressure is available it will trigger the valve and then take WQ readings.
            p.setMotors("M1-S");                //shutoff pressure supply
            
            if (press > p_thresh) {                   //pres > n where n is the threshold pressure in psi for the valve to open
                water_exchange(flush);              //cycles solution for n SECONDS at begining of hour cycle (where n is the number in the '()')
                delay(2*60*1000);                   //stay on for 2 minutes to adjust to new water solution then take readings
                take_reading();                     //takes reading, about 9 second time usage
                System.sleep(D3,RISING,(zzz - flush - 9 - 2*60));            //takes into account time used for each function the D3, RISING is arbritrary
                //deep sleep is not used because then it would forget what iteration out of 12 it is in
            }
            
            else {
                delay((awake)*1000);        //stays on for remainder of awake seconds
                System.sleep(D3,RISING,(zzz - awake));            //takes into account time used for each function the D3, RISING is arbritrary
                //deep sleep is not used because then it would forget what iteration out of 12 it is in
            }
            
        }   
        
        else if (t == 11) {
            p.setMotors("M1-F-100");            //Turns on pressure
            take_pressure();                    //Takes pressure reading on the water pipes. Eventually if pressure is available it will trigger the valve and then take WQ readings.
            p.setMotors("M1-S");                //shutoff pressure supply
            
            delay(awake*1000);        //stays on for remainder of awake seconds
            
            System.sleep(SLEEP_MODE_DEEP, zzz - awake);   //deep sleep for zzz minutes, will do complete reset after time -- saves energy
            //deep sleep mode just saves on power, every little bit counts!
        }
        
        else{
            p.setMotors("M1-F-100");            //Turns on pressure
            take_pressure();                    //Takes pressure reading on the water pipes. Eventually if pressure is available it will trigger the valve and then take WQ readings.
            p.setMotors("M1-S");                //shutoff pressure supply
            
            delay(awake*1000);                      //Just have one for awake seconds
            System.sleep(D3,RISING,(zzz - awake));      //takes into account time used for each function 
        }
        //volts = p.batteryVolts();             //publishes battery level --- for Phobot
        //Particle.publish("battery voltage is: " + String(volts)); //publishes onto cloud
    }
            
   /*for(int del = 0; del < 11; del++) { //60  minutes loop --- CHECK THE MATH SINCE THIS IS OUTDATED - maybe one day I'll make algorithms to make it easier ¯\_(ツ)_/¯
        
        Particle.publish("Ready for commands");         //can see on particle console that the photon is infact on -- BUT MAYBE SHUT THIS OFF TO SAVE ON DATA
        delay(5000);
        
        //delay(10 *1000);              //awake for 10 seconds to take in new updates
        
        if(del == 0){                   //if at start of cycle
        //exchanges water and takes pH, ORP, EC, Temp   
            
            water_exchange(flush);           //exchanges old water out for new water for flush seconds at begining of hour cycle
            take_reading();                 //takes readings of pH, orp, ec, temp
            delay(20*1000); 
            System.sleep(D3,RISING,zzz);     //takes into account time used for each function 
            
        }
        
        
        else if( del != 10 && del != 1){ //as del = 10 is the last loop
            
            //take_pressure();
            take_reading();                 //takes readings of pH, orp, ec, temp
            System.sleep(D3,RISING,zzz); 
            //pins picked arbritarily, will put the whole system into sleep until time has elapsed
        }
        
        else{ 
        //meaning end of loop - it will go into deep sleep mode for however long is specified in zzz
                    take_reading();             //takes readings of pH, orp, ec, temp
            System.sleep(SLEEP_MODE_DEEP, zzz); // will sleep for zzz minutes
        }
    } 
    */
    
}




void take_reading() {
//takes readings of all the parameters (-pressure) of the continuously flowing system

    for(int read = 0; read < 3; read++){
    //takes 3 readings of each parameter ~ 9 seconds run time
        
        computerdata[0] = 'r';              //sends the command to read
        
        // TOOK OUT = in <= to make just < -- could be source of error if any arises but should still work the same
        for (int i=0; i < 4; i++){    //Initialize for-loop to address all 4 (0,1,2,3) positions of the circuits address[] array (goes through all the sensor ID's)
            
            
            Wire.beginTransmission(address[i]);     //call the circuit by its ID number.
            Wire.write(computerdata);               //transmit the read command.
            Wire.endTransmission();                 //end the I2C data transmission.
            
            if(i==1 || i==3){
                delay(800);                     //wait the correct amount of time for each specific circuit to complete its instruction.
            }
            
            else if(i==2){                      
                delay(600);
            }
            
            else if (i==0){
                delay(400);
            }
            
            //part of sample code will need arduino connected to computer to read these messages via the terminal
            Wire.requestFrom(address[i], 48, 1);    //call the circuit and request 48 bytes (this may be more than we need -- from https://www.arduino.cc/en/Reference/WireRead it might only send 1 byte)
            code = Wire.read();                     //the first byte is the response code, we read this separately.
            Serial.println(code);
            switch (code) {                         //switch case based on what the response code is.
                case 1:                        //decimal 1.
                Serial.println("Success");   //means the command was successful.
                break;                       //exits the switch case.
                
                case 2:                        //decimal 2.
                Serial.println("Failed");    //means the command has failed.
                break;                       //exits the switch case.
                
                case 254:                      //decimal 254.
                Serial.println("Pending");   //means the command has not yet been finished calculating.
                break;                       //exits the switch case.
                
                case 255:                      //decimal 255.
                Serial.println("No Data");   //means there is no further data to send.
                break;                       //exits the switch case.
            }
            
            for (int j=0; Wire.available(); j++) {   //are there bytes to receive.
                in_char = Wire.read();              //receive a byte. (ASCII number)
                _data[j] = in_char;                 //load this byte into our array.
                if (in_char == 0) {                 //if we see that we have been sent a null command.
                    Wire.endTransmission();         //end the I2C data transmission.
                    break;                          //exit the while loop.
                }
            }
            
            
            //since EC sensor takes four different types of readings but sends as one, must parse them
            if(i==2){
                //break up the EC ASCII by comma to get the four different measurements it acutally takes
                //Serial.println(_data);
                
                ec = strtok(_data, ",");                //let's pars the string at each comma.
                tds = strtok(NULL, ",");                //let's pars the string at each comma.
                sal = strtok(NULL, ",");                //let's pars the string at each comma.
                sg = strtok(NULL, ",");                 //let's pars the string at each comma.
                String edata[4] = {ec, tds, sal, sg};   //creates a string of the ASCII values from the EC sensor
                
                for(int j = 0; j < 4; j++){
                    createDataStream(econd[j],edata[j]);
                    Serial.println(econd[j]+" "+edata[j]);
                }
               
            }
            
           
            else{
            //if the sensor sending readings is not the EC sensor
                createDataStream(sensors[i], String(_data));
                Serial.println(sensors[i]+" "+_data);
            }
          
            delay(400);
            
            Wire.beginTransmission(address[i]); //call the circuit by its ID number.
            Wire.write("Sleep");                //transmit the command sleep to lower power consumption..
            Wire.endTransmission();             //end the I2C data transmission.
            
          
            memset (_data,0, sizeof(_data));    //clear _data for next reading
        }
    }
    
    
    //delay(24500); //delays so system is awake for 30 seconds total before sleeping incase of commands
    
}




void water_exchange(double n){
//turns valve on for  ~n seconds
    
    p.setMotors("M3-F-100");     //send pulse to turn on valve
    delay(1000);
    p.setMotors("M3-S");
    
    delay(n*1000);              //valve left open for n seconds
    
    //take_pressure();          //takes pressure reading WHILE valve is on - commented out since pressure sensor is not connected
    
    delay(300);
    
    p.setMotors("M3-B-100");    //send pulse to turn off valve
    delay(1000);
    p.setMotors("M3-S");
}



float take_pressure(){
//Takes pressure only reading 

    float pressure=0;                       //variable to assign pressure value
    delay(2*1000);                       //set delay for supply voltage to regulate
    adc = analogRead(pres_pin);         //read from the ADC
    mv= adc*0.000805;                   //convert ADC points to millivolts  
    pressure= (37.5*mv) - 12.5 + 20;    //convert millivolts to pressure in PSI and takes into account voltage divider (25*1.5) the +20 is to take into account that the supply voltage is 4V instead of 5V
    if(pressure <0){ pressure=0;}       //remove negative value if sensors is not under pressure   
    
    createDataStream("pressure",String(pressure));      //sends pressure reading to grafana
    
    //Serial.println("Pressure is " + pressure);
    return pressure;
}





//For ARB as database over _TEST
void createDataStream(String name, String meas){
//function to create the data string to send to influx 
    // Get current time in UNIX time 
    String tie = String(Time.now())+"000000000";
    //get device ID to use as tag
    //String myID = System.deviceID();
    // Create data string for the parameter to pass into write influx
    String data;
    data = name+",node_id="+node_id+" value="+meas+" "+tie;
    
        
    //write to influx and will try for 5 times before just taking a new reading
    for(int j=0; j<5; j++){
        if (writeinflux(data) == 1){
            Serial.println("A success!");
            break;
        }
        else{
            Serial.println("Trying again");
        }
    }
}




int writeinflux(String data){
//function that writes a measurement given as a string to influx and returns 1 if successful and 0 if failed

// Send data over to influx
    if(client.connect(HOSTNAME, HOSTNUM)){
    
        Serial.println("Connected");
        
        // Actual function
        client.printlnf("POST %s", POSTNAME);
        client.printlnf("HOST: %s:%d", HOSTNAME, HOSTNUM);
        client.println("User-Agent: Photon/1.0");
        client.printlnf("Content-Length: %d", data.length());
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.println();
        client.print(data);
        client.println();  
        client.flush();
        client.stop();
        

        return 1;
    }
    
    else
    {
        Serial.println("Connection Failed!");
    }
    
    return 0;
}


