#include "unity.h"
#include "fff.h"
#include "servo.h"
#include "mock_avr_io.h"

#define FFF_GLOBALS
#include "../fff.h"

// Fake delay function (for _delay_ms and _delay_us in AVR code)
FAKE_VOID_FUNC(_delay_ms, unsigned int);
FAKE_VOID_FUNC(_delay_us, unsigned int);

// Setup and teardown for Unity
void setUp(void) {
    // Reset all fakes before each test
    RESET_FAKE(_delay_ms);
    RESET_FAKE(_delay_us);

    // Reset the mocked registers if needed
    DDRE = 0;
    PORTE = 0;
}

void tearDown(void) {
    // Nothing to clean up
}

void test_servo_angle_min(void) {
    // Test minimum angle (0 degrees)
    servo(0);

    // The DDRE register should be set to output
    TEST_ASSERT_NOT_EQUAL(0, DDRE & (1 << PE3));

    // You can add more assertions based on how you want to test the behavior
    // For example: verify delay was called
    TEST_ASSERT_GREATER_THAN(0, _delay_ms_fake.call_count);
}

void test_servo_angle_mid(void) {
    // Test mid-range angle (90 degrees)
    servo(90);

    // Check register is set for output
    TEST_ASSERT_NOT_EQUAL(0, DDRE & (1 << PE3));
    TEST_ASSERT_GREATER_THAN(0, _delay_ms_fake.call_count);
}

void test_servo_angle_max(void) {
    // Test maximum angle (180 degrees)
    servo(180);

    // Check register is set for output
    TEST_ASSERT_NOT_EQUAL(0, DDRE & (1 << PE3));
    TEST_ASSERT_GREATER_THAN(0, _delay_ms_fake.call_count);
}

void test_servo_angle_out_of_bounds(void) {
    // Test angle above the valid range (e.g. 255)
    servo(255);

    // Should clamp to 180 internally
    TEST_ASSERT_NOT_EQUAL(0, DDRE & (1 << PE3));
    TEST_ASSERT_GREATER_THAN(0, _delay_ms_fake.call_count);
}

// Main test runner
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_servo_angle_min);
    RUN_TEST(test_servo_angle_mid);
    RUN_TEST(test_servo_angle_max);
    RUN_TEST(test_servo_angle_out_of_bounds);
    return UNITY_END();
}
