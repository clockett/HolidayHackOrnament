//HolidayHackOrnament
// Women Who Code Seattle - Holiday Hardware Hack
// Carolyn Kempkes carolyn@locodogsoftware.com

#include "Adafruit_FONA.h"
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define FONA_RX  9
#define FONA_TX  8
#define FONA_RST 4
#define FONA_RI  7

#define PIN 6

enum Pattern { UNDEF, OFF, White, Green, Red, Christmas, Rainbow, RainbowCycle, TheaterChase, TheaterChaseRainbow, MENU };
char CmdUnknownStr [] = "Unknown command";
char CmdMenuStr [] = "Off, White, Green, Red, Christmas, Rainbow, RainbowCycle, TheaterChase, TheaterChaseRainbow, B(0-255), MENU (uppercase or lowercase)";
char CmdOffStr [] = "OFF";
char CmdWhiteStr [] = "WHITE";
char CmdGreenStr [] = "GREEN";
char CmdRedStr [] = "RED";
char CmdChristmasStr [] = "CHRISTMAS";
char CmdRainbowStr [] = "RAINBOW";
char CmdRainbowCycleStr [] = "RAINBOWCYCLE";
char CmdTheaterChaseStr [] = "THEATERCHASE";
char CmdTheaterChaseRainbowStr [] = "THEATERCHASERAINBOW";
char CmdBrightnessStr [] = "B";
char CmdDeleteFailedStr [] = "Failed to delete an SMS";
char CmdPrefix [] = "Sent from your Twilio trial account - ";

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);

// neopixel ring object: number of neopixels, pin number, LED type
Adafruit_NeoPixel ring = Adafruit_NeoPixel(22, PIN, NEO_RGBW + NEO_KHZ800);

// neopixel ring default pattern
Pattern pattern = Christmas;

// fona - this is a buffer used to get the message
char messageBuffer[255];

// fona - this is the sender to reply to
char sender[25];

// fona we default to using software serial
#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

// use this for FONA 800 and 808s
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
uint8_t type;
uint8_t brightness;
uint16_t pixels;
int colorCount = 0;

// initial setup
void setup() {

  // fona setup
  // while(!Serial); // only for testing purposes

  Serial.begin(115200);
  Serial.println(F("FONA basic test"));
  Serial.println(F("Initializing....(May take 3 seconds)"));

  fonaSerial->begin(4800);
  if (!fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while(1);
  }

  // Print SIM card IMEI number.
  char imei[15] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("SIM card IMEI: "); Serial.println(imei);
  }

  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case FONA800L:
      Serial.println(F("FONA 800L")); break;
    case FONA800H:
      Serial.println(F("FONA 800H")); break;
    case FONA808_V1:
      Serial.println(F("FONA 808 (v1)")); break;
    case FONA808_V2:
      Serial.println(F("FONA 808 (v2)")); break;
    case FONA3G_A:
      Serial.println(F("FONA 3G (American)")); break;
    case FONA3G_E:
      Serial.println(F("FONA 3G (European)")); break;
    default: 
      Serial.println(F("???")); break;
  }
  
  ring.begin();

  pixels = ring.numPixels();
  Serial.print("Pixels: "); Serial.println(pixels);

  brightness = ring.getBrightness();
  Serial.print("Brightness: "); Serial.println(brightness);
  ring.setBrightness(5); // set to less brightness
  
  // configure GPRS APN
  fona.setGPRSNetworkSettings(F("wireless.twilio.com"));

  // show all white to see that they are working
  colorAll(ring.Color(255, 255, 255, 255));
  colorsOff(20);

}

void loop() {
  checkMessages();
  playPattern();
}

