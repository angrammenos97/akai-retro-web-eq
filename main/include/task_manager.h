#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

/*
 * @brief Initialize the task manager
 */
void task_manager_init(void);

/*
 * @brief Deinitialize the task manager
 */
void task_manager_deinit(void);

/**
 * @brief Start the task manager
 */
void task_manager_start(void);

/**
 * @brief Stop the task manager
 */
void task_manager_stop(void);

#endif // TASK_MANAGER_H