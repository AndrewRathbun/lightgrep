/*
 * Copyright 2024 Aon Cyber Solutions
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#pragma once

#include <algorithm>

#include "basic.h"

#define CONTINUATION(cp, i, end, used) \
  if (++i == end) { \
    cp = -1; \
    return used; \
  } \
  b = *i & 0xFF; \
  if (b < 0x80 || 0xBF < b) { \
    cp = -1; \
    return used; \
  } \
  cp = (cp << 6) | (b & 0x3F)

template <class Iterator>
int utf8_to_unicode(int& cp, Iterator i, const Iterator& end) {

  if (i == end) {
    cp = -1;
    return 0;
  }

  cp = *i & 0xFF;

  // 1-byte sequence
  if (cp < 0x80) {
    return 1;
  }

  byte b;
  // 2-byte sequence
  if (cp < 0xE0) {
    // overlong
    if (cp < 0xC2) {
      cp = -1;
      return 1;
    }

    cp &= 0x1F;
    CONTINUATION(cp, i, end, 2);

    return 2;
  }
  // 3-byte sequence
  else if (cp < 0xF0) {
    cp &= 0x0F;
    CONTINUATION(cp, i, end, 2);
    CONTINUATION(cp, i, end, 3);

    // check for overlong and UTF-16 surrogates
    if (cp < 0x800 || (0xD7FF < cp && cp < 0xE000)) {
      cp = -1;
    }

    return 3;
  }
  // 4-byte sequence
  else if (cp < 0xF5) {
    cp &= 0x07;
    CONTINUATION(cp, i, end, 2);
    CONTINUATION(cp, i, end, 3);
    CONTINUATION(cp, i, end, 4);

    // check for overlong and too high
    if (cp < 0x10000 || cp > 0x10FFFF) {
      cp = -1;
    }

    return 4;
  }
  else {
    // bad start byte
    cp = -1;
    return 1;
  }
}

#undef CONTINUATION

template <class Iterator>
int unicode_to_ascii(byte& b, Iterator& i, const Iterator& end) {
  if (i == end) {
    return -1;
  }

  const int cp = *i++;
  if (cp < 0 || 0xFF < cp) {
    return -1;
  }

  b = cp;
  return 1;
}

template <class Decoder, class InputIterator, class OutputIterator>
OutputIterator transform_to_unicode(InputIterator ibegin,
                                    InputIterator iend,
                                    OutputIterator obegin,
                                    Decoder decoder)
{
  int cp;
  while (ibegin != iend) {
    ibegin += decoder(cp, ibegin, iend);
    *obegin++ = cp;
  }
  return obegin;
}

template <class InputIterator, class OutputIterator>
OutputIterator transform_utf8_to_unicode(InputIterator ibegin,
                                         InputIterator iend,
                                         OutputIterator obegin)
{
  return transform_to_unicode(ibegin, iend, obegin,
                              utf8_to_unicode<InputIterator>);
}

/*
template <class InputIterator, class OutputIterator>
OutputIterator transform_unicode_to_ascii(InputIterator ibegin,
                                          InputIterator iend,
                                          OutputIterator obegin)
{
  byte b;
  int ret;
  while (ibegin != iend) {
    ret = unicode_to_ascii(b, ibegin, iend);
    if (ret < 0) {

    }
    ibegin += unicode_to_ascii(b, ibegin, iend);
    *obegin++ = b;
  }
  return obegin;
}
*/
