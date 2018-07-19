//Bodie Malik
//Summer 2018

/*
 * This program is the classic tetris game programmed to work on an arduino UNO.
 * For the input, I used the SparkFun Joystick shield kit (link below), and for the
 * display, I used the Adafruit HT1632C 24x16 RED LED matrix (link also below).
 * 
 * This program was written for this hardware, but its pretty barebones, and could
 * probably be modified to work on other different hardware.
 * 
 * The rotation system used is based off the SRS or Super Rotation System.
 * This rotation system allows you to do tons of crazy turns that isn't possible in
 * older versions of tetris. The only side effect is that the I piece behaves weirdly in SRS. 
 * I was able to save some repeating data after realizing that the CCW kick values is just -1 times
 * the CW kick values. You can read more about the SRS rotation system here:
 * http://tetris.wikia.com/wiki/SRS
 * 
 * Have fun with this code!
 * 
 * Links to hardware used:
 * https://www.sparkfun.com/products/9760
 * https://www.adafruit.com/product/555
 */

#include <EEPROM.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HT1632.h"


//************************************
//Input pins from the controller
//************************************
#define BTN_STICK 2
#define BTN_RIGHT 3
#define BTN_UP 4
#define BTN_DOWN 5
#define BTN_LEFT 6
#define JOY_VERT 1
#define JOY_HORZ 0

//Other input defines.
#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_LEFT 2
#define DIR_RIGHT 3
#define JOY_DEADZONE 300


//***********************************
//Pins for the LED Display
//***********************************
#define LED_DATA 8
#define LED_WR 9
#define LED_CS 10


//***********************************
//          CONSTANT VALUES
//***********************************
#define FPS_MILLIS 33

#define GAME_WIDTH 10
#define GAME_HEIGHT 24

#define EEPROM_RAND_SEED 0
#define EEPROM_BRIGHTNESS 4

const PROGMEM uint8_t MINO_O[] = {
	0, 0, 0, 0,
	0, 1, 1, 0,
	0, 1, 1, 0,
	0, 0, 0, 0
};

const PROGMEM uint8_t MINO_I[] = {
	0, 0, 0, 0,
	0, 0, 0, 0,
	1, 1, 1, 1,
	0, 0, 0, 0
};

const PROGMEM uint8_t MINO_L[] = {
	0, 0, 0,
	1, 1, 1,
	0, 0, 1
};

const PROGMEM uint8_t MINO_J[] = {
	0, 0, 0,
	1, 1, 1,
	1, 0, 0
};

const PROGMEM uint8_t MINO_S[] = {
	0, 0, 0,
	1, 1, 0,
	0, 1, 1
};

const PROGMEM uint8_t MINO_Z[] = {
	0, 0, 0,
	0, 1, 1,
	1, 1, 0
};

const PROGMEM uint8_t MINO_T[] = {
	0, 0, 0,
	1, 1, 1,
	0, 1, 0
};

const PROGMEM int KICK_CW_X[] = {
	0, -1, -1, 0, -1,
	0, 1, 1, 0, 1,
	0, 1, 1, 0, 1,
	0, -1, -1, 0, -1 
};

const PROGMEM int KICK_CW_Y[] = {
	0, 0, 1, -2, -2,
	0, 0, -1, 2, 2,
	0, 0, 1, -2, -2,
	0, 0, -1, 2, 2
};

const PROGMEM int KICK_CW_XI[] = {
	0, -2, 1, -2, 1,
	0, -1, 2, -1, 2,
	0, 2, -1, 2, -1,
	0, 1, -2, 1, -2
};

const PROGMEM int KICK_CW_YI[] = {
	0, 0, 0, -1, 2,
	0, 0, 0, 2, -1,
	0, 0, 0, 1, -2,
	0, 0, 0, -2, 1
};


//**************************************
//          GLOBAL VARIABLES
//**************************************
Adafruit_HT1632LEDMatrix screen = Adafruit_HT1632LEDMatrix(LED_DATA, LED_WR, LED_CS);

//TETRIS BOARD
//The tetris board is just an array of unsigned 8bit ints.
//It could have been bools, but uint8_t helped save on type conversions when drawing the board.
//It also means that the code could be modified for other displays to show colors on different values!
//Also, I ended up not using 2D arrays. It makes the math a little more interesting, but I think it was worth it.
// index = GAME_WIDTH * y + x
uint8_t TetrisBoard[GAME_WIDTH * GAME_HEIGHT];


