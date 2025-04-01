#ifndef MI_IO_CRYPTO_H
#define MI_IO_CRYPTO_H

#include <Arduino.h>

// Функции шифрования/дешифрования для протокола miIO
void miioEncrypt(const uint8_t* key, const uint8_t* input, size_t input_len, uint8_t* output);
void miioDecrypt(const uint8_t* key, const uint8_t* input, size_t input_len, uint8_t* output);

#endif // MI_IO_CRYPTO_H
