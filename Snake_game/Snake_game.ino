// Title: Snake Game
// Version: v0.1
// Description: Snake game for custom arduino shield
// Date: 2024/06/10

// hardware interface
#define data 30  // shift register pins
#define shift 32
#define latch 34
const byte cols[8] = {22, 23, 24, 25, 26, 27, 28, 29};  // columns pin mapping
enum Dir {UP=31, RIGHT=33, LEFT=35, DOWN=37};           // buttons pin mapping

// constants
enum Mode {OPEN=0, PLAY, END};        // finite state machine
const int FRAME_PAUSE = 2;

// animations
// row-wise big endian
// (0, 0) is at the top left corner
const uint8_t opening[3][8] = {  // looping rectangular wave
  {0xff, 0x81, 0x81, 0x99, 0x99, 0x81, 0x81, 0xff},
  {0x00, 0x00, 0x3c, 0x24, 0x24, 0x3c, 0x00, 0x00},
  {0x00, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x7e, 0x00}
};

const uint8_t ending[8][8] = {  // GAMEOVER
  {0x00, 0x7c, 0x40, 0x5c, 0x44, 0x7c, 0x00, 0x00},
  {0x00, 0x10, 0x28, 0x38, 0x28, 0x28, 0x00, 0x00},
  {0x00, 0x44, 0x6c, 0x54, 0x44, 0x44, 0x00, 0x00},
  {0x00, 0x7c, 0x40, 0x7c, 0x40, 0x7c, 0x00, 0x00},
  {0x00, 0x38, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x44, 0x44, 0x28, 0x28, 0x10, 0x00, 0x00},
  {0x00, 0x7c, 0x40, 0x7c, 0x40, 0x7c, 0x00, 0x00},
  {0x00, 0x70, 0x48, 0x48, 0x70, 0x50, 0x48, 0x00}
};

// linked list node
struct Node {
  Node* next;
  byte x;
  byte y;
};

// variables
Node* head = new Node{NULL, 0, 0};  // head of snake
Node* tail = head;                  // tail of snake
Node* food = new Node{NULL, 3, 3};  // food to make snake grow
uint8_t board[8] = {0};  // game board (HIGH for occupied, LOW for vacant)
byte snakeLen;           // length of snake
Dir dir;                 // direction snake is facing
Mode mode = OPEN;        // current game mode

// write initial values
void initVals() {
  head->next = NULL;
  head->x = 0;
  head->y = 0;
  tail = head;
  food->x = 3;
  food->y = 3;
  snakeLen = 1;
  dir = RIGHT;
}

void setup() {
  // set up shift
  pinMode(data, OUTPUT);
  pinMode(shift, OUTPUT);
  pinMode(latch, OUTPUT);
  digitalWrite(data, LOW);

  // set up columns
  for(byte i = 0; i < 8; i++) {
    pinMode(cols[i], OUTPUT);
  }

  // set up buttons
  pinMode(UP, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);

  // other set up
  Serial.begin(9600);
  randomSeed(analogRead(0));

  // set initial game values
  initVals();
}

// shift and latch one bit in the register
void shiftOnce() {
  digitalWrite(shift, HIGH);
  digitalWrite(shift, LOW);
  digitalWrite(latch, HIGH);
  digitalWrite(latch, LOW);
}

// read one bit of a matrix
bool readBit(uint8_t *mat, byte x, byte y) {
  // big endian (x==0 is MSB)
  return (mat[y] >> (7 - x)) & 1;
}

// write one bit to the game board
void writeBit(bool val, byte x, byte y) {
  // bit hacks to set bit
  if (val) {
    board[y] |= (1 << (7 - x));
  } else {
    board[y] &= ~(1 << (7 - x));
  }
}

