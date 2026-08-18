#pragma once

// PBR Demo 对外固定使用小写 rhi 命名空间。RHI 核心目前声明为 RHI，
// 而基本整数别名位于全局命名空间；这个适配头把命名差异限制在 Demo 目录内，
// 避免改动公共 RHI API。
#include "AliasDefinitions.hpp"
#include "Math.hpp"
#include "RHI/Device/RHIDevice.hpp"
#include "RHI/Device/RHIDeviceFactory.hpp"
#include "RHI/UI/RHIUI.hpp"

namespace rhi {
using namespace RHI;
using ::i8;
using ::i16;
using ::i32;
using ::i64;
using ::u8;
using ::u16;
using ::u32;
using ::u64;
namespace ui = RHI::UI;
} // namespace rhi

namespace math = Math;
