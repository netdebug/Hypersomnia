#pragma once

typedef unsigned int GLuint;

namespace augs {
	class renderer;

	namespace graphics {
		class fbo {
			friend class ::augs::renderer;
			GLuint fboId, textureId, width, height;
			bool created;
			fbo& operator=(const fbo&) {}

			static GLuint currently_bound_fbo;
		public:
			fbo(); ~fbo();
			fbo(int width, int height);
			void create(int width, int height);
			void destroy();

			void use() const;
			void guarded_use() const;

			int get_width() const, get_height() const;
			GLuint get_texture_id() const;

			static void use_default();
		};
	}
}