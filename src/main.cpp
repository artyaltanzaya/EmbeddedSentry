#include "mbed.h"

// Define LED output pin
DigitalOut led(LED3);

// Define button input pins
DigitalIn enterKeyButton(PB_14);
DigitalIn recordButton(PA_0);

// Define accelerometer object
AnalogIn x_axis(PF_10);
AnalogIn y_axis(PF_9);
AnalogIn z_axis(PF_8);

// Define data buffer for recording and replaying movements
#define MAX_MOVEMENT_SIZE 100
int16_t movementBuffer[MAX_MOVEMENT_SIZE][3];
int movementSize = 0;

// Define tolerances for movement detection and replaying
#define TOLERANCE 0.1
#define TOLERANCE_RECORD 0.2

// Define state variables
enum State {IDLE, RECORDING, REPLAYING};
State state = IDLE;

// Function to read accelerometer data and update LED status
void updateAccelerometer()
{
    int16_t x = x_axis.read()*100;
    int16_t y = y_axis.read()*100;
    int16_t z = z_axis.read()*100;

    switch(state)
    {
        case IDLE:
            if (recordButton.read() == 1)
            {
                // Start recording
                state = RECORDING;
                movementSize = 0;
            }
            break;

        case RECORDING:
            if (recordButton.read() == 0)
            {
                // Stop recording
                state = IDLE;
            }
            else if (movementSize < MAX_MOVEMENT_SIZE)
            {
                // Record movement
                movementBuffer[movementSize][0] = x;
                movementBuffer[movementSize][1] = y;
                movementBuffer[movementSize][2] = z;
                movementSize++;
            }
            break;

        case REPLAYING:
            if (enterKeyButton.read() == 0)
            {
                // Stop replaying
                state = IDLE;
            }
            else if (movementSize > 0)
            {
                // Check if current movement matches recorded movement
                bool success = true;
                for (int i = 0; i < movementSize; i++)
                {
                    int16_t xDiff = abs(x - movementBuffer[i][0]);
                    int16_t yDiff = abs(y - movementBuffer[i][1]);
                    int16_t zDiff = abs(z - movementBuffer[i][2]);
                    if (xDiff > TOLERANCE || yDiff > TOLERANCE || zDiff > TOLERANCE)
                    {
                        success = false;
                        break;
                    }
                }

                if (success)
                {
                    // Indicate successful unlock
                    led = 1;
                    wait_us(500000);
                    led = 0;
                }
            }
            break;

        default:
            state = IDLE;
            break;
    }
}

int main()
{
    // Loop forever
    while (true)
    {
        updateAccelerometer();

        // Check if enter key is pressed to start replaying movement
        if (enterKeyButton.read() == 1 && state == IDLE && movementSize > 0)
        {
            state = REPLAYING;
            printf("Replaying Movement %d:\n", movementSize);
            printf("State %d:\n", state);
        }

        // Print out movements in buffer during recording
        if (state == RECORDING)
        {
            printf("Recording Movement %d:\n", movementSize);
            printf("X: %d, Y: %d, Z: %d\n", movementBuffer[movementSize-1][0], movementBuffer[movementSize-1][1], movementBuffer[movementSize-1][2]);
        }
    }
}
