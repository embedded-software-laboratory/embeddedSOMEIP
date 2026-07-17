/*
The MIT License
Copyright (c) 2026 Lehrstuhl Informatik 11 - RWTH Aachen University
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE

This file is part of embeddedSOMEIP.

Author: i11 - Embedded Software, RWTH Aachen University
*/

#ifndef SOMEIP_PBUFWRAPPER_HPP
#define SOMEIP_PBUFWRAPPER_HPP

#include <cassert>
#include <cstring>
#include <lwip/pbuf.h>
#include "someIp/logging/BaseLogger.hpp"

constexpr char PBUF_WRAPPER_TAG[] = "PBUF_W";
using PBUF_WRAPPER_LOGGER = BaseLogger<PBUF_WRAPPER_TAG>;

class PbufWrapper
{
public:
  PbufWrapper() : m_ptr_pbuf(nullptr) {}

  explicit PbufWrapper(pbuf *ptr_p) : m_ptr_pbuf(ptr_p)
  {
    assert(m_ptr_pbuf && "PbufWrapper constructed with null pbuf");
  }

  ~PbufWrapper()
  {
    release();
  }

  // copying would double free
  PbufWrapper(const PbufWrapper &) = delete;
  PbufWrapper &operator=(const PbufWrapper &) = delete;

  PbufWrapper(PbufWrapper &&ref_other) noexcept : m_ptr_pbuf(ref_other.m_ptr_pbuf) 
  {
    ref_other.m_ptr_pbuf = nullptr;
  }

  PbufWrapper &operator=(PbufWrapper &&ref_other) noexcept
  {
    if (this != &ref_other)
    {
      release();
      m_ptr_pbuf = ref_other.m_ptr_pbuf;
      ref_other.m_ptr_pbuf = nullptr;
    }
    return *this;
  }

  pbuf *get() const { return m_ptr_pbuf; }

  // do not use p after this call
  void AddInFront(pbuf *ptr_p)
  {
    pbuf_chain(ptr_p, m_ptr_pbuf);
    pbuf_free(m_ptr_pbuf);
    m_ptr_pbuf = ptr_p;
  }

  PbufWrapper slice_copy(uint32_t length, uint32_t offset)
  {
    if (!m_ptr_pbuf || length == 0 || offset + length > m_ptr_pbuf->tot_len)
    {
      PBUF_WRAPPER_LOGGER::debug("slice_copy: invalid args");
      //std::cout << "slice_copy: invalid args" << std::endl;
      return PbufWrapper(); 
    }

    pbuf *ptr_dst = pbuf_alloc(PBUF_RAW, (u16_t)length, PBUF_RAM);
    if(!ptr_dst)
    {
      PBUF_WRAPPER_LOGGER::debug("slice_copy: OOM allocating %u bytes", length);
      //std::cout << "slice_copy: OOM allocating "<< length << "bytes" << std::endl;
      return PbufWrapper();
    }

    u16_t copied = pbuf_copy_partial(m_ptr_pbuf, ptr_dst->payload, (u16_t)length, (u16_t)offset);
    if(copied!=length)
    {
      PBUF_WRAPPER_LOGGER::debug("slice_copy: copy failed ");
      //std::cout << "slice_copy: copy failed " << std::endl; 
      pbuf_free(ptr_dst);
      return PbufWrapper();
    }  

    PBUF_WRAPPER_LOGGER::debug("slice_copy: copied %u bytes", length);
    //std::cout << "slice_copy: copied "<< length <<" bytes" << std::endl;
    return PbufWrapper(std::move(ptr_dst));
  }

