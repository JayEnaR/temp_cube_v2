#pragma once

/**
 * Initialize the device button.
 *
 * A short press while the device is awake enters button-only deep sleep.
 * A long press erases NVS and restarts the device so provisioning can run.
 */
void button_init(void);

/**
 * Enable the button GPIO as a deep-sleep wake source.
 */
void button_enable_wakeup_source(void);
