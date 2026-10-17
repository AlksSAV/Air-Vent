
#include <Wire.h>

#include "ASOLED.h"

#include <EEPROM.h>
#include "DHT_NEW.h"
DHT _dht2;


DHT _dht1;

bool En_132791156_1;
String text_132791156_1;
int x_132791156_1;
int y_132791156_1;
char textStr_132791156_1[60];
bool En_132791156_2;
String text_132791156_2;
int x_132791156_2;
int y_132791156_2;
char textStr_132791156_2[60];
bool En_132791156_3;
String text_132791156_3;
int x_132791156_3;
int y_132791156_3;
char textStr_132791156_3[60];
bool En_132791156_4;
String text_132791156_4;
int x_132791156_4;
int y_132791156_4;
char textStr_132791156_4[60];
bool En_132791156_5;
String text_132791156_5;
int x_132791156_5;
int y_132791156_5;
char textStr_132791156_5[60];
bool En_132791156_6;
String text_132791156_6;
int x_132791156_6;
int y_132791156_6;
char textStr_132791156_6[60];
bool En_132791156_7;
String text_132791156_7;
int x_132791156_7;
int y_132791156_7;
char textStr_132791156_7[60];
bool En_132791156_8;
String text_132791156_8;
int x_132791156_8;
int y_132791156_8;
char textStr_132791156_8[60];
bool En_132791156_9;
String text_132791156_9;
int x_132791156_9;
int y_132791156_9;
char textStr_132791156_9[60];
bool En_132791156_10;
String text_132791156_10;
int x_132791156_10;
int y_132791156_10;
char textStr_132791156_10[60];
bool En_268240131_1;
bool En_132791156_11;
String text_132791156_11;
int x_132791156_11;
int y_132791156_11;
char textStr_132791156_11[60];
const byte _menuParametrsArray[]  PROGMEM =  {1, 3, 2, 0, 0, 0, 3, 5, 1, 4, 2, 0, 2, 3, 1, 0, 0, 0, 2, 4, 1, 4, 1, 0};
int _menuValueArray_int[2];
const int _menuConstantValuesArray_int[]  PROGMEM =  {1, 60, 20, 10, -10};
 const char _flprogMenuNameString1[] PROGMEM = "T max";
 const char _flprogMenuNameString2[] PROGMEM = "T min";
const char* const _flprogMenuStringsArray[] PROGMEM = { _flprogMenuNameString1,  _flprogMenuNameString2};
struct _menuItemStricture {
 int startInArrayIndex;
};
struct _menuMainStricture {
 byte startIndex;
 byte stopIndex;
 bool isSwitchMenuAroundRing;
_menuItemStricture  currentItem;
};
_menuItemStricture _MenuItems[2];
_menuMainStricture _MainMenus[1];
float _gtv1; //Температура наружная
float _gtv2; //Относительная влажность наружная
float _gtv3; //Абсолютная влажность наружная
float _gtv4; //Температура внутри
float _gtv5; //Относительная влажность внутри
float _gtv6; //Абсолютная влажность внутри.
int _gtv7;
int _gtv8;
bool _gtv9;
bool _gtv11;
bool _gtv12;
bool _gtv13;
int Menu_Value_1_TVD = 0;
bool _bounseInputD2S = 0;
bool _bounseInputD2O = 0;
unsigned long _bounseInputD2P = 0UL;
bool _tim2I = 0;
bool _tim2O = 0;
unsigned long _tim2P = 0UL;
bool FTrig_1_Out = 0;
bool FTrig_1_OldStat = 0;
bool _bounseInputD4S = 0;
bool _bounseInputD4O = 0;
unsigned long _bounseInputD4P = 0UL;
bool _trgt1 = 0;
bool _trgt1I = 0;
unsigned long _dht1LRT = 0UL;
unsigned long _dht1Tti = 0UL;
bool FTrig_2_Out = 0;
bool FTrig_2_OldStat = 0;
bool _tim3I = 0;
bool _tim3O = 0;
unsigned long _tim3P = 0UL;
bool FTrig_4_Out = 0;
bool FTrig_4_OldStat = 0;
bool _trgt2 = 0;
bool _trgt2I = 0;
float _strFunpow2 = 0.00;
bool _trgrt1 = 0;
bool _trgrt1I = 0;
bool _trgrt2 = 0;
bool _trgrt2I = 0;
int Menu_Value_2_TVD = 0;
bool FTrig_3_Out = 0;
bool FTrig_3_OldStat = 0;
float _strFunpow1 = 0.00;
bool _bounseInputD3S = 0;
bool _bounseInputD3O = 0;
unsigned long _bounseInputD3P = 0UL;
bool _tim1I = 0;
bool _tim1O = 0;
unsigned long _tim1P = 0UL;
unsigned long _dht2LRT = 0UL;
unsigned long _dht2Tti = 0UL;
String _MenuBlock_168323611_MNO;
String _MenuBlock_168323611_VNO;
bool _MenuBlock_168323611_OEIS = 0;
bool _MenuBlock_168323611_OMUIS = 0;
bool _MenuBlock_168323611_OMDIS = 0;
bool _MenuBlock_168323611_OVUIS = 0;
bool _MenuBlock_168323611_OVDIS = 0;