//This is the bag that randomized pieces come from.
//Read the "generateBag()" function for more info
uint8_t Bag[16];
int BagPosition = 0;


// FALLING
//This is the piece that is currently "falling" or being placed.
//It is essencially a mini tetris board.
//To move the piece, you don't have to translate every block in the board.
//You only need to move the X, Y coordinates of the falling piece in relation
//to the TetrisBoard.
uint8_t Falling[4*4];
int FallingX = 0;
int FallingY = 0;
uint8_t FallingSize = 4; //Width and Height of falling piece.
uint8_t FallingNumber = 0; 
uint8_t FallingRotation = 0;
bool FallingSwapped = false;

// HOLDING
//Info on the piece that you are holding.
//Allows you to save your current piece and switch it out for another.
bool Holding = false;
uint8_t HoldingNumber = 0;


// MOVE DELAY
//This is the timer used to determine how fast the piece should move from left to right.
//MOVE_DELAY_LONG is used when you first start moving a piece, so that you don't accidentally move it twice.
//MOVE_DELAY_SHORT is used after you hold a direction for a while, making the piece start moving faster.
#define MOVE_DELAY_LONG 300
#define MOVE_DELAY_SHORT 150
int MoveDelay = MOVE_DELAY_LONG;


// FALL DELAY
//The time it takes for a pice to fall one line.
#define FALL_DELAY 1200
int FallDelay = FALL_DELAY;


// STAMP DELAY
//The max ammount of time you are allowed to slide a piece before you are forced to place it.
#define STAMP_DELAY 1800
int StampDelay = STAMP_DELAY;
int TimeNotMoving = 0;


// FAST FALL DELAY
//Time it takes to fall a line while you are holding the down button.
#define FAST_FALL_DELAY 130
int FastFallDelay = FAST_FALL_DELAY;


//***********************************************************************************************************
//											SETUP
//***********************************************************************************************************

void setup() {
	//Serial.begin(9600);
	
	//Use random seed from perminant storage.
	randomSeed( EEPROMReadLong( EEPROM_RAND_SEED ) );
	//Write new random seed to perminant storage. 
	EEPROMWriteLong( EEPROM_RAND_SEED, random( 2147483647 ));


	//Set pin mode for all button inputs.
	//Use the internal pullup resistor since i'm lazy and
	//don't want to connect extra resistors.
	//This does mean that the input you recieve will be reversed though.
	pinMode(BTN_STICK, INPUT_PULLUP);
	pinMode(BTN_RIGHT, INPUT_PULLUP);
	pinMode(BTN_UP, INPUT_PULLUP);
	pinMode(BTN_DOWN, INPUT_PULLUP);
	pinMode(BTN_LEFT, INPUT_PULLUP);


	//initialize screen.
	screen.begin(ADA_HT1632_COMMON_16NMOS);
	screen.clearScreen();
	screen.setBrightness( EEPROM.read(EEPROM_BRIGHTNESS) );

	//border only needs to be drawn once.
	drawBorder();

	//Reset Board
	resetBoard();

	generateBag(0);
	generateBag(8);
	
	spawnMino();
}



//*************************************************************************************************
//								MAIN LOOP
//*************************************************************************************************

