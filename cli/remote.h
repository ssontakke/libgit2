/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef CLI_remote_h__
#define CLI_remote_h__

typedef enum {
	CLI_REMOTE_PHASE_NONE,
	CLI_REMOTE_PHASE_RECEIVING,
	CLI_REMOTE_PHASE_RESOLVING,
	CLI_REMOTE_PHASE_DONE,
} cli_remote_phase;

typedef struct {
	cli_remote_phase phase;

	git_buf cur_line;
	size_t last_len;

	git_time_t receive_start;
	git_time_t receive_finish;
} cli_remote_progress;

extern int cli_remote_print_progress(const char *str, int len, void *data);
extern int cli_remote_print_transfer_progress(const git_indexer_progress *stats, void *data);

#endif /* CLI_remote_h__ */
