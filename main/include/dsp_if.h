#ifndef DSP_IF_H
#define DSP_IF_H

/*
 * @brief Initialize the audio DSP processing
 */
void dsp_init(void);

/*
 * @brief Deinitialize the audio DSP processing
 */
void dsp_deinit(void);

/*
 * @brief Process the audio data through the DSP pipeline
 */
void dsp_process(void);

#endif // DSP_IF_H