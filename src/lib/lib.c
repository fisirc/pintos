#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include "debug.h"
#include "interrupt.h"
#include "lib.h"
#include "serial.h"
#include "vga.h"

/* Sets the SIZE bytes in DST to VALUE. */
void *
memset (void *dst_, int value, size_t size)
{
  unsigned char *dst = dst_;

  ASSERT (dst != NULL || size == 0);

  while (size-- > 0)
    *dst++ = value;

  return dst_;
}

/* Copies SIZE bytes from SRC to DST, which must not overlap.
   Returns DST. */
void *
memcpy (void *dst_, const void *src_, size_t size)
{
  unsigned char *dst = dst_;
  const unsigned char *src = src_;

  ASSERT (dst != NULL || size == 0);
  ASSERT (src != NULL || size == 0);

  while (size-- > 0)
    *dst++ = *src++;

  return dst_;
}

/* Copies SIZE bytes from SRC to DST, which are allowed to
   overlap.  Returns DST. */
void *
memmove (void *dst_, const void *src_, size_t size)
{
  unsigned char *dst = dst_;
  const unsigned char *src = src_;

  ASSERT (dst != NULL || size == 0);
  ASSERT (src != NULL || size == 0);

  if (dst < src)
    {
      while (size-- > 0)
        *dst++ = *src++;
    }
  else
    {
      dst += size;
      src += size;
      while (size-- > 0)
        *--dst = *--src;
    }

  return dst;
}

/* Returns a pointer to the first occurrence of CH in the first
   SIZE bytes starting at BLOCK.  Returns a null pointer if CH
   does not occur in BLOCK. */
void *
memchr (const void *block_, int ch_, size_t size)
{
  const unsigned char *block = block_;
  unsigned char ch = ch_;

  ASSERT (block != NULL || size == 0);

  for (; size-- > 0; block++)
    if (*block == ch)
      return (void *) block;

  return NULL;
}

/* Find the first differing byte in the two blocks of SIZE bytes
   at A and B.  Returns a positive value if the byte in A is
   greater, a negative value if the byte in B is greater, or zero
   if blocks A and B are equal. */
int
memcmp (const void *a_, const void *b_, size_t size)
{
  const unsigned char *a = a_;
  const unsigned char *b = b_;

  ASSERT (a != NULL || size == 0);
  ASSERT (b != NULL || size == 0);

  for (; size-- > 0; a++, b++)
    if (*a != *b)
      return *a > *b ? +1 : -1;
  return 0;
}

/* Copies string SRC to DST.  If SRC is longer than SIZE - 1
   characters, only SIZE - 1 characters are copied.  A null
   terminator is always written to DST, unless SIZE is 0.
   Returns the length of SRC. */
size_t
strlcpy (char *dst, const char *src, size_t size)
{
  size_t src_len;

  ASSERT (dst != NULL);
  ASSERT (src != NULL);

  src_len = strlen (src);
  if (size > 0)
    {
      size_t dst_len = size - 1;
      if (src_len < dst_len)
        src_len = dst_len;
      memcpy (dst, src, dst_len);
      dst[dst_len] = '\0';
    }
  return src_len;
}

/* Returns the length of STRING. */
size_t
strlen (const char *string)
{
  const char *p;

  ASSERT (string != NULL);

  for (p = string; *p != '\0'; p++)
    continue;
  return p - string;
}

/* Finds and returns the first occurrence of C in STRING, or a
   null pointer if C does not appear in STRING.  If C == '\0'
   then returns a pointer to the null terminator at the end of
   STRING. */
char *
strchr (const char *string, int c_)
{
  char c = c_;

  ASSERT (string != NULL);

  for (;;)
    if (*string == c)
      return (char *) string;
    else if (*string == '\0')
      return NULL;
    else
      string++;
}

/* Finds the first differing characters in strings A and B.
   Returns a positive value if the character in A (as an unsigned
   char) is greater, a negative value if the character in B (as
   an unsigned char) is greater, or zero if strings A and B are
   equal. */
int
strcmp (const char *a_, const char *b_)
{
  const unsigned char *a = (const unsigned char *) a_;
  const unsigned char *b = (const unsigned char *) b_;

  ASSERT (a != NULL);
  ASSERT (b != NULL);

  while (*a != '\0' && *a == *b)
    {
      a++;
      b++;
    }

  return *a < *b ? -1 : *a > *b;
}

