#pragma once
#include <algorithm>
#include <type_traits>

#include "augs/ensure.h"
#include "augs/pad_bytes.h"

#include "augs/templates/container_traits.h"
#include "augs/templates/byte_type_for.h"
#include "augs/templates/is_variant.h"

#include "augs/readwrite/byte_readwrite_overload_traits.h"
#include "augs/readwrite/byte_readwrite_traits.h"

namespace augs {
	namespace detail {	
		template <class Archive, class Serialized>
		void read_raw_bytes(
			Archive& ar, 
			Serialized* const location, 
			const std::size_t object_count
		) {
			verify_byte_readwrite_safety<Archive, Serialized>();

			const auto byte_count = object_count * sizeof(Serialized);
			ar.read(reinterpret_cast<byte_type_for_t<Archive>*>(location), byte_count);
		}

		template <class Archive, class Serialized>
		void write_raw_bytes(
			Archive& ar, 
			const Serialized* const location, 
			const std::size_t object_count
		) {
			verify_byte_readwrite_safety<Archive, Serialized>();
			
			const auto byte_count = object_count * sizeof(Serialized);
			ar.write(reinterpret_cast<const byte_type_for_t<Archive>*>(location), byte_count);
		}
	}

	template <class Archive, class Serialized>
	void read_bytes(Archive& ar, Serialized& storage) {
		static_assert(is_byte_stream_v<Archive>, "Trying to read from a non-byte stream.");

		if constexpr(has_byte_read_overload_v<Archive, Serialized>) {
			static_assert(has_byte_write_overload_v<Archive, Serialized>, "Has read_object_bytes overload, but no write_object_bytes overload.");

			read_object_bytes(ar, storage);
		}
		else if constexpr(is_byte_readwrite_appropriate_v<Archive, Serialized>) {
			detail::read_raw_bytes(ar, &storage, 1);
		}
		else if constexpr(is_optional_v<Serialized>) {
			bool has_value = false;
			read_bytes(ar, has_value);

			if (has_value) {
				typename Serialized::value_type value;
				read_bytes(ar, value);
				storage.emplace(std::move(value));
			}
		}
		else if constexpr(is_variable_size_container_v<Serialized>) {
			read_variable_size_container(ar, storage);
		}
		else {
			verify_has_introspect(storage);

			introspect(
				[&](auto, auto& member) {
					using T = std::decay_t<decltype(member)>;
					
					if constexpr (!is_padding_field_v<T>) {
						read_bytes(ar, member);
					}
				},
				storage
			);
		}
	}

	template <class Archive, class Serialized>
	void write_bytes(Archive& ar, const Serialized& storage) {
		static_assert(is_byte_stream_v<Archive>, "Trying to write to a non-byte stream.");

		if constexpr(has_byte_write_overload_v<Archive, Serialized>) {
			static_assert(has_byte_read_overload_v<Archive, std::decay_t<Serialized>&>, "Has write_object_bytes overload, but no read_object_bytes overload.");

			write_object_bytes(ar, storage);
		}
		else if constexpr(is_byte_readwrite_appropriate_v<Archive, Serialized>) {
			detail::write_raw_bytes(ar, &storage, 1);
		}
		else if constexpr(is_optional_v<Serialized>) {
			write_bytes(ar, storage.has_value());

			if (storage) {
				write_bytes(ar, *storage);
			}
		}
		else if constexpr(is_container_v<Serialized>) {
			write_container(ar, storage);
		}
		else {
			verify_has_introspect(storage);

			introspect(
				[&](auto, const auto& member) {
					using T = std::decay_t<decltype(member)>;

					if constexpr (!is_padding_field_v<T>) {
						write_bytes(ar, member);
					}
				},
				storage
			);
		}
	}
	
