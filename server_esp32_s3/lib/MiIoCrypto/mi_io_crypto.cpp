#include "mi_io_crypto.h"
#include <mbedtls/aes.h>

// Функция шифрования данных для протокола miIO
void miioEncrypt(const uint8_t* key, const uint8_t* input, size_t input_len, uint8_t* output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    
    // Копируем входные данные в выходной буфер
    memcpy(output, input, input_len);
    
    // Шифруем блоки по 16 байт (AES-128)
    for (size_t i = 0; i < input_len; i += 16) {
        // Проверяем, что у нас есть полный блок
        if (i + 16 <= input_len) {
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, &output[i], &output[i]);
        } else {
            // Последний неполный блок - дополняем нулями
            uint8_t block[16] = {0};
            memcpy(block, &output[i], input_len - i);
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, block, block);
            memcpy(&output[i], block, input_len - i);
        }
    }
    
    mbedtls_aes_free(&aes);
}

// Функция дешифрования данных для протокола miIO
void miioDecrypt(const uint8_t* key, const uint8_t* input, size_t input_len, uint8_t* output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, 128);
    
    // Копируем входные данные в выходной буфер
    memcpy(output, input, input_len);
    
    // Дешифруем блоки по 16 байт (AES-128)
    for (size_t i = 0; i < input_len; i += 16) {
        // Проверяем, что у нас есть полный блок
        if (i + 16 <= input_len) {
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, &output[i], &output[i]);
        } else {
            // Последний неполный блок - дополняем нулями
            uint8_t block[16] = {0};
            memcpy(block, &output[i], input_len - i);
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, block, block);
            memcpy(&output[i], block, input_len - i);
        }
    }
    
    mbedtls_aes_free(&aes);
}
