/*
 * 
 * 
 * 
 * 
 */

#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
#include <Time.h>         //http://www.arduino.cc/playground/Code/Time  
#include <Wire.h>         //http://arduino.cc/en/Reference/Wire (included with Arduino IDE)

#include <TimerOne.h>
 
//last time the WDT was ACKd by the application
unsigned long lastUpdate=0;
 
//time, in ms, after which a reset should be triggered
unsigned long wdtTimeout=65000;

#include <LPS.h>   // for the pressure sensor

LPS ps;

int lastHour = 99;
int lastSec = 99;
int lastMin = 99;
int took5s = 0;
int took30s = 0;    
int took5m = 0;
int took15m = 0;
int took30m = 0;

int alert = 0;
int alert_light = 40;
int fail_count = 0;

char graphite[ ] = "10.0.1.19";
char dweet[ ] = "dweet.io";

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

#include <SPI.h>
#include <SFE_CC3000.h>
#include <SFE_CC3000_Client.h>

// Pins
#define CC3000_INT      2   // Needs to be an interrupt pin (D2/D3)
#define CC3000_EN       7   // Can be any digital pin
#define CC3000_CS       10  // Preferred is pin 10 on Uno

// IP address assignment method
#define USE_DHCP        1   // 0 = static IP, 1 = DHCP

// Connection info data lengths
#define IP_ADDR_LEN     4   // Length of IP address in bytes
#define MAC_ADDR_LEN    6   // Length of MAC address in bytes

// Constants
char ap_ssid[] = "BitsThroughTheAir";                  // SSID of network
char ap_password[] = "bamto65030";          // Password of network
unsigned int ap_security = WLAN_SEC_WPA2; // Security of network
unsigned int timeout = 30000;             // Milliseconds
//const char static_ip_addr[] = "0.0.0.0";

// Global Variables
SFE_CC3000 wifi = SFE_CC3000(CC3000_INT, CC3000_EN, CC3000_CS);
SFE_CC3000_Client client = SFE_CC3000_Client(wifi);
SFE_CC3000_Client client2 = SFE_CC3000_Client(wifi);

#include "DHT.h"      // for the temperature/humidity sensor
#define DHTPIN 30     // what pin we're connected to 
#define DHTTYPE DHT22   // DHT 22  (AM2302)

DHT dht(DHTPIN, DHTTYPE);

char mPrefix[ ] = "test.orchid.";
char dThing[ ] = "test-orchid";
char metric1[ ] = "temp ";
char metric2[ ] = "hum ";
char metric3[ ] = "press ";

#define TZ_OFFSET  28800 // for pst to zulu for graphite

char hi_temp[ ] = "ALERT! HOT!";

float humSum = 0;
float tempSum = 0;
float pressSum = 0;
int datCount = 0;
float avgHum = 0;
float avgTemp = 0;
float avgPress = 0;

