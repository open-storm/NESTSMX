// This #include adds PhoBot header
#include "PhoBot.h"

//I2C EZO ID numbers
#define pH_address 99
#define ORP_address 98
#define EC_address 100
#define RTD_address 102
#define NUM_SENSORS 4

#define pres_pin A0                //sets analog pin number for pressure sensor 

#define PRES_READ_PERIOD 5  //defined in minutes. Take a pressure reading every 5 minutes
#define SENS_READ_PERIOD 60 //defined in minutes. Take a sensor reading every 60 minutes
#define AWAKE_TIME 30 //defined in seconds. How long to stay awake after taking reading
#define FLUSH_TIME 5  //defined in seconds. How long to turn valve on

#define NUM_READINGS 3
#define PRESSURE_THRESHOLD 20
#define NODE_ID "MEX007"

#define POSTNAME ""
#define HOSTNAME ""
#define HOSTNUM 0

// Client to communicate to influx
TCPClient client;

//Phobot to control peripherals
PhoBot p = PhoBot();

//creates a vector of the sensor address numbers
const int address[]={RTD_address,ORP_address,EC_address,pH_address};
// array of sensor names that will be how it's reported to grafana for FROM
String sensors[4] = {"Temp", "ORP", "EC", "pH"};
// creats a string for the different measurements the EC sensor takes and how it's reported to grafana for FROM
String econd[4] = {"EC", "TDS", "SAL", "SG"};

String measurements[4] = {"", "", "", ""};


char computerdata[40];              //we make an 40 byte character array to hold incoming data from a pc/mac/other.
byte code = 0;                      //used to hold the I2C response code.


char _data[200];                    //we make a 200-byte character array to hold incoming data from any circuit.
byte in_char = 0;                   //used as a 1 byte buffer to store inbound bytes from the pH Circuit.


char *ec;                       //char pointer used in string parsing.
char *tds;                      //char pointer used in string parsing.
char *sal;                      //char pointer used in string parsing.
char *sg;                       //char pointer used in string parsing.

double adc = 0;                 //variable used for analog to digital conversion
float mv=0;                     //millivolts measured from the ADC
int awake = (30);               //how many SECONDS the photon will remain awake to recieve updates or whatever

int presCounter = 0;
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

void loop(){
    unsigned long startTime = millis();
    //bootLegWake(); //wake up
    p.setMotors("M3-B-100");
    delay(1000);
    p.setMotors("M3-S");
    
    //start of hour cycle
    if (presCounter == 0){
        p.setMotors("M1-F-100");
        float press = take_pressure();
        p.setMotors("M1-S");
        
        if (press > PRESSURE_THRESHOLD){
            water_exchange(FLUSH_TIME);
            delay(2*60*1000); //delay 2 minutes
            take_reading();
        }
        else {
            delay(awake*1000); //stay on for awake seconds
        }
        
    }
    else{
        p.setMotors("M1-F-100");            //Turns on pressure
        take_pressure();                    //Takes pressure reading on the water pipes. Eventually if pressure is available it will trigger the valve and then take WQ readings.
        p.setMotors("M1-S");                //shutoff pressure supply
        delay(awake*1000);                      //Just have one for awake seconds
    }
    
    presCounter = (presCounter + 1) % (SENS_READ_PERIOD / PRES_READ_PERIOD);
    
    //TODO
    /* do full reset once our pressure counter hits 0 again to save more power
    //To do even past that, see if we can have presCounter saved somewhere to always deep sleep...it's 4 bytes of information -_-
    if (presCounter == 0){
        //deep sleep
    }
    else{
        //normal sleep
    }
    */ 
    //while boron sleep is unestablished, simply delay
    unsigned long timeElapsed= millis() - startTime; //gets time elapsed in milliseconds
    delay(PRES_READ_PERIOD*60*1000 - timeElapsed);
    
}


void take_reading() {
//takes readings of all the parameters (-pressure) of the continuously flowing system

    for(int read = 0; read < NUM_READINGS; read++){
    //takes 3 readings of each parameter ~ 9 seconds run time
        computerdata[0] = 'r';              //sends the command to read
        
        for (int i=0; i < NUM_SENSORS; i++){    //Initialize for-loop to address all 4 (0,1,2,3) positions of the circuits address[] array (goes through all the sensor ID's)
            
            if (i == 3){
                computerdata[1] = 't';
                strcpy(computerdata+2, measurements[0].c_str());
            }   

            Wire.beginTransmission(address[i]);     //call the circuit by its ID number.
            Wire.write(computerdata);               //transmit the read command.
            Wire.endTransmission();                 //end the I2C data transmission.
            
            if(i==1 || i==3){
                delay(900);                     //wait the correct amount of time for each specific circuit to complete its instruction.
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
            measurements[i] = _data;
            
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
            
          
            memset(_data,0, sizeof(_data));    //clear _data for next reading
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
    data = name+",node_id="+NODE_ID+" value="+meas+" "+tie;
    
        
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

//sleepTime given in Seconds
void bootLegSleep(int sleepTime){
    if (sleepTime <= 0){
        return;
    }
    Cellular.off();
    delay(sleepTime*1000);
    bootLegWake();
    
}

bool bootLegWake(){
    Cellular.on();
    Cellular.connect();
    while(Cellular.connecting());
    return true;
}

//timeOut given in seconds
bool bootLegWake(int timeOut){
    timeOut = timeOut *1000; //convert timeOut into milliseconds
    Cellular.on();
    Cellular.connect();
    int currTime = 0;
    int startTime = millis();
    while(Cellular.connecting() && currTime < timeOut){
        currTime = millis()-startTime;
    }
    return currTime < timeOut;
}




