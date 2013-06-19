//low level humi sensor interface
byte hih6130_fetch_humidity_temperature(unsigned int *p_H_dat, unsigned int *p_T_dat){
    //interface code copyright Peter H Anderson, Baltimore, MD, Nov, '11
    // You may use it, but please give credit.  
    
    byte address, Hum_H, Hum_L, Temp_H, Temp_L, _status;
    unsigned int H_dat, T_dat;
    address = 0x27;;
    Wire.beginTransmission(address); 
    Wire.endTransmission();
    delay(100);
    
    Wire.requestFrom((int)address, (int) 4);
    Hum_H = Wire.read();
    Hum_L = Wire.read();
    Temp_H = Wire.read();
    Temp_L = Wire.read();
    Wire.endTransmission();
    
    _status = (Hum_H >> 6) & 0x03;
    Hum_H = Hum_H & 0x3f;
    H_dat = (((unsigned int)Hum_H) << 8) | Hum_L;
    T_dat = (((unsigned int)Temp_H) << 8) | Temp_L;
    T_dat = T_dat / 4;
    *p_H_dat = H_dat;
    *p_T_dat = T_dat;
    return(_status);
}
