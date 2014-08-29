/*
 * encoding_iconv.cpp
 *
 *  Created on: Jul 13, 2014
 *      Author: lion
 */

#include <osconfig.h>

#ifdef _WIN32
#include <win_iconv.h>
#else

#include "config.h"

#ifdef HAVE_ICONV_H
#include <iconv.h>
#else
#include <stddef.h>
#endif

#include <dlfcn.h>

#endif

#include "encoding_iconv.h"

namespace fibjs
{

#ifdef _WIN32

#define _iconv_open iconv_open
#define _iconv_close iconv_close
#define _iconv iconv

inline void init_iconv()
{
}

#elif !defined(FreeBSD)

#define _iconv_open iconv_open
#define _iconv_close iconv_close
#define _iconv ((size_t (*)(iconv_t, const char **, size_t *, char **, size_t *))&iconv)

inline void init_iconv()
{
}

#else

#ifndef HAVE_ICONV_H

typedef void *iconv_t;

static iconv_t iconv_open(const char *tocode, const char *fromcode)
{
    return (iconv_t) - 1;
}

static int iconv_close(iconv_t cd)
{
    return 0;
}

static size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft,
                    char **outbuf, size_t *outbytesleft)
{
    return 0;
}

#endif

static size_t (*_iconv)(iconv_t, const char **, size_t *, char **, size_t *);
static iconv_t (*_iconv_open)(const char *, const char *);
static int (*_iconv_close)(iconv_t);

inline void init_iconv()
{
    static bool _init = false;

    if (!_init)
    {
        _init = true;

        void *handle = dlopen("libiconv.so", RTLD_LAZY);

        if (handle)
        {
            _iconv = (size_t (*)(iconv_t, const char **, size_t *, char **, size_t *))dlsym(handle, "iconv");
            if (!_iconv)
                _iconv = (size_t (*)(iconv_t, const char **, size_t *, char **, size_t *))dlsym(handle, "libiconv");

            _iconv_open = (iconv_t (*)(const char *, const char *))dlsym(handle, "iconv_open");
            if (!_iconv_open)
                _iconv_open = (iconv_t (*)(const char *, const char *))dlsym(handle, "libiconv_open");

            _iconv_close = (int (*)(iconv_t))dlsym(handle, "iconv_close");
            if (!_iconv_close)
                _iconv_close = (int (*)(iconv_t))dlsym(handle, "libiconv_close");
        }

        if (!_iconv || !_iconv_open || !_iconv_close)
        {
            _iconv = (size_t (*)(iconv_t, const char **, size_t *, char **, size_t *))iconv;
            _iconv_open = iconv_open;
            _iconv_close = iconv_close;
        }
    }
}

#endif

encoding_iconv::encoding_iconv(const char *charset) :
    m_iconv_en(NULL), m_iconv_de(NULL)
{
    init_iconv();
    if (charset)
        m_charset = charset;
    else
        m_charset = "utf-8";
}

encoding_iconv::~encoding_iconv()
{
    if (m_iconv_en)
        _iconv_close((iconv_t)m_iconv_en);

    if (m_iconv_de)
        _iconv_close((iconv_t)m_iconv_de);
}

void encoding_iconv::open(const char *charset)
{
    if (m_iconv_en)
    {
        _iconv_close((iconv_t)m_iconv_en);
        m_iconv_en = NULL;
    }

    if (m_iconv_de)
    {
        _iconv_close((iconv_t)m_iconv_de);
        m_iconv_de = NULL;
    }

    m_charset = charset;
}

result_t encoding_iconv::encode(const char *data, std::string &retVal)
{
    if (!qstricmp(m_charset.c_str(), "utf8") || !qstricmp(m_charset.c_str(), "utf-8"))
        retVal = data;
    else
    {
        if (!m_iconv_en)
        {
            m_iconv_en = _iconv_open(m_charset.c_str(), "utf-8");
            if (m_iconv_en == (iconv_t)(-1))
                return CHECK_ERROR(Runtime::setError("Unknown charset."));
        }

        size_t sz = qstrlen(data);

        retVal.resize(sz * 2);
        char *output_buf = &retVal[0];
        size_t output_size = retVal.length();

        size_t n = _iconv((iconv_t)m_iconv_en, &data, &sz, &output_buf, &output_size);

        if (n == (size_t) - 1)
            return CHECK_ERROR(Runtime::setError("convert error."));

        retVal.resize(retVal.length() - output_size);
    }

    return 0;
}

result_t encoding_iconv::encode(const char *data, obj_ptr<Buffer_base> &retVal)
{
    std::string strBuf;

    result_t hr = encode(data, strBuf);
    if (hr < 0)
        return hr;

    retVal = new Buffer(strBuf);

    return 0;
}

result_t encoding_iconv::decode(const std::string &data, std::string &retVal)
{
    if (!qstricmp(m_charset.c_str(), "utf8") || !qstricmp(m_charset.c_str(), "utf-8"))
        retVal = data;
    else
    {
        if (!m_iconv_de)
        {
            m_iconv_de = _iconv_open("utf-8", m_charset.c_str());
            if (m_iconv_de == (iconv_t)(-1))
                return CHECK_ERROR(Runtime::setError("Unknown charset."));
        }

        size_t sz = data.length();
        const char *ptr = data.c_str();
        std::string strBuf;

        strBuf.resize(sz * 2);
        char *output_buf = &strBuf[0];
        size_t output_size = strBuf.length();

        size_t n = _iconv((iconv_t)m_iconv_de, &ptr, &sz, &output_buf, &output_size);

        if (n == (size_t) - 1)
            return CHECK_ERROR(Runtime::setError("convert error."));

        strBuf.resize(strBuf.length() - output_size);

        retVal = strBuf;
    }

    return 0;
}

result_t encoding_iconv::decode(Buffer_base *data, std::string &retVal)
{
    if (!qstricmp(m_charset.c_str(), "utf8") || !qstricmp(m_charset.c_str(), "utf-8"))
        data->toString(retVal);
    else
    {
        if (!m_iconv_de)
        {
            m_iconv_de = _iconv_open("utf-8", m_charset.c_str());
            if (m_iconv_de == (iconv_t)(-1))
                return CHECK_ERROR(Runtime::setError("Unknown charset."));
        }

        std::string strData;
        std::string strBuf;

        data->toString(strData);

        size_t sz = strData.length();
        const char *ptr = strData.c_str();

        strBuf.resize(sz * 2);
        char *output_buf = &strBuf[0];
        size_t output_size = strBuf.length();

        size_t n = _iconv((iconv_t)m_iconv_de, &ptr, &sz, &output_buf, &output_size);

        if (n == (size_t) - 1)
            return CHECK_ERROR(Runtime::setError("convert error."));

        strBuf.resize(strBuf.length() - output_size);

        retVal = strBuf;
    }

    return 0;
}

}

