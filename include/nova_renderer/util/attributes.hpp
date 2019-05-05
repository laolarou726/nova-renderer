#pragma once

#ifndef NOVA_NODISCARD
#ifndef SWIG
#define NOVA_NODISCARD [[nodiscard]]
#else
#define NOVA_NODISCORD
#endif
#endif

#ifndef NOVA_INTERFACE
#ifndef SWIG
#define NOVA_INTERFACE [[interface]]
#else
#define NOVA_INTERFACE
#endif
#endif