#ifndef LE_RESOURCE_HANDLE_T_INL
#define LE_RESOURCE_HANDLE_T_INL

#include <stdint.h>
#include <string>
#include <le_renderer/le_renderer.h>

struct le_resource_handle_data_t {
	LeResourceType type;
	enum FlagBits : uint8_t {
		eIsUnset   = 0,
		eIsVirtual = 1u << 0,
		eIsStaging = 1u << 1,
	};
	uint8_t               num_samples      = 0;       // number of samples log 2 if image
	uint8_t               flags            = 0;       // used for buffer resources: staging or virtual
	uint16_t              index            = 0;       // index if virtual buffer
	le_resource_handle_t *reference_handle = nullptr; // if auto-generated from another handle, we keep a reference to the parent.
	std::string           debug_name;

	bool operator==( le_resource_handle_data_t const &rhs ) const noexcept {
		return type == rhs.type &&
		       num_samples == rhs.num_samples &&
		       flags == rhs.flags &&
		       index == rhs.index &&
		       reference_handle == rhs.reference_handle &&
		       debug_name == rhs.debug_name;
	}
	bool operator!=( le_resource_handle_data_t const &rhs ) const noexcept {
		return !operator==( rhs );
	}
};

// FIXME: we need an equality operator for this.

// todo: use fnvhash

struct le_resource_handle_data_hash {

	static constexpr uint64_t FNV1A_PRIME_64_CONST = 0x100000001b3;

	inline uint64_t operator()( le_resource_handle_data_t const &key ) const noexcept {
		uint64_t hash = ( uint64_t )key.reference_handle;

		uint8_t value = key.num_samples;
		hash          = hash ^ value;
		hash          = hash * FNV1A_PRIME_64_CONST;

		value = key.flags;
		hash  = hash ^ value;
		hash  = hash * FNV1A_PRIME_64_CONST;

		value = ( key.index >> 8 ) & 0xff;
		hash  = hash ^ value;
		hash  = hash * FNV1A_PRIME_64_CONST;

		value = key.index & 0xff;
		hash  = hash ^ value;
		hash  = hash * FNV1A_PRIME_64_CONST;

		if ( !key.debug_name.empty() ) {
			for ( char const *i = key.debug_name.c_str(); *i != 0; ++i ) {
				uint8_t value = static_cast<const uint8_t &>( *i );
				hash          = hash ^ value;
				hash          = hash * FNV1A_PRIME_64_CONST;
			}
		}
		return hash;
	}
};

struct le_resource_handle_t {
	le_resource_handle_data_t data;
};

struct le_img_resource_handle_t : le_resource_handle_t {
};
struct le_buf_resource_handle_t : le_resource_handle_t {
};
struct le_blas_resource_handle_t : le_resource_handle_t {
};
struct le_tlas_resource_handle_t : le_resource_handle_t {
};

#endif