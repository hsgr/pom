/*
This is the Arduino program responsible for managing the
Popeye on Mars greenhouse hydroponics and shell mechanisms.
System programming by Sotirios Pangiotou,
peripheral drivers authored and provided by respective vendors
*/

/*Libraries*/
#include <SoftwareSerial.h>
#include <Wire.h>

/*Constants*/
boolean dbg_mode=true;

//Arduino I/O pin configuration
//Debug mapping
const int ph_rxpin=99;
const int ph_txpin=99;                                             
const int shell_close_pin=         5;
const int shell_open_pin=          6;
const int harvester_enable_pin=    2;
const int sprinkler_enable_pin=   99;
const int o2_valve_blow_pin=      99;
const int o2_valve_suck_pin=      99;
const int outer_valve_blow_pin=   99;
const int outer_valve_suck_pin=   99;
const int doser_increase_pin=     99;
const int doser_decrease_pin=     99;//currently n/c
const int heater_enable_pin=       4;

//plant growth interval is 40 days
const unsigned long farming_time_interval=
  40 * 24
  *(unsigned long)3600
  *(unsigned long)1000;

//harvesting interval is 20 mins
const unsigned long harvest_interval=20*60*1000UL;

//heating mgmt
const double max_heat_temp=23.0;//Celsius
const double min_noheat_temp=20.0;

//ph mgmt
const float ph_hi_thr=6.3;//pH units
const float ph_lo_thr=4.2;

//pressure mgmt
const float max_pressure=27.0;//kPa
const float min_pressure=22.0;
const float max_pressure_pumpin=26.0;
const float min_pressure_pumpout=23.0;

//O2 mgmt
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

//humidity mgmt
const float max_humidity=0.8;
const float min_humidity=0.4;
const float max_humidity_sprinkle=0.75;
const float min_humidity_dehydrate=0.5;
const unsigned long dehydrating_leak_interval=3000;
const unsigned long dehydrating_idle_interval=500;
const unsigned long dehydrating_suck_interval=5000;

//final dehydration
const unsigned long final_dehy_interval=5*60*1000UL;
const unsigned long final_idle_interval=15*60*1000UL;

//daylight mgmt
const unsigned long daylight_hold_timeout=600;//ms
const float daylight_hi_thres=600.0;
const float daylight_lo_thres=400.0;

/*Automation status and timing variables*/

/*
System state is:
50 for casual operation
  (device is good to go once it has landed,in fact)
70 for harvesting start
  71 for harvesting in progress
90 for drying start
  91 for drying in progress (forever yay)
*/
int system_state=50;
unsigned long cur_time;

//when did we start producing?
unsigned long farming_start_timestamp;
//and harvesting?
unsigned long harvest_timestamp;

//is the environment in daylight state?
boolean daylight=true;
boolean daylight_last_measure=false;
unsigned long daylight_hold_timestamp;

//true for resistor heating on
boolean heating_on=false;

//dome pressure mgmt
/*
-1 if pumping air inside the dome
0 if idle
1 if letting air flow outside
*/
int outer_valve_state=0;

//O2 tank mgmt
/*-1 if pumping air in o2 storage,
0 if idle, 1 if letting air flow outside*/
int o2_valve_state=0;
/*
Drawing air in the O2 tank stater
0 for idle
1 for prep sucking by blowing
2 for prep sucking by waiting
3 for sucking O2 in
*/
int o2_valve_daylight_state=0;

//humudity mgmt
boolean sprinkler_on=false;
boolean dehydrating=false;//overrides normal pressure mgmt
boolean dehydrating_override=false;
/*dehydration sequence:
0 for leak, 1 for idle, 2 for pull, 3 for idle again*/
int dehydrating_state=0;
unsigned long dehydrating_timestamp;

//final dehydration
boolean final_dehy;
unsigned long final_dehy_timestamp;

SoftwareSerial ph_serial(ph_rxpin, ph_txpin);

// the setup routine runs once when you press reset:
void setup(){
  
  init_io();
  if(!dbg_mode)init_sensors();
  init_actuators();
  
  cur_time=millis();
  daylight_hold_timestamp=cur_time;
  farming_start_timestamp=cur_time;
}

