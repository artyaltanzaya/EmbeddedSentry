#include <mbed.h>
#include <math.h>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include "stm32f4xx_hal.h"
#include "stm32f429i_discovery_lcd.h"

// 20% creativity: We 


// For some reason, the LCD screen was throwing an error about wait_ms not being
// implemented, so we've included our own implementation here.
extern "C" void wait_ms(int ms) {
    for (int i = 0; i < ms; i++) {
        for (int j = 0; j < 1000; j++) {
            __asm("nop");
        }
    }
}

// First we'll define a struct that'll hold gyro data. The gyro gives us x,y,z values.
struct GyroData
{
  int16_t x;
  int16_t y;
  int16_t z;
};

// Next we'll define a couple constant values used throughout the program. This starts
// with the maximum the maximum number of data points collected in our gesture, as well
// as a carefully tuned "distance threshold" value used to compare whether two gestures
// are the same or not. To make the unlocks more forgiving, just increase the dist threshold.
const int MAX_ARR_LENGTH = 100;
const double DIST_THRESHOLD = 15000;


// Set up the serial interface
SPI spi(PF_9, PF_8, PF_7);
DigitalOut cs(PC_1);

// Set up the button used to start/stop recording
InterruptIn enter_key(USER_BUTTON);

void setFlag();
void setMode();
void setupGyro();
void initDisplay();
bool compareSequences(const std::vector<GyroData>& seq1, const std::vector<GyroData>& seq2, double threshold);
double computeDistance(const std::vector<int16_t>& x, const std::vector<int16_t>& y);
GyroData readGyro();

volatile bool flag = false;

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
    std::vector<GyroData> record_data(MAX_ARR_LENGTH);
    std::vector<GyroData> attempt_data(MAX_ARR_LENGTH);
    int data_index = 0;
  
    // For creativity points, we've also included a "locked out" feature. This means that if 
    // you can't unlock the device in the number of tries seen below, you'll be locked out
    // for good >:D
    int remaining_attempts = 5;
    bool correct = false;

    // We keep track of states and jump between them on button presses. For example, we start out
    // in the IDLE state, and once the USER button on the board is pressed, this indicates that
    // we want to start to record the gesture. Then, you'll press the button again to stop recording
    // and once again to try to unlock, etc.
    enter_key.fall(&updateState);
    while (1) {
        if (flag) {
            switch(state) 
            {
                case IDLE:
                    // We'll start by clearing the existing data in the vectors if we end up here.
                    std::fill(record_data.begin(), record_data.end(), GyroData());
                    std::fill(attempt_data.begin(), attempt_data.end(), GyroData());
                    BSP_LCD_Clear(LCD_COLOR_WHITE);
                    printf("IDLE -> RECORDING\n");
                    state = RECORDING;
                    break;
                case RECORDING: 
                    // Now we'll being recording, and data will be collected continuously (waiting 50ms)
                    // between calls, until the button is pressed again or until you run out of
                    // room in the vector.
                    printf("RECORDING -> WAITING\n");
                    BSP_LCD_Clear(LCD_COLOR_WHITE);
                    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);  
                    BSP_LCD_DisplayStringAt(0, LINE(5), (uint8_t *)"Recording...", CENTER_MODE);
                    state = WAITING;
                    break;
                case WAITING:
                    // Intermediate state between recording and unlocking
                    printf("WAITING -> UNLOCKING\n");
                    BSP_LCD_Clear(LCD_COLOR_WHITE);
                    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);  
                    BSP_LCD_DisplayStringAt(0, LINE(5), (uint8_t *)"Unlock?", CENTER_MODE);
                    state = UNLOCKING;
                    data_index = 0;
                    break;
                case UNLOCKING: 
                    // See else-if statements below for more, but now we'll record the unlock gesture
                    // into a separate vector to compare w/ later.
                    printf("UNLOCKING -> CHECKING\n");
                    state = CHECKING;
                    break;         
                case CHECKING:
                    // This is the core functionality of the program. We'll just compare the two sequences
                    // by looking at the average distance between the points in the vectors. So nothing 
                    // super complicated, and this could definitely be improved upon.
                    printf("Checking sequences\n");
                    correct = compareSequences(record_data, attempt_data, DIST_THRESHOLD);
                    if (!correct) {
                        BSP_LCD_Clear(LCD_COLOR_WHITE);
                        BSP_LCD_SetTextColor(LCD_COLOR_RED);  
                        BSP_LCD_DisplayStringAt(0, LINE(5), (uint8_t *)"Incorrect!", CENTER_MODE); 
                        char buffer[32];
                        sprintf(buffer, "Remaining: %d", remaining_attempts);
                        BSP_LCD_DisplayStringAt(0, LINE(6), (uint8_t *)buffer, CENTER_MODE);                       
                        if (remaining_attempts == 0) {
                            state = LOCKED;
                            break;
                        } else {
                            state = WAITING;
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
                    // If locked, you'll always stay locked.
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
            // The following two code blocks store the gyro data into our vector of gyro objects.
            if (data_index < MAX_ARR_LENGTH) {
                record_data[data_index] = readGyro();
                printf("%d, %d, %d\n", record_data[data_index].x, record_data[data_index].y, record_data[data_index].z);
                data_index++;
            }
            else {
                printf("Max length reached. Done recording.\n");
            }
        } else if (state == UNLOCKING) {
            if (data_index < MAX_ARR_LENGTH) {
                attempt_data[data_index] = readGyro();
                printf("%d, %d, %d\n", attempt_data[data_index].x, attempt_data[data_index].y, attempt_data[data_index].z);
                data_index++;
            }
            else {
                printf("Max length reached. Done trying to unlock.\n");
            }
        }
    }
}

