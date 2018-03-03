#if BUILD_UNIT_TESTS
#include <catch.hpp>
#include "augs/templates/history.h"

struct command_context {
	int a_value = 0;
	int b_value = 0;
};

struct A_command {
	int old_val = -1;
	int new_val = -1;

	A_command(command_context& c, const int new_val) : new_val(new_val) {
		old_val = c.a_value;
	}

	void undo(command_context& context) const {
		context.a_value = old_val;
	}

	void redo(command_context& context) const {
		context.a_value = new_val;
	}
};

struct B_command {
	int old_val;
	int new_val;

	B_command(command_context& c, const int new_val) : new_val(new_val) {
		old_val = c.b_value;
	}

	void undo(command_context& context) const {
		context.b_value = old_val;
	}

	void redo(command_context& context) const {
		context.b_value = new_val;
	}
};

#define test_mark_as_current() \
REQUIRE(hist.has_unsaved_changes()); \
hist.mark_current_revision_as_saved(); \
REQUIRE(!hist.has_unsaved_changes());

TEST_CASE("Templates History") {
	command_context context;
	augs::history<A_command, B_command> hist;

	REQUIRE(!hist.has_unsaved_changes());

	REQUIRE(context.a_value == 0);
	REQUIRE(context.b_value == 0);

	{
		hist.execute_new(A_command { context, 6 }, context);

		REQUIRE(context.a_value == 6);
		REQUIRE(context.b_value == 0);

		test_mark_as_current();
	}

	{
		hist.execute_new(A_command { context, 3 }, context);
		hist.execute_new(A_command { context, 8 }, context);

		REQUIRE(hist.has_unsaved_changes());
		REQUIRE(context.a_value == 8);
		REQUIRE(context.b_value == 0);

		hist.undo(context);

		REQUIRE(context.a_value == 3);
		REQUIRE(context.b_value == 0);

		hist.undo(context);

		REQUIRE(context.a_value == 6);

		hist.undo(context);
		hist.mark_current_revision_as_saved();

		REQUIRE(context.a_value == 0);

		hist.undo(context);

		REQUIRE(context.a_value == 0);

		hist.undo(context);

		REQUIRE(!hist.has_unsaved_changes());

		hist.redo(context);
		REQUIRE(hist.has_unsaved_changes());
		hist.undo(context);
		REQUIRE(!hist.has_unsaved_changes());
	}

	{
		hist.execute_new(B_command { context, 23 }, context);

		REQUIRE(hist.has_unsaved_changes());
		REQUIRE(context.a_value == 0);
		REQUIRE(context.b_value == 23);

		hist.execute_new(B_command { context, 58 }, context);

		REQUIRE(context.a_value == 0);
		REQUIRE(context.b_value == 58);

		hist.execute_new(A_command { context, 389 }, context);

		REQUIRE(context.a_value == 389);
		REQUIRE(context.b_value == 58);

		test_mark_as_current();

		hist.undo(context);
		hist.undo(context);
		hist.undo(context);
		hist.undo(context);
		hist.undo(context);

		REQUIRE(context.a_value == 0);
		REQUIRE(context.b_value == 0);

		hist.redo(context);
		hist.redo(context);
		hist.redo(context);
		hist.redo(context);
		hist.redo(context);

		REQUIRE(!hist.has_unsaved_changes()); 

		REQUIRE(context.a_value == 389);
		REQUIRE(context.b_value == 58);

		hist.undo(context);
		hist.undo(context);
		hist.undo(context);
		hist.undo(context);
		hist.undo(context);

		REQUIRE(context.a_value == 0);
		REQUIRE(context.b_value == 0);

		hist.execute_new(A_command { context, -2 }, context);

		REQUIRE(context.a_value == -2);
		REQUIRE(context.b_value == 0);

		// test that the other entries were erased

		hist.redo(context);
		hist.redo(context);
		hist.redo(context);
		hist.redo(context);
		hist.redo(context);

		REQUIRE(context.a_value == -2);
		REQUIRE(context.b_value == 0);

		test_mark_as_current();
	}
}

TEST_CASE("Templates HistoryUnsavedChangesTest") {
	command_context context;
	augs::history<A_command, B_command> hist;

	REQUIRE(!hist.has_unsaved_changes());

	REQUIRE(context.a_value == 0);
	REQUIRE(context.b_value == 0);

	// test that if the saved revision gets deleted,
	// we always have unsaved changes

	hist.execute_new(A_command { context, -2 }, context);

	REQUIRE(context.a_value == -2);
	REQUIRE(context.b_value == 0);

	test_mark_as_current();

	hist.undo(context);

	REQUIRE(context.a_value == 0);
	REQUIRE(context.b_value == 0);

	hist.execute_new(B_command { context, -3 }, context);

	REQUIRE(context.a_value == 0);
	REQUIRE(context.b_value == -3);

	test_mark_as_current();
}

#endif