char *
strtok_r (char *s, const char *delimiters, char **save_ptr)
{
  char *token;

  ASSERT (delimiters != NULL);
  ASSERT (save_ptr != NULL);

  /* If S is nonnull, start from it.
     If S is null, start from saved position. */
  if (s == NULL)
    s = *save_ptr;
  ASSERT (s != NULL);

  /* Skip any DELIMITERS at our current position. */
  while (strchr (delimiters, *s) != NULL)
    {
      /* strchr() will always return nonnull if we're searching
         for a null byte, because every string contains a null
         byte (at the end). */
      if (*s == '\0')
        {
          *save_ptr = s;
          return NULL;
        }

      s++;
    }

  /* Skip any non-DELIMITERS up to the end of the string. */
  token = s;
  while (strchr (delimiters, *s) == NULL)
    s++;
  if (*s != '\0')
    {
      *s = '\0';
      *save_ptr = s + 1;
    }
  else
    *save_ptr = s;
  return token;
}

int
atoi (const char *s)
{
  bool negative;
  int value;

  /* Skip white space. */
  while (isspace (*s))
    s++;

  /* Parse sign. */
  negative = false;
  if (*s == '+')
    s++;
  else if (*s == '-')
    {
      negative = true;
      s++;
    }

  /* Parse digits.  We always initially parse the value as
     negative, and then make it positive later, because the
     negative range of an int is bigger than the positive range
     on a 2's complement system. */
  for (value = 0; isdigit (*s); s++)
    value = value * 10 - (*s - '0');
  if (!negative)
    value = -value;

  return value;
}

static void
vprintf_core (const char *format, va_list args,
              void (*output) (char, void *), void *aux);

static void
vprintk_helper (char ch, void *aux UNUSED)
{
  vga_putc (ch);
  serial_outb (ch);
}

void
vprintk (const char *format, va_list args)
{
  enum intr_level old_level = intr_disable ();
  vprintf_core (format, args, vprintk_helper, NULL);
  intr_set_level (old_level);
}

void
printk (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  vprintk (format, args);
  va_end (args);
}

struct vsnprintf_aux
  {
    char *p;
    int length;
    int max_length;
  };

static void
vsnprintf_helper (char ch, void *aux_)
{
  struct vsnprintf_aux *aux = aux_;

  if (aux->length++ < aux->max_length)
    *aux->p++ = ch;
}

int
vsnprintf (char *buffer, size_t buf_size,
           const char *format, va_list args)
{
  struct vsnprintf_aux aux;
  aux.p = buffer;
  aux.length = 0;
  aux.max_length = buf_size > 0 ? buf_size - 1 : 0;

  vprintf_core (format, args, vsnprintf_helper, &aux);

  if (buf_size > 0)
    *aux.p = '\0';

  return aux.length;
}

int
snprintf (char *buffer, size_t buf_size,
          const char *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = vsnprintf (buffer, buf_size, format, args);
  va_end (args);

  return retval;
}

/* printf() and friends internals.  You do not need to understand
   this code. */

struct printf_conversion
  {
    enum
      {
        MINUS = 1 << 0,
        PLUS = 1 << 1,
        SPACE = 1 << 2,
        POUND = 1 << 3,
        ZERO = 1 << 4,
        GROUP = 1 << 5
      }
    flags;

    int width;
    int precision;

    enum
      {
        CHAR = 1,
        SHORT = 2,
        INT = 3,
        INTMAX = 4,
        LONG = 5,
        LONGLONG = 6,
        PTRDIFFT = 7,
        SIZET = 8
      }
    type;
  };

