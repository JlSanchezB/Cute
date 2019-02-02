//////////////////////////////////////////////////////////////////////////
// Cute engine - Common definition shared in the render system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_COMMON_H_
#define RENDER_COMMON_H_

#include <core/string_hash.h>

namespace render
{
	using RenderClassType = StringHash32<"RenderClassType"_namespace>;
	using ResourceName = StringHash32<"ResourceName"_namespace>;
	using PassName = StringHash32<"PassName"_namespace>;
}


#endif //RENDER_COMMON_H_
