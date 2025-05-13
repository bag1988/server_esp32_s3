#include <lcd_utils.h>
// Функция для преобразования UTF-8 кириллицы в коды A02 для LCD с контроллером HD44780
String utf8ToA02(const String &text) {
  String result;
  for (unsigned int i = 0; i < text.length(); i++) {
    unsigned char c = text[i];
    
    // Если это ASCII символ (0-127)
    if (c < 128) {
      result += c;
    } 
    // Если это начало UTF-8 последовательности для кириллицы
    else if (c == 0xD0 || c == 0xD1) {
      if (i + 1 >= text.length()) break; // Защита от выхода за границы строки
      
      unsigned char nextChar = text[i+1];
      i++; // Пропускаем следующий байт
      
      // Кириллица в UTF-8 начинается с байтов 0xD0 или 0xD1
      if (c == 0xD0) {
        switch (nextChar) {
          // Заглавные буквы
          case 0x90: result += (char)0xA0; break; // А
          case 0x91: result += (char)0xA1; break; // Б
          case 0x92: result += (char)0xA2; break; // В
          case 0x93: result += (char)0xA3; break; // Г
          case 0x94: result += (char)0xA4; break; // Д
          case 0x95: result += (char)0xA5; break; // Е
          case 0x81: result += (char)0xA6; break; // Ё (0xD0 0x81)
          case 0x96: result += (char)0xA7; break; // Ж
          case 0x97: result += (char)0xA8; break; // З
          case 0x98: result += (char)0xA9; break; // И
          case 0x99: result += (char)0xAA; break; // Й
          case 0x9A: result += (char)0xAB; break; // К
          case 0x9B: result += (char)0xAC; break; // Л
          case 0x9C: result += (char)0xAD; break; // М
          case 0x9D: result += (char)0xAE; break; // Н
          case 0x9E: result += (char)0xAF; break; // О
          case 0x9F: result += (char)0xE0; break; // П
          case 0xA0: result += (char)0xE1; break; // Р
          case 0xA1: result += (char)0xE2; break; // С
          case 0xA2: result += (char)0xE3; break; // Т
          case 0xA3: result += (char)0xE4; break; // У
          case 0xA4: result += (char)0xE5; break; // Ф
          case 0xA5: result += (char)0xE6; break; // Х
          case 0xA6: result += (char)0xE7; break; // Ц
          case 0xA7: result += (char)0xE8; break; // Ч
          case 0xA8: result += (char)0xE9; break; // Ш
          case 0xA9: result += (char)0xEA; break; // Щ
          case 0xAA: result += (char)0xEB; break; // Ъ
          case 0xAB: result += (char)0xEC; break; // Ы
          case 0xAC: result += (char)0xED; break; // Ь
          case 0xAD: result += (char)0xEE; break; // Э
          case 0xAE: result += (char)0xEF; break; // Ю
          case 0xAF: result += (char)0xF0; break; // Я
          
          // Строчные буквы (первая часть)
          case 0xB0: result += (char)0xB0; break; // а
          case 0xB1: result += (char)0xB1; break; // б
          case 0xB2: result += (char)0xB2; break; // в
          case 0xB3: result += (char)0xB3; break; // г
          case 0xB4: result += (char)0xB4; break; // д
          case 0xB5: result += (char)0xB5; break; // е
          //case 0x91: result += (char)0xB6; break; // ё (0xD1 0x91)
          case 0xB6: result += (char)0xB7; break; // ж
          case 0xB7: result += (char)0xB8; break; // з
          case 0xB8: result += (char)0xB9; break; // и
          case 0xB9: result += (char)0xBA; break; // й
          case 0xBA: result += (char)0xBB; break; // к
          case 0xBB: result += (char)0xBC; break; // л
          case 0xBC: result += (char)0xBD; break; // м
          case 0xBD: result += (char)0xBE; break; // н
          case 0xBE: result += (char)0xBF; break; // о
          case 0xBF: result += (char)0xC0; break; // п
          
          default: result += '?'; break; // Неизвестный символ
        }
      } else if (c == 0xD1) {
        switch (nextChar) {
          // Строчные буквы (вторая часть)
          case 0x80: result += (char)0xC1; break; // р
          case 0x81: result += (char)0xC2; break; // с
          case 0x82: result += (char)0xC3; break; // т
          case 0x83: result += (char)0xC4; break; // у
          case 0x84: result += (char)0xC5; break; // ф
          case 0x85: result += (char)0xC6; break; // х
          case 0x86: result += (char)0xC7; break; // ц
          case 0x87: result += (char)0xC8; break; // ч
          case 0x88: result += (char)0xC9; break; // ш
          case 0x89: result += (char)0xCA; break; // щ
          case 0x8A: result += (char)0xCB; break; // ъ
          case 0x8B: result += (char)0xCC; break; // ы
          case 0x8C: result += (char)0xCD; break; // ь
          case 0x8D: result += (char)0xCE; break; // э
          case 0x8E: result += (char)0xCF; break; // ю
          case 0x8F: result += (char)0xD0; break; // я
          
          // Специальная обработка для ё
          case 0x91: result += (char)0xB6; break; // ё
          
          default: result += '?'; break; // Неизвестный символ
        }
      }
    } else {
      // Другие UTF-8 символы (не кириллица)
      result += '?';
    }
  }
  return result;
}