// Set mode for SPI
void setMode() {          
  cs=0;
  spi.write(0x20);
  spi.write(0xCF);
  cs=1;
}

// Now we'll read the gyro data, which consists of two 8-bit values that we'll stitch
// together to form the 16-bit value. We first have to send dummy data to get data back.
GyroData readGyro()
{
    cs = 0;
    spi.write(0xE8);
    int xl = spi.write(0x00);
    int xh = spi.write(0x00);
    int yl = spi.write(0x00);
    int yh = spi.write(0x00);
    int zl = spi.write(0x00);
    int zh = spi.write(0x00);
    cs = 1;

    GyroData data;
    data.x = (xh << 8) | (xl);
    data.y = (yh << 8) | (yl);
    data.z = (zh << 8) | (zl);
    wait_us(50000); 

    return data;
}

// Set up gyro
void setupGyro() {
    cs = 1;
    setMode();
    spi.format(8, 3);
    spi.frequency(100000);
}

// This is the core comparison code. Nothing too special, but we're first just converting the vector into
// 3 different arrays of integers to pass to the "computeDistance" function separately. This just made 
// things a little bit more easy to debug.
bool compareSequences(const std::vector<GyroData>& seq1, const std::vector<GyroData>& seq2, double threshold) {
    int n1 = seq1.size();
    int n2 = seq2.size();
    int n = std::max(n1, n2);

    std::vector<int16_t> x1(n), y1(n), z1(n);
    std::vector<int16_t> x2(n), y2(n), z2(n);

    // Pad up to larger vector size.
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

    // Compute average distances
    double distX = computeDistance(x1, x2);
    double distY = computeDistance(y1, y2);
    double distZ = computeDistance(z1, z2);
    printf("Dist: %d, %d, %d\n", (int)distX, (int)distY, (int)distZ);
    
    // If all x,y,z distances are less than the threshold we set initially, return "true" 
    // and unlock the device.
    bool correct = distX < DIST_THRESHOLD && distY < DIST_THRESHOLD && distZ < DIST_THRESHOLD;
    printf("Correct? %d\n", correct);

    return correct;
}

double computeDistance(const std::vector<int16_t>& x, const std::vector<int16_t>& y) {
    int n = std::min(x.size(), y.size());
    double distance = 0;
    int N = 0;
    for (int i = 0; i < n; i++) {
        //printf("%d, %d\n", x.at(i), y.at(i));
        if (x[i] != 0 && y[i] != 0) {
            distance += std::abs((double)x[i] - (double)y[i]);
            N++;
        }
    }
    return N == 0 ? 0 : distance / N;
}

// Initialize the HAL display.
void initDisplay() {
    HAL_Init();
    BSP_LCD_Init();

    BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_DisplayOn();
    BSP_LCD_Clear(LCD_COLOR_WHITE);
}