/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <git2.h>
#include "cli.h"
#include "cmd.h"
#include "error.h"

#include "path.h"
#include "futils.h"
#include "remote.h"

static const char *branch, *remote_path, *local_path;
static int show_help, quiet, checkout = 1, bare;
static bool local_path_exists;

static const cli_opt_spec opts[] = {
	{ CLI_OPT_SWITCH, "help",        0,   &show_help,     1, NULL,                 NULL,                                  CLI_OPT_USAGE_HIDDEN },
	{ CLI_OPT_SWITCH, "quiet",       'q', &quiet,         1, NULL,                 "quiet" },
	{ CLI_OPT_SWITCH, "no-checkout", 'n', &checkout,      0, NULL,                 "don't checkout HEAD" },
	{ CLI_OPT_SWITCH, "bare",        0,   &bare,          1, NULL,                 "don't create a working directory" },
	{ CLI_OPT_VALUE,  "branch",      'b', &branch,        0, "name",               "branch to check out",                 },
	{ CLI_OPT_LITERAL },
	{ CLI_OPT_ARG,    "repository",  0,   &remote_path,   0, "repository",         "repository path" },
	{ CLI_OPT_ARG,    "directory",   0,   &local_path,    0, "directory",          "directory to clone into",             CLI_OPT_USAGE_REQUIRED },
	{ 0 }
};

static int print_usage(bool error)
{
	cli_opt_usage_fprint(error ? stderr : stdout, PROGRAM_NAME, "clone", opts);
	return error ? 129 : 0;
}

static char *compute_local_path(const char *orig_path)
{
	const char *slash;
	char *local_path;

	if ((slash = strrchr(orig_path, '/')) == NULL &&
	    (slash = strrchr(orig_path, '\\')) == NULL)
		local_path = git__strdup(orig_path);
	else
		local_path = git__strdup(slash + 1);

	return local_path;
}

static bool validate_local_path(const char *path)
{
	if (!git_path_exists(path))
		return false;

	if (!git_path_isdir(path) || !git_path_is_empty_dir(path)) {
		fprintf(stderr, "fatal: destination path '%s' already exists and is not an empty directory.\n",
			path);
		exit(128);
	}

	return true;
}

static void cleanup(void)
{
	int rmdir_flags = GIT_RMDIR_REMOVE_FILES;

	if (local_path_exists)
		rmdir_flags |= GIT_RMDIR_SKIP_ROOT;

	if (!git_path_isdir(local_path))
		return;

	if (git_futils_rmdir_r(local_path, NULL, rmdir_flags) < 0)
		cli_die_git(NULL);
}

static void sighandler(int signum)
{
	GIT_UNUSED(signum);

	cleanup();
	exit(130);
}

int cmd_clone(int argc, char **argv)
{
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	cli_remote_progress progress = { 0, GIT_BUF_INIT };
	git_repository *repo;
	cli_opt invalid_opt;
	char *computed_path = NULL;

	if (cli_opt_parse(&invalid_opt, opts, argv + 1, argc - 1)) {
	        cli_opt_status_fprint(stderr, &invalid_opt);
		return print_usage(true);
	}

	if (show_help)
		return print_usage(false);

	if (!remote_path) {
		cli_error("you must specify a repository to clone.");
		return print_usage(true);
	}

	if (bare)
		clone_opts.bare = 1;

	if (branch)
		clone_opts.checkout_branch = branch;

	if (!checkout)
		clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_NONE;

	if (!local_path)
		local_path = computed_path = compute_local_path(remote_path);

	local_path_exists = validate_local_path(local_path);

	signal(SIGINT, sighandler);

	if (!local_path_exists &&
	    git_futils_mkdir(local_path, 0777, 0) < 0)
		cli_die_os("could not create directory '%s'", local_path);

	if (!quiet) {
		clone_opts.fetch_opts.callbacks.sideband_progress = cli_remote_print_progress;
		clone_opts.fetch_opts.callbacks.transfer_progress = cli_remote_print_transfer_progress;
		clone_opts.fetch_opts.callbacks.payload = &progress;

		printf("Cloning into '%s'...\n", local_path);
	}

	if (git_clone(&repo, remote_path, local_path, &clone_opts) < 0) {
		cleanup();
		cli_die_git("could not clone");
	}

	git__free(computed_path);
	git_repository_free(repo);

	return 0;
}
