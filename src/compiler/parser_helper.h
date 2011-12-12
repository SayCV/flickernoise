/*
 * Milkymist SoC (Software)
 * Copyright (C) 2007, 2008, 2009, 2010 Sebastien Bourdeauducq
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PARSER_HELPER_H
#define __PARSER_HELPER_H

#include <fpvm/ast.h>
#include <fpvm/fpvm.h>

struct parser_comm {
	union {
		struct ast_node *parseout;
		struct fpvm_fragment *fragment;
	} u;
	const char *(*assign_default)(struct parser_comm *comm,
	    const char *label, struct ast_node *node);
	const char *(*assign_per_frame)(struct parser_comm *comm,
	    const char *label, struct ast_node *node);
	const char *(*assign_per_vertex)(struct parser_comm *comm,
	    const char *label, struct ast_node *node);
};

const char *fpvm_parse(const char *expr, int start_token,
    struct parser_comm *comm);
void fpvm_parse_free(struct ast_node *node);

#endif /* __PARSER_HELPER_H */
