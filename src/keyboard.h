void handle_keyboard(bool down, SDL_Keycode sym, SDL_Scancode scancode);

/* -autokeys: type a string into the SMC key FIFO with no window and no user.
 *
 * The keyboard was the one path no automated test could reach: everything else
 * has a conformance test, so every other fault was caught before hardware,
 * while a dead keyboard could only be found by a person sitting at a DE10-Nano.
 * This closes that hole -- it feeds the SAME FIFO that handle_keyboard() feeds,
 * so an autokeys run exercises the real I2C GETKEY path end to end.
 *
 * autokeys_set() takes ASCII; "
" is accepted as Enter. */
void autokeys_set(const char *text);
void autokeys_step(uint32_t clocks);