void setup()
{
pinMode(2, INPUT_PULLUP); 
pinMode(3, INPUT_PULLUP); 
pinMode(4, INPUT_PULLUP); 
pinMode(14, OUTPUT); 
digitalWrite(14, 0); 


if(((readByteFromEEPROM(0, 0, 0x0))) != 102) {
(updateByteToEEPROM(0, 0, 0x0, (102)));
(updateIntegerToEEPROM(1, 0, 0x0, (40)));
(updateIntegerToEEPROM(3, 0, 0x0, (7)));
}
_bounseInputD2O =  digitalRead(2);
_bounseInputD3O =  digitalRead(3);
_bounseInputD4O =  digitalRead(4);
_dht1.setup(15);
_dht1LRT = millis();
_dht1Tti = millis(); 
_dht2.setup(16);
_dht2LRT = millis();
_dht2Tti = millis(); 
 LD.init(); 
 LD.setNormalDisplay();   
 LD.SetNormalOrientation(); 
  //LD.SetTurnedOrientation();
 LD.setBrightness(255);
 LD.clearDisplay(); 












 _MenuItems[0].startInArrayIndex = 0;
 _MenuItems[1].startInArrayIndex = 12;
_MainMenus[0].startIndex = 1;
_MainMenus[0].isSwitchMenuAroundRing = 1;
_MainMenus[0].stopIndex = 2;
_MainMenus[0].currentItem = _MenuItems[0];
_menuValueArray_int[0] = (readIntegerFromEEPROM(1, 0, 0x0));
_menuValueArray_int[1] = (readIntegerFromEEPROM(3, 0, 0x0));

}
void loop()
{

bool _tempVariable_bool;
byte _tempVariable_byte;

bool  _bounceInputTmpD3 =  (digitalRead (3));

if (_bounseInputD3S) 
    {
     if (millis() >= (_bounseInputD3P + 40)) 
         {_bounseInputD3O= _bounceInputTmpD3; _bounseInputD3S=0;}
     }
else
    {
     if (_bounceInputTmpD3 != _bounseInputD3O )
         {_bounseInputD3S=1; _bounseInputD3P = millis();} 
      } 
bool  _bounceInputTmpD4 =  (digitalRead (4));

if (_bounseInputD4S) 
    {
     if (millis() >= (_bounseInputD4P + 40)) 
         {_bounseInputD4O= _bounceInputTmpD4; _bounseInputD4S=0;}
     }
else
    {
     if (_bounceInputTmpD4 != _bounseInputD4O )
         {_bounseInputD4S=1; _bounseInputD4P = millis();} 
      } 
bool  _bounceInputTmpD2 =  (digitalRead (2));

if (_bounseInputD2S) 
    {
     if (millis() >= (_bounseInputD2P + 40)) 
         {_bounseInputD2O= _bounceInputTmpD2; _bounseInputD2S=0;}
     }
else
    {
     if (_bounceInputTmpD2 != _bounseInputD2O )
         {_bounseInputD2S=1; _bounseInputD2P = millis();} 
      } 




//Плата:1
//Наименование:Плата обработки
if(_isTimer(_dht1Tti, 60000)) {
if(_isTimer(_dht1LRT,( _dht1.getMinimumSamplingPeriod()))) {
_dht1.readSensor();
_dht1LRT = millis(); 
_dht1Tti = millis(); 
}
}
_gtv2 = _dht1.humidity;
_gtv1 = _dht1.temperature;
_strFunpow1 = pow(2.71828, ((17.67)*(_gtv1))/((_gtv1)+(243.5)));
_gtv3 = (((_strFunpow1)*(13.247))*(_gtv2))/((_gtv1)+(273.15));
digitalWrite(14, ( ((_gtv3) < (_gtv6)) && ((_gtv1) > (_gtv7)) && ((_gtv1) < (_gtv8)) ));
if(_isTimer(_dht2Tti, 60000)) {
if(_isTimer(_dht2LRT,( _dht2.getMinimumSamplingPeriod()))) {
_dht2.readSensor();
_dht2LRT = millis(); 
_dht2Tti = millis(); 
}
}
_gtv5 = _dht2.humidity;
_gtv4 = _dht2.temperature;
_strFunpow2 = pow(2.71828, ((17.67)*(_gtv4))/((_gtv4)+(243.5)));
_gtv6 = (((_strFunpow2)*(13.247))*(_gtv5))/((_gtv4)+(273.15));

//Плата:2
//Наименование:Плата дисплеев

En_132791156_1 = _gtv9;
text_132791156_1 = String("   out in");
x_132791156_1 = 0;
y_132791156_1 = 0;
 if (En_132791156_1) { text_132791156_1.toCharArray(textStr_132791156_1, text_132791156_1.length()+1);
 		LD.printString_12x16(textStr_132791156_1, x_132791156_1, y_132791156_1);
		 }
En_132791156_2 = _gtv9;
text_132791156_2 = ((String("T  ")) + ((String((int(_gtv1)), DEC))));
x_132791156_2 = 0;
y_132791156_2 = 2;
 if (En_132791156_2) { text_132791156_2.toCharArray(textStr_132791156_2, text_132791156_2.length()+1);
 		LD.printString_12x16(textStr_132791156_2, x_132791156_2, y_132791156_2);
		 }
En_132791156_3 = _gtv9;
text_132791156_3 = ((String("H  ")) + ((String((int(_gtv2)), DEC))));
x_132791156_3 = 0;
y_132791156_3 = 4;
 if (En_132791156_3) { text_132791156_3.toCharArray(textStr_132791156_3, text_132791156_3.length()+1);
 		LD.printString_12x16(textStr_132791156_3, x_132791156_3, y_132791156_3);
		 }
En_132791156_4 = _gtv9;
text_132791156_4 = ((String("A  ")) + ((String((int(_gtv3)), DEC))));
x_132791156_4 = 0;
y_132791156_4 = 6;
 if (En_132791156_4) { text_132791156_4.toCharArray(textStr_132791156_4, text_132791156_4.length()+1);
 		LD.printString_12x16(textStr_132791156_4, x_132791156_4, y_132791156_4);
		 }
En_132791156_5 = _gtv9;
text_132791156_5 = (String((int(_gtv4)), DEC));
x_132791156_5 = 85;
y_132791156_5 = 2;
 if (En_132791156_5) { text_132791156_5.toCharArray(textStr_132791156_5, text_132791156_5.length()+1);
 		LD.printString_12x16(textStr_132791156_5, x_132791156_5, y_132791156_5);
		 }
En_132791156_7 = _gtv9;
text_132791156_7 = (String((int(_gtv5)), DEC));
x_132791156_7 = 85;
y_132791156_7 = 4;
 if (En_132791156_7) { text_132791156_7.toCharArray(textStr_132791156_7, text_132791156_7.length()+1);
 		LD.printString_12x16(textStr_132791156_7, x_132791156_7, y_132791156_7);
		 }
En_132791156_6 = _gtv9;
text_132791156_6 = (String((int(_gtv6)), DEC));
x_132791156_6 = 85;
y_132791156_6 = 6;
 if (En_132791156_6) { text_132791156_6.toCharArray(textStr_132791156_6, text_132791156_6.length()+1);
 		LD.printString_12x16(textStr_132791156_6, x_132791156_6, y_132791156_6);
		 }
En_132791156_8 = _gtv13;
text_132791156_8 = ((String("Tmin ")) + ((String(_gtv7, DEC))));
x_132791156_8 = 15;
y_132791156_8 = 1;
 if (En_132791156_8) { text_132791156_8.toCharArray(textStr_132791156_8, text_132791156_8.length()+1);
 		LD.printString_12x16(textStr_132791156_8, x_132791156_8, y_132791156_8);
		 }
En_132791156_9 = _gtv13;
text_132791156_9 = ((String("Tmax ")) + ((String(_gtv8, DEC))));
x_132791156_9 = 15;
y_132791156_9 = 4;
 if (En_132791156_9) { text_132791156_9.toCharArray(textStr_132791156_9, text_132791156_9.length()+1);
 		LD.printString_12x16(textStr_132791156_9, x_132791156_9, y_132791156_9);
		 }

//Плата:3
//Наименование:Плата упрвления
if (_gtv11)
{ if (_tim3I) { if (_isTimer(_tim3P, 1500)) {_tim3O = 1;}} else {_tim3I =1; _tim3P = millis();}} else {_tim3O = 0; _tim3I = 0;}
FTrig_1_Out = 0;
if ((!(_gtv12))&&(FTrig_1_OldStat)){FTrig_1_Out = 1;}
FTrig_1_OldStat = _gtv12;
bool  _tmp1 = ( (_gtv11) || (( (FTrig_1_Out) && (_gtv13) )) );
if (_tmp1)  { if (! _trgt1I) _trgt1 = ! _trgt1; } 
_trgt1I = _tmp1; 
if (_trgt1) { if (_trgrt1I) { _trgrt1 = 0;} else {_trgrt1 = 1; _trgrt1I = 1;} } else {_trgrt1 = 0; _trgrt1I = 0;}; 
 FTrig_2_Out = 0;
if ((!(_trgt1))&&(FTrig_2_OldStat)){FTrig_2_Out = 1;}
FTrig_2_OldStat = _trgt1;
FTrig_3_Out = 0;
if ((!(_gtv12))&&(FTrig_3_OldStat)){FTrig_3_Out = 1;}
FTrig_3_OldStat = _gtv12;
bool  _tmp2 = ( (_tim3O) || (( (!(_gtv9)) && (!(_gtv13)) && (FTrig_1_Out) )) );
if (_tmp2)  { if (! _trgt2I) _trgt2 = ! _trgt2; } 
_trgt2I = _tmp2; 
if (_trgt2) { if (_trgrt2I) { _trgrt2 = 0;} else {_trgrt2 = 1; _trgrt2I = 1;} } else {_trgrt2 = 0; _trgrt2I = 0;}; 
 FTrig_4_Out = 0;
if ((!(_trgt2))&&(FTrig_4_OldStat)){FTrig_4_Out = 1;}
FTrig_4_OldStat = _trgt2;
En_268240131_1 = ( (_trgrt1) || (FTrig_2_Out) || (FTrig_3_Out) || (_trgrt2) || (FTrig_4_Out) );
if (En_268240131_1) LD.clearDisplay(); 
_gtv13 = ( (_trgt1) && (_gtv12) && (!(_trgt2)) );
_gtv9 = ( (!(_trgt1)) && (_gtv12) && (!(_trgt2)) );
 if (_trgt2) {
_tempVariable_bool  =  1;
 if ( ! _MenuBlock_168323611_OEIS ) {_MenuBlock_168323611_OEIS = 1;
} 
_tempVariable_byte = pgm_read_byte(&_menuParametrsArray[((_MainMenus[0].currentItem ).startInArrayIndex)+10]);
_MenuBlock_168323611_MNO = _readStringFromProgmem ((char*)pgm_read_word(&(_flprogMenuStringsArray[_tempVariable_byte - 1])));
_MenuBlock_168323611_VNO = _menuOutputValueString (0);
} else {
_tempVariable_bool  =  0;
if (_MenuBlock_168323611_OEIS){
_MenuBlock_168323611_OEIS = 0;
_menuUpdateToEEpromItems();
}
_MenuBlock_168323611_MNO = "";
_MenuBlock_168323611_VNO = "";
}
if(_gtv11) { if ( ! _MenuBlock_168323611_OMUIS) {_MenuBlock_168323611_OMUIS = 1;if (_tempVariable_bool){  _menuUpEvents(0);
_menuUpdateToEEpromItems();
}}} else {_MenuBlock_168323611_OMUIS = 0;}
if(0) { if ( ! _MenuBlock_168323611_OMDIS) {_MenuBlock_168323611_OMDIS = 1;if (_tempVariable_bool){   _menuDownEvents(0);
_menuUpdateToEEpromItems();
}}} else {_MenuBlock_168323611_OMDIS = 0;}
if(!(_bounseInputD3O)) { if ( ! _MenuBlock_168323611_OVUIS) {_MenuBlock_168323611_OVUIS = 1; if (_tempVariable_bool){  _valueUpEvents(0); } }} else {_MenuBlock_168323611_OVUIS = 0; }
if(!(_bounseInputD4O)) { if ( ! _MenuBlock_168323611_OVDIS) {_MenuBlock_168323611_OVDIS = 1; if (_tempVariable_bool){  _valueDownEvents(0); }} } else {_MenuBlock_168323611_OVDIS = 0; }
En_132791156_11 = _trgt2;
text_132791156_11 = ((_MenuBlock_168323611_VNO) + (String("   ")));
x_132791156_11 = 60;
y_132791156_11 = 4;
 if (En_132791156_11) { text_132791156_11.toCharArray(textStr_132791156_11, text_132791156_11.length()+1);
 		LD.printString_12x16(textStr_132791156_11, x_132791156_11, y_132791156_11);
		 }
En_132791156_10 = _trgt2;
text_132791156_10 = _MenuBlock_168323611_MNO;
x_132791156_10 = 40;
y_132791156_10 = 1;
 if (En_132791156_10) { text_132791156_10.toCharArray(textStr_132791156_10, text_132791156_10.length()+1);
 		LD.printString_12x16(textStr_132791156_10, x_132791156_10, y_132791156_10);
		 }
if(!(_trgt2)){Menu_Value_2_TVD = _menuValueArray_int[0];}
_gtv8 = Menu_Value_2_TVD;
if(!(_trgt2)){Menu_Value_1_TVD = _menuValueArray_int[1];}
_gtv7 = Menu_Value_1_TVD;
if(( (!(_bounseInputD2O)) || (!(_bounseInputD3O)) || (!(_bounseInputD4O)) )) {_tim1O = 1; _tim1I = 1;} else { if(_tim1I) {_tim1I = 0; _tim1P = millis();} else { if (_tim1O) {if ( _isTimer(_tim1P, 15000)) _tim1O = 0;}}}
_gtv12 = _tim1O;
if (_tim1O)
{ if (_tim2I) { if (_isTimer(_tim2P, 1000)) {_tim2O = 1;}} else {_tim2I =1; _tim2P = millis();}} else {_tim2O = 0; _tim2I = 0;}
_gtv11 = ( (_tim2O) && (_tim1O) && (!(_bounseInputD2O)) );






}

