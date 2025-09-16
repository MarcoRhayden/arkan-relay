#pragma once
#include "application/ports/IFrameCodec.hpp"

namespace arkan::relay::infrastructure::codec
{
class FrameCodec_Noop : public arkan::relay::application::ports::IFrameCodec
{
  // default implementations already act as no-op
};
}  // namespace arkan::relay::infrastructure::codec