void init_io(){
  
  if(dbg_mode){
    //activate debug serial
    Serial.begin(9600);
  }
  else{
    //activate production peripherl buses
    Wire.begin();
    ph_serial.begin(38400);
  }
  pinMode(sprinkler_enable_pin,OUTPUT);
  pinMode(shell_open_pin,OUTPUT);
  pinMode(shell_close_pin,OUTPUT);
  pinMode(harvester_enable_pin,OUTPUT);
  pinMode(o2_valve_blow_pin,OUTPUT);
  pinMode(o2_valve_suck_pin,OUTPUT);
  pinMode(outer_valve_blow_pin,OUTPUT);
  pinMode(outer_valve_suck_pin,OUTPUT);
  pinMode(doser_increase_pin,OUTPUT);
  pinMode(doser_decrease_pin,OUTPUT);
  pinMode(heater_enable_pin,OUTPUT); 
}

void init_sensors(){
  init_bmp085();
  init_vcl4000();
}

void init_actuators(){
  digitalWrite(sprinkler_enable_pin,LOW);
  digitalWrite(shell_open_pin,LOW);
  digitalWrite(shell_close_pin, LOW);
  digitalWrite(harvester_enable_pin,LOW);
  digitalWrite(o2_valve_blow_pin,LOW);
  digitalWrite(o2_valve_suck_pin,LOW);
  digitalWrite(outer_valve_blow_pin,LOW);
  digitalWrite(outer_valve_suck_pin,LOW);
  digitalWrite(doser_increase_pin,LOW);
  digitalWrite(doser_decrease_pin,LOW);
  digitalWrite(heater_enable_pin,LOW);
}

//here is the heart of all automation
void loop(){
  cur_time=millis();
  float lux=get_daylight();
  float temp = get_temperature();
  float ph=get_ph(temp);
  float pres=get_dome_pressure();
  float o2_dens=get_o2_density();
  float humi=get_humidity();
  
  if(system_state==50){
    
    daylight_mgmt(lux);
    temperature_mgmt(temp);
    ph_mgmt(ph);
    if(!dehydrating){
      pressure_mgmt(pres);
    }
    o2_mgmt(o2_dens);
    humidity_mgmt(humi);
    
    //report debug
    if(dbg_mode){
      Serial.print(system_state);
      Serial.print(' ');
      Serial.print(lux);
      Serial.print(' ');
      Serial.print(temp);
      Serial.print(' ');
      Serial.print(ph);
      Serial.print(' ');
      Serial.print(pres);
      Serial.print(' ');
      Serial.print(o2_dens);
      Serial.print(' ');
      Serial.print(humi);
      Serial.print(' ');
      Serial.println(temp);
    }
    
    if(cur_time-farming_start_timestamp>farming_time_interval){
      system_state=70;
    }
    
    delay(300);
  }
  else if(system_state==70){
    start_harvester();
    stop_doser();
    stop_sprinkler();
    update_o2_valve_state(0);
    
    harvest_timestamp=millis();
    system_state=71;
  }
  else if(system_state==71){
    if(cur_time-harvest_timestamp>harvest_interval){
      stop_harvester();
      dehydrating_state=0;
      dehydrating=true;
      system_state=90;
      final_dehy_timestamp=cur_time;
      final_dehy=true;
    }
    delay(1000);
  }
  else if(system_state==90){
    temperature_mgmt(temp);
    daylight_mgmt(lux);
    final_humi_mgmt(pres);
    delay(500);
  }
  else{
    Serial.println("dafuq");
  }
}

void daylight_mgmt(float lux){
  //determine daylight
  
  if(daylight_last_measure&&lux<daylight_lo_thres){
    daylight_last_measure=false;
    daylight_hold_timestamp=cur_time;
  }
  else if(!daylight_last_measure&&lux>daylight_hi_thres){
    daylight_last_measure=true;
    daylight_hold_timestamp=cur_time;
  }
  else if(cur_time-daylight_hold_timestamp>daylight_hold_timeout){
    daylight=daylight_last_measure;
  }
  if(daylight){
    open_shell();
  }
  else{
    close_shell();
  }
}

void temperature_mgmt(float temp){
  //master temperature mgmt
  if(heating_on){
    if(temp>max_heat_temp){
      heating_on=false;
      stop_heating();
    }
  }
  else{
    if(temp<min_noheat_temp){
      heating_on=true;
      start_heating();
    }
  }
}

void ph_mgmt(float ph){
  //nutrient solution pH mgmt
  if(ph>ph_hi_thr){
    doser_decrease_ph();
  }
  else if(ph<ph_lo_thr){
    doser_increase_ph();
  }
}

