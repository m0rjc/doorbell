#pragma once
#define NODE_NAME_LEN 25
#define NODE_ID_LEN 8

// Notify index where uing task notifications to wake up known tasks.
#define TASK_NOTIFY_INDEX 0

#define GPIO_SEL_N(n) ((uint64_t)(((uint64_t)1)<<n))
