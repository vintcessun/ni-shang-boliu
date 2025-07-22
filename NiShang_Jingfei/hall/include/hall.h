#ifndef __HALL_H_
#define __HALL_H_

#define get_gpio_pos(x) (x - 25)
#define get_gpio(x) gpio_data[get_gpio_pos(x)]

#define INIT_STATE_MACHINE(x)                  \
	bool x##0_state = true, x##3_state = true; \
	int middle_state_##x = 0;                  \
	int current_nums_##x = 0, last_nums_##x = 0;

#define CLICK_LOG_I(x) #x " is clicked\n"

#define STATE_MACHINE(x)                                                 \
	if (get_gpio(x##0) == 1) {                                           \
		if (x##0_state) {                                                \
			LOG_I(CLICK_LOG_I(x##0));                                    \
			x##0_state = false;                                          \
			middle_state_##x = 1;                                        \
		}                                                                \
	}                                                                    \
	if (get_gpio(x##3) == 1) {                                           \
		if (x##3_state) {                                                \
			LOG_I(CLICK_LOG_I(x##3));                                    \
			x##3_state = false;                                          \
			middle_state_##x = 2;                                        \
		}                                                                \
	}                                                                    \
	current_nums_##x = get_gpio(x##1) + get_gpio(x##2);                  \
	LOG_I("current_nums = %d , last_nums = %d , %s\n", current_nums_##x, \
		  last_nums_##x, #x);                                            \
	if (current_nums_##x > last_nums_##x) {                              \
		if (middle_state_##x != 0) {                                     \
			LOG_I("bigger middle_state=%d\n", middle_state_##x);         \
		}                                                                \
		if (middle_state_##x == 1) {                                     \
			(x)[0]--;                                                    \
			(x)[1]++;                                                    \
			middle_state_##x = 0;                                        \
			x##0_state = true;                                           \
			last_nums_##x = current_nums_##x;                            \
		} else if (middle_state_##x == 2) {                              \
			(x)[1]++;                                                    \
			(x)[2]--;                                                    \
			middle_state_##x = 0;                                        \
			x##3_state = true;                                           \
			last_nums_##x = current_nums_##x;                            \
		}                                                                \
	} else if (current_nums_##x < last_nums_##x) {                       \
		if (middle_state_##x != 0) {                                     \
			LOG_I("smaller middle_state=%d\n", middle_state_##x);        \
		}                                                                \
		if (middle_state_##x == 1) {                                     \
			(x)[0]++;                                                    \
			(x)[1]--;                                                    \
			middle_state_##x = 0;                                        \
			x##0_state = true;                                           \
			last_nums_##x = current_nums_##x;                            \
		} else if (middle_state_##x == 2) {                              \
			(x)[1]--;                                                    \
			(x)[2]++;                                                    \
			middle_state_##x = 0;                                        \
			x##3_state = true;                                           \
			last_nums_##x = current_nums_##x;                            \
		}                                                                \
	}

int hall_main(void);

#endif	//__HALL_H_