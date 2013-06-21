//pH stamp driver code by Sotirios Panagiotou
float ph_stamp_fetch_ph(float temp){
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