void setup(void)
{
  ConnectionInfo  connection_info;
  IPAddress remote_ip;
  unsigned int num_pings = 3;
  PingReport ping_report = {0};
  int i;
  
//    cli();
    Serial.begin(9600);
    Wire.begin();
    
    pinMode(alert_light, OUTPUT);
    digitalWrite(alert_light, HIGH);
    
    setSyncProvider(RTC.get);   // the function to get the time from the RTC
    
    delay(1000);
    
    digitalWrite(alert_light, LOW);
    
    lcd.init();
    lcd.backlight();
    
    if (!ps.init())
    {
      lcd.print("ERR:press!");
      digitalWrite(alert_light, HIGH);
      alert = 1;
    }
    
    ps.enableDefault();
    
    if(timeStatus() != timeSet) {
        lcd.print("! RTC");
        alert = 1;
        digitalWrite(alert_light, HIGH);
    } else
        lcd.print("RTC ok ");
     
      // Initialize CC3000 (configure SPI communications)
    if ( wifi.init() ) {
      lcd.print("wifi ok");
    } else {
      lcd.print("! wifi!");
      alert = 1;
      digitalWrite(alert_light, HIGH);
    }
    lcd.setCursor(0,1);
    lcd.print(ap_ssid);
    if(!wifi.connect(ap_ssid, ap_security, ap_password, timeout)) {
      lcd.clear();
      lcd.print("!APcnct");
      alert = 1;
      digitalWrite(alert_light, HIGH);
      delay(10000);
      resetFunc();
    }
 
 lcd.clear();
    
  if( !wifi.getConnectionInfo(connection_info) ) {
    lcd.print("Err:Cnxn!");
    digitalWrite(alert_light, HIGH);
    alert = 1;
    for (int i=0; i<= 100; i++) {
        digitalWrite(alert_light, LOW);
        delay(100);
        digitalWrite(alert_light, HIGH);
        delay(100);
    }
    resetFunc();
  } else {
    lcd.print("ok ");
  }
  
    // Perform a DNS lookup to get the IP address of a host
  Serial.print("Ping:");
  if ( !wifi.dnsLookup(dweet, &remote_ip) ) {
    lcd.println("ErrDNS");
    alert = 1;
    digitalWrite(alert_light, HIGH);
    for (int i=0; i<= 100; i++) {
      digitalWrite(alert_light, LOW);
      delay(100);
      digitalWrite(alert_light, HIGH);
      delay(100);
    }
    resetFunc();
  } else {
    lcd.print("ok ");
  }
 
  // Ping IP address of remote host
  Serial.print("Pinging ");

  if ( !wifi.ping(remote_ip, ping_report, 3, 56, 1000) ) {
    lcd.println("Err!ping");
    alert = 1;
    digitalWrite(alert_light, HIGH);
          for (int i=0; i<= 100; i++) {
        digitalWrite(alert_light, LOW);
        delay(100);
        digitalWrite(alert_light, HIGH);
        delay(100);
      }
    resetFunc();
  } else {
    lcd.print("ok ");
  } 

    // Perform a DNS lookup to get the IP address of a host
  Serial.print("Ping:");
  if ( !wifi.dnsLookup(graphite, &remote_ip) ) {
    lcd.println("ErrDNS");
    alert = 1;
    digitalWrite(alert_light, HIGH);
          for (int i=0; i<= 100; i++) {
        digitalWrite(alert_light, LOW);
        delay(100);
        digitalWrite(alert_light, HIGH);
        delay(100);
      }
    resetFunc();
  } else {
    lcd.print("ok ");
  }
 
  // Ping IP address of remote host
  Serial.print("Pinging ");

  if ( !wifi.ping(remote_ip, ping_report, 3, 56, 1000) ) {
    lcd.println("Err!ping");
    alert = 1;
    digitalWrite(alert_light, HIGH);
          for (int i=0; i<= 100; i++) {
        digitalWrite(alert_light, LOW);
        delay(100);
        digitalWrite(alert_light, HIGH);
        delay(100);
      }
    resetFunc();
  } else {
    lcd.print("ok");
  }
  
    delay(1000);
    lcd.clear();
    
    lastSec = second();
    lastMin = minute();
    lastHour = hour();
    
//    wdt_enable(WDTO_8S);
}

void loop(void)
{
  
  int s = second();  
  int m = minute();
  int h = hour();
  
  if (s != lastSec) {   
    lastSec = s;
    oneSecLoop();  
  }
  
  if ((s % 5 == 0) && (took5s == 0)) { // every 5 sec
    took5s = 1;
    fiveSecLoop();
  }
  if (s % 5 > 1) {
    took5s = 0;
  }
  
  if ((s % 30 == 0) && (took30s == 0)) { // every 30 seconds
    took30s = 1;
    thirtySecLoop();
  }  
  if (s % 30 > 0) {
    took30s = 0;
  }
  
  if (m != lastMin) { // every minute
    lastMin = m;
    oneMinLoop();   
  }
  
  if ((m % 5 == 0) && (took5m == 0)) { // every 5 minutes
    took5m = 1;
    fiveMinLoop(); 
  }  
  if (m % 5 > 0) {
    took5m = 0;
  }
  
  if ((m % 15 == 0) && (took15m == 0)) { // every 15 minutes  
    took15m = 1;
    fifteenMinLoop();
  }
  if (m % 15 > 0) {
    took15m = 0;
  }
  
  if ((m % 30 == 0) && (took30m == 0)) { // every 30 minutes  
    took30m = 1;
    thirtyMinLoop();
  } 
  if (m % 30 > 0) {
    took30m = 0;
  }

  if (h != lastHour) { // every hour
    lastHour = h;
    oneHourLoop();   
  }  
  
  lastUpdate=millis();  // update the watchdog timer
  delay(10);
  
}