	namespace detail {
		template <class Archive, class Serialized>
		void read_bytes_n(
			Archive& ar,
			Serialized* const storage,
			const std::size_t n
		) {
			if constexpr(is_byte_readwrite_appropriate_v<Archive, Serialized>) {
				detail::read_raw_bytes(ar, storage, n);
			}
			else {
				for (std::size_t i = 0; i < n; ++i) {
					augs::read_bytes(ar, storage[i]);
				}
			}
		}

		template <class Archive, class Serialized>
		void write_bytes_n(
			Archive& ar,
			const Serialized* const storage,
			const std::size_t n
		) {
			if constexpr(is_byte_readwrite_appropriate_v<Archive, Serialized>) {
				detail::write_raw_bytes(ar, storage, n);
			}
			else {
				for (std::size_t i = 0; i < n; ++i) {
					augs::write_bytes(ar, storage[i]);
				}
			}
		}
	}

	template <class Archive, class Container, class container_size_type = std::size_t>
	void read_variable_size_container(
		Archive& ar, 
		Container& storage, 
		container_size_type = container_size_type()
	) {
		container_size_type s;
		read_bytes(ar, s);

		if (s == 0) {
			return;
		}

		if constexpr(can_access_data_v<Container>) {
			storage.resize(s);
			detail::read_bytes_n(ar, storage.data(), storage.size());
		}
		else {
			if constexpr(can_reserve_v<Container>) {
				storage.reserve(s);
			}

			if constexpr(is_associative_v<Container>) {
				while (s--) {
					typename Container::key_type key{};
					typename Container::mapped_type mapped{};

					read_bytes(ar, key);
					read_bytes(ar, mapped);

					storage.emplace(std::move(key), std::move(mapped));
				}
			}
			else {
				while (s--) {
					typename Container::value_type val;

					read_bytes(ar, val);

					storage.emplace(std::move(val));
				}
			}
		}
	}

	template <class Archive, class Container, class container_size_type = std::size_t>
	void write_container(
		Archive& ar, 
		const Container& storage, 
		container_size_type = {}
	) {
		const auto s = storage.size();
		ensure(s <= std::numeric_limits<container_size_type>::max());
		write_bytes(ar, static_cast<container_size_type>(s));

		if constexpr(can_access_data_v<Container>) {
			detail::write_bytes_n(ar, storage.data(), storage.size());
		}
		else {
			for (const auto& obj : storage) {
				write_bytes(ar, obj);
			}
		}
	}

	template <class Archive, class Container, class container_size_type = std::size_t>
	void read_capacity(
		Archive& ar, 
		Container& storage,
		container_size_type = {}
	) {
		container_size_type c;
		read_bytes(ar, c);
		storage.reserve(c);
	}

	template<class Archive, class Container, class container_size_type = std::size_t>
	void write_capacity(Archive& ar, const Container& storage) {
		const auto c = static_cast<container_size_type>(storage.capacity());
		ensure(c <= std::numeric_limits<container_size_type>::max());
		write_bytes(ar, c);
	}

	template<class Archive, std::size_t count>
	void read_flags(Archive& ar, std::array<bool, count>& storage) {
		static_assert(count > 0, "Can't read_bytes a null array");

		std::array<std::byte, (count - 1) / 8 + 1> compressed_storage;
		read_bytes(ar, compressed_storage);

		for (std::size_t bit = 0; bit < count; ++bit) {
			storage[bit] = std::to_integer<int>((compressed_storage[bit / 8] >> (bit % 8)) & static_cast<std::byte>(1)) != 0;
		}
	}

	template<class Archive, std::size_t count>
	void write_flags(Archive& ar, const std::array<bool, count>& storage) {
		static_assert(count > 0, "Can't write_bytes a null array");

		std::array<std::byte, (count - 1) / 8 + 1> compressed_storage;
		std::fill(compressed_storage.begin(), compressed_storage.end(), static_cast<std::byte>(0));

		for (std::size_t bit = 0; bit < count; ++bit) {
			if (storage[bit]) {
				compressed_storage[bit / 8] |= static_cast<std::byte>(1 << (bit % 8));
			}
		}

		write_bytes(ar, compressed_storage);
	}
}