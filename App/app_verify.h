#ifndef APP_VERIFY_H_
#define APP_VERIFY_H_
void cpature_check();
void CopyFlash(WADLIST *list, uint32_t foffset);
int VerifySDCard(void **errString1, void **errString2);
int VerifyFlash(void **p1, void **p2, int flash_size);
int VerifyFont(void **p1, void **p2, int flash_size);
#endif