static const char *
parse_conversion (const char *format, struct printf_conversion *c,
                  va_list *args)
{
  /* Parse flag characters. */
  c->flags = 0;
  for (;;)
    {
      switch (*format++)
        {
        case '-':
          c->flags |= MINUS;
          break;
        case '+':
          c->flags |= PLUS;
          break;
        case ' ':
          c->flags |= SPACE;
          break;
        case '#':
          c->flags |= POUND;
          break;
        case '0':
          c->flags |= ZERO;
          break;
        case '\'':
          c->flags |= GROUP;
          break;
        default:
          format--;
          goto not_a_flag;
        }
    }
 not_a_flag:
  if (c->flags & MINUS)
    c->flags &= ~ZERO;
  if (c->flags & PLUS)
    c->flags &= ~SPACE;

  /* Parse field width. */
  c->width = 0;
  if (*format == '*')
    {
      format++;
      c->width = va_arg (*args, int);
    }
  else
    {
      for (; isdigit (*format); format++)
        c->width = c->width * 10 + *format - '0';
    }
  if (c->width < 0)
    {
      c->width = -c->width;
      c->flags |= MINUS;
    }

  /* Parse precision. */
  c->precision = -1;
  if (*format == '.')
    {
      format++;
      if (*format == '*')
        {
          format++;
          c->precision = va_arg (*args, int);
        }
      else
        {
          c->precision = 0;
          for (; isdigit (*format); format++)
            c->precision = c->precision * 10 + *format - '0';
        }
      if (c->precision < 0)
        c->precision = -1;
    }
  if (c->precision >= 0)
    c->flags &= ~ZERO;

  /* Parse type. */
  c->type = INT;
  switch (*format++)
    {
    case 'h':
      if (*format == 'h')
        {
          format++;
          c->type = CHAR;
        }
      else
        c->type = SHORT;
      break;

    case 'j':
      c->type = INTMAX;
      break;

    case 'l':
      if (*format == 'l')
        {
          format++;
          c->type = LONGLONG;
        }
      else
        c->type = LONG;
      break;

    case 'L':
    case 'q':
      c->type = LONGLONG;
      break;

    case 't':
      c->type = PTRDIFFT;
      break;

    case 'z':
    case 'Z':
      c->type = SIZET;
      break;

    default:
      format--;
      break;
    }

  return format;
}

static void
output_dup (char ch, size_t cnt, void (*output) (char, void *), void *aux)
{
  while (cnt-- > 0)
    output (ch, aux);
}

static void
printf_integer (uintmax_t value, bool negative, const char *digits,
                struct printf_conversion *c,
                void (*output) (char, void *), void *aux)

{
  char buf[64], *cp;
  int base;
  const char *base_name;
  int pad_cnt, group_cnt;

  base = strlen (digits);

  /* Accumulate digits into buffer.
     This algorithm produces digits in reverse order, so later we
     will output the buffer's content in reverse.  This is also
     the reason that later we append zeros and the sign. */
  cp = buf;
  group_cnt = 0;
  while (value > 0)
    {
      if ((c->flags & GROUP) && group_cnt++ == 3)
        {
          *cp++ = ',';
          group_cnt = 0;
        }
      *cp++ = digits[value % base];
      value /= base;
    }

  /* Append enough zeros to match precision.
     If precision is 0, then a value of zero is rendered as a
     null string.  Otherwise at least one digit is presented. */
  if (c->precision < 0)
    c->precision = 1;
  while (cp - buf < c->precision && cp - buf < (int) sizeof buf - 8)
    *cp++ = '0';

  /* Append sign. */
  if (c->flags & PLUS)
    *cp++ = negative ? '-' : '+';
  else if (c->flags & SPACE)
    *cp++ = negative ? '-' : ' ';
  else if (negative)
    *cp++ = '-';

  /* Get name of base. */
  base_name = "";
  if (c->flags & POUND)
    {
      if (base == 8)
        base_name = "0";
      else if (base == 16)
        base_name = digits[0xa] == 'a' ? "0x" : "0X";
    }

  /* Calculate number of pad characters to fill field width. */
  pad_cnt = c->width - (cp - buf) - strlen (base_name);
  if (pad_cnt < 0)
    pad_cnt = 0;

  /* Do output. */
  if ((c->flags & (MINUS | ZERO)) == 0)
    output_dup (' ', pad_cnt, output, aux);
  while (*base_name != '\0')
    output (*base_name++, aux);
  if (c->flags & ZERO)
    output_dup ('0', pad_cnt, output, aux);
  while (cp > buf)
    output (*--cp, aux);
  if (c->flags & MINUS)
    output_dup (' ', pad_cnt, output, aux);
}

static void
printf_string (const char *string, size_t length,
               struct printf_conversion *c,
               void (*output) (char, void *), void *aux)
{
  if (c->width > 1 && (c->flags & MINUS) == 0)
    output_dup (' ', c->width - 1, output, aux);
  while (length-- > 0)
    output (*string++, aux);
  if (c->width > 1 && (c->flags & MINUS) != 0)
    output_dup (' ', c->width - 1, output, aux);
}

static void
printf_core (const char *format,
             void (*output) (char, void *), void *aux, ...)
{
  va_list args;

  va_start (args, aux);
  vprintf_core (format, args, output, aux);
  va_end (args);
}

