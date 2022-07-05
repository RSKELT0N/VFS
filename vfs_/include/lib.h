#ifndef _LIB_H_
#define _LIB_H_

#include "config.h"

#define PBWIDTH 60
#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"

namespace VFS {

    namespace lib_ {

        inline int countDigit(int n)
        {
            int count = 0;
            while (n != 0)
            {
                n = n / 10;
                ++count;
            }
            return count;
        }

        inline void printProgress(double percentage) {
            int val = (int) (percentage * 100);
            int lpad = (int) (percentage * PBWIDTH);
            int rpad = PBWIDTH - lpad;
            printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
            fflush(stdout);
        }

        inline std::vector<std::string> split(const char* line, char sep) noexcept {
            std::vector<std::string> tokens;
            std::stringstream ss(line);
            std::string x;

            while ((getline(ss, x, sep))) {
                if (x != "")
                    tokens.push_back(x);
            }
            return tokens;
        }

        inline constexpr unsigned int hash(const char *s, int off = 0) {
            return !s[off] ? 5381 : (hash(s, off+1)*33) ^ s[off];
        }

        inline int itoa_(int value, char *sp, int radix, int amt) {
            char tmp[amt];// be careful with the length of the buffer
            char *tp = tmp;
            uint64_t i = 0;
            unsigned v = 0;

            int sign = (radix == 10 && value < 0);
            if (sign)
                v = -value;
            else
                v = (unsigned)value;

            while (v || tp == tmp) {
                i = v % radix;
                v /= radix;
                if (i < 10)
                  *tp++ = i+'0';
                else
                  *tp++ = i + 'a' - 10;
            }

            uint64_t len = tp - tmp;

            if (sign) {
                *sp++ = '-';
                len++;
            }

            while (tp > tmp)
                *sp++ = *--tp;

            return len;
        }

    }
}

#endif // _LIB_H_