void loop() {
	//Always update input at the beginning of every frame.
	updateInput();
	
	TimeNotMoving += FPS_MILLIS;

	//*********************************
	// Left-Right movement
	//*********************************
	
	//Save current position. Later, if collision detected, revert to this position.
	int orgX = FallingX;
	int orgTimeNotMoving = TimeNotMoving;

	if( getJoy(DIR_LEFT) )
	{
		MoveDelay -= FPS_MILLIS;
		if( MoveDelay <= 0 )
		{
			FallingX -= 1;
			TimeNotMoving = 0;
			MoveDelay += MOVE_DELAY_SHORT;
		}

		if( getJoyPressed(DIR_LEFT) )
		{
			FallingX -= 1;
			TimeNotMoving = 0;
		}
		
	}
	else if( getJoy(DIR_RIGHT) )
	{
		MoveDelay -= FPS_MILLIS;
		if( MoveDelay <= 0 )
		{
			FallingX += 1;
			TimeNotMoving = 0;
			MoveDelay += MOVE_DELAY_SHORT;
		}

		if( getJoyPressed(DIR_RIGHT) )
		{
			FallingX += 1;
			TimeNotMoving = 0;
		}
	}
	else
	{
		MoveDelay = MOVE_DELAY_LONG;
	}
	
	//Check for collision after moving.
	if( collideFalling() )
	{
		FallingX = orgX;
		TimeNotMoving = orgTimeNotMoving;
	}

	
	//*********************************
	// Rotation
	//*********************************
	if(getButtonPressed(BTN_RIGHT))
	{
		//Which kick line should we read from?
		int kickMain = FallingRotation * 5;
		rotateCW();

		if(FallingNumber != 0) //If Falling is not the O tetrimino, then we need to test some kick values.
		{
			int *KickDataPointerX;
			int *KickDataPointerY;

			if(FallingNumber == 1)
			{
				KickDataPointerX = KICK_CW_XI;
				KickDataPointerY = KICK_CW_YI;
			}
			else
			{
				KickDataPointerX = KICK_CW_X;
				KickDataPointerY = KICK_CW_Y;
			}
			
			bool kickComplete = false;
			
			for(int i = 0; i < 5; i++)
			{
				int testX = pgm_read_word_near( KickDataPointerX + kickMain + i );
				int testY = pgm_read_word_near( KickDataPointerY + kickMain + i );
				if( !kickTest( testX, testY) )
				{
					FallingX += testX;
					FallingY += testY;
					kickComplete = true;
					TimeNotMoving = 0;
					break;
				}
			}
	
			if(!kickComplete) //Kick failed. Rotate back to original orientation.
			{
				rotateCCW();
			}
			
		}
		
	}
	else if(getButtonPressed(BTN_LEFT))
	{
		rotateCCW();
		//Which kick line should we read from?
		//Notice this line is after the rotate function, unlike the CW rotate.
		//This allows us to re-use the same rotation data, even though we are going the opposite direction.
		//The only other difference is that all kick values must be multiplied by -1
		int kickMain = FallingRotation * 5;

		if(FallingNumber != 0) //If Falling is not the O tetrimino, then we need to test some kick values.
		{
			int *KickDataPointerX;
			int *KickDataPointerY;

			if(FallingNumber == 1)
			{
				KickDataPointerX = KICK_CW_XI;
				KickDataPointerY = KICK_CW_YI;
			}
			else
			{
				KickDataPointerX = KICK_CW_X;
				KickDataPointerY = KICK_CW_Y;
			}
			
			bool kickComplete = false;

			for(int i = 0; i < 5; i++)
			{
				int testX = -1 * pgm_read_word_near( KickDataPointerX + kickMain + i );
				int testY = -1 * pgm_read_word_near( KickDataPointerY + kickMain + i );
				
				if( !kickTest( testX, testY) )
				{
					FallingX += testX;
					FallingY += testY;
					kickComplete = true;
					TimeNotMoving = 0;
					break;
				}
			}
	
			if(!kickComplete) //Kick failed. Rotate back to original orientation.
			{
				rotateCW();
			}
		}
	}


	//********************************************
	//		Holding mechanic
	//********************************************
	
	if(getButtonPressed(BTN_UP) && !getButtonDown(BTN_STICK) && !FallingSwapped)
	{
		FallingSwapped = true;
		
		if(!Holding)
		{
			Holding = true;
			HoldingNumber = FallingNumber;
			spawnMino();
		}
		else
		{
			uint8_t tempNum = FallingNumber;

			spawnMino(HoldingNumber);
			HoldingNumber = tempNum;
		}
	}
	
	//********************************************
	// Falling
	//********************************************
	
	bool doClear = false;
	int clearY = -1;
	
	FallDelay -= FPS_MILLIS;
	StampDelay -= FPS_MILLIS;
	if( getJoy(DIR_DOWN) )
		FastFallDelay -= FPS_MILLIS;


	//Normal falling movement happens when the user is not sitting on something.
	if(!isSitting())
	{
		if(FallDelay <= 0 || FastFallDelay <= 0)
		{
			FallingY -= 1;
			FallDelay = FALL_DELAY;
			FastFallDelay = FAST_FALL_DELAY;
			StampDelay = STAMP_DELAY;
			TimeNotMoving = 0;
		}
	}
	else//If you are sitting on something, allow player to slide longer than normal (unless he is not moving.)
	{
		if(TimeNotMoving > 500 || StampDelay <= 0)
		{
			stamp();
			doClear = true;
			clearY = FallingY;
			StampDelay = STAMP_DELAY;
			
			spawnMino();
		}
	}


	// Teleport down if you press the hard drop button.
	if(getButtonPressed(BTN_DOWN) && !getButtonDown(BTN_STICK))
	{
		while( !collideFalling() )
		{
			FallingY -= 1;
		}

		FallingY += 1;
		
		stamp();
		doClear = true;
		clearY = FallingY;
		spawnMino();
	}

	//When a tetrimino was stamped, check to clear lines.
	if(doClear)
		clearLines(clearY);


	//Change brightness settings?
	if(getButtonDown(BTN_STICK) && getButtonPressed(BTN_UP))
	{
		increaseBrightness();
	}
	else if(getButtonDown(BTN_STICK) && getButtonPressed(BTN_DOWN))
	{
		decreaseBrightness();
	}
	
	//Always finish the frame by drawing.
	draw();

	//Delay until next frame. Uses target frame rate.
	delay(millis() % FPS_MILLIS);
}



