#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Auxiliary data for vsnprintf_helper(). */
struct vsnprintf_aux
  {
    char *p;            /* Current output position. */
    int length;         /* Length of output string. */
    int max_length;     /* Max length of output string. */
  };

static void vsnprintf_helper (char, void *);

/* Like vprintf(), except that output is stored into BUFFER,
   which must have space for BUF_SIZE characters.  Writes at most
   BUF_SIZE - 1 characters to BUFFER, followed by a null
   terminator.  BUFFER will always be null-terminated unless
   BUF_SIZE is zero.  Returns the number of characters that would
   have been written to BUFFER, not including a null terminator,
   had there been enough room. */
int
vsnprintf (char *buffer, size_t buf_size, const char *format, va_list args)
{
  /* Set up aux data for vsnprintf_helper(). */
  struct vsnprintf_aux aux;
  aux.p = buffer;
  aux.length = 0;
  aux.max_length = buf_size > 0 ? buf_size - 1 : 0;

  /* Do most of the work. */
  __vprintf (format, args, vsnprintf_helper, &aux);

  /* Add null terminator. */
  if (buf_size > 0)
    *aux.p = '\0';

  return aux.length;
}

/* Helper function for vsnprintf(). */
static void
vsnprintf_helper (char ch, void *aux_)
{
  struct vsnprintf_aux *aux = aux_;

  if (aux->length++ < aux->max_length)
    *aux->p++ = ch;
}

/* Like printf(), except that output is stored into BUFFER,
   which must have space for BUF_SIZE characters.  Writes at most
   BUF_SIZE - 1 characters to BUFFER, followed by a null
   terminator.  BUFFER will always be null-terminated unless
   BUF_SIZE is zero.  Returns the number of characters that would
   have been written to BUFFER, not including a null terminator,
   had there been enough room. */
int
snprintf (char *buffer, size_t buf_size, const char *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = vsnprintf (buffer, buf_size, format, args);
  va_end (args);

  return retval;
}

/* Writes formatted output to the console.
   In the kernel, the console is both the video display and first
   serial port.
   In userspace, the console is file descriptor 1.
*/
int
printf (const char *format, ...)
{
  va_list args;
  int retval;

  va_start (args, format);
  retval = vprintf (format, args);
  va_end (args);

  return retval;
}

/* printf() formatting internals. */

/* A printf() conversion. */
struct printf_conversion
  {
    /* Flags. */
    enum
      {
        MINUS = 1 << 0,         /* '-' */
        PLUS = 1 << 1,          /* '+' */
        SPACE = 1 << 2,         /* ' ' */
        POUND = 1 << 3,         /* '#' */
        ZERO = 1 << 4,          /* '0' */
        GROUP = 1 << 5          /* '\'' */
      }
    flags;

    /* Minimum field width. */
    int width;

    /* Numeric precision.
       -1 indicates no precision was specified. */
    int precision;

    /* Type of argument to format. */
    enum
      {
        CHAR = 1,               /* hh */
        SHORT = 2,              /* h */
        INT = 3,                /* (none) */
        INTMAX = 4,             /* j */
        LONG = 5,               /* l */
        LONGLONG = 6,           /* ll */
        PTRDIFFT = 7,           /* t */
        SIZET = 8               /* z */
      }
    type;
  };

struct integer_base
  {
    int base;                   /* Base. */
    const char *digits;         /* Collection of digits. */
    const char *signifier;      /* Prefix used with # flag. */
    int group;                  /* Number of digits to group with ' flag. */
  };

static const struct integer_base base_d = {10, "0123456789", "", 3};
static const struct integer_base base_o = {8, "01234567", "0", 3};
static const struct integer_base base_x = {16, "0123456789abcdef", "0x", 4};
static const struct integer_base base_X = {16, "0123456789ABCDEF", "0X", 4};

static const char *parse_conversion (const char *format,
                                     struct printf_conversion *,
                                     va_list *);
static void format_integer (uintmax_t value, bool negative,
                            const struct integer_base *,
                            const struct printf_conversion *,
                            void (*output) (char, void *), void *aux);
static void output_dup (char ch, size_t cnt,
                        void (*output) (char, void *), void *aux);
static void format_string (const char *string, size_t length,
                           struct printf_conversion *,
                           void (*output) (char, void *), void *aux);

void
__vprintf (const char *format, va_list args,
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

      /* Parse conversion specifiers. */
      format = parse_conversion (format, &c, &args);

      /* Do conversion. */
      switch (*format)
        {
        case 'd':
        case 'i':
          {
            /* Signed integer conversions. */
            intmax_t value;

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

            format_integer (value < 0 ? -value : value,
                            value < 0, &base_d, &c, output, aux);
          }
          break;

        case 'o':
        case 'u':
        case 'x':
        case 'X':
          {
            /* Unsigned integer conversions. */
            uintmax_t value;
            const struct integer_base *b;

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
              case 'o': b = &base_o; break;
              case 'u': b = &base_d; break;
              case 'x': b = &base_x; break;
              case 'X': b = &base_X; break;
              default: NOT_REACHED ();
              }

            format_integer (value, false, b, &c, output, aux);
          }
          break;

        case 'c':
          {
            /* Treat character as single-character string. */
            char ch = va_arg (args, int);
            format_string (&ch, 1, &c, output, aux);
          }
          break;

        case 's':
          {
            /* String conversion. */
            const char *s = va_arg (args, char *);
            if (s == NULL)
              s = "(null)";

            /* Limit string length according to precision.
               Note: if c.precision == -1 then strnlen() will get
               SIZE_MAX for MAXLEN, which is just what we want. */
            format_string (s, strnlen (s, c.precision), &c, output, aux);
          }
          break;

        case 'p':
          {
            /* Pointer conversion.
               Format non-null pointers as %#x. */
            void *p = va_arg (args, void *);

            c.flags = POUND;
            if (p != NULL)
              format_integer ((uintptr_t) p, false, &base_x, &c, output, aux);
            else
              format_string ("(nil)", 5, &c, output, aux);
          }
          break;

        case 'f':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'n':
          /* We don't support floating-point arithmetic,
             and %n can be part of a security hole. */
          __printf ("<<no %%%c in kernel>>", output, aux, *format);
          break;

        default:
          __printf ("<<no %%%c conversion>>", output, aux, *format);
          break;
        }
    }
}