void checkMessages() {

  if (fona.getNetworkStatus() != 1) {
    Serial.println("Waiting for cell connection");
  }

  // read all SMS
  int8_t smsnum = fona.getNumSMS();
  uint16_t smslen;
  int8_t smsn = 0;

   Serial.print(F("***** SMS Count=")); Serial.println(smsnum);

  if (smsnum < 1) {
   Serial.println(F("No messages"));
   return;
  }

  if ((type == FONA3G_A) || (type == FONA3G_E)) {
    smsn = 0; // zero indexed
    smsnum--;
  } else {
    smsn = 1; // 1 indexed
  }
  
  Serial.print(F("** SMS #")); Serial.println(smsn);
  Serial.println(F("checkMessages 1"));

  for ( ; smsn <= smsnum; smsn++) {
    
    Serial.print(F("Reading SMS#")); Serial.println(smsn);
   
    if (! fona.getSMSSender(smsn, sender, sizeof(sender))) {
      Serial.println(F("Failed to get the sender"));
      sender[0] = 0;
    }

    uint8_t len = fona.readSMS(smsn, messageBuffer, 250, &smslen); // pass in buffer and max len!
    // if the length is zero, its a special case where the index number is higher
    // so increase the max we'll look at!
    if (len == 0) {
      Serial.println(F("[empty slot]"));
      smsnum++;
      continue;
    }

    // delete message or text back the error
    if (!fona.deleteSMS(smsn)) {
      //sendSMS(CmdDeleteFailedStr);
      Serial.println(F("Failed to delete an SMS"));
    }

    Serial.print(F("***** SMS #")); Serial.print(smsn);
    Serial.println(messageBuffer);
    Serial.print(F("From: ")); Serial.println(sender);
    Serial.println(F("*****"));

    // create a new string from the message to use to trim off the twilio prefix if there is one
    String str(messageBuffer);
    if (str.startsWith(CmdPrefix, 0)) {
      str.remove(0, 38);
    }
    str.trim();
    str.toUpperCase();

    Serial.print(F("** Command = "));
    Serial.println(str);

    uint8_t brightnessValue;
    String brightnessString;
    if (str.startsWith(CmdBrightnessStr)) {
      brightnessString = str.substring(1);
      brightnessValue = brightnessString.toInt();
      ring.setBrightness(brightnessValue);
      Serial.print(F("   Setting brightness to "));
      Serial.println(brightnessString);
    }

    Pattern prevPattern = pattern;
    Serial.print(F("prev pattern: "));Serial.println(prevPattern); 

    char *reply = CmdUnknownStr;
    if (str == CmdMenuStr) {
      pattern = MENU;
      reply = CmdMenuStr;
    } else if (str == CmdOffStr) {
      pattern = OFF;
      reply = CmdOffStr;
    } else if (str == CmdWhiteStr) {
      pattern = White;
      reply = CmdWhiteStr;
    } else if (str == CmdGreenStr) {
      pattern = Green;
      reply = CmdGreenStr;
    } else if (str == CmdRedStr) {
      pattern = Red;
      reply = CmdRedStr;
    } else if (str == CmdChristmasStr) {
      pattern = Christmas;
      reply = CmdChristmasStr;
    } else if (str == CmdRainbowStr) {
      pattern = Rainbow;
      reply = CmdRainbowStr;
    } else if (str == CmdRainbowCycleStr) {
      pattern = RainbowCycle;
      reply = CmdRainbowCycleStr;
    } else if (str == CmdTheaterChaseStr) {
      pattern = TheaterChase;
      reply = CmdTheaterChaseStr;
    } else if (str == CmdTheaterChaseRainbowStr) {
     pattern = TheaterChaseRainbow;
     reply = CmdTheaterChaseRainbowStr;
    }
        
    Serial.print(F("pattern: "));Serial.println(pattern); 
    sendSMS(reply);

    if (prevPattern != pattern) {
      Serial.println(F("Changing the pattern"));
      colorCount = 0;
    }
  }
}

