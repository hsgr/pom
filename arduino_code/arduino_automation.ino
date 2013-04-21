/*
This is the Arduino Uno program responsible for managing the
Popeye on Mars greenhouse mechanisms.
 */
/*
functionality implemented:
In casual mode:
- properly controlling electrical heating according to simulated
temperature sensor input
- properly controlling nutrient solution acidity according to simulated
pH sensor input
- properly stabilising dome pressure accordin to simulated input
In drying mode:
-TODO
*/
// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
int led = 13;

/*
System state is:
50 for casual operation
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
const unsigned long ph_doser_timeout=1500;
unsigned long ph_action_timestamp;

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
//Clean the valve in daybreak
boolean todo_clean_o2_valve=false;
unsigned long o2_valve_cleaning_timestamp;
const unsigned long o2_valve_cleaning_interval=1000;

// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);    
  
  // start dbg serial port at 9600 bps, no need to wait
  Serial.begin(9600);
  
  ph_action_timestamp=millis();
}

// the loop routine runs over and over again forever:
void loop() {
  
  unsigned long cur_time=millis();
  if(system_state==50){
     
    //TODO determine daylight, timing
    
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
    
    //air pH mgmt
    float ph=get_ph();
    if(cur_time-ph_action_timestamp>ph_doser_timeout){
      if(ph>ph_hi_thr){
        doser_decrease_ph();
        ph_action_timestamp=millis();
      }
      else if(ph<ph_lo_thr){
        doser_increase_ph();
        ph_action_timestamp=millis();
      }
    }
    
    //dome pressure mgmt
    float pres=get_dome_pressure();
     
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
    //of pumipng in or out, check when to stop pumping
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
    
    //o2 concentration mgmt
    float o2_dens=get_02_density();
    //TODO determine daylight for cleaning timing
    
    
    if(daylight){
      //day
      if(todo_clean_o2_valve){
        todo_clean_o2_valve=false;
        o2_valve_cleaning_timestamp=millis();
        update_o2_valve_state(1);
      }
      
      if(cur_time-o2_valve_cleaning_timestamp<o2_valve_cleaning_interval){
        //cleaning time
        if(o2_valve_state!=1){
          update_o2_valve_state(1);
        }
      }
      else{
        //stable day o2 concentration is storing or not
        if(o2_valve_state==0){
          if(o2_dens>max_day_o2_density){
            //start valve
            update_o2_valve_state(-1);
          }
        }
        else if(o2_valve_state==-1){
          if(o2_dens<min_day_o2_density){
            update_o2_valve_state(0);
          } 
        }
        else if(o2_valve_state==1){
          update_o2_valve_state(0);
        }
      }
    }
    else{
      //night
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
    
    //report debug
    Serial.print(temp);
    Serial.print(' ');
    Serial.println(ph);
    Serial.println(' ');
    Serial.println(pres);
    
    delay(100);
  }
  else{
    Serial.println("dafuq");
  }
}

double get_temperature(){
  return analogRead(A0)/25.6;
}

void start_heating(){
  digitalWrite(led,HIGH);
}

void stop_heating(){
  digitalWrite(led,LOW);
}

float get_ph(){
  return analogRead(A1)/1024.0*10.0;
}

void doser_increase_ph(){
  //TODO
}
void doser_decrease_ph(){
  //TODO
}

//dome mgmt interface
float get_dome_pressure(){
  //TODO
  return analogRead(A2)/1024.0*40.0;
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
float get_02_density(){
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

//end 02 concentration mgmt interface

//daylight sensing interface
bool get_daylight(){
  return analogRead(A4)>500;
}
//end daylight sensing interface
