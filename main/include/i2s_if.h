#ifndef I2S_IF_H
#define I2S_IF_H

/*
 * @brief Initialize Rx and Tx I2S interfaces
 *
 * @return Pointer to the received audio data buffer
 */
void i2s_init(void);

/*
 * @brief Deinitialize Rx and Tx I2S interfaces
 */
void i2s_deinit(void);

/*
 * @brief Receive audio data from the I2S Rx channel
 */
void i2s_receive(void);

/*
 * @brief Transmit audio data to the I2S Tx channel
 */
void i2s_transmit(void);

#endif // I2S_IF_H