/*
Hindsight
Copyright (C) 2026 Digital Ground Game

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "check.hpp"
#include "claude-models.hpp"

#include <string>

namespace {

void finds_each_model()
{
	for (const ClaudeModel &model : kClaudeModels)
		CHECK(&claude_model(model.id) == &model);
}

void unknown_ids_get_the_default()
{
	CHECK(std::string(kDefaultClaudeModel.id) == "claude-haiku-5-5");
	CHECK(&claude_model("") == &kDefaultClaudeModel);
	CHECK(&claude_model("claude-haiku-4-5") == &kDefaultClaudeModel);
}

void every_model_has_a_label_and_prices()
{
	for (const ClaudeModel &model : kClaudeModels) {
		CHECK(std::string(model.label).size() > 0);
		CHECK(model.input_dollars_per_million > 0);
		CHECK(model.output_dollars_per_million > model.input_dollars_per_million);
	}
}

} // namespace

int main()
{
	finds_each_model();
	unknown_ids_get_the_default();
	every_model_has_a_label_and_prices();
	return check::finish();
}