//**************************************************************
//					ROTATION FUNCTIONS
//**************************************************************

//Rotates the falling piece CW.
void rotateCW()
{
	uint8_t oldFalling[16];

	//Transpose
	copyFromFalling(oldFalling);
	
	for(int x = 0; x < FallingSize; x++)
	{
		for(int y = 0; y < FallingSize; y++)
		{
			int newx = y;
			int newy = x;

			Falling[FallingSize * y + x] = oldFalling[FallingSize * newy + newx];
		}
	}

	//Reverse each row
	copyFromFalling(oldFalling);

	for(int y = 0; y < FallingSize; y++)
	{
		int inverseY = FallingSize - y - 1;
		for(int x = 0; x < FallingSize; x++)
		{
			Falling[FallingSize * y + x] = oldFalling[FallingSize * inverseY + x];
		}
	}

	FallingRotation = (FallingRotation + 1) % 4;
}


//Rotates the falling piece CCW.
void rotateCCW()
{
	uint8_t oldFalling[16];

	//Reverse each row
	copyFromFalling(oldFalling);

	for(int y = 0; y < FallingSize; y++)
	{
		int inverseY = FallingSize - y - 1;
		for(int x = 0; x < FallingSize; x++)
		{
			Falling[FallingSize * y + x] = oldFalling[FallingSize * inverseY + x];
		}
	}

	//Transpose
	copyFromFalling(oldFalling);
	
	for(int x = 0; x < FallingSize; x++)
	{
		for(int y = 0; y < FallingSize; y++)
		{
			int newx = y;
			int newy = x;

			Falling[FallingSize * y + x] = oldFalling[FallingSize * newy + newx];
		}
	}

	FallingRotation = (FallingRotation + 3) % 4;
}


//**************************************************************************************
//                                 INPUT FUNCTIONS
//**************************************************************************************
//Store inputs from one frame before so that you can tell if the user just pressed a button.
bool OldButtons[5];
bool NewButtons[5];

int OldHorz;
int NewHorz;
int OldVert;
int NewVert;

//Reads new inputs and stores them in their variables.
void updateInput()
{
	for(int i = 0; i < 5; i++)
	{
		OldButtons[i] = NewButtons[i];
		NewButtons[i] = digitalRead(i + 2) == HIGH;
	}

	OldHorz = NewHorz;
	OldVert = NewVert;

	NewHorz = analogRead(JOY_HORZ);
	NewVert = analogRead(JOY_VERT);
}

//Returs true if "button" is being held down.
//example: bool foo = getButtonDown(BTN_LEFT);
bool getButtonDown(int button)
{
	return !(NewButtons[button - 2]);
}

//Returns true only on the first frame that a button is pressed.
bool getButtonPressed(int button)
{
	return (!NewButtons[button - 2] && OldButtons[button - 2]);
}

//Returns true only on the first frame that a button is released.
bool getButtonReleased(int button)
{
	return (NewButtons[button - 2] && !OldButtons[button - 2]);
}

//Returns true if the joystick is being held in direction "dir"
//example: bool heldLeft = getJoy(DIR_LEFT);
bool getJoy(int dir)
{
	switch(dir)
	{
		case DIR_UP:
			return (NewVert > 512 + JOY_DEADZONE);
		case DIR_DOWN:
			return (NewVert < 512 - JOY_DEADZONE);
		case DIR_LEFT:
			return (NewHorz < 512 - JOY_DEADZONE);
		case DIR_RIGHT:
			return (NewHorz > 512 + JOY_DEADZONE);
	}
}