void playPattern() {
  
  Serial.print(F("playPattern: "));
  
  switch(pattern) {
    case UNDEF:
      ring.show();
      Serial.println(CmdUnknownStr);
      break;
    case OFF:
      colorsOff(50);
      Serial.println(CmdOffStr);
      break;
    case White:
      colorWipe(ring.Color(255, 255, 255, 255), 10);
      Serial.println(CmdWhiteStr);
      break;
    case Green:
      colorWipe(ring.Color(255, 0, 0, 0), 10);
     Serial.println(CmdGreenStr);
      break;
    case Red:
      colorWipe(ring.Color(0, 255, 0, 0), 10);
      Serial.println(CmdRedStr);
      break;
    case Christmas:
      christmas(30);
      Serial.println(CmdChristmasStr);
      break;
    case Rainbow:
      rainbow(100);
      Serial.println(CmdRainbowStr);
      break;
    case RainbowCycle:
      rainbowCycle(20);
      Serial.println(CmdRainbowCycleStr);
      break;
    case TheaterChase:
      theaterChase(ring.Color(0, 0, 127, 0), 20);
      Serial.println(CmdTheaterChaseStr);
      break;
    case TheaterChaseRainbow:
      theaterChaseRainbow(ring.Color(0, 0, 127, 0));
      Serial.println(CmdTheaterChaseRainbowStr);
      break;
   }
}

void sendSMS(char *message) 
{
  Serial.flush();
  if (!fona.sendSMS(sender, message)) {
    Serial.println(F("Failed to send text reply"));
  } else {
    Serial.println(F("Sent SMS message"));
  }
}

// Initialize all pixels to 'off'
void colorsOff(uint8_t wait) {
  Serial.println(F("colorsOff"));
  colorWipe(ring.Color(0, 0, 0, 0), wait);
}

void christmas(uint8_t wait) {
  Serial.println(F("christmas"));
  int i;
  for(i=0; i<ring.numPixels(); i++) {
    if (colorCount == 0) {
      ring.setPixelColor(i, ring.Color(255, 0, 0, 0));
    }
    else if (colorCount == 1) {
      ring.setPixelColor(i, ring.Color(0, 255, 0, 0));
    }
    else if (colorCount == 2) {
      ring.setPixelColor(i, ring.Color(255, 255, 255, 255));
    }
    colorCount++;
    if (colorCount == 3) {
      colorCount = 0;
    }
    delay(wait);
    ring.show();
  }
}

// Fill the dots one after the other with a color
void colorAll(uint32_t c) {
  Serial.println(F("colorWipe"));
  for(uint16_t i=0; i<ring.numPixels(); i++) {
    ring.setPixelColor(i, c);
  }
  ring.show();
}

// The following methods are from
// https://learn.adafruit.com/florabrella/test-the-neopixel-strip

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  Serial.println(F("colorWipe"));
  for(uint16_t i=0; i<ring.numPixels(); i++) {
    ring.setPixelColor(i, c);
    delay(wait);
    ring.show();
  }
}

void rainbow(uint8_t wait) {
  Serial.println(F("rainbow"));
  uint16_t i, j;
  for(j=0; j<256; j++) {
    for(i=0; i<ring.numPixels(); i++) {
      ring.setPixelColor(i, Wheel((i+j) & 255));
    }
    ring.show();
    delay(wait);
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;
  Serial.println(F("rainbowCycle"));
  for(j=0; j<256; j++) { // 1 cycles of all colors on wheel
    for(i=0; i< ring.numPixels(); i++) {
      ring.setPixelColor(i, Wheel(((i * 256 / ring.numPixels()) + j) & 255));
    }
    ring.show();
    delay(wait);
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  Serial.println(F("theaterChase"));
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 4; q++) {
      for (uint16_t i=0; i < ring.numPixels(); i=i+4) {
        ring.setPixelColor(i+q, c);    //turn every fourth pixel on
      }
      ring.show();

      delay(wait);

      for (uint16_t i=0; i < ring.numPixels(); i=i+4) {
        ring.setPixelColor(i+q, 0);        //turn every fourth pixel off
      }
    }
  }
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  Serial.println(F("theaterChaseRainbow"));
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 4; q++) {
      for (uint16_t i=0; i < ring.numPixels(); i=i+4) {
        ring.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      ring.show();

      delay(wait);

      for (uint16_t i=0; i < ring.numPixels(); i=i+4) {
        ring.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return ring.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return ring.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return ring.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