// draw matrix to led
void draw(uint8_t *mat) {
  bool first = true; // first row check
  for(byte y = 0; y < 8; y++) {
    for(byte x = 0; x < 8; x++) {
      digitalWrite(cols[x], !readBit(mat, x, y));
    }
    if (first) { // shift a HIGH bit if first row
      digitalWrite(data, HIGH);
      shiftOnce();
      digitalWrite(data, LOW);
      first = false;
    } else {
      shiftOnce();
    }
    readButtons();
    delay(FRAME_PAUSE);
  }
  shiftOnce(); // remove HIGH bit
}

// opening splash screen
void splash() {
  for(byte i = 0; i < 3; i++) {
    for(byte j = 0; j < 4; j++) {
      draw(opening[i]);
    }
  }
}

void runGame() {
  char dx = 0; // small signed number
  char dy = 0;
  switch (dir) {
    case UP: dy = -1; break;
    case RIGHT: dx = 1; break;
    case LEFT: dx = -1; break;
    case DOWN: dy = 1; break;
  }

  // pointer to new position
  Node* newPos = new Node{NULL, head->x + dx, head->y + dy};

  // if out of range
  if (newPos->x < 0 || newPos->x >= 8 || newPos->y < 0 || newPos->y >= 8) {
    mode = END;
    return;
  }

  // if new position occupied
  if (readBit(board, newPos->x, newPos->y)) {
    // if there is food in new position
    if (newPos->x == food->x && newPos->y == food->y) {
      head->next = newPos;
      head = head->next;
      snakeLen++;
      makeFood();
    } else {
      mode = END;
    }
    return;
  }

  // move head
  head->next = newPos;
  head = head->next;
  writeBit(1, head->x, head->y);

  // move tail
  Node* oldTail = tail;
  tail = tail->next;
  writeBit(0, oldTail->x, oldTail->y);
  delete oldTail;

  // draw food
  writeBit(1, food->x, food->y);

  for(byte i = 0; i < 12; i++) {
    draw(board);
  }
}

// game over animation
void gameOver() {
  for(byte i = 0; i < 8; i++) {
    for(byte j = 0; j < 16; j++) {
      draw(ending[i]);
    }
  }
  cleanup();
  mode = OPEN;
}

void readButtons() {
  // read direction
  Dir tempDir;
  if (!digitalRead(UP)) {
    tempDir = UP;
  } else if (!digitalRead(RIGHT)) {
    tempDir = RIGHT;
  } else if (!digitalRead(LEFT)) {
    tempDir = LEFT;
  } else if (!digitalRead(DOWN)) {
    tempDir = DOWN;
  } else {
    return;
  }

  // start the game if in opening mode
  if (mode == OPEN) {
    mode = PLAY;
    return;
  }

  // cannot move in opposite direction
  if ((dir == LEFT && tempDir == RIGHT) || (dir == RIGHT && tempDir == LEFT)
      || (dir == UP && tempDir == DOWN) || (dir == DOWN && tempDir == UP)) {
    return;
  }
  // write direction
  dir = tempDir;
}

// generate new food
void makeFood() {
  byte y;
  do { // pick a row that's not completely occupied
    y = random(8);
  } while (board[y] == 0xFF);
  byte x;
  do { // pick a vacant position in that row
    x = random(8);
    // due to the way this is programmed,
    // eating corner food results in instant death
  } while (readBit(board, x, y) || ((x == 0 || x == 7) && (y == 0 || y == 7)));
  food->x = x;
  food->y = y;
}

void cleanup() {
  // clear game board
  for (byte i = 0; i < 8; i++) {
    board[i] = 0x00;
  }

  // deallocate memory for snake
  while(tail->next != NULL && tail->next->next != NULL) {
    Node* oldTail = tail;
    tail = tail->next;
    delete oldTail;
  }

  // set initial game values
  initVals();
}

// main game loop
void loop() {
  switch(mode) {
    case OPEN:
      splash();
      break;
    case PLAY:
      runGame();
      break;
    case END:
      gameOver();
      break;
  };
}
