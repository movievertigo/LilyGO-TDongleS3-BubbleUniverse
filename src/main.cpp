#include <TFT_eSPI.h>
#include <OneButton.h>
#include <FastLED.h>
#include "EEPROM.h"
#include "pinconfig.h"

const float sizes[]    = {0.25f, 0.50f, 0.50f, 0.50f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f};
const float xOffsets[] = {0.50f, 0.00f, 0.50f, 1.00f, 2.00f, 1.25f, 0.50f,-0.25f,-1.00f,-0.25f, 0.50f, 1.25f, 0.50f};
const float yOffsets[] = {0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 1.00f, 1.50f, 1.00f, 0.50f, 0.00f,-0.50f, 0.00f, 0.50f};

const int sinTableSize = 1<<16;
const float sinTableScale = sinTableSize / (2*PI);

const int pixelSize = TFT_WIDTH < TFT_HEIGHT ? TFT_WIDTH : TFT_HEIGHT;
const int pixelOffsetX = (TFT_WIDTH - pixelSize)/2;
const int pixelOffsetY = (TFT_HEIGHT - pixelSize)/2;

const int eepromAddress_PositionIndex = 0;

float* sinTable;
uint16_t *offscreen1, *offscreen2, *offscreen;

TFT_eSPI screen = TFT_eSPI();
OneButton button(BTN_PIN, true);
CRGB leds;

int curveCount = 256;
int curveStep = 4;
int iterationCount = 512;
float speed = 0.0001f;
float size, xOffset, yOffset;
float positionChangeFactor = 0.1f;
uint8_t currentPositionIndex;

float ang1inc = curveStep * 2 * PI / 235;
float ang2inc = curveStep;

int frame = 0;
int totalTime = 0;

void createSinTable()
{
    sinTable = (float*)malloc(sinTableSize * sizeof(float));

    for (int i = 0; i < sinTableSize/4; ++i)
    {
        float value = sin(i * 2 * PI / sinTableSize);
        sinTable[i] = value;
        sinTable[sinTableSize/2-i-1] = value;
        sinTable[sinTableSize/2+i] = -value;
        sinTable[sinTableSize-i-1] = -value;
    }
}

void nextPosition()
{
    currentPositionIndex = (currentPositionIndex + 1) % (sizeof(sizes) / sizeof(float));
    EEPROM.write(eepromAddress_PositionIndex, currentPositionIndex);
    EEPROM.commit();
}

void setup()
{
    pinMode(TFT_LEDA_PIN, OUTPUT);
    digitalWrite(TFT_LEDA_PIN, 1);

    offscreen1 = (uint16_t*)malloc(TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t));
    offscreen2 = (uint16_t*)malloc(TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t));
    offscreen = offscreen1;

    Serial.begin(115200);

    EEPROM.begin(1);
    currentPositionIndex = EEPROM.read(eepromAddress_PositionIndex) % (sizeof(sizes) / sizeof(float));
    size = sizes[currentPositionIndex];
    xOffset = xOffsets[currentPositionIndex];
    yOffset = yOffsets[currentPositionIndex];

    screen.begin();
    screen.setRotation(0);
    screen.setSwapBytes(true);
    screen.fillScreen(TFT_BLACK);
    screen.initDMA();

    leds = CRGB(0,0,0);
    FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(&leds, 1);
    FastLED.show();

    button.attachClick(nextPosition);

    createSinTable();

    digitalWrite(TFT_LEDA_PIN, 0);
}

void loop()
{
    button.tick();
    size = sizes[currentPositionIndex] * positionChangeFactor + size * (1-positionChangeFactor);
    xOffset = xOffsets[currentPositionIndex] * positionChangeFactor + xOffset * (1-positionChangeFactor);
    yOffset = yOffsets[currentPositionIndex] * positionChangeFactor + yOffset * (1-positionChangeFactor);

    leds = CRGB(-sinTable[(millis()*10)&(sinTableSize-1)]*63+64, 0, sinTable[(millis()*10)&(sinTableSize-1)]*127+128);
    FastLED.show();

    float time = millis() * speed;

    memset(offscreen, 0, TFT_WIDTH*TFT_HEIGHT*2);

    float ang1Start = time, ang2Start = time;

    for (int i = 0; i < curveCount; i += curveStep)
    {
        int red = (i<<8)/curveCount;

        float x = 0, y = 0;
        for (int j = 0; j < iterationCount; ++j)
        {
            int sinoffset1 = (int)((ang1Start + x) * sinTableScale) & (sinTableSize-1);
            int sinoffset2 = (int)((ang2Start + y) * sinTableScale) & (sinTableSize-1);

            x = sinTable[sinoffset1] + sinTable[sinoffset2];
            y = sinTable[(sinoffset1+sinTableSize/4) & (sinTableSize-1)] + sinTable[(sinoffset2+sinTableSize/4) & (sinTableSize-1)];
            int px = (x*size+xOffset)*pixelSize + pixelOffsetX;
            int py = (y*size+yOffset)*pixelSize + pixelOffsetY;
            int green = (j<<8)/iterationCount;
            int blue = 255 - (red+green)/2;
            if (px >= 0 && px < TFT_WIDTH && py >= 0 && py < TFT_HEIGHT)
            {
                offscreen[py*TFT_WIDTH + px] = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
            }
        }

        ang1Start += ang1inc;
        ang2Start += ang2inc;
    }

    screen.pushImageDMA(0, 0, TFT_WIDTH, TFT_HEIGHT, offscreen);
    offscreen = offscreen == offscreen1 ? offscreen2 : offscreen1;
}
