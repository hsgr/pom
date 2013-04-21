#include <SoftwareSerial.h>
#include <Wire.h>

/*
This is the Arduino Mega program responsible for managing the
Popeye on Mars greenhouse mechanisms.
 */
/*
functionality implemented:
In casual mode:
- properly controlling electrical heating according to simulated
temperature sensor input
- properly controlling nutrient solution acidity according to simulated
pH sensor input
- properly stabilising dome pressure, rel.humidity according to simulated input
In drying mode:
-TODO
*/

const int VCNL4000_ADDRESS=0x13; // 0x26 write, 0x27 read

// VCNL4000 Register Map
const int COMMAND_0=0x80;  // starts measurments, relays data ready info
const int PRODUCT_ID =0x81;  // product ID/revision ID, should read 0x11
const int IR_CURRENT =0x83;  // sets IR current in steps of 10mA 0-200mA
const int AMBIENT_PARAMETER =0x84;  // Configures ambient light measures
const int AMBIENT_RESULT_MSB =0x85;  // high byte of ambient light measure
const int AMBIENT_RESULT_LSB =0x86;  // low byte of ambient light measure
const int PROXIMITY_RESULT_MSB =0x87;  // High byte of proximity measure
const int PROXIMITY_RESULT_LSB= 0x88;  // low byte of proximity measure
const int PROXIMITY_FREQ= 0x89;  // Proximity IR test signal freq, 0-3
const int PROXIMITY_MOD =0x8A;  // proximity modulator timing


// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
int led = 13;

unsigned int farming_start_timestamp;
unsigned int farming_time_interval = 40*24*3600*1000;

/*
System state is:
50 for casual operation
70 for harvesting
90 for drying start
91 for drying in progress
*/
int system_state=50;

//is the env in daylight state?
boolean daylight=true;

//true for resistor heating on
boolean res_mode=false;

const double max_heat_temp=20.0;//Celsius
const double min_noheat_temp=10.0;

const float ph_hi_thr=5.3;//pH units
const float ph_lo_thr=5.2;

/*
-1 if pumping air inside the dome
0 if idle
1 if letting air flow outside
*/
int outer_valve_state=0;
const float max_pressure=27.0;//kPa
const float min_pressure=22.0;
const float max_pressure_pumpin=26.0;
const float min_pressure_pumpout=23.0;

/*
-1 if pumping air in o2 stroage
0 if idle
1 if letting air flow outside
*/
int o2_valve_state=0;
/*Stop or set o2 output during night, o2 lvl falls naturally*/
const float max_night_o2_density=0.2;
const float min_night_o2_density=0.1;
/*Stop or set 02 input during day, o2 lvl rises naturally*/
const float max_day_o2_density=0.2;
const float min_day_o2_density=0.1;
//Clean the valve before sucking O2 in
unsigned long o2_valve_cleaning_timestamp;
const unsigned long o2_valve_clean_blow=1000;
const unsigned long o2_valve_clean_idle=500;
/*
0 for idle
3 for sucking
1 for prep sucking by blowing
2 for prep sucking by waiting
*/
int o2_valve_daylight_state=0;

const float max_humidity=0.8;
const float min_humidity=0.4;
const float max_humidity_sprinkle=0.75;
const float min_humidity_dehydrate=0.5;
boolean sprinkler_on=false;
boolean dehydrating=false;
boolean dehydrating_override=false;
/*
0 for leak
1 for idle
2 for pull
3 for idle
*/
int dehydrating_state=0;
unsigned long dehydrating_timestamp;
const unsigned long dehydrating_leak_interval=3000;
const unsigned long dehydrating_idle_interval=500;
const unsigned long dehydrating_suck_interval=5000;

//daylight
boolean daylight_last_measure=false;
unsigned long daylight_hold_timestamp;
const unsigned int daylight_hold_timeout=60000;//ms
const float daylight_hi_thres=600.0;
const float daylight_lo_thres=400.0;

const int ph_rxpin=2;                                                  
const int ph_txpin=3;                                             
SoftwareSerial ph_serial(ph_rxpin, ph_txpin);

// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);    
  
  // start dbg serial port at 9600 bps, no need to wait
  Serial.begin(9600);
  
  Wire.begin();
  
  bmp085Calibration();
  
  ph_serial.begin(38400);
  
  
  /* Test that the device is working correctly */
  byte temp = vcl4000_read_byte(PRODUCT_ID);
  if (temp != 0x11)  // Product ID Should be 0x11
  {
    Serial.print("Something's wrong. Not reading correct ID: 0x");
    Serial.println(temp, HEX);
  }
  else
    Serial.println("VNCL4000 Online...");
  
  /* Now some VNCL400 initialization stuff
     Feel free to play with any of these values, but check the datasheet first!*/
  vcl4000_write_byte(AMBIENT_PARAMETER, 0x0F);  // Single conversion mode, 128 averages
  vcl4000_write_byte(IR_CURRENT, 20);  // Set IR current to 200mA
  vcl4000_write_byte(PROXIMITY_FREQ, 2);  // 781.25 kHz
  vcl4000_write_byte(PROXIMITY_MOD, 0x81);  // 129, recommended by Vishay

  
  unsigned int cur_time=millis();
  daylight_hold_timestamp=cur_time;
  farming_start_timestamp=cur_time;
  
}

// the loop routine runs over and over again forever:
void loop(){
  
  unsigned long cur_time=millis();
  if(system_state==50){
     
    //determine daylight
    float lux=get_daylight();
    if(daylight_last_measure&&lux<daylight_lo_thres){
      daylight_last_measure=false;
      daylight_hold_timestamp=cur_time;
    }
    else if(!daylight_last_measure&&lux>daylight_hi_thres){
      daylight_last_measure=true;
      daylight_hold_timestamp=cur_time;
    }
    else if(cur_time-daylight_hold_timestamp>daylight_hold_timeout){
      if(daylight!=daylight_last_measure){
        if(daylight_last_measure){
          open_shell();
        }
        else{
          close_shell();
        }
      }
      daylight=daylight_last_measure;
    }
   
    
    //master temperature mgmt
    float temp = get_temperature();
    if(res_mode){
      if(temp>max_heat_temp){
        res_mode=false;
        stop_heating();
      }
    }
    else{
      if(temp<min_noheat_temp){
        res_mode=true;
        start_heating();
      }
    }
    
    //nutrient solution pH mgmt
    float ph=get_ph(temp);
    
    if(ph>ph_hi_thr){
      doser_decrease_ph();
    }
    else if(ph<ph_lo_thr){
      doser_increase_ph();
    }
    
    //dome pressure mgmt
    float pres=get_dome_pressure();
    if(!dehydrating){ 
      if(outer_valve_state==0){
        if(pres>max_pressure){
          update_outer_valve_state(1);
        }
        else if(pres<min_pressure){
          //get some air inside
          //WARNING this will alter the atmo composition
          update_outer_valve_state(-1);
        }
      }
      //of pumping in or out, check when to stop pumping
      else if(outer_valve_state==-1){
        if(pres>max_pressure_pumpin){
          update_outer_valve_state(0);
        }
      }
      else if(outer_valve_state==1){
        if(pres<min_pressure_pumpout){
          update_outer_valve_state(0);
        }
      }
    } 
    
    //o2 concentration mgmt
    float o2_dens=get_o2_density();
    if(daylight){
      if(o2_dens<min_day_o2_density){
        dehydrating_override=true;
      }
      else{
        dehydrating_override=false;
      }
      //day
      if(o2_valve_daylight_state==0){
        if(o2_dens>max_day_o2_density){
          //initiate suction sequence
          update_o2_valve_state(1);
          o2_valve_cleaning_timestamp=cur_time;
          o2_valve_daylight_state=1;
        }
      }
      else if(o2_valve_daylight_state==1){
        if(cur_time-o2_valve_cleaning_timestamp>o2_valve_clean_blow){
          update_o2_valve_state(0);
          o2_valve_cleaning_timestamp=cur_time;
          o2_valve_daylight_state=2;
        }
      }
      else if(o2_valve_daylight_state==2){
        if(cur_time-o2_valve_cleaning_timestamp>o2_valve_clean_idle){
          update_o2_valve_state(-1);
          o2_valve_daylight_state=3;
        }
      }
      else if(o2_valve_daylight_state==3){
        if(o2_dens<min_day_o2_density){
            update_o2_valve_state(0);
            o2_valve_daylight_state=0;
          }
      }
    }
    else{
      //night
      if(o2_dens<min_night_o2_density){
        dehydrating_override=true;
      }
      else{
        dehydrating_override=false;
      }
      if(o2_valve_state==0){
        if(o2_dens<min_night_o2_density){
          update_o2_valve_state(1);
        }
      }
      else if(o2_valve_state==1){
        if(o2_dens>max_night_o2_density){
          update_o2_valve_state(0);
        }
      }
      else if(o2_valve_state==-1){
        update_o2_valve_state(0);
      }
    }
    
    //humidity mgmt
    //nutrient solution pH mgmt
    float humi=get_humidity();
    //sprinkler
    if(sprinkler_on){
      if(humi>max_humidity_sprinkle){
        stop_sprinkler();
         sprinkler_on=false;
      }
    }
    else{
      if(humi<min_humidity){
        set_sprinkler();
        sprinkler_on=true;
      }
    }
    //dehydration
    if(!dehydrating){
      if(humi>max_humidity){
        dehydrating=true;
        dehydrating_state=0;
        dehydrating_timestamp=cur_time;
        update_outer_valve_state(1);
      }
    }
    else{
      if(dehydrating_override){
        update_outer_valve_state(0);
      }
      else{
        if(dehydrating_state==0){
          if(cur_time-dehydrating_timestamp>dehydrating_leak_interval){
            update_outer_valve_state(0);
            dehydrating_timestamp=cur_time;
            dehydrating_state=1;
          }
        }
        else if(dehydrating_state==1){
          if(cur_time-dehydrating_timestamp>dehydrating_idle_interval){
            update_outer_valve_state(-1);
            dehydrating_timestamp=cur_time;
            dehydrating_state=2;
          }
        }
        else if(dehydrating_state==2){
          if(cur_time-dehydrating_timestamp>dehydrating_suck_interval){
            update_outer_valve_state(0);
            dehydrating_timestamp=cur_time;
            dehydrating_state=3;
          }
        }
        else if(dehydrating_state==3){
          if(cur_time-dehydrating_timestamp>dehydrating_idle_interval){
            update_outer_valve_state(1);
            dehydrating_timestamp=cur_time;
            dehydrating_state=0;
          }
        }
        if(humi<min_humidity_dehydrate){
          dehydrating=false;
          update_outer_valve_state(0);
        }
      }
    }
    
    if(daylight){
      open_shell();
    }
    else{
      close_shell();
    }
    
    //report debug
    Serial.print(temp);
    Serial.print(' ');
    Serial.println(ph);
    Serial.println(' ');
    Serial.println(pres);
    Serial.println(' ');
    Serial.println(o2_dens);
    
    if(cur_time-farming_start_timestamp>farming_time_interval){
      system_state=70;
    }
    
    delay(100);
  }
  else if(system_state==70){
    start_harvester();
  }
  else{
    Serial.println("dafuq");
  }
}

