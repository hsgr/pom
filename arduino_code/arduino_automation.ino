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
- properly stabilising dome pressure accordin to simulated inpu
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

//true for resistor heating on
boolean res_mode=false;

const double max_heat_temp=20.0;//Celsius
const double min_noheat_temp=10.0;

const float ph_hi_thr=5.3;//pH units
const float ph_lo_thr=5.2;
const int ph_doser_timeout=1500;
int ph_action_timestamp;

/*
-1 if pumping air inside
0 if idle
1 if letting air flow outside
*/
int outer_valve_state=0;
const float max_pressure=27.0;//kPa
const float min_pressure=22.0;
const float max_pressure_pumpin=26.0;
const float min_pressure_pumpout=23.0;

// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);    
  
  // start serial port at 9600 bps:
  Serial.begin(9600);
  while (!Serial) {
     ; // wait for serial port to connect. Needed for Leonardo only
  }
  
  ph_action_timestamp=millis();
}

// the loop routine runs over and over again forever:
void loop() {
  
  int cur_time=millis();
  if(system_state==50){
   
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

float get_dome_pressure(){
  //TODO
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
