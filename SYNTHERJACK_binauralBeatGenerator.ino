/**************************************************************************
syntherjack.net
Binaural Beat Generator
2021-01-25 v1.0
https://syntherjack.net/binaural-beat-generator-1-5-arduino/
 **************************************************************************/
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <EncoderStepCounter.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <util/crc16.h>

// Screen dimension
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Encoder pins and interrupts
#define ENCODER_PIN1 2
#define ENCODER_INT1 digitalPinToInterrupt(ENCODER_PIN1)
#define ENCODER_PIN2 3
#define ENCODER_INT2 digitalPinToInterrupt(ENCODER_PIN2)

// Declaration for SSD1306 display connected using software SPI:
#define OLED_MOSI  15
#define OLED_CLK   14
#define OLED_DC    17
#define OLED_CS    18
#define OLED_RESET 16

// AD9833 communication pins
#define GEN_FSYNC1  8                       // Chip select pin for AD9833 1
#define GEN_FSYNC2  9                       // Chip select pin for AD9833 2
#define GEN_CLK     13                      // CLK and DATA pins are shared with multiple AD9833.
#define GEN_DATA    11

// Buttons pins
#define BUTTON_GEN1_SET              7
#define BUTTON_GEN2_SET              6
#define BUTTON_JOG_DIAL_MODE_SET     5

// Create instance for SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// Create instance for one full step encoder
EncoderStepCounter encoder(ENCODER_PIN1, ENCODER_PIN2);

// AD9833 Waveform Module
const int SINE = 0x2000;                    // Define AD9833's waveform register value.
const int SQUARE = 0x2028;                  // When we update the frequency, we need to
const int TRIANGLE = 0x2002;                // define the waveform when we end writing. 

const float refFreq = 25000000.0;           // On-board crystal reference frequency
float freq_init1 = 440.0;                    // Initial frequency for Generator 1
float freq_init2 = 442.0;                    // Initial frequency for Generator 2
float freq_target1 = 440.0;                 // Target frequency for Generator 1
float freq_target2 = 442.0;                 // Target frequency for Generator 2                            

// Table with note symbols, used for display
const char *IndexToNote[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};

// Table with note frequencies, from A#0 to C-6, used for freq <-> note conversion
const float IndexToFreq[63] PROGMEM = {29.14, 30.87, 32.70, 34.65, 36.71, 38.89, 41.20, 43.65, 46.25, 49.00, 51.91, 55.00, 
                                     58.27, 61.74, 65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.83, 110.00, 
                                     116.54, 123.47, 130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00,
                                     233.08, 246.94, 261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 
                                     466.16, 493.88, 523.25, 554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99, 830.61, 880.00,
                                     932.33, 987.77, 1046.50};

#define BASE_NOTE_FREQUENCY  16.3516 // C-0, Core of note/cent offset calculations, keep it accurate

// Definition of generated frequency range
#define MIN_GENERATED_FREQ  30.0 // Do not change to lower if you like your speakers
#define MAX_GENERATED_FREQ  1000.0

// Definition of jog dial modes
#define JOG_DIAL_FINE     0
#define JOG_DIAL_COARSE   1

// Definition of binaural generator setting modes
#define MODE_FREQ_FREQ    0  
#define MODE_FREQ_OFFSET  1
#define MODE_NOTE_OFFSET  2
#define MODE_SAVE         3

// Dedinitions of what is displayed on screen and in which order
#define GEN1_NOTE         0
#define GEN1_FREQ         1
#define GEN2_FREQ         2
#define GEN2_OFFSET       3

// Definition of selected generator number
#define GEN1    0
#define GEN2    1

unsigned char setting_mode = 0; // Default mode 
unsigned char prev_setting_mode = 0;
bool setting_mode_changed = false;
bool setting_mode_confirmed = true;
unsigned char jog_dial_mode = JOG_DIAL_COARSE;
float jog_dial_multipier = 10; //0.1 for fine, 10 for coarse
unsigned char adjusted_gen = 0;
bool device_startup = true; 
bool refresh_screen = false;

