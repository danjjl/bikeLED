#define NUM_RGB       (15)         // Number of WS281X we have connected
#define NUM_BYTES     (NUM_RGB*3)  // Number of LEDs (3 per each WS281X)
#define LED_PIN       (8)          // Digital port number
#define PORT          (PORTB)      // Digital pin's port
#define PORT_PIN      (PORTB0)     // Digital pin's bit position

#define NUM_BITS      (8)          // Constant value: bits per byte

#define HALL_PIN      (2)          // Hall sensor pin

typedef struct{
  uint8_t led;
  uint16_t angle;
  uint8_t r;
  uint8_t g;
  uint8_t b;
}command;

uint8_t* rgb_arr = NULL;
uint32_t t_f;
int i;
int j;
unsigned long new_delay;
unsigned long wait;
uint32_t max_led = 15;
int increment = -1;
volatile unsigned long time = 0;
volatile unsigned long period = 0;
volatile uint32_t counter = 0;
volatile bool new_turn = false;
bool reset = false;
volatile bool trigger = false;

# define CMD_LEN 15*6
command cmd[CMD_LEN];


void sortCmd(command cmd[]){
  command tmp;
  for(i=1; i<CMD_LEN; i++){
    j = i;
    while(j > 0 && cmd[j].angle < cmd[j-1].angle){
      tmp.led=cmd[j].led; tmp.angle=cmd[j].angle; tmp.r=cmd[j].r; tmp.g=cmd[j].g; tmp.b=cmd[j].b;
      cmd[j].led=cmd[j-1].led; cmd[j].angle=cmd[j-1].angle; cmd[j].r=cmd[j-1].r; cmd[j].g=cmd[j-1].g; cmd[j].b=cmd[j-1].b;
      cmd[j-1].led=tmp.led; cmd[j-1].angle=tmp.angle; cmd[j-1].r=tmp.r; cmd[j-1].g=tmp.g; cmd[j-1].b=tmp.b;
      j--;
    }
  }
}

void setup(){
  // Initialize LED and set Blue
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, 0);
  if((rgb_arr = (uint8_t *)malloc(NUM_BYTES))){
    memset(rgb_arr, 0, NUM_BYTES);
  }
   for(j=0; j< NUM_RGB; j++){
    setColorRGB(j, 0, 0, 255);
  }
  render();

  //Initialize Hall sensor
  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), hall_trigger, FALLING);

  //Generate rainbow array
  for(j=0; j<NUM_RGB; j++){
    cmd[j*6].led=j; cmd[j*6].angle=(0 + j*15)%360; cmd[j*6].r=255; cmd[j*6].g=0; cmd[j*6].b=0;               //r
    cmd[j*6+1].led=j; cmd[j*6+1].angle=(60 + j*15)%360; cmd[j*6+1].r=255; cmd[j*6+1].g=50; cmd[j*6+1].b=0;   //o
    cmd[j*6+2].led=j; cmd[j*6+2].angle=(120 + j*15)%360; cmd[j*6+2].r=255; cmd[j*6+2].g=200; cmd[j*6+2].b=0; //y
    cmd[j*6+3].led=j; cmd[j*6+3].angle=(180 + j*15)%360; cmd[j*6+3].r=0; cmd[j*6+3].g=120; cmd[j*6+3].b=0;   //g
    cmd[j*6+4].led=j; cmd[j*6+4].angle=(240 + j*15)%360; cmd[j*6+4].r=0; cmd[j*6+4].g=0; cmd[j*6+4].b=255;   //b
    cmd[j*6+5].led=j; cmd[j*6+5].angle=(300 + j*15)%360; cmd[j*6+5].r=100; cmd[j*6+5].g=0; cmd[j*6+5].b=180; //p
  }
  //sort commands
  sortCmd(cmd);
  Serial.begin(9600);
}

void hall_trigger(){
  if(!new_turn && !trigger){
    trigger = true;
    period = MIN(millis() - time, 1000);
    time = millis();
    new_turn = true;
    trigger = false;
  }
}

void loop(){
  // Wait for new turn
  while(!new_turn);
  wait = 0;
  reset = true;
  for(j=0; j<CMD_LEN; j++){
    new_delay = period*cmd[j].angle/360;
    if(new_delay < wait){
      new_delay = wait;
    }
    delay(new_delay - wait);
    wait += new_delay - wait;
    if(reset && wait > period/4){
      new_turn = false;
      reset = false;
    }
    if(!reset && new_turn){
      break;
    }
    setColorRGB(cmd[j].led, cmd[j].r, cmd[j].g, cmd[j].b);
    while(j<CMD_LEN-1 && cmd[j].angle == cmd[j+1].angle){
      j++;
      setColorRGB(cmd[j].led, cmd[j].r, cmd[j].g, cmd[j].b);
    }
    render();
  }
}

