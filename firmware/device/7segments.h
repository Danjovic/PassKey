/*
    REFRESH DISPLAY:
    refresh display 1 segment at a time. 

*/

#define PORT_SEG_A  PORTC 
#define  BIT_SEG_A  1

#define PORT_SEG_B  PORTC
#define  BIT_SEG_B  0

#define PORT_SEG_C  PORTD 
#define  BIT_SEG_C  7

#define PORT_SEG_D  PORTD 
#define  BIT_SEG_D  5

#define PORT_SEG_E  PORTD 
#define  BIT_SEG_E  6

#define PORT_SEG_F  PORTC 
#define  BIT_SEG_F  4

#define PORT_SEG_G  PORTC 
#define  BIT_SEG_G  5

#define PORT_SEG_DP PORTB 
#define  BIT_SEG_DP 0

#define _A_ON  PORT_SEG_A |=  (1<< BIT_SEG_A) 
#define _A_OFF PORT_SEG_A &= ~(1<< BIT_SEG_A) 

#define _B_ON  PORT_SEG_B |=  (1<< BIT_SEG_B) 
#define _B_OFF PORT_SEG_B &= ~(1<< BIT_SEG_B) 

#define _C_ON  PORT_SEG_C |=  (1<< BIT_SEG_C) 
#define _C_OFF PORT_SEG_C &= ~(1<< BIT_SEG_C) 

#define _D_ON  PORT_SEG_D |=  (1<< BIT_SEG_D) 
#define _D_OFF PORT_SEG_D &= ~(1<< BIT_SEG_D) 

#define _E_ON  PORT_SEG_E |=  (1<< BIT_SEG_E) 
#define _E_OFF PORT_SEG_E &= ~(1<< BIT_SEG_E) 

#define _F_ON  PORT_SEG_F |=  (1<< BIT_SEG_F) 
#define _F_OFF PORT_SEG_F &= ~(1<< BIT_SEG_F) 

#define _G_ON  PORT_SEG_G |=  (1<< BIT_SEG_G) 
#define _G_OFF PORT_SEG_G &= ~(1<< BIT_SEG_G) 

#define _DP_ON  PORT_SEG_DP |=  (1<< BIT_SEG_DP) 
#define _DP_OFF PORT_SEG_DP &= ~(1<< BIT_SEG_DP) 

// #define T_SENGMENT_ON 80   // 80 calls for each segment

#define _A 0
#define _B 1
#define _C 2
#define _D 3
#define _E 4
#define _F 5
#define _G 6
#define _DP 7



//  Digits Map
//  BIT   7 6 5 4 3 2 1 0
//       DP g f e d c b a               
//
//             aaaaa  
//           f       b    
//           f       b    
//           f       b    
//             ggggg
//           e       c
//           e       c
//           e       c
//             ddddd    DP
//





const PROGMEM char digitos [16] = { 0x3f, // 0 }
                                    0x06, // 1
                                    0x5b, // 2
                                    0x4f, // 3
                                    0x66, // 4
                                    0x6d, // 5
                                    0x7d, // 6
                                    0x07, // 7
                                    0x7f, // 8
                                    0x6f, // 9
                                    0x77, // A
                                    0x7c, // B
                                    0x5c, // C
                                    0x5e, // D
                                    0x39, // E
                                    0x73  // F
};									 


void refresh_display( uint8_t digits);