bool _isTimer(unsigned long startTime, unsigned long period )
  {
  unsigned long currentTime;
currentTime = millis();
if (currentTime>= startTime) {return (currentTime>=(startTime + period));} else {return (currentTime >=(4294967295-startTime+period));}
  }
String _readStringFromProgmem (char *string)
{
String result = String("");
while (pgm_read_byte(string)!='\0')
{
result=result+ char(pgm_read_byte(string));
	string++;
	}
return result;
}
 void _menuUpEvents (byte menuIndex)
{
byte tempIndex = pgm_read_byte(&_menuParametrsArray[((_MainMenus[menuIndex]).currentItem).startInArrayIndex]);
byte parIndex = pgm_read_byte(&_menuParametrsArray[ (((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+5]);
byte parentStartIndex;
byte parentStopIndex;
if (parIndex == 0) {parentStartIndex = (_MainMenus[menuIndex]).startIndex; parentStopIndex = (_MainMenus[menuIndex]).stopIndex;}
else { parentStartIndex = pgm_read_byte(&_menuParametrsArray[( (_MenuItems[parIndex - 1]).startInArrayIndex)+3]);
 parentStopIndex = pgm_read_byte(&_menuParametrsArray[( (_MenuItems[parIndex-1]).startInArrayIndex)+4]); }
if (tempIndex == parentStartIndex) {if((_MainMenus[menuIndex]).isSwitchMenuAroundRing) {(_MainMenus[menuIndex]).currentItem = _MenuItems[parentStopIndex -1]; return;} else{return;}}
(_MainMenus[menuIndex]).currentItem = _MenuItems[tempIndex - 2];
return;
}
 void _menuDownEvents (byte menuIndex)
{
byte tempIndex = pgm_read_byte(&_menuParametrsArray[((_MainMenus[menuIndex]).currentItem).startInArrayIndex]);
byte parIndex = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+5]);
byte parentStartIndex;
byte parentStopIndex;
if (parIndex == 0) {parentStartIndex = (_MainMenus[menuIndex]).startIndex; parentStopIndex = (_MainMenus[menuIndex]).stopIndex;}
else { parentStartIndex = pgm_read_byte(&_menuParametrsArray[( (_MenuItems[parIndex-1]).startInArrayIndex)+3]);
 parentStopIndex = pgm_read_byte(&_menuParametrsArray[((_MenuItems[parIndex-1]).startInArrayIndex)+4]); }
if (tempIndex == parentStopIndex) {if((_MainMenus[menuIndex]).isSwitchMenuAroundRing) {(_MainMenus[menuIndex]).currentItem = _MenuItems[parentStartIndex -1]; return;} else{return;}}
(_MainMenus[menuIndex]).currentItem = _MenuItems[tempIndex];
return;
}
 void _valueUpEvents (byte menuIndex)
{
byte valIndex = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+2]);
byte itemType = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+1]);
byte indexMax = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+6]);
byte indexStep = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+8]);
if (itemType == 3) {if (! indexMax == 0) { if ( ! (int(pgm_read_word(&_menuConstantValuesArray_int[indexMax -1])) > int(_menuValueArray_int[valIndex -1]))) {return;} }
_menuValueArray_int[valIndex -1] = _menuValueArray_int[valIndex -1] + (pgm_read_word(&_menuConstantValuesArray_int[indexStep -1])); }
}
 void _valueDownEvents (byte menuIndex)
{

byte valIndex = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+2]);
byte itemType = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+1]);
byte indexMin = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+7]);
byte indexStep = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+8]);
if (itemType == 3) { if (! indexMin == 0) { if ( ! ((int(pgm_read_word(&_menuConstantValuesArray_int[indexMin - 1]))) < int(_menuValueArray_int[valIndex - 1]))) {return;}} 
_menuValueArray_int[valIndex - 1] = _menuValueArray_int[valIndex - 1] - (pgm_read_word(&_menuConstantValuesArray_int[indexStep - 1])); }
}
 String _menuOutputValueString (byte menuIndex)
{
byte itemType = pgm_read_byte(&_menuParametrsArray[ (((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+1]);
byte valIndex = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+2]);
byte indexMin = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+7]);
byte indexMax = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+6]);
if( valIndex == 0){ return "";}
byte convFormat = pgm_read_byte(&_menuParametrsArray[(((_MainMenus[menuIndex]).currentItem).startInArrayIndex)+9]);
if(itemType == 3) { return _convertNamber(itemType, convFormat, valIndex, indexMax, indexMin);}
return "";
}
 void _menuUpdateToEEpromItems()
{
(updateIntegerToEEPROM(1, 0, 0x0, ((_menuValueArray_int[0]))));
(updateIntegerToEEPROM(3, 0, 0x0, ((_menuValueArray_int[1]))));
}
String _convertNamber(byte itemType, byte convFormat, byte valIndex, byte indexMax, byte indexMin)
{
if (itemType== 3) {
if (convFormat == 4) { return String((_menuValueArray_int[valIndex - 1 ]),DEC);}
if (convFormat == 5) { return String((_menuValueArray_int[valIndex - 1]),HEX);}
if (convFormat == 6) { return String((_menuValueArray_int[valIndex -1]),BIN);}
}
}
 byte readByteFromEEPROM(int addres, byte bitAddres, byte chipAddres)
{
return EEPROM.read(addres);
}
void updateByteToEEPROM(int addres, byte bitAddres, byte chipAddres, byte value)
{
return EEPROM.update(addres, value);
}
 int readIntegerFromEEPROM(int addres, byte bitAddres, byte chipAddres)
{
byte hight = readByteFromEEPROM( addres,  bitAddres,  chipAddres);
byte low = readByteFromEEPROM( (addres+1),  bitAddres,  chipAddres);
return  word(hight, low);
}
 void updateIntegerToEEPROM(int addres, byte bitAddres, byte chipAddres, int value)
{
updateByteToEEPROM( addres,  bitAddres,  chipAddres, highByte(value));
updateByteToEEPROM( (addres+1),  bitAddres,  chipAddres, lowByte(value));
}