void oneSecLoop(void)
{
//  Serial.println("Took the 1 second loop.");
  digitalClockDisplay();
//  wdt_reset();

  if (alert == 1) {
    if (second() % 2) {
      digitalWrite(alert_light, HIGH);
    } else {
      digitalWrite(alert_light, LOW); 
    }
  }

  if (fail_count >= 2) {
    lcd.clear();
    lcd.print("discon ");
    wifi.disconnect();
    lcd.print("recon");
    wifi.init();
    if(!wifi.connect(ap_ssid, ap_security, ap_password, timeout)) {
      lcd.clear();
      lcd.print("!APcnct");
      alert = 1;
      digitalWrite(alert_light, HIGH);
      for (int i=0; i<= 100; i++) {
        digitalWrite(alert_light, LOW);
        delay(100);
        digitalWrite(alert_light, HIGH);
        delay(100);
      }
      resetFunc();
    }
    
    fail_count = 0;  // try again if we were actually able to reconnect.
    alert = 0;
    digitalWrite(alert_light, LOW);
    lcd.clear();
  }

}  

void fiveSecLoop(void)
{
  Serial.println("5");
  takeMeasurements();  
}

void thirtySecLoop(void)
{
  Serial.println("30");
  if (avgTemp > 60.0) {
    Serial.println(hi_temp);
    alert = 1;
    digitalWrite(alert_light, HIGH);
  }
  
}

void oneMinLoop(void) 
{
  Serial.println("1");
  sendGraphite();
}

void fiveMinLoop(void)
{
  Serial.println("5");
  sendDweet();
}

void fifteenMinLoop(void)
{
//  Serial.println("15");
}

void thirtyMinLoop(void)
{
//  Serial.println("30");
}

void oneHourLoop(void)
{
//  Serial.println("1H");
}

void digitalClockDisplay(void)
{
  
    lcd.setCursor(0,0);
    // digital clock display of the time
    
    if (hour() < 10) { // fix display if before 10 am 
      lcd.print(" ");
    }
    
    lcd.print(hour());
    printDigits(minute());
    printDigits(second());
//    lcd.print(' ');
//    lcd.print(day());
//    lcd.print(' ');
//    lcd.print(month());
//    lcd.print(' ');
//    lcd.print(year()); 
//    lcd.println();

  if (fail_count > 0) {
    lcd.setCursor(9,0);
    lcd.print("       ");
    lcd.setCursor(9,0);
    lcd.print("E:");
    lcd.print(fail_count);
  }
  
}

void printDigits(int digits)
{
    // utility function for digital clock display: prints preceding colon and leading 0
    lcd.print(':');
    if(digits < 10)
        lcd.print('0');
    lcd.print(digits);
}

void sendDweet(void)
{
  digitalWrite(alert_light, HIGH);
  lcd.setCursor(9,0);
  
  if (datCount < 3) {
    lcd.print("Data!");
    digitalWrite(alert_light, LOW);
    return;
  }
  
  lcd.print("Dweet");
  
  if ( !client2.connect(dweet, 80) ) {
    lcd.setCursor(9,0);
    lcd.println("DTCPcnxn");
    alert = 1;
    digitalWrite(alert_light, HIGH);
    fail_count++;
    return;
  }
  
  lcd.print("!");
  
  Serial.print("GET /dweet/for/");
  Serial.print(mPrefix);
  Serial.print("test-orchid");
  Serial.print("?hum=");
  Serial.print(avgHum);
  Serial.print("&temp=");
  Serial.print(avgTemp);
  Serial.print("&press=");
  Serial.print(avgPress);
  Serial.println(" HTTP/1.1");
  Serial.print("Host: ");
  Serial.println(dweet);
  Serial.println("Connection: close");
  Serial.println();
  
  client2.print("GET /dweet/for/");
  client2.print(dThing);
  client2.print("?hum=");
  client2.print(avgHum);
  client2.print("&temp=");
  client2.print(avgTemp);
  client2.print("&press=");
  client2.print(avgPress);
  client2.println(" HTTP/1.1");
  client2.print("Host: ");
  client2.println(dweet);
  client2.println("Connection: close");
  client2.println();
  
   // If there are incoming bytes, print them
  if ( client2.available() ) {
    char c = client2.read();
    Serial.print(c);
  }
  
  client2.stop();
  
  lcd.setCursor(9,0);
  lcd.print("       ");
  digitalWrite(alert_light, LOW);
  
//   // Close socket
//    if ( !client.close() ) {
//      Serial.println("Err:sktclose");
//    }
  
}

