/**
 * @file    app_util.c
 * @brief   字符串与数字工具（纯逻辑，可脱离硬件在 PC 上做单元测试）
 */
#include "app_util.h"

#include <string.h>

static char util_upper(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

uint8_t util_str_eq_ci(const char *a, const char *b)
{
    if (a == 0 || b == 0)
    {
        return 0;
    }
    while (*a != '\0' && *b != '\0')
    {
        if (util_upper(*a) != util_upper(*b))
        {
            return 0;
        }
        a++;
        b++;
    }
    return (uint8_t)((*a == '\0') && (*b == '\0'));
}

char *util_trim(char *s)
{
    char *end;
    if (s == 0)
    {
        return s;
    }
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t'))
    {
        end--;
    }
    *end = '\0';
    return s;
}

const char *util_skip_spaces(const char *s)
{
    if (s == 0)
    {
        return s;
    }
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    return s;
}

uint8_t util_parse_u32(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    uint8_t  digits = 0;

    s = util_skip_spaces(s);
    if (s == 0 || out == 0)
    {
        return 0;
    }
    while (*s >= '0' && *s <= '9')
    {
        uint32_t next = v * 10U + (uint32_t)(*s - '0');
        if (next < v)      /* 溢出保护 */
        {
            return 0;
        }
        v = next;
        digits++;
        s++;
    }
    if (digits == 0U)
    {
        return 0;
    }
    *out = v;
    return 1;
}

uint8_t util_parse_fixed(const char *s, uint32_t *out_x10, uint8_t max_dec)
{
    uint32_t whole = 0;
    uint32_t frac = 0;
    uint32_t mul = 1;
    uint8_t  digits = 0;
    uint8_t  frac_digits = 0;
    uint8_t  i;

    if (out_x10 == 0 || max_dec == 0U || max_dec > 3U)
    {
        return 0;
    }
    s = util_skip_spaces(s);
    if (s == 0)
    {
        return 0;
    }

    while (*s >= '0' && *s <= '9')
    {
        whole = whole * 10U + (uint32_t)(*s - '0');
        digits++;
        s++;
    }

    if (*s == '.')
    {
        s++;
        while (*s >= '0' && *s <= '9' && frac_digits < max_dec)
        {
            frac = frac * 10U + (uint32_t)(*s - '0');
            frac_digits++;
            s++;
        }
        /* 超过 max_dec 的小数位直接丢弃（不四舍五入，避免实现歧义） */
        while (*s >= '0' && *s <= '9')
        {
            s++;
        }
        if (digits == 0U && frac_digits == 0U)
        {
            return 0;
        }
    }
    else if (digits == 0U)
    {
        return 0;
    }

    for (i = 0; i < max_dec; i++)
    {
        mul *= 10U;
    }
    while (frac_digits < max_dec)
    {
        frac *= 10U;
        frac_digits++;
    }

    *out_x10 = whole * mul + frac;
    return 1;
}

void util_format_fixed(char *buf, uint32_t x10, uint8_t dec)
{
    uint32_t div = 1;
    uint32_t whole;
    uint32_t frac;
    uint8_t  i;
    char     tmp[12];
    uint8_t  n = 0;

    if (buf == 0)
    {
        return;
    }
    if (dec == 0U)
    {
        div = 1;
    }
    else
    {
        for (i = 0; i < dec; i++)
        {
            div *= 10U;
        }
    }
    whole = x10 / div;
    frac  = x10 % div;

    /* 整数部分逐位倒序写入临时缓冲 */
    if (whole == 0U)
    {
        tmp[n++] = '0';
    }
    else
    {
        while (whole > 0U && n < (uint8_t)sizeof(tmp))
        {
            tmp[n++] = (char)('0' + (whole % 10U));
            whole /= 10U;
        }
    }
    i = n;
    while (i > 0U)
    {
        *buf++ = tmp[--i];
    }

    if (dec > 0U)
    {
        *buf++ = '.';
        /* 小数部分：从高位往低位取，先补齐前导零 */
        for (i = 0; i < dec; i++)
        {
            div /= 10U;
            *buf++ = (char)('0' + ((frac / div) % 10U));
        }
    }
    *buf = '\0';
}
