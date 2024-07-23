/**********************************************************************/
//** ENGR-2350 Template Project 
//** NAME: Nicholas Danas and Curran Flanders
//** RIN: 662055547 and 662017081
//** This is the base project for several activities and labs throughout
//** the course.  The outline provided below isn't necessarily *required*
//** by a C program; however, this format is required within ENGR-2350
//** to ease debugging/grading by the staff.
/**********************************************************************/

// We'll always add this include statement. This basically takes the
// code contained within the "engr_2350_msp432.h" file and adds it here.
#include "engr2350_msp432.h"

// Add function prototypes here as needed.
void GPIO_Init(void);
void Timer_Init(void);
void CCR_ISR(void);
void PWM_Init(void);
void Go_Straight(uint16_t distance);
void Turn(uint16_t angle);
void Drive(uint16_t * distances, uint16_t * angles);

// Add global variables here as needed.
//encoder counting variables
Timer_A_ContinuousModeConfig continuousConfig;
Timer_A_CaptureModeConfig captureModeConfigRight;
Timer_A_CaptureModeConfig captureModeConfigLeft;
Timer_A_UpModeConfig pwmConfig;
Timer_A_CompareModeConfig pwmCompareConfigLeft; // CCR3 for left motor (2.6)
Timer_A_CompareModeConfig pwmCompareConfigRight; // CCR4 for right motor (2.7)
uint32_t enc_total_left = 0;
uint32_t enc_total_right = 0;
int32_t enc_counts_track_left = 0;
int32_t enc_counts_track_right = 0;
int32_t enc_counts_left = 0;
int32_t enc_counts_right = 0;
uint8_t path_running = 0;//1 if r path 2 if i path
uint8_t r_btn_pressed = 0;
uint8_t i_btn_pressed = 0;
uint16_t pathR1[] = {1830,860,860,860,1300};
uint16_t pathR2[] = {85,85,85,228};
uint16_t pathI1[] = {860,430,1830,430,860};
uint16_t pathI2[] = {180,90,285,180};
uint16_t capture_value_right = 0; // storing capture value from ccr
uint16_t capture_value_left = 0;
uint16_t last_capture_value_right = 0;
uint16_t last_capture_value_left = 0;
//PWM control variables
uint32_t wheel_speed_sum_left = 0;
uint32_t wheel_speed_sum_right = 0;
uint16_t current_pwm_value_left = 250;
uint16_t current_pwm_value_right = 250;
uint8_t measurement_count_left = 0;
uint8_t measurement_count_right = 0;
uint16_t stop_counts = 0;
uint8_t wheel_radius = 35;
uint16_t wheel_rotations = 0; //in 1/360 increments
uint8_t i = 0;
int main(void)    /*** Main Function ***/
{  
    // Add local variables here, as needed.

    // We always call the "SysInit()" first to set up the microcontroller
    // for how we are going to use it.
    SysInit();
    GPIO_Init();
    PWM_Init();
    Timer_Init();
    // Place initialization code (or run-once) code here

    while(1)
    {  
        while (!path_running){
            if (!GPIO_getInputPinValue(GPIO_PORT_P4,GPIO_PIN0)){
                __delay_cycles(240e3);
                r_btn_pressed = 1;
            }
            else{
                if (r_btn_pressed){
                    path_running = 1;
                    r_btn_pressed = 0;
                    i_btn_pressed = 0;
                    break;
                }

            }
            if (!GPIO_getInputPinValue(GPIO_PORT_P4,GPIO_PIN2)){
                __delay_cycles(240e3);
                i_btn_pressed = 1;
            }
            else{
                if (i_btn_pressed){
                    path_running = 2;
                    r_btn_pressed = 0;
                    i_btn_pressed = 0;
                    break;
                }

            }

        }

        if (path_running == 1){
            Drive(&pathR1,&pathR2);
        }
        else if (path_running == 2){
            Drive(&pathI1, &pathI2);
        }
        path_running = 0;

        // Place code that runs continuously in here


    }   
}   

// Add function declarations here as needed
void GPIO_Init(void){
    // Set motor enable and direction pins as output
    GPIO_setAsOutputPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);
    GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7); // Enable motors

    // Set motor direction pins (assuming high for forward)
    GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN5 | GPIO_PIN4);


    //PWM output
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2, GPIO_PIN6, GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2, GPIO_PIN7, GPIO_PRIMARY_MODULE_FUNCTION);
    // Set encoder pins as inputs
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P10, GPIO_PIN4, GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P10, GPIO_PIN5, GPIO_PRIMARY_MODULE_FUNCTION);

    //bumper button inputs
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P4, GPIO_PIN0 | GPIO_PIN2);

}



void PWM_Init(void){
    // Initialize the Timer in Up mode
    pwmConfig.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    pwmConfig.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    pwmConfig.timerPeriod = 1000; // This sets the PWM period
    Timer_A_configureUpMode(TIMER_A0_BASE, &pwmConfig);

    // Initialize Compare registers for PWM on the motors
    pwmCompareConfigLeft.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_4; // CCR4 for left motor
    pwmCompareConfigLeft.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    pwmCompareConfigLeft.compareValue = 250;

    pwmCompareConfigRight.compareRegister = TIMER_A_CAPTURECOMPARE_REGISTER_3; // CCR3 for right motor
    pwmCompareConfigRight.compareOutputMode = TIMER_A_OUTPUTMODE_RESET_SET;
    pwmCompareConfigRight.compareValue = 250;

    Timer_A_initCompare(TIMER_A0_BASE, &pwmCompareConfigLeft);
    Timer_A_initCompare(TIMER_A0_BASE, &pwmCompareConfigRight);

    // Start the timer
    Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
}