  // returned wrapper owns the first len bytes, this keeps the rest
  PbufWrapper slice_move(uint32_t length)
  { 
    if (!m_ptr_pbuf || length == 0 || m_ptr_pbuf->tot_len < length)
    { 
      PBUF_WRAPPER_LOGGER::debug("slice_move: invalid args");
      return PbufWrapper(); 
    }
    if (m_ptr_pbuf->tot_len == length)
    {
      PBUF_WRAPPER_LOGGER::debug("slice_move: whole buffer");
      pbuf* ptr_out = m_ptr_pbuf;
      m_ptr_pbuf = nullptr;
      return PbufWrapper(ptr_out);
    }

    // find the pbuf holding that offset and its relative offset
    uint16_t relative_offset = 0;
    pbuf* ptr_split_buffer = pbuf_skip(m_ptr_pbuf, length, &relative_offset);

    if (!ptr_split_buffer)
    {
      PBUF_WRAPPER_LOGGER::debug("slice_move: pbuf_skip failed");
      return PbufWrapper();
    } 
      
    // split aligns with pbuf boundary, zero copy
    if(relative_offset == 0)
    {
      PBUF_WRAPPER_LOGGER::debug("slice_move: boundary split");
      // segment = [m_ptr_pbuf,...,prev_end]   remainder = [split_buf,...]
      pbuf *ptr_remainder = ptr_split_buffer;
      pbuf *ptr_segment = m_ptr_pbuf;
      pbuf *ptr_prev_end = m_ptr_pbuf;
      while(ptr_prev_end && ptr_prev_end->next != ptr_split_buffer)
      {
        ptr_prev_end = ptr_prev_end->next;
      }
      if(ptr_prev_end)
      {
        ptr_prev_end->next = nullptr;
        correct_tot_len(ptr_segment, ptr_remainder->tot_len); // shrink
        ptr_prev_end->tot_len = ptr_prev_end->len;
      }

      m_ptr_pbuf = ptr_remainder;
      return PbufWrapper(ptr_segment);
    }
    
    // split inside pbuf (relative_offset != 0)
    const u16_t first_part = relative_offset;
    const u16_t second_part = (u16_t)(ptr_split_buffer->len-relative_offset);

    pbuf *ptr_intermediate = pbuf_alloc(PBUF_RAW, second_part, PBUF_RAM);
    if(!ptr_intermediate)
    {
      PBUF_WRAPPER_LOGGER::debug("slice_move: OOM allocating %u bytes", second_part);
      return PbufWrapper();
    }
    memcpy(ptr_intermediate->payload,(u8_t*)ptr_split_buffer->payload+first_part,second_part);
    ptr_intermediate->len = second_part;
    ptr_intermediate->tot_len = second_part; // corrected later by chain func 
    ptr_intermediate->next = nullptr;

    // segment = [m_ptr_pbuf,...,prev_end|secondpart]    remainder_raw = [firstpart|secondpart] [remainder,...]
    pbuf *ptr_segment_head = m_ptr_pbuf;
    // second part is prepended later
    pbuf *ptr_remainder_raw = ptr_split_buffer->next;
    ptr_split_buffer->next = nullptr;
    ptr_split_buffer->len = first_part;
    // tot_len drops the remainder and the second part
    correct_tot_len(ptr_segment_head, second_part + (ptr_remainder_raw ? ptr_remainder_raw->tot_len : 0 ));

    // free intermediate to prevent a double free
    if(ptr_remainder_raw)
    {
      pbuf_chain(ptr_intermediate, ptr_remainder_raw);
      pbuf_free(ptr_remainder_raw);
      ptr_remainder_raw = nullptr; 
    }
    m_ptr_pbuf = ptr_intermediate;
    ptr_intermediate = nullptr;
    
    return PbufWrapper(ptr_segment_head);
  }

  void publicRelease()
  {
    if(m_ptr_pbuf)
    {
      // only free if still referenced
      if(m_ptr_pbuf->ref > 0)
      {
        auto releasedNumber = pbuf_free(m_ptr_pbuf);
        PBUF_WRAPPER_LOGGER::debug("released %d pbuf(s)", releasedNumber);
        m_ptr_pbuf = nullptr; 
      }  
    }
  }

  pbuf* detach()
  {
    pbuf* ptr_tmp = m_ptr_pbuf;
    m_ptr_pbuf = nullptr; 
    return ptr_tmp; 
  }
private:
  void release() {
    if (m_ptr_pbuf) {
      if(m_ptr_pbuf->ref > 0)
      {
        auto releasedNumber = pbuf_free(m_ptr_pbuf);
        PBUF_WRAPPER_LOGGER::debug("released %d pbuf(s)", releasedNumber);
        m_ptr_pbuf = nullptr;
      }
    }
  }

  void correct_tot_len(pbuf* ptr_head, u16_t difference)
  {
    for(pbuf* ptr_p = ptr_head; ptr_p != nullptr; ptr_p = ptr_p->next)
    {
      ptr_p->tot_len = (u16_t)(ptr_p->tot_len - difference);
    }
  }

  pbuf* m_ptr_pbuf;
};

#endif // SOMEIP_PBUFWRAPPER_HPP
