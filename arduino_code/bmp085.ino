/* BMP085 Extended Example Code
  by: Jim Lindblom
  SparkFun Electronics
  date: 1/18/11
  license: CC BY-SA v3.0 - http://creativecommons.org/licenses/by-sa/3.0/
  
  Get bmp085_pressure and bmp085_temperature from the BMP085 and calculate altitude.
  Serial.print it out at 9600 baud to serial monitor.

  Update (7/19/11): I've heard folks may be encountering issues
  with this code, who're running an Arduino at 8MHz. If you're 
  using an Arduino Pro 3.3V/8MHz, or the like, you may need to 
  increase some of the delays in the bmp085ReadUP and 
  bmp085ReadUT functions.
*/

#include <Wire.h>

#define BMP085_ADDRESS 0x77  // I2C address of BMP085

const unsigned char bmp085_OSS = 0;  // Oversampling Setting

// Calibration values
int bmp085_ac1;
int bmp085_ac2; 
int bmp085_ac3; 
unsigned int bmp085_ac4;
unsigned int bmp085_ac5;
unsigned int bmp085_ac6;
int bmp085_b1; 
int bmp085_b2;
int bmp085_mb;
int bmp085_mc;
int bmp085_md;

// bmp085_b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
long bmp085_b5; 

short bmp085_temperature;
long bmp085_pressure;

void boop()
{
  bmp085_temperature = bmp085GetTemperature(bmp085ReadUT());
  bmp085_pressure = bmp085GetPressure(bmp085ReadUP());
  Serial.print("Temperature: ");
  Serial.print(bmp085_temperature, DEC);
  Serial.println(" *0.1 deg C");
  Serial.print("Pressure: ");
  Serial.print(bmp085_pressure, DEC);
  Serial.println(" Pa");
  Serial.println();
  delay(1000);
}

// Stores all of the bmp085's calibration values into global variables
// Calibration values are required to calculate temp and bmp085_pressure
// This function should be called at the beginning of the program
void bmp085Calibration()
{
  bmp085_ac1 = bmp085ReadInt(0xAA);
  bmp085_ac2 = bmp085ReadInt(0xAC);
  bmp085_ac3 = bmp085ReadInt(0xAE);
  bmp085_ac4 = bmp085ReadInt(0xB0);
  bmp085_ac5 = bmp085ReadInt(0xB2);
  bmp085_ac6 = bmp085ReadInt(0xB4);
  bmp085_b1 = bmp085ReadInt(0xB6);
  bmp085_b2 = bmp085ReadInt(0xB8);
  bmp085_mb = bmp085ReadInt(0xBA);
  bmp085_mc = bmp085ReadInt(0xBC);
  bmp085_md = bmp085ReadInt(0xBE);
}

// Calculate bmp085_temperature given ut.
// Value returned will be in units of 0.1 deg C
short bmp085GetTemperature(unsigned int ut)
{
  long x1, x2;
  
  x1 = (((long)ut - (long)bmp085_ac6)*(long)bmp085_ac5) >> 15;
  x2 = ((long)bmp085_mc << 11)/(x1 + bmp085_md);
  bmp085_b5 = x1 + x2;

  return ((bmp085_b5 + 8)>>4);  
}

// Calculate bmp085_pressure given up
// calibration values must be known
// bmp085_b5 is also required so bmp085GetTemperature(...) must be called first.
// Value returned will be bmp085_pressure in units of Pa.
long bmp085GetPressure(unsigned long up)
{
  long x1, x2, x3, b3, b6, p;
  unsigned long b4, b7;
  
  b6 = bmp085_b5 - 4000;
  // Calculate B3
  x1 = (bmp085_b2 * (b6 * b6)>>12)>>11;
  x2 = (bmp085_ac2 * b6)>>11;
  x3 = x1 + x2;
  b3 = (((((long)bmp085_ac1)*4 + x3)<<bmp085_OSS) + 2)>>2;
  
  // Calculate B4
  x1 = (bmp085_ac3 * b6)>>13;
  x2 = (bmp085_b1 * ((b6 * b6)>>12))>>16;
  x3 = ((x1 + x2) + 2)>>2;
  b4 = (bmp085_ac4 * (unsigned long)(x3 + 32768))>>15;
  
  b7 = ((unsigned long)(up - b3) * (50000>>bmp085_OSS));
  if (b7 < 0x80000000)
    p = (b7<<1)/b4;
  else
    p = (b7/b4)<<1;
    
  x1 = (p>>8) * (p>>8);
  x1 = (x1 * 3038)>>16;
  x2 = (-7357 * p)>>16;
  p += (x1 + x2 + 3791)>>4;
  
  return p;
}

// Read 1 byte from the BMP085 at 'address'
char bmp085Read(unsigned char address)
{
  unsigned char data;
  
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  
  Wire.requestFrom(BMP085_ADDRESS, 1);
  while(!Wire.available())
    ;
    
  return Wire.read();
}

// Read 2 bytes from the BMP085
// First byte will be from 'address'
// Second byte will be from 'address'+1
int bmp085ReadInt(unsigned char address)
{
  unsigned char msb, lsb;
  
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(address);
  Wire.endTransmission();
  
  Wire.requestFrom(BMP085_ADDRESS, 2);
  while(Wire.available()<2)
    ;
  msb = Wire.read();
  lsb = Wire.read();
  
  return (int) msb<<8 | lsb;
}

// Read the uncompensated bmp085_temperature value
unsigned int bmp085ReadUT()
{
  unsigned int ut;
  
  // Write 0x2E into Register 0xF4
  // This requests a bmp085_temperature reading
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x2E);
  Wire.endTransmission();
  
  // Wait at least 4.5ms
  delay(5);
  
  // Read two bytes from registers 0xF6 and 0xF7
  ut = bmp085ReadInt(0xF6);
  return ut;
}

// Read the uncompensated bmp085_pressure value
unsigned long bmp085ReadUP()
{
  unsigned char msb, lsb, xlsb;
  unsigned long up = 0;
  
  // Write 0x34+(bmp085_OSS<<6) into register 0xF4
  // Request a bmp085_pressure reading w/ oversampling setting
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF4);
  Wire.write(0x34 + (bmp085_OSS<<6));
  Wire.endTransmission();
  
  // Wait for conversion, delay time dependent on bmp085_OSS
  delay(2 + (3<<bmp085_OSS));
  
  // Read register 0xF6 (MSB), 0xF7 (LSB), and 0xF8 (XLSB)
  Wire.beginTransmission(BMP085_ADDRESS);
  Wire.write(0xF6);
  Wire.endTransmission();
  Wire.requestFrom(BMP085_ADDRESS, 3);
  
  // Wait for data to become available
  while(Wire.available() < 3)
    ;
  msb = Wire.read();
  lsb = Wire.read();
  xlsb = Wire.read();
  
  up = (((unsigned long) msb << 16) | ((unsigned long) lsb << 8) | (unsigned long) xlsb) >> (8-bmp085_OSS);
  
  return up;
}


