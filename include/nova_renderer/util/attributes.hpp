#pragma once

#ifndef NOVA_NODISCARD
#ifndef SWIG
#define NOVA_NODISCARD [[nodiscard]]
#else
#define NOVA_NODISCORD
#endif
#endif

#ifndef NOVA_INTERFACE
#if !defined(SWIG) && !defined(_MSVC_VER)
#define NOVA_INTERFACE [[nova::interface]]
#else
#define NOVA_INTERFACE
#endif
#endif