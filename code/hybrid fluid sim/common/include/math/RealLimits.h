//[]---------------------------------------------------------------[]
//|                                                                 |
//| Copyright (C) 2018, 2019 Orthrus Group.                         |
//|                                                                 |
//| This software is provided 'as-is', without any express or       |
//| implied warranty. In no event will the authors be held liable   |
//| for any damages arising from the use of this software.          |
//|                                                                 |
//| Permission is granted to anyone to use this software for any    |
//| purpose, including commercial applications, and to alter it and |
//| redistribute it freely, subject to the following restrictions:  |
//|                                                                 |
//| 1. The origin of this software must not be misrepresented; you  |
//| must not claim that you wrote the original software. If you use |
//| this software in a product, an acknowledgment in the product    |
//| documentation would be appreciated but is not required.         |
//|                                                                 |
//| 2. Altered source versions must be plainly marked as such, and  |
//| must not be misrepresented as being the original software.      |
//|                                                                 |
//| 3. This notice may not be removed or altered from any source    |
//| distribution.                                                   |
//|                                                                 |
//[]---------------------------------------------------------------[]
//
// OVERVIEW: RealLimits.h
// ========
// Class definition for real number limits.
//
// Author: Paulo Pagliosa
// Last revision: 03/06/2019

#ifndef __RealLimits_h
#define __RealLimits_h

#include "core/Globals.h"
#include <limits>

namespace cg
{ // begin namespace cg
  
namespace math
{ // begin namespace math


/////////////////////////////////////////////////////////////////////
//
// Limits: real number limits class
// ======
template <typename real>
struct Limits
{
  HOST DEVICE
  static constexpr real eps()
  {
    return std::numeric_limits<real>::min();
  }

  HOST DEVICE
  static constexpr real inf()
  {
    return std::numeric_limits<real>::max();
  }

}; // Limits

} // end namespace math

} // end namespace cg

#endif // __RealLimits_h