//Returns true on the first frame that the joystick moves.
bool getJoyPressed(int dir)
{
	switch(dir)
	{
		case DIR_UP:
			return (NewVert > 512 + JOY_DEADZONE) && (OldVert <= 512 + JOY_DEADZONE);
		case DIR_DOWN:
			return (NewVert < 512 - JOY_DEADZONE) && (OldVert >= 512 - JOY_DEADZONE);
		case DIR_LEFT:
			return (NewHorz < 512 - JOY_DEADZONE) && (OldHorz >= 512 - JOY_DEADZONE);
		case DIR_RIGHT:
			return (NewHorz > 512 + JOY_DEADZONE) && (OldHorz <= 512 + JOY_DEADZONE);
	}
}

//returns true on the first frame that the joystick returns to nuetral.
bool getJoyReleased(int dir)
{
	switch(dir)
	{
		case DIR_UP:
			return (NewVert <= 512 + JOY_DEADZONE) && (OldVert > 512 + JOY_DEADZONE);
		case DIR_DOWN:
			return (NewVert >= 512 - JOY_DEADZONE) && (OldVert < 512 - JOY_DEADZONE);
		case DIR_LEFT:
			return (NewHorz >= 512 - JOY_DEADZONE) && (OldHorz < 512 - JOY_DEADZONE);
		case DIR_RIGHT:
			return (NewHorz <= 512 + JOY_DEADZONE) && (OldHorz > 512 + JOY_DEADZONE);
	}
}


//**************************************************************************************
//                                 DRAW FUNCTIONS
//**************************************************************************************
void draw()
{
	drawBoard();
	drawFalling();

	drawNextPiece(12,20,0);
	drawNextPiece(12,15,1);
	drawNextPiece(12,10,2);

	if(Holding)
		drawExtraPiece(12,0,HoldingNumber);
	
	//drawInputDebug();
	
	screen.writeScreen();
}

void drawBoard()
{
	for(int x = 0; x < GAME_WIDTH; x++)
	{
		for(int y = 0; y < GAME_HEIGHT; y++)
		{
			screen.drawPixel(y,x + 1, getBlock(x,y));
		}
	}
}

void drawBorder()
{
	/*
	for(int x = 0; x < GAME_WIDTH + 1; x++)
	{
		screen.setPixel(GAME_HEIGHT,x);
	}
	*/

	for(int y = 0; y < GAME_HEIGHT; y++)
	{
		screen.setPixel(y, GAME_WIDTH + 1);
		screen.setPixel(y, 0);
	}
}

void drawFalling()
{
	for(int x = 0; x < FallingSize; x++)
	{
		for(int y = 0; y < FallingSize; y++)
		{
			if(Falling[FallingSize * y + x] > 0)
			{
				//Don't draw empty blocks. They are supposed to be transparent.
				screen.setPixel( y + FallingY, x + FallingX + 1);
			}
		}
	}
}

void drawNextPiece(int xpos, int ypos, int jmp)
{
	int minoNum = Bag[(BagPosition + jmp) % 16];

	drawExtraPiece(xpos, ypos, minoNum);
}

void drawExtraPiece(int xpos, int ypos, int num)
{
	int minoNum = num;
	int minoSize = 3;
	uint8_t *mino;

	switch( minoNum )
	{
		case 0: //MINO_O
			minoSize = 4;
			mino = MINO_O;
			break;
		case 1: //MINO_I
			minoSize = 4;
			mino = MINO_I;
			break;
		case 2: //MINO_L
			mino = MINO_L;
			break;
		case 3: //MINO_J
			mino = MINO_J;
			break;
		case 4: //MINO_S
			mino = MINO_S;
			break;
		case 5: //MINO_Z
			mino = MINO_Z;
			break;
		case 6: //MINO_T
			mino = MINO_T;
			break;
	}

	for(int x = 0; x < minoSize; x++)
	{
		for(int y = 0; y < minoSize; y++)
		{
			screen.drawPixel(ypos + y, xpos + x, pgm_read_byte_near( mino + (minoSize * y + x) ));
		}
	}

	if(minoSize == 3)
	{
		//Make sure to clear those extra pixels on the sides that may have been drawn by bigger pices.
		for(int y = 0; y < 4; y++)
		{
			screen.drawPixel(ypos + y, xpos + 3, 0);
		}

		for(int x = 0; x < 4; x++)
		{
			screen.drawPixel(ypos + 3, xpos + x, 0);
		}
	}
}

