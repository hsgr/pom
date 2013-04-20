/*
This is the Arduino Uno program responsible for managing the
Popeye on Mars greenhouse mechanisms.
 */
/*
functionality implemented:
- properly controlling electrical heating according to simulated
temperature sensor input
*/
// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
int led = 13;

boolean res_mode=false;

const int max_heat_temp=1000;

const int min_noheat_temp=100;

// the setup routine runs once when you press reset:
void setup() {                
  // initialize the digital pin as an output.
  pinMode(led, OUTPUT);    
  
  // start serial port at 9600 bps:
  Serial.begin(9600);
  while (!Serial) {
     ; // wait for serial port to connect. Needed for Leonardo only
  }
  
}

// the loop routine runs over and over again forever:
void loop() {
  if(res_mode){
    digitalWrite(led, HIGH);   // turn the LED on (HIGH is the voltage level)
  }
  else{
    digitalWrite(led,LOW);
  }
  delay(100);               // wait for a second
  int val = get_temperature();
  Serial.println(val);             // debug value
  
  if(res_mode){
    if(val>max_heat_temp){
      res_mode=false;
    }
  }
  else{
    if(val<min_noheat_temp){
      res_mode=true;
    }
  }
}

int get_temperature(){
  return analogRead(A0);
}
