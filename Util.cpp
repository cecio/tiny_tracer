#include "Util.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

#include "pin.H"

std::wstring util::hexdump(const uint8_t* in_buf, const size_t max_size)
{
    std::wstringstream ss;
    for (size_t i = 0; i < max_size; i++) {
        if (!PIN_CheckReadAccess(const_cast<uint8_t*>(in_buf) + i)) {
            break;
        }
        if (IS_PRINTABLE(in_buf[i])) {
            ss << char(in_buf[i]);
        }
        else {
            ss << "\\x" << std::setfill(L'0') << std::setw(2) << std::hex << (unsigned int)in_buf[i];
        }
    }
    return ss.str();
}

namespace util {

    template <class CHAR_T>
    size_t getStrLen(const CHAR_T* inp, size_t bytesMax)
    {
        const size_t maxInp = bytesMax / sizeof(CHAR_T);
        for (size_t i = 0; i < maxInp; i++) {
            if (!PIN_CheckReadAccess(const_cast<CHAR_T*>(inp) + i)) {
                return 0;
            }
            const CHAR_T c = inp[i];
            if (c == 0) return i; //end of string
            if (!IS_PRINTABLE(c) && !IS_ENDLINE(c)) return 0;
        }
        return 0;
    }
};

size_t util::getAsciiLen(const char *inp, size_t maxInp)
{
    return getStrLen<char>(inp, maxInp);
}

size_t util::getAsciiLenW(const wchar_t *inp, size_t maxInp)
{
    return getStrLen<wchar_t>(inp, maxInp);
}

std::string util::getDllName(const std::string& str)
{
    std::size_t len = str.length();
    if (!len) return str;

    std::size_t found = str.find_last_of("/\\");
    std::size_t ext = str.find_last_of(".");
    if (ext > len) ext = len;

    std::string name = str.substr(found + 1, ext - (found + 1));
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return std::tolower(c); });
    return name;
}

bool util::iequals(const std::string& a, const std::string& b)
{
    size_t aLen = a.size();
    if (b.size() != aLen) return false;

    for (size_t i = 0; i < aLen; ++i) {
        if (tolower(a[i]) != tolower(b[i])) return false;
    }
    return true;
}

size_t util::splitList(const std::string &sline, const char delimiter, std::vector<std::string> &args)
{
    std::istringstream f(sline);
    std::string s;
    while (getline(f, s, delimiter)) {
        args.push_back(s);
    }
    return args.size();
}

static inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

static inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

void util::trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

int util::loadInt(const std::string &str, bool as_hex)
{
     int intVal = 0;
     
     std::stringstream ss;
     ss << (as_hex ? std::hex : std::dec) << str;
     ss >> intVal;

     return intVal;
}

std::string util::stripQuotes(const std::string& str)
{
     std::string s = str;
     s.erase(std::remove(s.begin(), s.end(), '\"'), s.end());
     return s;
}

bool util::isStrEqualI(const std::string &str1, const std::string &str2)
{
    if (str1.length() != str2.length()) {
        return false;
    }
    for (size_t i = 0; i < str1.length(); i++) {
        if (tolower(str1[i]) != tolower(str2[i])) {
            return false;
        }
    }
    return true;
}

std::string util::getDirectory(const std::string& filepath)
{
    size_t pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos || pos == filepath.length()) {
        return filepath;
    }
    return filepath.substr(0, pos);
}

std::string util::getFilename(const std::string& filepath)
{
    size_t pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos || pos == filepath.length()) {
        return filepath;
    }
    return filepath.substr(pos + 1, filepath.length());
}