/* Parses conversion option characters starting at FORMAT and
   initializes C appropriately.  Returns the character in FORMAT
   that indicates the conversion (e.g. the `d' in `%d').  Uses
   *ARGS for `*' field widths and precisions. */
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

    case 't':
      c->type = PTRDIFFT;
      break;

    case 'z':
      c->type = SIZET;
      break;

    default:
      format--;
      break;
    }

  return format;
}

/* Performs an integer conversion, writing output to OUTPUT with
   auxiliary data AUX.  The integer converted has absolute value
   VALUE.  If NEGATIVE is true the value is negative, otherwise
   positive.  The output will use the given DIGITS, with
   strlen(DIGITS) indicating the output base.  Details of the
   conversion are in C. */
static void
format_integer (uintmax_t value, bool negative, const struct integer_base *b,
                const struct printf_conversion *c,
                void (*output) (char, void *), void *aux)
{
  char buf[64], *cp;            /* Buffer and current position. */
  const char *signifier;        /* b->signifier or "". */
  int precision;                /* Rendered precision. */
  int pad_cnt;                  /* # of pad characters to fill field width. */
  int group_cnt;                /* # of digits grouped so far. */

  /* Accumulate digits into buffer.
     This algorithm produces digits in reverse order, so later we
     will output the buffer's content in reverse.  This is also
     the reason that later we append zeros and the sign. */
  cp = buf;
  group_cnt = 0;
  while (value > 0)
    {
      if ((c->flags & GROUP) && group_cnt++ == b->group)
        {
          *cp++ = ',';
          group_cnt = 0;
        }
      *cp++ = b->digits[value % b->base];
      value /= b->base;
    }

  /* Append enough zeros to match precision.
     If requested precision is 0, then a value of zero is
     rendered as a null string, otherwise as "0". */
  precision = c->precision < 0 ? 1 : c->precision;
  if (precision < 0)
    precision = 1;
  while (cp - buf < precision && cp - buf < (int) sizeof buf - 8)
    *cp++ = '0';

  /* Append sign. */
  if (c->flags & PLUS)
    *cp++ = negative ? '-' : '+';
  else if (c->flags & SPACE)
    *cp++ = negative ? '-' : ' ';
  else if (negative)
    *cp++ = '-';

  /* Calculate number of pad characters to fill field width. */
  signifier = c->flags & POUND ? b->signifier : "";
  pad_cnt = c->width - (cp - buf) - strlen (signifier);
  if (pad_cnt < 0)
    pad_cnt = 0;

  /* Do output. */
  if ((c->flags & (MINUS | ZERO)) == 0)
    output_dup (' ', pad_cnt, output, aux);
  while (*signifier != '\0')
    output (*signifier++, aux);
  if (c->flags & ZERO)
    output_dup ('0', pad_cnt, output, aux);
  while (cp > buf)
    output (*--cp, aux);
  if (c->flags & MINUS)
    output_dup (' ', pad_cnt, output, aux);
}

/* Writes CH to OUTPUT with auxiliary data AUX, CNT times. */
static void
output_dup (char ch, size_t cnt, void (*output) (char, void *), void *aux)
{
  while (cnt-- > 0)
    output (ch, aux);
}

/* Formats the LENGTH characters starting at STRING according to
   the conversion specified in C.  Writes output to OUTPUT with
   auxiliary data AUX. */
static void
format_string (const char *string, size_t length,
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

/* Wrapper for __vprintf() that converts varargs into a
   va_list. */
void
__printf (const char *format,
          void (*output) (char, void *), void *aux, ...)
{
  va_list args;

  va_start (args, aux);
  __vprintf (format, args, output, aux);
  va_end (args);
}

/* Writes string S to the console, followed by a new-line
   character. */
int
puts (const char *s)
{
  while (*s != '\0')
    putchar (*s++);
  putchar ('\n');
  return 0;
}

/* Dumps the SIZE bytes in BUFFER to the console as hex bytes
   arranged 16 per line.  If ASCII is true then the corresponding
   ASCII characters are also rendered alongside. */
void
hex_dump (const void *buffer, size_t size, bool ascii)
{
  const size_t n_per_line = 16; /* Maximum bytes per line. */
  size_t n;                     /* Number of bytes in this line. */
  const uint8_t *p;             /* Start of current line in buffer. */

  for (p = buffer; p < (uint8_t *) buffer + size; p += n)
    {
      size_t i;

      /* Number of bytes on this line. */
      n = (uint8_t *) (buffer + size) - p;
      if (n > n_per_line)
        n = n_per_line;

      /* Print line. */
      for (i = 0; i < n; i++)
        printf ("%c%02x", i == n_per_line / 2 ? '-' : ' ', (unsigned) p[i]);
      if (ascii)
        {
          for (; i < n_per_line; i++)
            printf ("   ");
          printf (" |");
          for (i = 0; i < n; i++)
            printf ("%c", isprint (p[i]) ? p[i] : '.');
          for (; i < n_per_line; i++)
            printf (" ");
          printf ("|");
        }
      printf ("\n");
    }
}