void pressure_mgmt(float pres){
  //dome pressure mgmt
  
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

void o2_mgmt(float o2_dens){
  //o2 concentration mgmt
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
}

void humidity_mgmt(float humi){
  //humidity mgmt
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
}

//final dehydration, preservation state
void final_humi_mgmt(float pres){
  if(final_dehy){
    if(cur_time-final_dehy_timestamp>final_dehy_interval){
      //go idle
      final_dehy=false;
      final_dehy_timestamp=cur_time;
      update_outer_valve_state(0);
    }
    else{
      //dehumidifying the atmosphere, won't bother with rem.oxygen
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
    }
  }
  else{
    if(cur_time-final_dehy_timestamp>final_dehy_interval){
      final_dehy=true;
      final_dehy_timestamp=cur_time;
      update_outer_valve_state(1);
    }
    //normal pressure mgmt so dome
    //does not accidentally implode or something
    pressure_mgmt(pres);
  }
}

//Returns temperature in deg.Celsius
double get_temperature(){
  // bmp085_b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
  // so ...Temperature(...) must be called before ...Pressure(...).
  if(dbg_mode)return analogRead(A0)/10.23;
  return bmp085GetTemperature(bmp085ReadUT())*0.1;
}

void start_heating(){
  digitalWrite(heater_enable_pin,HIGH);
}

void stop_heating(){
  digitalWrite(heater_enable_pin,LOW);
}

float get_ph(float temp){
  if(dbg_mode)return analogRead(A1)/102.3;
  else return ph_stamp_fetch_ph(temp);
}

void doser_increase_ph(){
  digitalWrite(doser_increase_pin,HIGH);
  digitalWrite(doser_decrease_pin,LOW);
}
void doser_decrease_ph(){
  digitalWrite(doser_increase_pin,LOW);
  digitalWrite(doser_decrease_pin,HIGH);
}
void stop_doser(){
  digitalWrite(doser_increase_pin,LOW);
  digitalWrite(doser_decrease_pin,LOW);
}

//dome pressure mgmt interface
//Returns presure in kPa
float get_dome_pressure(){
  if(dbg_mode)return analogRead(A2)/10.23;
  return bmp085GetPressure(bmp085ReadUP())*0.001;
}

void update_outer_valve_state(int set){
  set_outer_valve_state(set);
  outer_valve_state=set;
}

void set_outer_valve_state(int set){
  switch(set){
    case -1:
      digitalWrite(outer_valve_blow_pin,LOW);
      digitalWrite(outer_valve_suck_pin,HIGH);
    break;
    case 0:
      digitalWrite(outer_valve_blow_pin,LOW);
      digitalWrite(outer_valve_suck_pin,LOW);
    break;
    case 1:
      digitalWrite(outer_valve_blow_pin,HIGH);
      digitalWrite(outer_valve_suck_pin,LOW);
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
  switch(set){
    case -1:
      digitalWrite(o2_valve_blow_pin,LOW);
      digitalWrite(o2_valve_suck_pin,HIGH);
    break;
    case 0:
      digitalWrite(o2_valve_blow_pin,LOW);
      digitalWrite(o2_valve_suck_pin,LOW);
    break;
    case 1:
      digitalWrite(o2_valve_blow_pin,HIGH);
      digitalWrite(o2_valve_suck_pin,LOW);
    break;
    default:break;
  }
}

//end o2 concentration mgmt interface
//humidity mgmt interface
float get_humidity(){
  if(dbg_mode)return analogRead(A4)/1023.0;
  byte _status;
  unsigned int H_dat, T_dat;
  float RH;
  _status = hih6130_fetch_humidity_temperature(&H_dat, &T_dat);
  RH = (float) H_dat * 6.10e-3;
  return RH;
}
void set_sprinkler(){
  digitalWrite(sprinkler_enable_pin,HIGH);
}
void stop_sprinkler(){
  digitalWrite(sprinkler_enable_pin,LOW);
}
//end humidity interface
//daylight sensing interface
float get_daylight(){
  if(dbg_mode)return analogRead(A5)/1.023;
  return vcnl4000_readAmbient()/10.0;
}
//end daylight sensing interface

//motors interface
void open_shell(){
  digitalWrite(shell_close_pin,LOW);
  digitalWrite(shell_open_pin,HIGH);
}
void close_shell(){
  digitalWrite(shell_open_pin,LOW);
  digitalWrite(shell_close_pin,HIGH);
}
void start_harvester(){
  digitalWrite(harvester_enable_pin,HIGH);
}
void stop_harvester(){
  digitalWrite(harvester_enable_pin,LOW);
}
//end motor interface
