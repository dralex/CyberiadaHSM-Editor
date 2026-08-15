/* -----------------------------------------------------------------------------
 * The Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The batch (console) mode driver
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses/
 *
 * ----------------------------------------------------------------------------- */

#ifndef CYBERIADA_SM_BATCH_DRIVER
#define CYBERIADA_SM_BATCH_DRIVER

#include <QString>

class CyberiadaSMEditorApplication;

// exit codes of the batch mode (see docs/TESTING.md)
enum BatchExitCode {
	batchOK = 0,
	batchUsageError = 1,
	batchLoadError = 2,
	batchInternalError = 3
};

int runBatchMode(CyberiadaSMEditorApplication& app, const QString& fileName);

#endif
