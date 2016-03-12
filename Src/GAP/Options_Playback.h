#ifndef _GAP_OPTIONS_PLAYBACK_H
#define _GAP_OPTIONS_PLAYBACK_H

#include <windows.h>

#define BUFFERSIZE_MIN (20)
#define BUFFERSIZE_MAX (2048)

#define DEF_WAVEDEV (WAVE_MAPPER)
#define DEF_BUFFERSIZE (256)
#define DEF_PRIORITYCLASS (NORMAL_PRIORITY_CLASS)
#define DEF_MAINPRIORITY (THREAD_PRIORITY_NORMAL)
#define DEF_PLAYBACKPRIORITY (THREAD_PRIORITY_NORMAL)

LRESULT CALLBACK PlaybackOptionsProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm);

#endif