//Б
const byte rus_B[8] PROGMEM = {
  0b11111,
  0b10000,
  0b10000,
  0b11110,
  0b10001,
  0b10001,
  0b11110,
  0b00000
};
//Г
const byte rus_G[8] PROGMEM = {
  0b11111,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b00000
};
//Д
const byte rus_D[8] PROGMEM = {
  0b00110,
  0b01010,
  0b01010,
  0b01010,
  0b01010,
  0b01010,
  0b11111,
  0b10001
};
//Ж
const byte rus_ZH[8] PROGMEM = {
  0b10101,
  0b10101,
  0b10101,
  0b01110,
  0b10101,
  0b10101,
  0b10101,
  0b00000
};
//З
const byte rus_Z[8] PROGMEM = {
  0b01110,
  0b10001,
  0b00001,
  0b00110,
  0b00001,
  0b10001,
  0b01110,
  0b00000
};
//И
const byte rus_I[8] PROGMEM = {
  0b10001,
  0b10001,
  0b10001,
  0b10011,
  0b10101,
  0b11001,
  0b10001,
  0b00000
};
//Й
const byte rus_II[8] PROGMEM = {
  0b10101,
  0b10001,
  0b10001,
  0b10011,
  0b10101,
  0b11001,
  0b10001,
  0b00000
};
//Л
const byte rus_L[8] PROGMEM = {
  0b00111,
  0b01001,
  0b01001,
  0b01001,
  0b01001,
  0b01001,
  0b10001,
  0b00000
};
//П
const byte rus_P[8] PROGMEM = {
  0b11111,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b00000
};
//У
const byte rus_U[8] PROGMEM = {
  0b10001,
  0b10001,
  0b10001,
  0b01111,
  0b00001,
  0b10001,
  0b01110,
  0b00000
};
//Ф
const byte rus_F[8] PROGMEM = {
  0b00100,
  0b01110,
  0b10101,
  0b10101,
  0b10101,
  0b01110,
  0b00100,
  0b00000
};
//Ц
const byte rus_TS[8] PROGMEM = {
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b11111,
  0b00001
};
//Ч
const byte rus_CH[8] PROGMEM = {
  0b10001,
  0b10001,
  0b10001,
  0b01111,
  0b00001,
  0b00001,
  0b00001,
  0b00000
};
//Ш
const byte rus_SH[8] PROGMEM = {
  0b10001,
  0b10001,
  0b10001,
  0b10101,
  0b10101,
  0b10101,
  0b11111,
  0b00000
};
//Щ
const byte rus_SCH[8] PROGMEM = {
  0b10001,
  0b10001,
  0b10001,
  0b10101,
  0b10101,
  0b10101,
  0b11111,
  0b00001
};
//Ъ
const byte rus_tverd[8] PROGMEM = {
  0b11000,
  0b01000,
  0b01000,
  0b01110,
  0b01001,
  0b01001,
  0b01110,
  0b00000
};
//Ы
const byte rus_Y[8] PROGMEM = {
  0b10001,
  0b10001,
  0b10001,
  0b11101,
  0b10011,
  0b10011,
  0b11101,
  0b00000
};
//Ь
const byte rus_myagk[8] PROGMEM = {
  0b10000,
  0b10000,
  0b10000,
  0b11110,
  0b10001,
  0b10001,
  0b11110,
  0b00000
};
//Э
const byte rus_EE[8] PROGMEM = {
  0b01110,
  0b10001,
  0b00001,
  0b00111,
  0b00001,
  0b10001,
  0b01110,
  0b00000
};
//Ю
const byte rus_YU[8] PROGMEM = {
  0b10010,
  0b10101,
  0b10101,
  0b11101,
  0b10101,
  0b10101,
  0b10010,
  0b00000
};
//Я
const byte rus_YA[8] PROGMEM = {
  0b01111,
  0b10001,
  0b10001,
  0b01111,
  0b00101,
  0b01001,
  0b10001,
  0b00000
};
const byte rus_b[8] PROGMEM = {
  0b00011,
  0b01100,
  0b10000,
  0b11110,
  0b10001,
  0b10001,
  0b01110,
  0b00000
};//б
const byte rus_v[8] PROGMEM = {
  0b00000,
  0b00000,
  0b11110,
  0b10001,
  0b11110,
  0b10001,
  0b11110,
  0b00000
};//в
const byte rus_g[8] PROGMEM = {
  0b00000,
  0b00000,
  0b11110,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b00000
};//г
const byte rus_d[8] PROGMEM = {
  0b00000,
  0b00000,
  0b00110,
  0b01010,
  0b01010,
  0b01010,
  0b11111,
  0b10001
};//д
const byte rus_yo[8] PROGMEM = {
  0b01010,
  0b00000,
  0b01110,
  0b10001,
  0b11111,
  0b10000,
  0b01111,
  0b00000
};//ё
const byte rus_zh[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10101,
  0b10101,
  0b01110,
  0b10101,
  0b10101,
  0b00000
};//ж
const byte rus_z[8] PROGMEM = {
  0b00000,
  0b00000,
  0b01110,
  0b10001,
  0b00110,
  0b10001,
  0b01110,
  0b00000
};//з
const byte rus_i[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10001,
  0b10011,
  0b10101,
  0b11001,
  0b10001,
  0b00000
};//и
const byte rus_ii[8] PROGMEM = {
  0b01010,
  0b00100,
  0b10001,
  0b10011,
  0b10101,
  0b11001,
  0b10001,
  0b00000
};//й
const byte rus_k[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10010,
  0b10100,
  0b11000,
  0b10100,
  0b10010,
  0b00000
};//к
const byte rus_l[8] PROGMEM = {
  0b00000,
  0b00000,
  0b00111,
  0b01001,
  0b01001,
  0b01001,
  0b10001,
  0b00000
};//л
const byte rus_m[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10001,
  0b11011,
  0b10101,
  0b10001,
  0b10001,
  0b00000
};//м
const byte rus_n[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10001,
  0b10001,
  0b11111,
  0b10001,
  0b10001,
  0b00000
};//н
const byte rus_p[8] PROGMEM = {
  0b00000,
  0b00000,
  0b11111,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b00000
};//п
const byte rus_t[8] PROGMEM = {
  0b00000,
  0b00000,
  0b11111,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b00000
};//т
const byte rus_f[8] PROGMEM = {
  0b00000,
  0b00000,
  0b00100,
  0b01110,
  0b10101,
  0b01110,
  0b00100,
  0b00000
};//ф
const byte rus_ts[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10010,
  0b10010,
  0b10010,
  0b10010,
  0b11111,
  0b00001
};//ц
const byte rus_ch[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10001,
  0b10001,
  0b01111,
  0b00001,
  0b00001,
  0b00000
};//ч
const byte rus_sh[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10101,
  0b10101,
  0b10101,
  0b10101,
  0b11111,
  0b00000
};//ш
const byte rus_sch[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10101,
  0b10101,
  0b10101,
  0b10101,
  0b11111,
  0b00001
};//щ
const byte rus_tverd_mal[8] PROGMEM = {
  0b00000,
  0b00000,
  0b11000,
  0b01000,
  0b01110,
  0b01001,
  0b01110,
  0b00000
};//ъ
const byte rus_y[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10001,
  0b10001,
  0b11101,
  0b10011,
  0b11101,
  0b00000
};//ы
const byte rus_myagk_mal[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10000,
  0b10000,
  0b11110,
  0b10001,
  0b11110,
  0b00000
};//ь
const byte rus_ee[8] PROGMEM = {
  0b00000,
  0b00000,
  0b01110,
  0b10001,
  0b00111,
  0b10001,
  0b01110,
  0b00000
};//э
const byte rus_yu[8] PROGMEM = {
  0b00000,
  0b00000,
  0b10010,
  0b10101,
  0b11101,
  0b10101,
  0b10010,
  0b00000
};//ю
const byte rus_ya[8] PROGMEM = {
  0b00000,
  0b00000,
  0b01111,
  0b10001,
  0b01111,
  0b00101,
  0b01001,
  0b00000
};//я
