#include <mbed.h>
#include <math.h>
#include <vector>
#include <cmath>
#include "stm32f4xx_hal.h"
#include "stm32f429i_discovery_lcd.h"

// This is so dumb, for some reason the LCD was throwing 
// an error w/ out it, so just had to define my own func
extern "C" void wait_ms(int ms) {
    for (int i = 0; i < ms; i++) {
        for (int j = 0; j < 1000; j++) {
            __asm("nop");
        }
    }
}


struct GyroData
{
  int16_t x;
  int16_t y;
  int16_t z;
};

const int threshold = 100;
const int max_sequence_length = 300;

volatile bool flag = false;

SPI spi(PF_9, PF_8, PF_7);
DigitalOut cs(PC_1);

// "Enter key" and "Record" button
InterruptIn enter_key(USER_BUTTON);

void setFlag();
void setMode();
void setupGyro();
void initDisplay();
bool compareSequences(const std::vector<GyroData>& seq1, const std::vector<GyroData>& seq2, double threshold);
double computeCrossCorrelation(const std::vector<double>& x, const std::vector<double>& y);
GyroData readGyro();


// Define state variables
enum State {IDLE, RECORDING, WAITING, UNLOCKING, CHECKING, LOCKED};
State state = IDLE;

void updateState() {
    flag = true;
}

int main() {
    setupGyro();
    initDisplay();

    // Now we'll initialize two arrays which contain the recorded unlock sequence 
    // and attempted one.
    std::vector<GyroData> record_data(max_sequence_length);
    std::vector<GyroData> attempt_data(max_sequence_length);
    int data_index = 0;
  
    int remaining_attempts = 5;
    bool correct = false;

    enter_key.fall(&updateState);
    while (1) {
        if (flag) {
            switch(state) 
            {
                case IDLE:
                    record_data.clear();
                    attempt_data.clear();
                    BSP_LCD_Clear(LCD_COLOR_WHITE);
                    printf("IDLE -> RECORDING\n");
                    state = RECORDING;
                    break;
                case RECORDING: 
                    printf("RECORDING -> WAITING\n");
                    BSP_LCD_Clear(LCD_COLOR_WHITE);
                    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);  
                    BSP_LCD_DisplayStringAt(0, LINE(5), (uint8_t *)"Recording...", CENTER_MODE);
                    state = WAITING;
                    break;
                case WAITING:
                    printf("WAITING -> UNLOCKING\n");
                    BSP_LCD_Clear(LCD_COLOR_WHITE);
                    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);  
                    BSP_LCD_DisplayStringAt(0, LINE(5), (uint8_t *)"Unlock?", CENTER_MODE);
                    state = UNLOCKING;
                    data_index = 0;
                    break;
                case UNLOCKING: 
                    printf("UNLOCKING -> CHECKING\n");
                    state = CHECKING;
                    break;         
                case CHECKING:
                    printf("Checking sequences\n");
                    correct = compareSequences(record_data, attempt_data, 100);
                    if (!correct) {
                        BSP_LCD_Clear(LCD_COLOR_WHITE);
                        BSP_LCD_SetTextColor(LCD_COLOR_RED);  
                        BSP_LCD_DisplayStringAt(0, LINE(5), (uint8_t *)"Incorrect!", CENTER_MODE);                        
                        if (remaining_attempts == 0) {
                            state = LOCKED;
                            break;
                        } else {
                            state = UNLOCKING;
                            remaining_attempts--;
                            printf("Incorrect. %d attempts remaining...\n", remaining_attempts);
                            break;
                        }
                    } else {
                        printf("Correct! Checking -> IDLE\n");
                        BSP_LCD_Clear(LCD_COLOR_WHITE);
                        BSP_LCD_SetTextColor(LCD_COLOR_GREEN);  
                        BSP_LCD_DisplayStringAt(0, LINE(5), (uint8_t *)"Unlocked :)", CENTER_MODE);   
                        state = IDLE;     
                        break;  
                    }
                case LOCKED: 
                    BSP_LCD_Clear(LCD_COLOR_WHITE);
                    BSP_LCD_SetTextColor(LCD_COLOR_RED);  
                    BSP_LCD_DisplayStringAt(0, LINE(5), (uint8_t *)"LOCKED.", CENTER_MODE);   
                    state = LOCKED;
                    break;
                default:
                    state = IDLE;
                    break;
            }
            flag = false;
        } else if (state == RECORDING) {
            if (data_index < max_sequence_length) {
                record_data[data_index] = readGyro();
                //printf("record_data[data_index]\n");
                data_index++;
            }
        } else if (state == UNLOCKING) {
            if (data_index < max_sequence_length) {
                attempt_data[data_index] = readGyro();
                //printf("Unlocking\n");
                data_index++;
            }
        }
    }
}

void setMode() {          
  cs=0;
  spi.write(0x20);
  spi.write(0xCF);
  cs=1;
}

GyroData readGyro()
{
  cs = 0;
  spi.write(0xE8);
  int OUT_X_L = spi.write(0x00);
  int OUT_X_H = spi.write(0x00);
  int OUT_Y_L = spi.write(0x00);
  int OUT_Y_H = spi.write(0x00);
  int OUT_Z_L = spi.write(0x00);
  int OUT_Z_H = spi.write(0x00);
  cs = 1;

  GyroData raw_data;
  raw_data.x = (OUT_X_H << 8) | (OUT_X_L);
  raw_data.y = (OUT_Y_H << 8) | (OUT_Y_L);
  raw_data.z = (OUT_Z_H << 8) | (OUT_Z_L);

  return raw_data;
}

void setupGyro() {
    cs = 1;
    setMode();
    spi.format(8, 3);
    spi.frequency(100000);
}

bool compareSequences(const std::vector<GyroData>& seq1, const std::vector<GyroData>& seq2, double threshold) {
    int n1 = seq1.size();
    int n2 = seq2.size();
    int n = std::max(n1, n2);

    std::vector<double> x1(n), y1(n), z1(n);
    std::vector<double> x2(n), y2(n), z2(n);

    for (int i = 0; i < n; i++) {
        if (i < n1) {
            x1[i] = seq1[i].x;
            y1[i] = seq1[i].y;
            z1[i] = seq1[i].z;
        } else {
            x1[i] = 0;
            y1[i] = 0;
            z1[i] = 0;
        }
        if (i < n2) {
            x2[i] = seq2[i].x;
            y2[i] = seq2[i].y;
            z2[i] = seq2[i].z;
        } else {
            x2[i] = 0;
            y2[i] = 0;
            z2[i] = 0;
        }
    }

    double corrX = computeCrossCorrelation(x1, x2);
    double corrY = computeCrossCorrelation(y1, y2);
    double corrZ = computeCrossCorrelation(z1, z2);
    printf("%d, %d, %d\n", (int)corrX, (int)corrY, (int)corrZ);

    return corrX > threshold && corrY > threshold && corrZ > threshold;
}

double computeCrossCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
    int n = x.size();
    double max_corr = 0;
    for (int shift = -n + 1; shift < n; shift++) {
        double corr = 0;
        for (int i = 0; i < n; i++) {
            int j = i + shift;
            if (j >= 0 && j < n) {
                corr += x[i] * y[j];
            }
        }
        corr /= n;
        if (corr > max_corr) {
            max_corr = corr;
        }
    }
    return max_corr;
}

void initDisplay() {
    HAL_Init();
    BSP_LCD_Init();

    BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_DisplayOn();
    BSP_LCD_Clear(LCD_COLOR_WHITE);
}