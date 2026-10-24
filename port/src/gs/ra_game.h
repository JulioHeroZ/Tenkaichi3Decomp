#ifndef BT3_RA_GAME_H
#define BT3_RA_GAME_H
#ifdef __cplusplus
extern "C" {
#endif
void PortRa_Init(void); /* before the render thread starts */
void PortRa_Tick(void); /* game thread only, once per vertical blank */
#ifdef __cplusplus
}
#endif
#endif
