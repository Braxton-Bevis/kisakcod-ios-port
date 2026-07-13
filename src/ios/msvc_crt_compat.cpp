// Implementations for the MSVC CRT functions that have no libc equivalent.
// See ios/msvc_crt_compat.h for the alias table.
#ifdef KISAK_IOS

#include <cctype>
#include <cstdio>
#include <ctime>

char *q_ios_strlwr(char *s)
{
    for (char *p = s; *p; ++p)
        *p = (char)tolower((unsigned char)*p);
    return s;
}

char *q_ios_strupr(char *s)
{
    for (char *p = s; *p; ++p)
        *p = (char)toupper((unsigned char)*p);
    return s;
}

char *q_ios_itoa(int value, char *str, int radix)
{
    // The engine only ever calls _itoa with radix 10.
    if (radix == 10)
    {
        sprintf(str, "%d", value);
        return str;
    }
    // Generic fallback for other radices (MSVC treats value as unsigned then).
    unsigned int v = (unsigned int)value;
    char tmp[33];
    int i = 0;
    do
    {
        int d = (int)(v % (unsigned int)radix);
        tmp[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= (unsigned int)radix;
    } while (v);
    char *out = str;
    while (i > 0)
        *out++ = tmp[--i];
    *out = '\0';
    return str;
}

long long q_ios_time64(long long *timer)
{
    const std::time_t nativeTime = std::time(nullptr);
    const long long result = static_cast<long long>(nativeTime);
    if (timer)
        *timer = result;
    return result;
}

std::tm *q_ios_localtime64(const long long *timer)
{
    if (!timer)
        return nullptr;
    const std::time_t nativeTime = static_cast<std::time_t>(*timer);
    return std::localtime(&nativeTime);
}

char *q_ios_ctime64(const long long *timer)
{
    if (!timer)
        return nullptr;
    const std::time_t nativeTime = static_cast<std::time_t>(*timer);
    return std::ctime(&nativeTime);
}

#endif // KISAK_IOS