void sendGraphite(void)
{
  unsigned long int nn = now();
  Serial.println(nn + TZ_OFFSET);
  
  digitalWrite(alert_light, HIGH);
  
  lcd.setCursor(9,0);

  if (datCount < 3) {
    lcd.print("Data!");
    digitalWrite(alert_light, LOW);
    return;
  }

  lcd.print("Grp:");
  
  if ( !client.connect(graphite, 2003) ) {  // if we don't have a connection, try to discon/recon and try again
    lcd.setCursor(9,0);
    lcd.print("GTCPcnxn");
    digitalWrite(alert_light, HIGH);
    lcd.clear();
    lcd.print("discon ");
    wifi.disconnect();
    lcd.print("recon");
    wifi.init();
    if(!wifi.connect(ap_ssid, ap_security, ap_password, timeout)) {
      lcd.clear();
      lcd.print("!APcnct");
      alert = 1;
      digitalWrite(alert_light, HIGH);
      for (int i=0; i<= 100; i++) {
        digitalWrite(alert_light, LOW);
        delay(100);
        digitalWrite(alert_light, HIGH);
        delay(100);
      }
      resetFunc();  // if we get here we dosconnected and could not reconnect
    }
    lcd.clear();
    digitalWrite(alert_light, LOW);
    if (!client.connect(graphite, 2003)) {
      resetFunc();
    }
  }
 
 // post the temperature
 
  lcd.print("1");
 
  Serial.print(mPrefix);
  Serial.print(metric1);
  Serial.print(avgTemp);
  Serial.print(" ");
  Serial.println(nn + TZ_OFFSET);
  
  client.print(mPrefix);
  client.print(metric1);
  client.print(avgTemp);
  client.print(" ");
  client.println(nn + TZ_OFFSET);
//  client.println(" ");
  
// Now post the humidity 

  lcd.print("2");

  Serial.print(mPrefix);
  Serial.print(metric2);
  Serial.print(avgHum);
  Serial.print(" ");
  Serial.println(nn + TZ_OFFSET);

  client.print(mPrefix);
  client.print(metric2);
  client.print(avgHum);
  client.print(" ");
  client.println(nn + TZ_OFFSET);
//  client.println(" ");
 
 // Now post the pressure

  lcd.print("3");
 
  Serial.print(mPrefix);
  Serial.print(metric3);
  Serial.print(avgPress);
  Serial.print(" ");
  Serial.println(nn + TZ_OFFSET);
 
  client.print(mPrefix);
  client.print(metric3);
  client.print(avgPress);
  client.print(" ");
  client.println(nn + TZ_OFFSET);
 // client.println(" ");

  client.stop();

//  
  if (minute() % 5 != 0) {  // if we're not going to dweet, then pause a bit so you can read display
    delay(200);             // this is since dweeting takes about 2 seconds
  }
  
  lcd.setCursor(9,0);
  lcd.print("       ");
  
  digitalWrite(alert_light, LOW);

}

void resetFunc(void){
  asm volatile ("  jmp 0");
}

void longWDT(void)
{
  if((millis()-lastUpdate) > wdtTimeout)
  {
    //enable interrupts so serial can work
//    sei();
 
    //detach Timer1 interrupt so that if processing goes long, WDT isn't re-triggered
    Timer1.detachInterrupt();
    alert = 1; 
    digitalWrite(alert_light, HIGH);
    //flush, as Serial is buffered; and on hitting reset that buffer is cleared
//    Serial.println("WDT triggered");
//    Serial.flush();
 
    //call to bootloader / code at address 0
    resetFunc();
  }
  
}

void takeMeasurements(void)
{
  lcd.setCursor(9,0);
  lcd.print("Measure");
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float hu = dht.readHumidity();
  
  // Read temperature as Celsius
  float tm = dht.readTemperature();
  
   // Check if any reads failed and exit early (to try again).
   if (isnan(hu) || isnan(tm)) {
     lcd.setCursor(9,0);
     lcd.print("DTH22! ");
     digitalWrite(alert_light, HIGH);
     alert = 1;
     return;
  }

  float pr = ps.readPressureMillibars();
    if (isnan(pr)) {
      lcd.setCursor(9,0);    
      lcd.print("PRESS! ");
      alert = 1;
      digitalWrite(alert_light, HIGH);
    return;
  }
  
//  Serial.println("Got readings.");

  if (datCount > 11) {
    humSum = humSum - avgHum;
    tempSum = tempSum - avgTemp;
    pressSum = pressSum - avgPress;
    datCount = datCount - 1;
  }

  humSum = humSum + hu;
  tempSum = tempSum + tm;
  pressSum = pressSum + pr;
  datCount = datCount + 1;
  
  avgHum = humSum / float(datCount);
  avgTemp = tempSum / float(datCount);
  avgPress = pressSum / float(datCount);
 
  lcd.setCursor(9,0);
  lcd.print("       "); 
  
  lcd.setCursor(0,1);
  
  lcd.print(avgTemp);
  lcd.print(" ");
  lcd.print(avgHum);
  lcd.print(" ");
  lcd.print(avgPress);

}