void Timer_Init(void){
    // Configure Timer_A for continuous mode for encoder reading
    continuousConfig.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    continuousConfig.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    continuousConfig.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_ENABLE;
    Timer_A_configureContinuousMode(TIMER_A3_BASE, &continuousConfig);

    // Configure CCR0 for Capture Mode for the right encoder
    captureModeConfigRight.captureRegister = TIMER_A_CAPTURECOMPARE_REGISTER_0; // CCR0 for the right wheel encoder
    captureModeConfigRight.captureMode = TIMER_A_CAPTUREMODE_RISING_EDGE; // Capture on both edges   Question tomorrow
    captureModeConfigRight.captureInputSelect = TIMER_A_CAPTURE_INPUTSELECT_CCIxA; // Use CCIxA as the input
    captureModeConfigRight.synchronizeCaptureSource = TIMER_A_CAPTURE_SYNCHRONOUS; // Synchronize with the timer clock
    captureModeConfigRight.captureInterruptEnable = TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE; // Enable capture interrupt


    // Configure CCR0 for Capture Mode for the left encoder
    captureModeConfigLeft.captureRegister = TIMER_A_CAPTURECOMPARE_REGISTER_1; // CCR1 for the right wheel encoder
    captureModeConfigLeft.captureMode = TIMER_A_CAPTUREMODE_RISING_EDGE; // Capture on both edges   Question tomorrow
    captureModeConfigLeft.captureInputSelect = TIMER_A_CAPTURE_INPUTSELECT_CCIxA; // Use CCIxA as the input
    captureModeConfigLeft.synchronizeCaptureSource = TIMER_A_CAPTURE_SYNCHRONOUS; // Synchronize with the timer clock
    captureModeConfigLeft.captureInterruptEnable = TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE; // Enable capture interrupt

    //Right
    Timer_A_initCapture(TIMER_A3_BASE, &captureModeConfigRight);
    Timer_A_registerInterrupt(TIMER_A3_BASE, TIMER_A_CCRX_AND_OVERFLOW_INTERRUPT, CCR_ISR);
    Timer_A_registerInterrupt(TIMER_A3_BASE, TIMER_A_CCR0_INTERRUPT, CCR_ISR);


    //Left
    Timer_A_initCapture(TIMER_A3_BASE, &captureModeConfigLeft);


    Timer_A_startCounter(TIMER_A3_BASE, TIMER_A_CONTINUOUS_MODE);
}




void Drive(uint16_t * distances, uint16_t * angles){


    GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);
    //remember to reset wheel rotations to 0
    for (i=0;i<4;i++){
        Go_Straight(distances[i]);
        Turn(angles[i]);
    }
    Go_Straight(distances[4]);
    GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN6 | GPIO_PIN7);
}

void Go_Straight(uint16_t distance){
    wheel_rotations = 0;
    stop_counts = distance*360/(wheel_radius*2*3.14159);
    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN5 | GPIO_PIN4);
    while (wheel_rotations < stop_counts){//note: check to see if we are using the correct variable

    }
}

void Turn(uint16_t angle){

    wheel_rotations = 0;
    stop_counts = 0.85*(((angle*74.5*3.14159)/180)/(2*3.14159*wheel_radius))*360;
    GPIO_setOutputLowOnPin(GPIO_PORT_P5, GPIO_PIN4);
    GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN5);
    while (wheel_rotations < stop_counts){

    }

}




// Add interrupt functions last so they are easy to find
void CCR_ISR(void){

    if (Timer_A_getEnabledInterruptStatus(TIMER_A3_BASE) == TIMER_A_INTERRUPT_PENDING){

        Timer_A_clearInterruptFlag(TIMER_A3_BASE);
        enc_counts_track_right += 65536;
        enc_counts_track_left += 65536;



    }

    if(Timer_A_getCaptureCompareEnabledInterruptStatus(TIMER_A3_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0) ){

        Timer_A_clearCaptureCompareInterrupt(TIMER_A3_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
        enc_total_right++;

    // Calculate enc_counts as enc_counts_track + capture value
        capture_value_right = Timer_A_getCaptureCompareCount(TIMER_A3_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_0);
        enc_counts_right = enc_counts_track_right + capture_value_right;
        enc_counts_track_right = -capture_value_right;

        wheel_speed_sum_right += enc_counts_right;
        wheel_rotations++;

        measurement_count_right++;

        if (measurement_count_right == 12){

            if((wheel_speed_sum_right / 12) > 65000){
                current_pwm_value_right++;

           }else{
               current_pwm_value_right--;
           }

            Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_3, current_pwm_value_right);
            measurement_count_right = 0;
            wheel_speed_sum_right = 0;




        }

    }
    if(Timer_A_getCaptureCompareEnabledInterruptStatus(TIMER_A3_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1) ){

        Timer_A_clearCaptureCompareInterrupt(TIMER_A3_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1);
        enc_total_left++;


        // Calculate enc_counts as enc_counts_track + capture value
        capture_value_left = Timer_A_getCaptureCompareCount(TIMER_A3_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_1);
        enc_counts_left = enc_counts_track_left + capture_value_left;
        enc_counts_track_left = -capture_value_left;


        wheel_speed_sum_left += enc_counts_left;


        measurement_count_left++;


        if (measurement_count_left == 12){

            if((wheel_speed_sum_left / 12) > 65000){
                current_pwm_value_left++;

           }else{
               current_pwm_value_left--;
           }

           Timer_A_setCompareValue(TIMER_A0_BASE, TIMER_A_CAPTURECOMPARE_REGISTER_4, current_pwm_value_left);
           measurement_count_left = 0;
           wheel_speed_sum_left = 0;




        }
    }




}
