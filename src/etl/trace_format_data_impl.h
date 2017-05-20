#pragma once

namespace etl
{

struct TraceFormatData
{
  PVOID data;
  size_t length;
  bool ValidFor(size_t l) const
  {
    return data && length >= l;
  }
  template <class T>
  T As() const
  {
    return *static_cast<const T *>(data);
  }
  void Advance(size_t n)
  {
    if(n > length)
    {
      data = nullptr;
      length = 0;
    }
    else
    {
      data = static_cast<std::uint8_t *>(data) + n;
      length -= n;
    }
  }
};

} // namespace etl

