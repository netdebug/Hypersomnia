#include "augs/templates/introspect.h"
#include "application/session_profiler.h"

/* So that we don't have to include generated/introspectors with the header */

session_profiler::session_profiler() {
	setup_names_of_measurements();
}

network_profiler::network_profiler() {
	setup_names_of_measurements();
}