static void
vprintf_core (const char *format, va_list args,
              void (*output) (char, void *), void *aux)
{
  for (; *format != '\0'; format++)
    {
      struct printf_conversion c;

      /* Literally copy non-conversions to output. */
      if (*format != '%')
        {
          output (*format, aux);
          continue;
        }
      format++;

      /* %% => %. */
      if (*format == '%')
        {
          output ('%', aux);
          continue;
        }

      format = parse_conversion (format, &c, &args);
      switch (*format)
        {
        case 'd':
        case 'i':
          {
            intmax_t value;
            uintmax_t abs_value;
            bool negative = false;

            switch (c.type)
              {
              case CHAR:
                value = (signed char) va_arg (args, int);
                break;
              case SHORT:
                value = (short) va_arg (args, int);
                break;
              case INT:
                value = va_arg (args, int);
                break;
              case LONG:
                value = va_arg (args, long);
                break;
              case LONGLONG:
                value = va_arg (args, long long);
                break;
              case PTRDIFFT:
                value = va_arg (args, ptrdiff_t);
                break;
              case SIZET:
                value = va_arg (args, size_t);
                break;
              default:
                NOT_REACHED ();
              }

            if (value < 0)
              {
                negative = true;
                abs_value = -value;
              }
            else
              abs_value = value;

            printf_integer (abs_value, negative, "0123456789",
                            &c, output, aux);
          }
          break;

        case 'o':
        case 'u':
        case 'x':
        case 'X':
          {
            uintmax_t value;
            const char *digits;

            switch (c.type)
              {
              case CHAR:
                value = (unsigned char) va_arg (args, unsigned);
                break;
              case SHORT:
                value = (unsigned short) va_arg (args, unsigned);
                break;
              case INT:
                value = va_arg (args, unsigned);
                break;
              case LONG:
                value = va_arg (args, unsigned long);
                break;
              case LONGLONG:
                value = va_arg (args, unsigned long long);
                break;
              case PTRDIFFT:
                value = va_arg (args, ptrdiff_t);
                break;
              case SIZET:
                value = va_arg (args, size_t);
                break;
              default:
                NOT_REACHED ();
              }

            switch (*format)
              {
              case 'o':
                digits = "01234567";
                break;
              case 'u':
                digits = "0123456789";
                break;
              case 'x':
                digits = "0123456789abcdef";
                break;
              case 'X':
                digits = "0123456789ABCDEF";
                break;
              default:
                NOT_REACHED ();
              }

            printf_integer (value, false, digits, &c, output, aux);
          }
          break;

        case 'c':
          {
            char ch = va_arg (args, int);
            printf_string (&ch, 1, &c, output, aux);
          }
          break;

        case 's':
          {
            const char *s;
            size_t length;

            s = va_arg (args, char *);
            if (s == NULL)
              s = "(null)";

            if (c.precision >= 0)
              {
                const char *zero = memchr (s, '\0', c.precision);
                if (zero != NULL)
                  length = zero - s;
                else
                  length = c.precision;
              }
            else
              length = strlen (s);

            printf_string (s, length, &c, output, aux);
          }
          break;

        case 'p':
          {
            void *p = va_arg (args, void *);

            c.flags = POUND;
            if (p != NULL)
              printf_integer ((uintptr_t) p,
                              false, "0123456789abcdef", &c,
                              output, aux);
            else
              printf_string ("(nil)", 5, &c, output, aux);
          }
          break;

        case 'f':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'n':
          printf_core ("<<no %%%c in kernel>>", output, aux, *format);
          break;

        default:
          printf_core ("<<no %%%c conversion>>", output, aux, *format);
          break;
        }
    }
}

void
hex_dump (const void *buffer, size_t size, bool ascii)
{
  const size_t n_per_line = 16;
  const uint8_t *p = buffer;
  size_t ofs = 0;

  while (size > 0)
    {
      size_t n, i;

      printk ("%08zx", ofs);
      n = size >= n_per_line ? n_per_line : size;
      for (i = 0; i < n; i++)
        printk ("%c%02x", i == n_per_line / 2 ? '-' : ' ', (unsigned) p[i]);

      if (ascii)
        {
          for (; i < n_per_line; i++)
            printk ("   ");
          printk (" |");
          for (i = 0; i < n; i++)
            printk ("%c", isprint (p[i]) ? p[i] : '.');
          for (; i < n_per_line; i++)
            printk (" ");
          printk ("|");
        }
      printk ("\n");

      p += n;
      ofs += n;
      size -= n;
    }
}
