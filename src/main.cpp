#include "mbed.h"

// Define LED output pin
DigitalOut led(LED3);

// Define button input pins
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


// Function to read accelerometer data and update LED status
void updateAccelerometer()
{
    int16_t x = x_axis.read()*100;
    int16_t y = y_axis.read()*100;
    int16_t z = z_axis.read()*100;

    // Detect movement for recording or replaying
    if (recordButton.read() == 1 && movementSize < MAX_MOVEMENT_SIZE)
    {
        movementBuffer[movementSize][0] = x;
        movementBuffer[movementSize][1] = y;
        movementBuffer[movementSize][2] = z;
        movementSize++;
    }
    else if (enterKeyButton.read() == 1 && movementSize > 0)
    {
        bool success = true;
        for (int i = 0; i < movementSize; i++)
        {
            int16_t xDiff = abs(x - movementBuffer[i][0]);
            int16_t yDiff = abs(y - movementBuffer[i][1]);
            int16_t zDiff = abs(z - movementBuffer[i][2]);
            if (xDiff > TOLERANCE_RECORD || yDiff > TOLERANCE_RECORD || zDiff > TOLERANCE_RECORD)
            {
                success = false;
                break;
            }
        }
        if (success)
        {
            led = 1;
            wait_us(0.5);
            led = 0;
        }
    }
    else if (movementSize > 0)
    {
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
            led = 1;
            wait_us(0.5);
            led = 0;
        }
    }
}

int main()
{
    // Loop forever
    while (true)
    {
        updateAccelerometer();
        // Print out movements in buffer
        printf("Recorded Movements:\n");
        for (int i = 0; i < movementSize; i++)
        {
            printf("X: %d, Y: %d, Z: %d\n", movementBuffer[i][0], movementBuffer[i][1], movementBuffer[i][2]);
        }
    }
}