// bmp085_b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
//Returns temperature in deg.Celsius
double get_temperature(){
  return bmp085GetTemperature(bmp085ReadUT())*0.1;
}

void start_heating(){
  digitalWrite(led,HIGH);
}

void stop_heating(){
  digitalWrite(led,LOW);
}

float get_ph(float temp){
  char sres[50];
  
  ph_serial.print(temp);
  
  ph_serial.print("R\r");
  int i=0;
  boolean str_complete=false;
  while(!str_complete){
    while(ph_serial.available()){
      sres[i]=ph_serial.read();
      if(sres[i]=='\r')str_complete=true;
      i++;
    }
  }
  if(sres[0]='c'){
    //NOTE error
    return 7.0;
  }
  else return (sres[0]-'0')*10.0+(sres[1]-'0')*1.0+
              (sres[3]-'0')*0.1+(sres[4]-'0')*0.01;
}

void doser_increase_ph(){
  //TODO
}
void doser_decrease_ph(){
  //TODO
}

//dome pressure mgmt interface
//Returns presure in kPa
float get_dome_pressure(){
  return bmp085GetPressure(bmp085ReadUP())*0.001;
}

void update_outer_valve_state(int set){
  set_outer_valve_state(set);
  outer_valve_state=set;
}

void set_outer_valve_state(int set){
  //TODO
  switch(set){
    case -1:
    
    break;
    case 0:
    
    break;
    case 1:
    
    break;
    
    default:break;
  }
}
//end dome mgmt interface

//o2 concentration mgmt interface
float get_o2_density(){
  //TODO
  return analogRead(A3)/1024.0;
}

void update_o2_valve_state(int set){
  set_o2_valve_state(set);
  o2_valve_state=set;
}

void set_o2_valve_state(int set){
  //TODO
  switch(set){
    case -1:
    
    break;
    case 0:
    
    break;
    case 1:
    
    break;
    
    default:break;
  }
}

//end o2 concentration mgmt interface
//humidity mgmt interface
float get_humidity(){
  byte _status;
   unsigned int H_dat, T_dat;
   float RH, T_C;
   
    _status = fetch_humidity_temperature(&H_dat, &T_dat);     
    
    RH = (float) H_dat * 6.10e-3;
    return RH;
}
void set_sprinkler(){
  //TODO
}
void stop_sprinkler(){
  //TODO
}
//end humidity interface
//daylight sensing interface
float get_daylight(){
  unsigned int res=readAmbient();
  return res/10.0;
}
//end daylight sensing interface

//motors interface
void open_shell(){
  //TODO
}
void close_shell(){
  //TODO
}
void start_harvester(){
  //TODO
}
//end motor interface
