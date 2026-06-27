#pragma once
#include <pebble.h>

typedef void (*SplashDoneHandler)(void);

// Show the intro splash (a train running across the screen), then call `done`.
void splash_push(SplashDoneHandler done);
