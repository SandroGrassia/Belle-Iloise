/* Talking Drum con drum "la Belle-Iloise"
  Data: 01/05/2017
  Hardware: ARDUINO MINI PRO 5V 16MHz

  2.0 08/05/2017
  doppia modalità 0 ed 1

  1.0 04/05/2017
  sostituito compressor con volume
  modifica calcolo note_number in base a sensitivity

  0.1 01/05/2017
  prima revisione
*/

#define midi_ch 0 // canale midi (0 == canale 1)
byte range = 24;  // massima escursione del pitch bend in semitoni; 12 per Roland JV1010
float pressure;
int piezo;
int volume;
float sensitivity;
int tune = 60;
int compressor;
int pressure_offset = 0; // offset del sensore di pressione
int pin_value[10][2];
int pitch_range = 48; // estensione totale in semitoni, deve essere pari
int pitch_bits = 14;  // bit del messaggio Pitch Bend
char msb;             // byte MSB del PitchBend
char lsb;             // byte LSB del PitchBend
int i;                // variabile generica
byte pin = 1;
unsigned long lock_piezo = 0;
unsigned long stop_sampling = 0;
unsigned long lock_switch = 0;
byte mode = 1;
int period;
int note_number;
int velocity;

void setup()
{
  Serial.begin(31250); // MIDI out
  pinMode(8, OUTPUT); // LED frontale

  // inizializza vettore lettura potenziometri
  for (i = 0; i <= 9; i++)
  {
    pin_value[i][0] = -10;
  }

  Update_Offset();
  Pitch_Bend_Range(range);
  pin_value[2][0] = analogRead(2);
  Update_Sensitivity(); // aggiorna mode
  digitalWrite(8, LOW);
}


void loop()
{
  pin_value[0][0] = analogRead(0);
  if (pin_value[0][0] > 6 && millis() > lock_piezo)
  {
    stop_sampling = millis() + 3;
    while (millis() <= stop_sampling)
    {
      pin_value[0][1] = analogRead(0);
      pin_value[0][0] = max(pin_value[0][0], pin_value[0][1]);
    }
    velocity = constrain(map((pin_value[0][0] - 6), 0, 100, 0, 127), 1, 127);
    if (mode == 0)
    { // mode 0 = pitch bend
      Note_On(tune, velocity);
    }
    if (mode == 1)
    { // mode 1 = notes
      note_number = (1.0 - (sensitivity * pressure)) * (tune - 127) + 127;
      Note_On(note_number, velocity);
    }
    lock_piezo = millis() + 60;
  }

  pin++;
  if (pin == 5)
    pin = 1;
  switch (pin)
  {
  case 1:
    if (Read_PIN(pin, 1) == 1)
    {
      Update_Pressure();
      if (mode == 0)
      {
        Pitch_Bend(64 + (sensitivity * pressure * 63.0));
      }
    }
    break;
  case 2:
    if (Read_PIN(pin, 2) == 1)
    {
      Update_Sensitivity();
    }
    break;
  case 3:
    if (Read_PIN(pin, 2) == 1)
    {
      Update_Tune();
    }
    break;
  case 4:
    if (Read_PIN(pin, 2) == 1)
    {
      Update_Volume();
    }
    break;
  }
} 

// lettura potenziometri
byte Read_PIN(byte pin, byte isteresi)
{
  pin_value[pin][1] = analogRead(pin);
  if (abs(pin_value[pin][1] - pin_value[pin][0]) > isteresi)
  {
    pin_value[pin][0] = pin_value[pin][1];
    return 1;
  }
  else
  {
    return 0;
  }
}

// invia messaggio midi con 2 byte dati
void MIDI_2(byte cmd, byte data1, byte data2)
{
  Serial.write(cmd);
  Serial.write(data1);
  Serial.write(data2);
}

// invia comando Note On
void Note_On(byte nota, byte velocity)
{
  MIDI_2(144 + midi_ch, nota, velocity);
}

// comando pitch bend
void Pitch_Bend(int value)
{
  static int value0 = 64;
  int run_value;
  if (value > value0)
  {
    for (run_value = (value0 + 1); run_value <= value; run_value++)
    {
      MIDI_2(224 + midi_ch, 0, run_value); // comando Pitch bend 7 bit; 224=canale midi 1
    }
    value0 = value;
  }

  else if (value < value0)
  {
    for (run_value = (value0 - 1); run_value >= value; run_value--)
    {
      MIDI_2(224 + midi_ch, 0, run_value); // comando Pitch bend 7 bit;  224=canale midi 1
    }
    value0 = value;
  }
}

void Pitch_Bend_Range(byte range)
{
  MIDI_2(176 + midi_ch, 101, 0);
  MIDI_2(176 + midi_ch, 100, 0);
  MIDI_2(176 + midi_ch, 6, range); // range in semitoni, per un'estensione totale pari a 2*range semitoni
  // MIDI_2(176 + midi_ch, 38, 0); // range in centesimi di semitono
  MIDI_2(176 + midi_ch, 101, 127);
  MIDI_2(176 + midi_ch, 100, 127);
}

void Update_Offset()
{
  pressure_offset = analogRead(1);
}

void Update_Pressure()
{
  pressure = map((pressure_offset - pin_value[1][0]), 0, 280, 0, 127);
  pressure = (float)(constrain(pressure, 0, 127)) / 127.0; //   0 --> 1.0
}

void Update_Sensitivity()
{
  if (pin_value[2][0] < 512)
  {
    if (mode == 0)
    {
      Pitch_Bend(64); // da inviare in caso di cambio mode da 0 a 1
    }
    mode = 1; // modo notes
    sensitivity = (512.0 - pin_value[2][0]) / 512.0; // da 1.0 (tutto a sx) a 0 (centro)
  }
  else
  {
    mode = 0;  // modo pitch bend
    sensitivity = (pin_value[2][0] - 512.0) / 511.0; // da 0 (centro) a 1.0 (tutto a dx)
  }
}

void Update_Tune()
{
  tune = pin_value[3][1] >> 3; // note number da inviare
}

void Update_Volume()
{
  volume = (pin_value[4][1] >> 3);  // 0 --> 127
  MIDI_2(176 + midi_ch, 7, volume); // comando volume change
}