void setColorRGB(uint16_t idx, uint8_t r, uint8_t g, uint8_t b){
  if(idx < NUM_RGB){
    uint8_t *p = &rgb_arr[idx*3];
    *p++ = g;
    *p++ = r;
    *p = b;
  }
}

void render(void){
  if(!rgb_arr) return;

  while((micros() - t_f) < 50L);  // wait for 50us (data latch)

  cli(); // Disable interrupts so that timing is as precise as possible
  volatile uint8_t
   *p    = rgb_arr,   // Copy the start address of our data array
    val  = *p++,      // Get the current byte value & point to next byte
    high = PORT |  _BV(PORT_PIN), // Bitmask for sending HIGH to pin
    low  = PORT & ~_BV(PORT_PIN), // Bitmask for sending LOW to pin
    tmp  = low,       // Swap variable to adjust duty cycle
    nbits= NUM_BITS;  // Bit counter for inner loop
  volatile uint16_t
    nbytes = NUM_BYTES; // Byte counter for outer loop
  asm volatile(
  // The volatile attribute is used to tell the compiler not to optimize
  // this section.  We want every instruction to be left as is.
  //
  // Generating an 800KHz signal (1.25us period) implies that we have
  // exactly 20 instructions clocked at 16MHz (0.0625us duration) to
  // generate either a 1 or a 0---we need to do it within a single
  // period.
  //
  // By choosing 1 clock cycle as our time unit we can keep track of
  // the signal's phase (T) after each instruction is executed.
  //
  // To generate a value of 1, we need to hold the signal HIGH (maximum)
  // for 0.8us, and then LOW (minimum) for 0.45us.  Since our timing has a
  // resolution of 0.0625us we can only approximate these values. Luckily,
  // the WS281X chips were designed to accept a +/- 300ns variance in the
  // duration of the signal.  Thus, if we hold the signal HIGH for 13
  // cycles (0.8125us), and LOW for 7 cycles (0.4375us), then the variance
  // is well within the tolerated range.
  //
  // To generate a value of 0, we need to hold the signal HIGH (maximum)
  // for 0.4us, and then LOW (minimum) for 0.85us.  Thus, holding the
  // signal HIGH for 6 cycles (0.375us), and LOW for 14 cycles (0.875us)
  // will maintain the variance within the tolerated range.
  //
  // For a full description of each assembly instruction consult the AVR
  // manual here: http://www.atmel.com/images/doc0856.pdf
    // Instruction        CLK     Description                 Phase
   "nextbit:\n\t"         // -    label                       (T =  0)
    "sbi  %0, %1\n\t"     // 2    signal HIGH                 (T =  2)
    "sbrc %4, 7\n\t"      // 1-2  if MSB set                  (T =  ?)
     "mov  %6, %3\n\t"    // 0-1   tmp'll set signal high     (T =  4)
    "dec  %5\n\t"         // 1    decrease bitcount           (T =  5)
    "nop\n\t"             // 1    nop (idle 1 clock cycle)    (T =  6)
    "st   %a2, %6\n\t"    // 2    set PORT to tmp             (T =  8)
    "mov  %6, %7\n\t"     // 1    reset tmp to low (default)  (T =  9)
    "breq nextbyte\n\t"   // 1-2  if bitcount ==0 -> nextbyte (T =  ?)
    "rol  %4\n\t"         // 1    shift MSB leftwards         (T = 11)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 13)
    "cbi   %0, %1\n\t"    // 2    signal LOW                  (T = 15)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 17)
    "nop\n\t"             // 1    nop                         (T = 18)
    "rjmp nextbit\n\t"    // 2    bitcount !=0 -> nextbit     (T = 20)
   "nextbyte:\n\t"        // -    label                       -
    "ldi  %5, 8\n\t"      // 1    reset bitcount              (T = 11)
    "ld   %4, %a8+\n\t"   // 2    val = *p++                  (T = 13)
    "cbi   %0, %1\n\t"    // 2    signal LOW                  (T = 15)
    "rjmp .+0\n\t"        // 2    nop nop                     (T = 17)
    "nop\n\t"             // 1    nop                         (T = 18)
    "dec %9\n\t"          // 1    decrease bytecount          (T = 19)
    "brne nextbit\n\t"    // 2    if bytecount !=0 -> nextbit (T = 20)
    ::
    // Input operands         Operand Id (w/ constraint)
    "I" (_SFR_IO_ADDR(PORT)), // %0
    "I" (PORT_PIN),           // %1
    "e" (&PORT),              // %a2
    "r" (high),               // %3
    "r" (val),                // %4
    "r" (nbits),              // %5
    "r" (tmp),                // %6
    "r" (low),                // %7
    "e" (p),                  // %a8
    "w" (nbytes)              // %9
  );
  sei();                          // Enable interrupts
  t_f = micros();                 // t_f will be used to measure the 50us
                                  // latching period in the next call of the
                                  // function.
}
