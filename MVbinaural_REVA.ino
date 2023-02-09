/********************************************
MVBinaural by Marcos Velazquez
v.1
https://wokwi.com/projects/337899977000878674
*********************************************/

#include <Adafruit_SSD1306.h>

// Screen dimension
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Encoder pins
//#define pinA 6 // ENCODER_CLK (D6)
//#define pinB  7 // ENCODER_DT (D7)
#define ENCODER_BTN 8


// Create instance for SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT);

// Record last click
int lastClk = HIGH;
int counter = 0; 


// For update func
int pinAStateCurrent = LOW;
int pinAStateLast = pinAStateCurrent;
int pinA = 3; // ENCODER_CLK (D3 because it includes external interrupts)
int pinB = 2; // ENCODER_DT (D2 because it includes external interrupts)






void setup() {
  Serial.begin(115200);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay(); // Clear the buffer
  splashScreen();
  display.display(); // Re-show display

  pinMode(pinA, INPUT);
  pinMode(pinB, INPUT);
  pinMode(ENCODER_BTN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(pinB),update,CHANGE);

/*
  // Draw a single pixel in white
  display.drawPixel(10, 10, WHITE);

  // Show the display buffer on the screen. You MUST call display() after
  // drawing commands to make them visible on screen!
  display.display();
  delay(2000);
  
  // Test functions
  testdrawline();
  testdrawchar();
  testscrolltext();

  // Clear buffer and reload display
  display.clearDisplay();
  display.display();
  */
}

void loop() {
  buttonPress();
  freqMenu();
}

void splashScreen() {
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(10, 0);

  display.println(F("MVBinaural\nV1.0"));
  display.display();
  delay(3000);

  display.clearDisplay();
}

void freqMenu() {
  display.setTextSize(1); // Draw 1X-scale text
  display.setTextColor(WHITE);
  display.setCursor(10, 0);

  display.clearDisplay();
  display.println(F("Frequency: "));
  display.println(counter);
  display.display();
}

void buttonPress() {
  if (digitalRead(ENCODER_BTN) == LOW) {
    Serial.println("Button pressed");
  }
  
  else if (digitalRead(ENCODER_BTN) == HIGH) {
    // Serial.println("Button released");
  }
}


void update() {
  pinAStateCurrent = digitalRead(pinA);

    // If minimal movement occured
   if ((pinAStateLast == LOW) && (pinAStateCurrent == HIGH)) {

     if (digitalRead(pinB) == HIGH) {
      counter++;
      Serial.println(counter);
     }
    else {
      counter--;
      Serial.println(counter);
    }
  }
  pinAStateLast = pinAStateCurrent;

}



void workingRotary() {
  // Clears display
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Minimum Hz: 30
  // Maximum Hz: 1000
  // Inc/dec by 5
  // OR
  // Inc/dec by 1


  int newClk = digitalRead(pinA);

  // If there was a change on the CLK pin..
  if (newClk != lastClk) {
    lastClk = newClk;
    int dtValue = digitalRead(pinB);

    if (newClk == LOW && dtValue == HIGH) {
      counter++;
      // Serial.println("Rotated clockwise ⏩");
      Serial.println(counter);
    }

    if (newClk == LOW && dtValue == LOW) {
      counter--;
      //Serial.println("Rotated counterclockwise ⏪");
      Serial.println(counter);
    }
  }
}




































void testdrawline() {
  int16_t i;

  display.clearDisplay(); // Clear display buffer

  for(i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, WHITE);
    display.display(); // Update screen with each newly-drawn line
    delay(1);
  }
  for(i=0; i<display.height(); i+=4) {
    display.drawLine(0, 0, display.width()-1, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=0; i<display.width(); i+=4) {
    display.drawLine(0, display.height()-1, i, 0, WHITE);
    display.display();
    delay(1);
  }
  for(i=display.height()-1; i>=0; i-=4) {
    display.drawLine(0, display.height()-1, display.width()-1, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=display.width()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, i, 0, WHITE);
    display.display();
    delay(1);
  }
  for(i=display.height()-1; i>=0; i-=4) {
    display.drawLine(display.width()-1, display.height()-1, 0, i, WHITE);
    display.display();
    delay(1);
  }
  delay(250);

  display.clearDisplay();

  for(i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, WHITE);
    display.display();
    delay(1);
  }
  for(i=0; i<display.width(); i+=4) {
    display.drawLine(display.width()-1, 0, i, display.height()-1, WHITE);
    display.display();
    delay(1);
  }

  delay(2000); // Pause for 2 seconds
}

void testdrawchar(void) {
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Not all the characters will fit on the display. This is normal.
  // Library will draw what it can and the rest will be clipped.
  for(int16_t i=0; i<256; i++) {
    if(i == '\n') display.write(' ');
    else          display.write(i);
  }

  display.display();
  delay(2000);
}

void testscrolltext(void) {
  display.clearDisplay();

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(10, 0);
  display.println(F("scroll"));
  display.display();      // Show initial text
  delay(100);

  // Scroll in various directions, pausing in-between:
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();
  delay(1000);
}
