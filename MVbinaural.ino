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
#define ENCODER_CLK 6
#define ENCODER_DT  7
#define ENCODER_BTN 8

// Create instance for SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT);

// Record last click
int lastClk = HIGH;

void setup() {
    Serial.begin(115200);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Re-show display as empty
  display.display();

  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  piMode(ENCODER_BTN, INPUT_PULLUP);

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
  workingRotary();
  workingButton();
}



void workingButton() {
  if (digitalRead(ENCODER_BTN) == LOW) {
    Serial.println("Button pressed");
  }
  
  else {
    Serial.println("Button released");
  }
}


void workingRotary() {
  // Clears display
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  int newClk = digitalRead(ENCODER_CLK);

  // If there was a change on the CLK pin..
  if (newClk != lastClk) {
    lastClk = newClk;
    int dtValue = digitalRead(ENCODER_DT);

    if (newClk == LOW && dtValue == HIGH) {
      Serial.println("Rotated clockwise ⏩");
    }

    if (newClk == LOW && dtValue == LOW) {
      Serial.println("Rotated counterclockwise ⏪");
    }
  }
}



/*
void showRotaryNumber(void) {
  

  //display.write(pinA);
  display.write(pinALast);

  while (true) {
    pinALast = digitalRead(pinA);
    //Serial.begin(9600);
    //display.clearDisplay();
    //display.write("in outer loop!");
    //display.display();

  if (pinA != pinALast) {
    //display.write("this works!");
    display.display();
  }
}

  display.display();
  delay(2000);
}
*/


































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