signed char note_target = 47; //A-4, 440Hz 
#define NOTE_CALCULATION_OFFSET 10 //base note for calculation is C-0, but note table starts from A#0 

bool button_lock = false;
unsigned char button_lock_counter = 0;
#define BUTTON_LOCK_LOOP_COUNT 50 // for button debouncing

// Define selected/not selected value interface symbols
#define NOT_SELECTED    "\x09" 
#define SELECTED        "\x10"
#define NOT_USED        "\x20"
String value_selection_symbol[] = {NOT_SELECTED, SELECTED, SELECTED, NOT_SELECTED};

// Define EEPROM addresses to store variables, , 4 bytes per float (freq_target1, freq_target2)
#define FREQ_TARGET1_ADR    0
#define FREQ_TARGET2_ADR    4
#define NOTE_TARGET_ADR     8
#define SETTING_MODE_ADR    9
#define EEPROM_CRC_ADR      11
#define EEPROM_DATA_LENGTH  10  // 10 bytes of EEPROM written (without crc)

signed int encoder_position = 0;
char buffer[10];

bool mode_selection = false;
bool gen1_button_lock = false;
bool gen2_button_lock = false;
bool gen12_button_lock = false;
bool jog_button_lock = false; 

float noteIndex;


void setup() {
  Serial.begin(9600);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("Error: SSD1306 display allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Initialize encoder
  encoder.begin();
  
  // Initialize interrupts
  attachInterrupt(ENCODER_INT1, interrupt, CHANGE);
  attachInterrupt(ENCODER_INT2, interrupt, CHANGE);
  
  // Define pins function
  pinMode(GEN_FSYNC1, OUTPUT);                      // GEN_FSYNC1
  pinMode(GEN_FSYNC2, OUTPUT);                      // GEN_FSYNC2
  pinMode(BUTTON_GEN1_SET, INPUT_PULLUP);           // button - GEN1 frequency
  pinMode(BUTTON_GEN2_SET, INPUT_PULLUP);           // button - GEN2 frequency
  pinMode(BUTTON_JOG_DIAL_MODE_SET, INPUT_PULLUP);  // button - set jog dial mode
  
  // Display start page
  DisplayStartPage();
  display.setTextSize(2); //

  // Load selected registers from EEPROM
  EEPROM.get(FREQ_TARGET1_ADR, freq_init1);
  EEPROM.get(FREQ_TARGET2_ADR, freq_init2);
  EEPROM.get(NOTE_TARGET_ADR, note_target);
  EEPROM.get(SETTING_MODE_ADR, setting_mode);

    // Protection against going into space if no data in EEPROM
  unsigned int read_crc; 
  EEPROM.get(EEPROM_CRC_ADR, read_crc); // Get crc from EEPROM saved along with other data on MODE_SAVE
  
  unsigned int calc_crc = EEPROMcrc(EEPROM_DATA_LENGTH);   // Calculate crc from EEPROM data

  // If no correct data in EEPROM, set defaults 
  if (calc_crc != read_crc){ // Compare saved and calculated crc
    Serial.println(F("Warning: EEPROM data not saved/corrupted"));
    freq_init1 = 440.0;
    freq_init2 = 442.0;
    note_target = 47; //A-4
    setting_mode = MODE_FREQ_FREQ;
  }
 
//  Serial.println(F("EEPROM read"));
//  Serial.println(freq_init1, 6);
//  Serial.println(freq_init2, 6);
//  Serial.println(note_target, 6);
//  Serial.println(setting_mode, 6);
//  Serial.println(read_crc, 6);
//  Serial.println(calc_crc, 6);

  // Init SPI for AD9833
  SPI.begin();
  delay(50);  

  // Set both AD9833 CS pins to high (don't accept data)
  digitalWrite(GEN_FSYNC1, HIGH);
  digitalWrite(GEN_FSYNC2, HIGH);
  
  AD9833reset(GEN_FSYNC1);                                   // Reset AD9833 module after power-up.
  delay(50);
  AD9833init(freq_init1, SINE, GEN_FSYNC1);                  // Set the frequency and Sine Wave output

  AD9833reset(GEN_FSYNC2);                                   // Reset AD9833 module after power-up.
  delay(50);
  AD9833init(freq_init2, SINE, GEN_FSYNC2);                  // Set the frequency and Sine Wave output

  //can be also readed from eeprom
  freq_target1 = freq_init1;
  freq_target2 = freq_init2;

  noteIndex = FreqToNote(freq_target1);
}

void interrupt() {
  encoder.tick();
}

void loop() {
  
  // Buttons handling - choosing generator
  if(!digitalRead(BUTTON_GEN1_SET) && digitalRead(BUTTON_GEN2_SET) && !gen1_button_lock){  // If only BUTTON_GEN1_SET pushed
      if (mode_selection) mode_selection = false; // If in mode selection mode, go to normal operation
      gen1_button_lock = true;
      gen2_button_lock = true;
      jog_button_lock = true;
      adjusted_gen = GEN1;
      refresh_screen = true;
  }  
  if(!digitalRead(BUTTON_GEN2_SET) && digitalRead(BUTTON_GEN1_SET) && !gen2_button_lock){  // If only BUTTON_GEN2_SET pushed
      if (mode_selection) mode_selection = false; // If in mode selection mode, go to normal operation
      gen1_button_lock = true;
      gen2_button_lock = true;
      jog_button_lock = true;
      adjusted_gen = GEN2;
      refresh_screen = true;
  }
  // Buttons handling - going into setting mode
  if(!digitalRead(BUTTON_GEN1_SET) && !digitalRead(BUTTON_GEN2_SET) && !gen12_button_lock){  // If only BUTTON_GEN2_SET pushed
    gen12_button_lock = true;
    jog_button_lock = true;
    mode_selection = ~mode_selection;
    prev_setting_mode = setting_mode; // Remember previous settig more to restore it in case of MODE_SAVE selection
    refresh_screen = true;
  }
  // Buttons handling - changing mode or jog dial multiplier
  if(!digitalRead(BUTTON_JOG_DIAL_MODE_SET) && !jog_button_lock){
    jog_button_lock = true;
    if (!mode_selection){ 
      if(jog_dial_mode == JOG_DIAL_FINE){
        jog_dial_mode = JOG_DIAL_COARSE;
        jog_dial_multipier = 10;
      } // if JOG_DIAL_COARSE
      else{
        jog_dial_mode = JOG_DIAL_FINE;
        jog_dial_multipier = 0.1;  
      }
      //no refresh needed, multiplier change not displayed
    }
    else{
      setting_mode += 1;
      if (setting_mode > 3) setting_mode = 0;
      setting_mode_changed = true;
      refresh_screen = true;    
    }
  }

  // Enable locked buttons after several loops only if nothing is pushed
  if((gen1_button_lock || gen2_button_lock || gen12_button_lock || jog_button_lock) && (digitalRead(BUTTON_GEN1_SET) && digitalRead(BUTTON_GEN2_SET) && digitalRead(BUTTON_JOG_DIAL_MODE_SET))){
    button_lock_counter += 1;
    if(button_lock_counter > BUTTON_LOCK_LOOP_COUNT){ // Keep button locked for few ms
      gen1_button_lock = false;
      gen2_button_lock = false;
      gen12_button_lock = false;
      jog_button_lock = false;
      button_lock_counter = 0;  
    }
  }
  
  // If encoder interrupt triggered, read encoders position
  encoder.tick();
  signed char encoder_position = encoder.getPosition();
  
  //update screen and freguency registers only if encoder position changed or device was just started
  if ((encoder_position != 0 || device_startup) && !mode_selection) { 
    refresh_screen = true; // New frequencies approaching, update interface 
    // Set values corresponding to mode and selected generator (1 or 2)
    switch (setting_mode){ 
      case MODE_FREQ_FREQ: // Mode, where exact frequencies [Hz] for both generator are set
        setting_mode_changed = false;
        if (adjusted_gen == GEN1){ 
          freq_target1 = freq_target1 + encoder_position * jog_dial_multipier;
          // Protection against going out of range
          if (freq_target1 < MIN_GENERATED_FREQ) freq_target1 = MIN_GENERATED_FREQ;
          if (freq_target1 > MAX_GENERATED_FREQ) freq_target1 = MAX_GENERATED_FREQ;
        }
        else{ 
          freq_target2 = freq_target2 + encoder_position * jog_dial_multipier; 
          // Protection against going out of range
          if (freq_target2 < MIN_GENERATED_FREQ) freq_target2 = MIN_GENERATED_FREQ;
          if (freq_target2 > MAX_GENERATED_FREQ) freq_target2 = MAX_GENERATED_FREQ;
        }
        break;
      case MODE_FREQ_OFFSET: // Mode, where exact frequency [Hz] for generator 1 and offset [Hz] for generator 2 is set
        setting_mode_changed = false;
        if (adjusted_gen == GEN1){ 
          float offset = freq_target2 - freq_target1;
          freq_target1 = freq_target1 + encoder_position * jog_dial_multipier;
          freq_target2 = freq_target2 + encoder_position * jog_dial_multipier;        

          // Protection against going out of range
          if (offset <= 0){ // GEN1 leading 
            if (freq_target2 < MIN_GENERATED_FREQ){ 
              freq_target2 = MIN_GENERATED_FREQ;
              freq_target1 = MIN_GENERATED_FREQ - offset;
            }
            if (freq_target1 > MAX_GENERATED_FREQ){
              freq_target1 = MAX_GENERATED_FREQ;
              freq_target2 = MAX_GENERATED_FREQ + offset;
            }
          }
          else{ //GEN2 leading
            if (freq_target1 < MIN_GENERATED_FREQ){ 
              freq_target1 = MIN_GENERATED_FREQ;
              freq_target2 = MIN_GENERATED_FREQ + offset;
            }
            if (freq_target2 > MAX_GENERATED_FREQ){
              freq_target2 = MAX_GENERATED_FREQ;
              freq_target1 = MAX_GENERATED_FREQ - offset;
            }  
          }
        }
        else{ 
          freq_target2 = freq_target2 + encoder_position * jog_dial_multipier; 
          // Protection against going out of range
          if (freq_target2 < MIN_GENERATED_FREQ) freq_target2 = MIN_GENERATED_FREQ;
          if (freq_target2 > MAX_GENERATED_FREQ) freq_target2 = MAX_GENERATED_FREQ;
        }
        break;
      case MODE_NOTE_OFFSET: // Mode, where note for generator 1 and offset [Hz] for generator 2 is set
        if (adjusted_gen == GEN1){ 
          if (setting_mode_changed){    //if setting mode was just changed, check the note setting to smoothly change from non-zero cents to zero cents
            if (noteIndex == round(noteIndex)){ //if 0 cents
               //Serial.println("Goto normal mode");
               setting_mode_changed = false; //proceed in a normal way
            }
            else if (noteIndex > round(noteIndex) && encoder_position > 0){ //if note with +cents and want to go up
              //go to next note with 0c
              note_target = round(noteIndex) - NOTE_CALCULATION_OFFSET + 1;
            }
            else if ((noteIndex > round(noteIndex) && encoder_position < 0) || (noteIndex < round(noteIndex) && encoder_position > 0)){ //if note with +cents and want to go down
              //same note with 0c  
              note_target = round(noteIndex) - NOTE_CALCULATION_OFFSET;
            }
            else if (noteIndex < round(noteIndex) && encoder_position < 0){ //if note with -cents and want to go down
              //go to prev note with 0c
              note_target = round(noteIndex) - NOTE_CALCULATION_OFFSET - 1;
            }
          }
          if (!setting_mode_changed){
            //proceed in a normal way
            note_target += encoder_position;
            if (note_target > 61) note_target = 61;
            if (note_target < 1) note_target = 1; //don't frequency fall below B-0, otherwise non zero cents will be displayed and it looks ugly
          }
          float offset = freq_target2 - freq_target1;
          freq_target1 = pgm_read_float(&IndexToFreq[note_target]);
          freq_target2 = freq_target1 + offset;
            
          setting_mode_changed = false;
          
          // Protection against going out of range
          if (offset <= 0){ // GEN1 leading 
            if (freq_target2 < MIN_GENERATED_FREQ){ 
              freq_target2 = MIN_GENERATED_FREQ;
              freq_target1 = MIN_GENERATED_FREQ - offset;
            }
            if (freq_target1 > MAX_GENERATED_FREQ){
              freq_target1 = MAX_GENERATED_FREQ;
              freq_target2 = MAX_GENERATED_FREQ + offset;
            }
          }
          else{ //GEN2 leading
            if (freq_target1 < MIN_GENERATED_FREQ){ 
              freq_target1 = MIN_GENERATED_FREQ;
              freq_target2 = MIN_GENERATED_FREQ + offset;
            }
            if (freq_target2 > MAX_GENERATED_FREQ){
              freq_target2 = MAX_GENERATED_FREQ;
              freq_target1 = MAX_GENERATED_FREQ - offset;
            }  
          }   
        } 
        else{ 
          freq_target2 = freq_target2 + encoder_position * jog_dial_multipier;
          // Protection against going out of range
          if (freq_target2 < MIN_GENERATED_FREQ) freq_target2 = MIN_GENERATED_FREQ;
          if (freq_target2 > MAX_GENERATED_FREQ) freq_target2 = MAX_GENERATED_FREQ;
        }  
        break;
      case MODE_SAVE:
        //no handling needed here
        break; 
    }

    // Set generators
    AD9833set(freq_target1, GEN_FSYNC1);
    AD9833set(freq_target2, GEN_FSYNC2);

//    Serial.println(F("Set gens"));
//    Serial.println(freq_target1, 6);
//    Serial.println(freq_target2, 6);
  }
  encoder.reset(); // Reset encoder position to 0
  
  // Update interface
  if(device_startup || refresh_screen){
    device_startup = false; // There can be only one startup
    refresh_screen = false;
    // Set values corresponding to mode and selected generator (1 or 2)
    switch (setting_mode){ 
      case MODE_FREQ_FREQ: // Mode, where exact frequencies [Hz] for both generator are set
        value_selection_symbol[GEN1_NOTE] = NOT_USED; 
        value_selection_symbol[GEN2_OFFSET] = NOT_USED;
        if (!mode_selection){
          if (adjusted_gen == GEN1){ 
            value_selection_symbol[GEN1_FREQ] = SELECTED;
            value_selection_symbol[GEN2_FREQ] = NOT_SELECTED;
          }
          else{ 
            value_selection_symbol[GEN1_FREQ] = NOT_SELECTED;
            value_selection_symbol[GEN2_FREQ] = SELECTED;
          }
        } 
        else{
          value_selection_symbol[GEN1_FREQ] = NOT_SELECTED;
          value_selection_symbol[GEN2_FREQ] = NOT_SELECTED;    
        }
        break;
      case MODE_FREQ_OFFSET: // Mode, where exact frequency [Hz] for generator 1 and offset [Hz] for generator 2 is set
        value_selection_symbol[GEN1_NOTE] = NOT_USED; 
        value_selection_symbol[GEN2_FREQ] = NOT_USED;
        if (!mode_selection){
          if (adjusted_gen == GEN1){ 
            value_selection_symbol[GEN1_FREQ] = SELECTED;
            value_selection_symbol[GEN2_OFFSET] = NOT_SELECTED;
          }
          else{ 
            value_selection_symbol[GEN1_FREQ] = NOT_SELECTED;
            value_selection_symbol[GEN2_OFFSET] = SELECTED;
          }
        }
        else{
          value_selection_symbol[GEN1_FREQ] = NOT_SELECTED;
          value_selection_symbol[GEN2_OFFSET] = NOT_SELECTED;  
        }
        break;
      case MODE_NOTE_OFFSET: // Mode, where note for generator 1 and offset [Hz] for generator 2 is set
        value_selection_symbol[GEN1_FREQ] = NOT_USED; 
        value_selection_symbol[GEN2_FREQ] = NOT_USED;
        if (!mode_selection){
          if (adjusted_gen == GEN1){ 
            value_selection_symbol[GEN1_NOTE] = SELECTED;
            value_selection_symbol[GEN2_OFFSET] = NOT_SELECTED;
          }
          else{ 
            value_selection_symbol[GEN1_NOTE] = NOT_SELECTED;
            value_selection_symbol[GEN2_OFFSET] = SELECTED;
          }
        }
        else{
          value_selection_symbol[GEN1_NOTE] = NOT_SELECTED;
          value_selection_symbol[GEN2_OFFSET] = NOT_SELECTED;  
        }
        break;
      case MODE_SAVE:
        if (mode_selection){
          value_selection_symbol[GEN1_NOTE] = 's';
          value_selection_symbol[GEN1_FREQ] = 'a'; 
          value_selection_symbol[GEN2_FREQ] = 'v';
          value_selection_symbol[GEN2_OFFSET] = 'e'; 
        }
        break; 
    }

    if (!mode_selection){ // If not in mode selecion, display frequencies/note
      if (setting_mode == MODE_SAVE){
        // Save selected registers to EEPROM
        EEPROM.put(FREQ_TARGET1_ADR, freq_target1);
        EEPROM.put(FREQ_TARGET2_ADR, freq_target2);
        EEPROM.put(NOTE_TARGET_ADR, note_target);
        EEPROM.put(SETTING_MODE_ADR, prev_setting_mode); // We are in MODE_SAVE, but want to save a mode before entering menu
        EEPROM.put(EEPROM_CRC_ADR, EEPROMcrc(EEPROM_DATA_LENGTH));

//        Serial.println(F("EEPROM write"));
//        Serial.println(freq_target1, 6);
//        Serial.println(freq_target2, 6);
//        Serial.println(note_target, 6);
//        Serial.println(prev_setting_mode, 6);
        
        // Change mode to previously selected and update screen
        setting_mode = prev_setting_mode;
        refresh_screen = true;
      }
      if (setting_mode != MODE_SAVE)
      {
        // Display new frequencies
        display.clearDisplay();
        display.setCursor(0,0); 
        
        //display ńote symbol and cent offset
        noteIndex = FreqToNote(freq_target1);
        String noteSymbol = (String)IndexToNote[round(noteIndex) % 12] + (String)(int)(round(noteIndex)/12);
        float noteOffset = noteIndex - round(noteIndex);
        int noteCents = (int)(abs(noteOffset) * 100);
        sprintf(buffer, "%2d", noteCents);

        if (noteCents == 0)
            display.println(value_selection_symbol[GEN1_NOTE] + "  " + noteSymbol + " " + buffer);
        else if (noteOffset > 0)
            display.println(value_selection_symbol[GEN1_NOTE] + "  " + noteSymbol + "+" + buffer);
        else if (noteOffset < 0)
            display.println(value_selection_symbol[GEN1_NOTE] + "  " + noteSymbol + "-" + buffer);
            
        display.setCursor(116, 0);
        display.print("c");
    
        // Display generator 1 frequency
        dtostrf(freq_target1, 6, 1, buffer);
        display.println(value_selection_symbol[GEN1_FREQ] + " " + (String)buffer);
        display.setCursor(104, 16);
        display.print("Hz");
    
        // Display generator 2 frequency
        dtostrf(freq_target2, 6, 1, buffer);
        display.println(value_selection_symbol[GEN2_FREQ] + " " + (String)buffer);
        display.setCursor(104, 32);
        display.print("Hz");
    
        // Display frequency offset between generators 1 and 2
        dtostrf(freq_target2 - freq_target1, 7, 1, buffer);
        display.println(value_selection_symbol[GEN2_OFFSET] + (String)buffer);
        display.setCursor(104, 48);
        display.print("Hz");
      
        display.display(); 
      } 
    }
    else{ // If in mode selection, display "help"
      display.clearDisplay();
      display.setCursor(0,0);
      display.print(value_selection_symbol[GEN1_NOTE]); 
      display.setCursor(20,0);
      display.print(F("Osc1 note")); 
      display.setCursor(0,16);
      display.print(value_selection_symbol[GEN1_FREQ]);
      display.setCursor(20,16);
      display.print(F("Osc1 freq"));
      display.setCursor(0,32);
      display.print(value_selection_symbol[GEN2_FREQ]);
      display.setCursor(20,32);
      display.print(F("Osc2 freq"));
      display.setCursor(0,48);
      display.print(value_selection_symbol[GEN2_OFFSET]);
      display.setCursor(20,48);
      display.print(F("Osc2 offs"));
      display.display();
    } 
  }
    
} //main loop end


// AD9833 related functions
// AD9833 documentation advises a 'Reset' on first applying power.
void AD9833reset(int syncpin) {
  WriteRegister(0x100, syncpin);   // Write '1' to AD9833 Control register bit D8.
  delay(10);
}

// Set the frequency and waveform registers in the selected via syncpin AD9833
void AD9833init(float frequency, int waveform, int syncpin) {
  long freq_word = (frequency * pow(2, 28)) / refFreq;

  int MSB = (int)((freq_word & 0xFFFC000) >> 14);    //Only lower 14 bits are used for data
  int LSB = (int)(freq_word & 0x3FFF);
  
  //Set control bits 15 ande 14 to 0 and 1, respectively, for frequency register 0
  LSB |= 0x4000;
  MSB |= 0x4000; 
  
  WriteRegister(0x2100, syncpin);               // Allow 28 bits to be loaded into a frequency register in two consecutive writes and reset internal registers to 0
  WriteRegister(LSB, syncpin);                  // Write lower 14 bits to AD9833 registers
  WriteRegister(MSB, syncpin);                  // Write upper 14 bits to AD9833 registers
  WriteRegister(0xC000, syncpin);               // Set phase register
  WriteRegister(waveform, syncpin);             // Exit & Reset to SINE
}

// Set the frequency registers in the AD9833.
void AD9833set(float frequency, int syncpin) {

  long freq_word = (frequency * pow(2, 28)) / refFreq;

  int MSB = (int)((freq_word & 0xFFFC000) >> 14);    //Only lower 14 bits are used for data
  int LSB = (int)(freq_word & 0x3FFF);
  
  // Set control bits 15 ande 14 to 0 and 1, respectively, for frequency register 0
  LSB |= 0x4000;
  MSB |= 0x4000; 

  // Set frequency registers without reseting or changing phase to avoid clicking
  WriteRegister(0x2000, syncpin);               // Allow 28 bits to be loaded into a frequency register in two consecutive writes
  WriteRegister(LSB, syncpin);                  // Write lower 14 bits to AD9833 registers
  WriteRegister(MSB, syncpin);                  // Write upper 14 bits to AD9833 registers
}

// Write to AD9833 register
void WriteRegister(int dat, int syncpin) { 
  // Display and AD9833 use different SPI MODES so it has to be set for the AD9833 here.
  SPI.setDataMode(SPI_MODE2);       
  
  digitalWrite(syncpin, LOW);           // Set FSYNC low before writing to AD9833 registers
  delayMicroseconds(10);              // Give AD9833 time to get ready to receive data.
  
  SPI.transfer(highByte(dat));        // Each AD9833 register is 32 bits wide and each 16
  SPI.transfer(lowByte(dat));         // bits has to be transferred as 2 x 8-bit bytes.

  digitalWrite(syncpin, HIGH);          //Write done. Set FSYNC high
}

// Display start page
void DisplayStartPage() {
  display.clearDisplay();
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(22,10); 
  display.println(F("Binaural Beat"));
  display.setCursor(34,22); 
  display.println(F("Generator"));
  display.setCursor(18,40); 
  display.println(F("syntherjack.net"));
  display.display();

  delay(1500); // Let the people read my homepage :/
}  

// Function converting frequency to offset from BASE_NOTE_FREQUENCY
float FreqToNote(float frequency){
    float x = (frequency / BASE_NOTE_FREQUENCY);
    float y = 12.0 * log(x)/log(2.0);
    return y;
}

// Function calculating 16-bit CRC for EEPROM
unsigned int EEPROMcrc(unsigned char data_length){
  unsigned int crc=0;
  for (int i=0; i <= data_length; i++) // for each character in the string
    crc= _crc16_update (crc, EEPROM[i]); // update the crc value
  return crc;
}