void increaseBrightness()
{
	uint8_t bright = EEPROM.read(EEPROM_BRIGHTNESS) + 1;
	
	if(bright >= 16)
		bright = 15;

	screen.setBrightness(bright);

	EEPROM.write(EEPROM_BRIGHTNESS, bright);
}

void decreaseBrightness()
{
	uint8_t bright = EEPROM.read(EEPROM_BRIGHTNESS) - 1;
	
	if(bright >= 16)
		bright = 0;

	screen.setBrightness(bright);

	EEPROM.write(EEPROM_BRIGHTNESS, bright);
}

/*
void drawInputDebug()
{
	screen.drawPixel(1,13, getButtonDown(BTN_DOWN));
	screen.drawPixel(3,13, getButtonDown(BTN_UP));
	screen.drawPixel(2,12, getButtonDown(BTN_LEFT));
	screen.drawPixel(2,14, getButtonDown(BTN_RIGHT));
	screen.drawPixel(2,13, getButtonDown(BTN_STICK));

	screen.drawPixel(0,13, getButtonPressed(BTN_DOWN));
	screen.drawPixel(4,13, getButtonPressed(BTN_UP));
	screen.drawPixel(2,11, getButtonPressed(BTN_LEFT));
	screen.drawPixel(2,15, getButtonPressed(BTN_RIGHT));
	screen.drawPixel(1,12, getButtonPressed(BTN_STICK));

	screen.drawPixel(9,13, getJoy(DIR_DOWN));
	screen.drawPixel(11,13, getJoy(DIR_UP));
	screen.drawPixel(10,12, getJoy(DIR_LEFT));
	screen.drawPixel(10,14, getJoy(DIR_RIGHT));

	screen.drawPixel(8,13, getJoyPressed(DIR_DOWN));
	screen.drawPixel(12,13, getJoyPressed(DIR_UP));
	screen.drawPixel(10,11, getJoyPressed(DIR_LEFT));
	screen.drawPixel(10,15, getJoyPressed(DIR_RIGHT));
}
*/

//****************************************************************
//                         BOARD FUNCTIONS
//****************************************************************
uint8_t getBlock(uint8_t x, uint8_t y)
{
	return getBlock(GAME_WIDTH * y + x);
}

uint8_t getBlock(int i)
{
	return TetrisBoard[i];
}

void setBlock(uint8_t x, uint8_t y, uint8_t val)
{
	setBlock(GAME_WIDTH * y + x, val);
}

void setBlock(int i, uint8_t val)
{
	TetrisBoard[i] = val;
}

bool isRowFull(int y)
{
	if(y >= GAME_HEIGHT)
		return false;
	
	for(int x = 0; x < GAME_WIDTH; x++)
	{
		if(getBlock(x,y) == 0)
			return false;
	}

	return true;
}

void clearLines(int startY)
{
	int gap = 0;

	for(int y = startY; y < GAME_HEIGHT; y++)
	{
		if(y < 0)
			continue;
		
		while( isRowFull(y + gap) )
			gap++;

		if(gap > 0)
			copyRow(y + gap, y);
	}
}


void copyRow(int fromY, int toY)
{
	if(fromY >= GAME_HEIGHT)
	{
		for(int x = 0; x < GAME_WIDTH; x++)
		{
			setBlock(x, toY, 0);
		}
	}
	else
	{
		for(int x = 0; x < GAME_WIDTH; x++)
		{
			setBlock(x, toY, getBlock(x, fromY));
		}	
	}
}

bool collideBlock(int x, int y)
{
	if(x < 0 || x >= GAME_WIDTH || y < 0 || y >= GAME_HEIGHT)
	{
		return true;
	}

	if( getBlock(x,y) > 0 )
		return true;
	else
		return false;
}

bool collideFalling()
{
	for(int x = 0; x < FallingSize; x++)
	{
		for(int y = 0; y < FallingSize; y++)
		{
			if( Falling[FallingSize * y + x] > 0 && collideBlock(x + FallingX, y + FallingY))
				return true;
		}
	}

	return false;
}

bool kickTest(int x, int y)
{
	FallingX += x;
	FallingY += y;
	bool ret = collideFalling();
	FallingX -= x;
	FallingY -= y;
	return ret;
}

