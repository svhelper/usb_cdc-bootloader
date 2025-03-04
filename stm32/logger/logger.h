//  Log messages to the debug console.  Note: Logging will be buffered in memory.  Messages will not 
//  be displayed until debug_flush() is called.
#ifndef LOGGER_H_
#define LOGGER_H_
#include <stdint.h>  //  For uint8_t
#include <stdlib.h>  //  For size_t

#ifdef DEBUG

#ifdef __cplusplus
extern "C" {  //  Allows functions below to be called by C and C++ code.
#endif
void enable_log(void);   //  Enable the debug log.
void disable_log(void);  //  Disable the debug log.
void debug_begin(uint16_t bps);     //  Open the debug console at the specified bits per second.
void debug_write(uint8_t ch);       //  Write a character to the buffered debug log.
void debug_print(const char *s);    //  Write a string to the buffered debug log.
void debug_println(const char *s);  //  Write a string plus newline to the buffered debug log.
void debug_printhex(uint8_t ch);    //  Write a char in hexadecimal to the buffered debug log.
void debug_print_int(int i);
void debug_print_unsigned(size_t l);
void debug_print_char(char ch);
void debug_print_float(float f);    //  Note: Always prints with 2 decimal places.
void debug_flush(void);             //  Flush the buffer of the debug log so that buffered data will appear.
#ifdef __cplusplus
}  //  End of extern C scope.
#endif

#ifdef __cplusplus  //  Overloaded debug functions for C++ only
//  Write an int / size_t / char / float to the buffered debug log.  Will not be displayed until debug_flush() is called.
void debug_print(int i);
void debug_print(size_t l);
void debug_print(char ch);
void debug_print(float f);  //  Note: Always prints with 2 decimal places.

//  Write an int / size_t / char / float plus newline to the buffered debug log.  Will not be displayed until debug_flush() is called.
void debug_println(int i);
void debug_println(size_t l);
void debug_println(char ch);
void debug_println(float f);  //  Note: Always prints with 2 decimal places.  Will not be displayed until debug_flush() is called.
#endif  //  __cplusplus

#else  /*DEBUG*/

#define enable_log()
#define disable_log()
#define debug_begin(bps)
#define debug_write(ch)
#define debug_print(s)
#define debug_println(s)
#define debug_printhex(ch)
#define debug_print_int(i)
#define debug_print_unsigned(l)
#define debug_print_char(ch)
#define debug_print_float(f)
#define debug_flush()

#endif /*DEBUG*/

#endif  //  LOGGER_H_
