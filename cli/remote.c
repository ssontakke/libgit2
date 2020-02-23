/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "buffer.h"
#include "remote.h"

static size_t linelen(const char *str, int len)
{
	int i;

	for (i = 0; i < len && str[i]; ++i) {
		if (str[i] == '\r' || str[i] == '\n')
			return i+1;
	}

	return i;
}

static int progress_printf(cli_remote_progress *progress, const char *fmt, ...)
{
	va_list ap;
	size_t i;

	va_start(ap, fmt);
	git_buf_vprintf(&progress->cur_line, fmt, ap);
	va_end(ap);

	if (git_buf_oom(&progress->cur_line))
		return -1;

	if (progress->cur_line.size == 0)
		return 0;

	if (progress->cur_line.ptr[progress->cur_line.size-1] == '\r' ||
		progress->cur_line.ptr[progress->cur_line.size-1] == '\n') {
		printf("%.*s", (int)(progress->cur_line.size-1), progress->cur_line.ptr);

		for (i = progress->cur_line.size; i < progress->last_len; ++i)
			printf(" ");

		printf("%c", progress->cur_line.ptr[progress->cur_line.size-1]);

		fflush(stdout);

		if (progress->cur_line.ptr[progress->cur_line.size-1] == '\r')
			progress->last_len = progress->cur_line.size;
		else
			progress->last_len = 0;

		git_buf_clear(&progress->cur_line);
	}

	return 0;
}

int cli_remote_print_progress(const char *str, int len, void *data)
{
	cli_remote_progress *progress = data;
	int line_len;

	while(len > 0) {
		line_len = linelen(str, len);

		if (line_len > 0 && git_buf_len(&progress->cur_line) == 0)
			progress_printf(progress, "remote: ");

		progress_printf(progress, "%.*s", line_len, str);

		if (git_buf_oom(&progress->cur_line))
			return -1;

		str += line_len;
		len -= line_len;
	}

	return 0;
}

static int progress_percent(int completed, int total)
{
	if (total == 0)
		return (completed == 0) ? 100 : 0;

	return (int)(((double)completed / (double)total) * 100);
}

int cli_remote_print_transfer_progress(const git_indexer_progress *stats, void *data)
{
	char *recv_units[] = { "B", "KiB", "MiB", "GiB", "TiB", NULL };
	char *rate_units[] = { "B/s", "KiB/s", "MiB/s", "GiB/s", "TiB/s", NULL };

	cli_remote_progress *progress = data;
	double recv_len, rate, elapsed;
	size_t recv_unit_idx = 0, rate_unit_idx = 0;
	bool done = false;

	if (progress->phase == CLI_REMOTE_PHASE_NONE) {
		progress->phase = CLI_REMOTE_PHASE_RECEIVING;
		progress->receive_start = time(NULL);
	}

	if (progress->phase == CLI_REMOTE_PHASE_RECEIVING) {
		recv_len = stats->received_bytes;

		elapsed = (double)(time(NULL) - progress->receive_start);
		rate = elapsed ? recv_len / elapsed : 0;

		while (recv_len > 1024 && recv_units[recv_unit_idx+1]) {
			recv_len /= 1024;
			recv_unit_idx++;
		}

		while (rate > 1024 && rate_units[rate_unit_idx+1]) {
			rate /= 1024;
			rate_unit_idx++;
		}

		if (stats->received_objects == stats->total_objects) {
			progress->phase = CLI_REMOTE_PHASE_RESOLVING;
			progress->receive_finish = time(NULL);
			done = true;
		}

		progress_printf(progress,
			"Receiving objects: %3d%% (%d/%d), %.2f %s | %.2f %s%s",
			progress_percent(stats->received_objects, stats->total_objects),
			stats->received_objects,
			stats->total_objects,
			recv_len, recv_units[recv_unit_idx],
			rate, rate_units[rate_unit_idx],
			done ? ", done.\n" : "\r");
	} else if (progress->phase == CLI_REMOTE_PHASE_RESOLVING) {
		if (stats->indexed_deltas == stats->total_deltas) {
			progress->phase = CLI_REMOTE_PHASE_DONE;
			done = true;
		}

		progress_printf(progress,
			"Resolving deltas: %3d%% (%d/%d)%s",
			progress_percent(stats->indexed_deltas, stats->total_deltas),
			stats->indexed_deltas, stats->total_deltas,
			done ? ", done.\n" : "\r");
	}
	return 0;
}