bool isSitting()
{
	FallingY -= 1;
	bool ret = collideFalling();
	FallingY += 1;
	return ret;
}

void stamp()
{
	FallingSwapped = false;
	
	for(int x = 0; x < FallingSize; x++)
	{
		for(int y = 0; y < FallingSize; y++)
		{
			if(Falling[FallingSize * y + x] > 0)
				setBlock(x + FallingX, y + FallingY, 1);
		}
	}
}

void spawnMino()
{
	//Spawns mino from bag.
	spawnMino(Bag[BagPosition]);

	BagPosition = (BagPosition + 1) % 16;
	if(BagPosition == 0)
		generateBag(8);
	else if(BagPosition == 8)
		generateBag(0);
}

void spawnMino(int mino)
{	
	FallingX = 3;
	FallingY = 20;
	FallingRotation = 0;
	FallingNumber = mino;
	
	//Setting the correct falling size...
	switch( mino )
	{
		case 0:
		case 1:
			FallingSize = 4;
			break;
		default:
			FallingSize = 3;
	}

	switch( mino )
	{
		case 0: //MINO_O
			copyToFalling( MINO_O );
			break;
		case 1: //MINO_I
			copyToFalling( MINO_I );
			break;
		case 2: //MINO_L
			copyToFalling( MINO_L );
			break;
		case 3: //MINO_J
			copyToFalling( MINO_J );
			break;
		case 4: //MINO_S
			copyToFalling( MINO_S );
			break;
		case 5: //MINO_Z
			copyToFalling( MINO_Z );
			break;
		case 6: //MINO_T
			copyToFalling( MINO_T );
			break;
	}

	if( collideFalling() )
		resetBoard();
}

void copyToFalling(uint8_t *mino)
{
	for(int i = 0; i < FallingSize * FallingSize; i++)
	{
		Falling[i] = pgm_read_byte_near( mino + i );
	}
}

void copyFromFalling(uint8_t *mino)
{
	for(int i = 0; i < FallingSize * FallingSize; i++)
	{
		mino[i] = Falling[i];
	}
}

void resetBoard()
{
	for(int i = 0; i < GAME_WIDTH * GAME_HEIGHT; i++)
	{
		TetrisBoard[i] = 0;
	}

	Holding = false;
	
	BagPosition = 0;
	generateBag(0);
	generateBag(8);
}

//Generates 8 pice indecies in the bag, starting at index "start"
//Puts each of the 7 tetriminos in the bag in random order.
//Then puts 1 extra random tetrimino in the bag.
void generateBag(int start)
{

	uint8_t sortedBag[] = {0, 1, 2, 3, 4, 5, 6};

	for(int i = 0; i < 7; i++)
	{
		int randIndex = random(0, 7-i);
		Bag[start + i] = sortedBag[randIndex];

		//swap the value we just grabbed, and put it in an index we won't use again.
		//Eliminates the chance of a piece getting picked twice.
		uint8_t temp = sortedBag[randIndex];
		sortedBag[randIndex] = sortedBag[6-i];
		sortedBag[6-i] = temp;
	}
	
	//put one random piece in, just for kicks.
	Bag[start + 7] = random(0,7);
}

/*
void printArr(uint8_t *arr, int len)
{
	for(int i = 0; i < len; i++)
	{
		Serial.print( arr[i] );
		Serial.print(" ");
	}
}
*/

//****************************************************************
//                      EEPROM STUFF
//****************************************************************
long EEPROMReadLong(long address)
{
	//Read the 4 bytes from the eeprom memory.
	long four = EEPROM.read(address);
	long three = EEPROM.read(address + 1);
	long two = EEPROM.read(address + 2);
	long one = EEPROM.read(address + 3);
	
	//Return the recomposed long by using bitshift.
	return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void EEPROMWriteLong(int address, long value)
{
	//Decomposition from a long to 4 bytes by using bitshift.
	//One = Most significant -> Four = Least significant byte
	byte four = (value & 0xFF);
	byte three = ((value >> 8) & 0xFF);
	byte two = ((value >> 16) & 0xFF);
	byte one = ((value >> 24) & 0xFF);
	//Write the 4 bytes into the eeprom memory.
	EEPROM.write(address, four);
	EEPROM.write(address + 1, three);
	EEPROM.write(address + 2, two);
	EEPROM.write(address + 3, one);
}
