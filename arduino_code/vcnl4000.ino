/* VCNL4000 Proximity and Ambient Light Sensor
   Example Code
   by: Jim Lindblom
   SparkFun Electronics
   date: August 30, 2011 (updated November 3, 2011)
   license: Beerware - Use this code however you want. If you find
   it useful you can by me a beer someday.
   
   There are a lot of parameters that you can play with on this sensor:
   * IR LED current: this can range from 0 (0mA, off) to 20 (200mA). Increase IR current, 
   and you increase the sensitivity. Write to the IR_CURRENT register to adjust this
   value.
   * Ambient Light Parameter: You can adjust the number of measurements that are
   averaged into one output. By default the code averages the maximum, 128. Write to
   the AMBIENT_PARAMETER to change this value.
   * Proximity Signal Frequency: The frequency of the IR test signal can be either
   3.125 MHZ, 1.5625 MHz, 781.25 kHz, or 390.625kHz. Writing a 0, 1, 2, or 3 to the 
   PROXIMITY_FREQ register will set this. The code sets it to 781.25 kHz by default.
   * Modulation delay and dead time: Not a lot of info in the datasheet on how
   to adjust these values. Write to the PROXIMITY_MOD register to change them. By 
   default this register is set to 0x81 (129) as recommended by Vishay.
   
   Try to optimize the sensor for your application. Play around with those parameters.
   
   The sensor gets more and more sensitive as an object gets closer to it. The proximity
   output does not share a linear relationship with distance. The max distance is about
   20cm. Minimum distance is a few mm.
   
   Hardware:
   This code was developed using the SparkFun breakout board:
   http://www.sparkfun.com/products/10901
   Connections are:
   VCNL4000 Breakout ---------------- Arduino
        3.3V  -----------------------  3.3V
        GND  ------------------------  GND
        SCL  ------------------------  A5
        SDA  ------------------------  A4
        IR+  ------------------------  5V (3.3V is fine too)
 */

// readProximity() returns a 16-bit value from the VCNL4000's proximity data registers
unsigned int readProximity()
{
  unsigned int data;
  byte temp;
  
  temp = vcl4000_read_byte(COMMAND_0);
  vcl4000_write_byte(COMMAND_0, temp | 0x08);  // command the sensor to perform a proximity measure
  
  while(!(vcl4000_read_byte(COMMAND_0)&0x20)) 
    ;  // Wait for the proximity data ready bit to be set
  data = vcl4000_read_byte(PROXIMITY_RESULT_MSB) << 8;
  data |= vcl4000_read_byte(PROXIMITY_RESULT_LSB);
  
  return data;
}

// readAmbient() returns a 16-bit value from the VCNL4000's ambient light data registers
unsigned int readAmbient()
{
  unsigned int data;
  byte temp;
  
  temp = vcl4000_read_byte(COMMAND_0);
  vcl4000_write_byte(COMMAND_0, temp | 0x10);  // command the sensor to perform ambient measure
  
  while(!(vcl4000_read_byte(COMMAND_0)&0x40)) 
    ;  // wait for the proximity data ready bit to be set
  data = vcl4000_read_byte(AMBIENT_RESULT_MSB) << 8;
  data |= vcl4000_read_byte(AMBIENT_RESULT_LSB);
  
  return data;
}

// vcl4000_write_byte(address, data) writes a single byte of data to address
void vcl4000_write_byte(byte address, byte data)
{
  Wire.beginTransmission(VCNL4000_ADDRESS);
  Wire.write(address);
  Wire.write(data);
  Wire.endTransmission();
}

// vcl4000_read_byte(address) reads a single byte of data from address
byte vcl4000_read_byte(byte address)
{
  byte data;
  
  Wire.beginTransmission(VCNL4000_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  Wire.requestFrom(VCNL4000_ADDRESS, 1);
  while(!Wire.available())
    ;
  data = Wire.read();

  return data;
}

